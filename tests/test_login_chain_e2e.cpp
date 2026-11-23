// 登录全链路 e2e（L174）：认证 → gateway 会话 → Base 经 database 激活 →
// World 准入/分配 → cell 创建实体。
//
// 进程内真实现直编（login_m1_admission / gateway_base_activation /
// gateway_push_downlink 各缝已分别验收——本批把四段串成一条真实链路）：
//   1. base = StoreHost（PlayerStore M1 面）+ 播种档案；
//   2. login = LoginServer（m1Enabled 准入面 + worldStoreM1Port=base 面 +
//      OnlineDirectory 内建目录——World 准入分配落目录）；
//   3. world+cell = CellHost（WorldApp 管理面 + cell 注册客户端）；
//   4. gateway = GatewayServer（loginM1Port=login 准入面 + baseM1Port=base
//      面 + worldM1Port=world 管理面）；
//   5. 真实 TCP 客户端连 gateway → PING 建会话 → login 端 LOGIN_REQUEST
//      认证签发 loginTicket（真实会话）→ gateway.authenticateSession 经
//      login M1 准入 + base 激活 + 目录 World 准入 → 客户端发
//      CELL_CREATE_ENTITY → gateway → world → cell 创建实体。
// 断言事实源：cell.resolveIntentPlayer(会话) == 玩家号（实体入 AOI）、
// base 锚点 Online + SessionBinding 已写、login 会话有效。
// Windows：link TCP/M1 底座不进批，用例本体空转。

#include "gateway/gateway_server.hpp"
#include "login/login_server.hpp"
#include "world_app/world_app.hpp"
#include "cell_host.hpp"
#include "store_host.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

namespace {

namespace netlink = apollo::net::link;
namespace protocol = apollo::protocol;
namespace wire = apollo::net::wire;

#if !defined(_WIN32)

const auto kTick = std::chrono::milliseconds(2);

const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

using apollo::contract::gen::MsgId;
constexpr std::uint16_t id_of(MsgId id) { return static_cast<std::uint16_t>(id); }

template <typename Pred>
bool wait_until(Pred pred, int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited += 2) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(kTick);
    }
    return pred();
}

std::uint16_t probe_free_port() {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    std::uint16_t port = 0;
    if (fd >= 0) {
        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (::bind(fd, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) == 0) {
            socklen_t len = sizeof addr;
            if (::getsockname(fd, reinterpret_cast<::sockaddr*>(&addr), &len) == 0) {
                port = ntohs(addr.sin_port);
            }
        }
        ::close(fd);
    }
    return port;
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-login-chain-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

std::vector<std::uint8_t> frame_bytes(std::uint32_t seq, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    netlink::append_frame(out, seq, flags, payload);
    return out;
}

class TcpTestClient {
public:
    ~TcpTestClient() { close(); }

    bool connect(const std::string& host, std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
        ::sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1 ||
            ::connect(fd_, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) != 0) {
            ::close(fd_);
            fd_ = -1;
            return false;
        }
        return true;
    }

    void sendAll(const std::vector<std::uint8_t>& bytes) {
        std::size_t off = 0;
        while (off < bytes.size()) {
            const ssize_t n = ::send(fd_, bytes.data() + off, bytes.size() - off, 0);
            if (n <= 0) {
                if (n < 0 && errno == EINTR) continue;
                return;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    bool recvLinkFrame(std::vector<std::uint8_t>& payload) {
        for (;;) {
            if (!pending_.empty()) {
                payload = std::move(pending_.front());
                pending_.pop_front();
                return true;
            }
            if (!fill()) return false;
            const auto outcome = parser_.feed(buf_.data(), buf_.size());
            if (outcome.fatal) return false;
            buf_.clear();
            for (auto& frame : outcome.frames) {
                pending_.push_back(std::move(frame.payload));
            }
        }
    }

    void close() {
        if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
    }

private:
    bool fill() {
        std::uint8_t chunk[4096];
        const ssize_t n = ::recv(fd_, chunk, sizeof chunk, 0);
        if (n <= 0) return false;
        buf_.insert(buf_.end(), chunk, chunk + n);
        return true;
    }
    int fd_ = -1;
    std::vector<std::uint8_t> buf_;
    netlink::FrameParser parser_;
    std::deque<std::vector<std::uint8_t>> pending_;
};

protocol::MessageHeader header_of(const std::vector<std::uint8_t>& frame) {
    return protocol::MessageCodec::parseHeader(frame.data(), frame.size());
}

protocol::CellCreateEntity make_player_spawn(std::uint64_t player_id,
                                             std::uint64_t space_id = 0) {
    protocol::CellCreateEntity msg;
    msg.entityId = player_id;
    msg.entityType = protocol::EntityType::PLAYER;
    msg.spaceId = space_id;
    return msg;
}

// ---- base / login / world+cell / gateway 装配 ----

player_store::PlayerStoreConfig base_store_config(const std::filesystem::path& dir) {
    player_store::PlayerStoreConfig config;
    config.host = "127.0.0.1";
    config.m1Enabled = true;
    config.m1Port = 0;
    config.m1SelfName = "base";
    config.m1RouteJson = kRouteJson;
    config.dataDir = (dir / "players").string();
    config.journalPath = (dir / "journal/persist.log").string();
    return config;
}

login::LoginConfig login_config() {
    login::LoginConfig config;
    config.host = "127.0.0.1";
    config.m1Enabled = true;
    config.m1Port = 0;                 // 准入面内核分配（admissionPort 读回）
    config.m1RouteJson = kRouteJson;
    config.m1OnlineDirectoryHost = "127.0.0.1";
    config.directoryEnabled = true;    // 内建 OnlineDirectory（World 准入分配）
    config.directoryPort = 0;          // 内核分配
    config.directoryEpochPath.clear(); // 不附持久化（进程内链路不涉盘）
    return config;
}

gateway::GatewayConfig gateway_config(std::uint16_t world_m1_port,
                                      std::uint16_t login_m1_port,
                                      std::uint16_t base_m1_port,
                                      std::uint32_t gateway_id = 77) {
    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = probe_free_port();
    config.sessionTimeoutMs = 600000;
    config.heartbeatCheckIntervalMs = 50;
    config.controlM1Port = 0;
    config.worldM1Host = "127.0.0.1";
    config.worldM1Port = world_m1_port;
    config.worldM1RouteJson = kRouteJson;
    config.loginM1Host = "127.0.0.1";
    config.loginM1Port = login_m1_port;
    config.baseM1Host = "127.0.0.1";
    config.baseM1Port = base_m1_port;
    config.gatewayId = gateway_id;
    return config;
}

// ---- 完整链路：真实 TCP 客户端 → gateway → (login 准入 / base 激活 /
//      world+cell) ----
bool login_chain_creates_cell_entity() {
    TemporaryDirectory files;

    // ① base：PlayerStore M1 面 + 播种 player1（login 认证用户 1001）。
    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    TEST_ASSERT(base.store->adoptPlayer(1001, "", 0, 0, 2), "seed player 1001 profile");

    // ② login：准入面 + base 客户端面 + 内建 OnlineDirectory（World 准入）。
    auto lconfig = login_config();
    lconfig.worldStoreM1Host = "127.0.0.1";
    lconfig.worldStoreM1Port = base.store->m1_listen_port();
    login::LoginServer login(lconfig);
    login.start();
    TEST_ASSERT(login.admissionPort() != 0, "login admission face up");
    TEST_ASSERT(login.onlineDirectory() != nullptr, "login online directory up");
    TEST_ASSERT(wait_until([&] { return login.baseActivationReady(); }, 3000),
                "login base activation link up");

    // ⑥a 占一个 login 会话号（dummy）：使后续真实登录的 login 会话号 ≠
    // gateway 会话号（两者编号空间独立）——证明准入以 ticket 跨端凭证为
    // 准，不依赖编号巧合对齐。
    protocol::LoginRequest dummyReq;
    dummyReq.username = "player1";
    dummyReq.password = "pass1";
    const auto dummyResp = login.dispatchRequest(
        protocol::MessageCodec::encode(dummyReq, 0));
    TEST_ASSERT(dummyResp.size() > sizeof(protocol::MessageHeader), "dummy login response");
    protocol::LoginResponse parsedDummy;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(dummyResp.begin() + sizeof(protocol::MessageHeader),
                                  dummyResp.end()), parsedDummy);
    TEST_ASSERT(parsedDummy.success, "dummy login accepted");

    // ③ world + cell：管理面 + cell 注册客户端。
    world_app::WorldAppConfig wconfig;
    wconfig.worldId = 1;
    wconfig.host = "127.0.0.1";
    wconfig.m1Port = 0;
    wconfig.m1RouteJson = kRouteJson;
    wconfig.tickRateMs = 20;
    auto world = std::make_unique<world_app::WorldApp>(wconfig);
    world->start();
    TEST_ASSERT(world->isRunning(), "world starts");
    cell_test::CellHost cell;
    cell.config = wconfig;
    TEST_ASSERT(cell.start(world->m1Link().listenPort(), 21, 0, &world->m1Link()),
                "cell host starts");
    cell.set_world_link(&world->m1Link());
    cell.pump_until_registered();
    TEST_ASSERT(cell.registered(), "cell registered with world");

    // ④ gateway：login 准入 + base 激活 + world M1 三面全接。
    auto gconfig = gateway_config(world->m1Link().listenPort(),
                                  login.admissionPort(),
                                  base.store->m1_listen_port());
    gateway::GatewayServer gateway(gconfig);
    gateway.start();

    // ⑤ 真实客户端连 gateway → PING 建会话。
    TcpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", gconfig.port), "tcp client connects");
    protocol::Ping ping;
    ping.timestamp = 1;
    client.sendAll(frame_bytes(1, 0,
        protocol::MessageCodec::encode(ping, 0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(client.recvLinkFrame(reply), "ping pong received");
    const auto session_id = header_of(reply).sessionId;
    TEST_ASSERT(session_id != 0, "gateway session created");

    // 布局/路由代次 stamp 前置（L120 拓扑+路由生产者接线）：login 的初始
    // 落点（preparePlayerOnline 在 LOGIN_REQUEST 处理时写目录）从目录登记表
    // 取布局代次与位置路由水位——须在 login 请求前注入 (world=1, space=1)
    // 的 968 投影（拓扑 71 / 归属版本 5），否则登录时登记表仍空 = 未声明 0。
    // 归属版本用 5（≠ 旧硬编码 1）：route_version 断言靠它分辨真生产者。
    login::SpacePositionProjection proj;
    proj.world_id = 1;
    proj.space_id = 1;
    proj.cell_id = 21;
    proj.assignment_version = 5;
    proj.topology_version = 71;
    proj.management_epoch = 3;
    proj.ready = true;
    proj.cell_host = "10.0.0.9";  // L122 直连端点发现面：归属 cell M1 监听端点
    proj.cell_port = 9200;
    TEST_ASSERT(login.positionProjectionRegistry()->apply_projection(proj, 0) ==
                    login::ProjectionVerdict::kApplied,
                "projection injected before login");

    // ⑥ login 端认证签发 loginTicket（真实会话：player1/pass1 → 1001；
    // 此时 login 会话号 = 2，gateway 会话号 = 1，编号不同）。
    protocol::LoginRequest loginReq;
    loginReq.username = "player1";
    loginReq.password = "pass1";
    const auto loginResp = login.dispatchRequest(
        protocol::MessageCodec::encode(loginReq, 0));
    TEST_ASSERT(loginResp.size() > sizeof(protocol::MessageHeader), "login response");
    const auto lh = header_of(loginResp);
    TEST_ASSERT(lh.type == static_cast<std::uint16_t>(protocol::MessageType::LOGIN_RESPONSE),
                "login response type");
    protocol::LoginResponse parsedLogin;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(loginResp.begin() + sizeof(protocol::MessageHeader),
                                  loginResp.end()), parsedLogin);
    TEST_ASSERT(parsedLogin.success, "login accepted");
    TEST_ASSERT(parsedLogin.playerId == 1001, "login player id");
    TEST_ASSERT(!parsedLogin.loginTicket.empty(), "login ticket issued");
    TEST_ASSERT(parsedLogin.gatewayHost == "127.0.0.1", "gateway host in response");
    TEST_ASSERT(parsedLogin.gatewayPort != 0, "gateway port in response");

    // ⑦ gateway 准入（经 login M1 真实校验 ticket）+ base 激活 + 目录 World 准入。
    TEST_ASSERT(wait_until([&] { return gateway.baseActivationReady(); }, 3000),
                "base activation link up");
    // 假 ticket 必须被 login 真准入面拒（面在场 = 无权威确认不放行）。
    TEST_ASSERT(!gateway.authenticateSession(session_id, 1001, "t-bogus"),
                "bogus login ticket rejected");
    TEST_ASSERT(gateway.authenticateSession(session_id, 1001, parsedLogin.loginTicket),
                "gateway auth accepted with real login ticket");
    // 登录落点携带登记表布局代次（不再恒 0 未声明）。
    const auto assigned =
        login.onlineDirectory()->resolveWorldAssignment(1001);
    TEST_ASSERT(assigned.has_value(), "assignment recorded");
    TEST_ASSERT(assigned->topology_version == 71,
                "admission stamps layout epoch from projection registry");
    // 路由版本不再是假值 1：= 投影归属版本水位（L120 路由生产者接线）。
    TEST_ASSERT(assigned->route_version == 5,
                "admission stamps position route watermark from projection registry");
    // 落点携带归属 cell 直连端点（L122 网关→cell 直连端点发现面）。
    TEST_ASSERT(assigned->cell_host == "10.0.0.9" && assigned->cell_port == 9200,
                "admission stamps direct cell endpoint from projection registry");

    // ⑧ 客户端发 CELL_CREATE_ENTITY → gateway → world → cell 创建实体。
    client.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(1001), session_id)));
    TEST_ASSERT(wait_until(
                    [&] { return cell.resolveIntentPlayer(session_id) == 1001; },
                    8000),
                "cell spawned player entity");

    // 断言 base 锚点 Online + SessionBinding 已写（gateway 激活面已做）。
    const auto avatar = base.store->findAvatar(1001);
    TEST_ASSERT(avatar != nullptr, "avatar loaded at base");
    TEST_ASSERT(avatar->state() == apollo::game::base::AvatarState::Online,
                "avatar online after chain");
    TEST_ASSERT(avatar->session_binding().is_bound(), "session binding written");
    TEST_ASSERT(avatar->session_binding().session_id == session_id,
                "binding carries gateway session id");
    TEST_ASSERT(avatar->session_binding().gateway_id == gconfig.gatewayId,
                "binding carries gateway process id");

    gateway.stop();
    cell.stop();
    world->stop();
    login.stop();
    return true;
}

// D10 用例 1（L125 多网关身份批）：两网关并行在线（gatewayId 77/88），两
// 客户端同时登录。缺口三项在此实测：
//   1. 网关 M1 self_name 按进程区分（"gateway-77"/"gateway-88"）——World
//      被动端对端槽位不再互挤（旧恒 "gateway" 时第二网关挤掉第一网关）。
//   2. 会话/请求号身份域基址分家（gateway_id<<32|1）——两网关首个会话
//      均号 1 亦合法（跨网关身份 = gateway_id + session_id），不互覆。
//   3. World 按 932 入站对端记 会话/请求 → 网关 peer，下行/回执按所属
//      网关寻路（非单租户 gateway_link）。
// 断言：两会话各自有效、实体各自入 AOI、base 锚点各自绑定所属网关。
bool login_chain_two_gateway_concurrent() {
    TemporaryDirectory files;

    // ① base：PlayerStore M1 面 + 播种 player1/player2。
    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    TEST_ASSERT(base.store->adoptPlayer(1001, "", 0, 0, 2), "seed player 1001 profile");
    TEST_ASSERT(base.store->adoptPlayer(1002, "", 0, 0, 2), "seed player 1002 profile");

    // ② login：准入面 + base 客户端面 + 内建 OnlineDirectory。
    auto lconfig = login_config();
    lconfig.worldStoreM1Host = "127.0.0.1";
    lconfig.worldStoreM1Port = base.store->m1_listen_port();
    login::LoginServer login(lconfig);
    login.start();
    TEST_ASSERT(login.admissionPort() != 0, "login admission face up");
    TEST_ASSERT(login.onlineDirectory() != nullptr, "login online directory up");
    TEST_ASSERT(wait_until([&] { return login.baseActivationReady(); }, 3000),
                "login base activation link up");

    // ③ world + cell：管理面 + cell 注册客户端（两网关共宿同一 world/cell）。
    world_app::WorldAppConfig wconfig;
    wconfig.worldId = 1;
    wconfig.host = "127.0.0.1";
    wconfig.m1Port = 0;
    wconfig.m1RouteJson = kRouteJson;
    wconfig.tickRateMs = 20;
    auto world = std::make_unique<world_app::WorldApp>(wconfig);
    world->start();
    TEST_ASSERT(world->isRunning(), "world starts");
    cell_test::CellHost cell;
    cell.config = wconfig;
    TEST_ASSERT(cell.start(world->m1Link().listenPort(), 21, 0, &world->m1Link()),
                "cell host starts");
    cell.set_world_link(&world->m1Link());
    cell.pump_until_registered();
    TEST_ASSERT(cell.registered(), "cell registered with world");

    // 布局/路由代次投影（同单网关批）：两玩家同落 (world=1, space=1, cell=21)。
    login::SpacePositionProjection proj;
    proj.world_id = 1;
    proj.space_id = 1;
    proj.cell_id = 21;
    proj.assignment_version = 5;
    proj.topology_version = 71;
    proj.management_epoch = 3;
    proj.ready = true;
    proj.cell_host = "10.0.0.9";
    proj.cell_port = 9200;
    TEST_ASSERT(login.positionProjectionRegistry()->apply_projection(proj, 0) ==
                    login::ProjectionVerdict::kApplied,
                "projection injected before login");

    // ④ 两网关（gatewayId 77/88）：各自 login 准入 + base 激活 + world M1。
    auto g1config = gateway_config(world->m1Link().listenPort(),
                                   login.admissionPort(), base.store->m1_listen_port(), 77);
    auto g2config = gateway_config(world->m1Link().listenPort(),
                                   login.admissionPort(), base.store->m1_listen_port(), 88);
    gateway::GatewayServer g1(g1config);
    gateway::GatewayServer g2(g2config);
    g1.start();
    g2.start();

    // ⑤ 两客户端分别连两网关 → PING 建会话。
    TcpTestClient c1;
    TcpTestClient c2;
    TEST_ASSERT(c1.connect("127.0.0.1", g1config.port), "client1 connects to g1");
    TEST_ASSERT(c2.connect("127.0.0.1", g2config.port), "client2 connects to g2");
    protocol::Ping ping;
    ping.timestamp = 1;
    c1.sendAll(frame_bytes(1, 0, protocol::MessageCodec::encode(ping, 0)));
    std::vector<std::uint8_t> reply1;
    TEST_ASSERT(c1.recvLinkFrame(reply1), "ping pong received on g1");
    const auto session1 = header_of(reply1).sessionId;
    c2.sendAll(frame_bytes(1, 0, protocol::MessageCodec::encode(ping, 0)));
    std::vector<std::uint8_t> reply2;
    TEST_ASSERT(c2.recvLinkFrame(reply2), "ping pong received on g2");
    const auto session2 = header_of(reply2).sessionId;
    TEST_ASSERT(session1 != 0 && session2 != 0, "both gateway sessions created");
    // 身份域基址分家：会话号携带网关身份域（gateway_id<<32），跨网关不互撞。
    TEST_ASSERT(session1 != session2, "sessions across gateways are distinct");

    // ⑥ login 端认证签发两票（player1→1001，player2→1002）。
    protocol::LoginRequest loginReq1;
    loginReq1.username = "player1";
    loginReq1.password = "pass1";
    const auto loginResp1 = login.dispatchRequest(
        protocol::MessageCodec::encode(loginReq1, 0));
    TEST_ASSERT(loginResp1.size() > sizeof(protocol::MessageHeader), "login1 response");
    protocol::LoginResponse parsedLogin1;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(loginResp1.begin() + sizeof(protocol::MessageHeader),
                                  loginResp1.end()), parsedLogin1);
    TEST_ASSERT(parsedLogin1.success && parsedLogin1.playerId == 1001,
                "login1 accepted for 1001");
    protocol::LoginRequest loginReq2;
    loginReq2.username = "player2";
    loginReq2.password = "pass2";
    const auto loginResp2 = login.dispatchRequest(
        protocol::MessageCodec::encode(loginReq2, 0));
    TEST_ASSERT(loginResp2.size() > sizeof(protocol::MessageHeader), "login2 response");
    protocol::LoginResponse parsedLogin2;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(loginResp2.begin() + sizeof(protocol::MessageHeader),
                                  loginResp2.end()), parsedLogin2);
    TEST_ASSERT(parsedLogin2.success && parsedLogin2.playerId == 1002,
                "login2 accepted for 1002");
    TEST_ASSERT(!parsedLogin1.loginTicket.empty() && !parsedLogin2.loginTicket.empty(),
                "both login tickets issued");

    // ⑦ 两网关并行准入（各自经 login M1 真校验 + base 激活 + 目录 World 准入）。
    TEST_ASSERT(wait_until([&] { return g1.baseActivationReady(); }, 3000),
                "g1 base activation link up");
    TEST_ASSERT(wait_until([&] { return g2.baseActivationReady(); }, 3000),
                "g2 base activation link up");
    TEST_ASSERT(g1.authenticateSession(session1, 1001, parsedLogin1.loginTicket),
                "g1 auth accepted for 1001");
    TEST_ASSERT(g2.authenticateSession(session2, 1002, parsedLogin2.loginTicket),
                "g2 auth accepted for 1002");
    const auto assigned1 = login.onlineDirectory()->resolveWorldAssignment(1001);
    const auto assigned2 = login.onlineDirectory()->resolveWorldAssignment(1002);
    TEST_ASSERT(assigned1.has_value() && assigned2.has_value(), "both assignments recorded");
    TEST_ASSERT(assigned1->topology_version == 71 && assigned1->route_version == 5,
                "assignment1 stamps layout/route epoch");
    TEST_ASSERT(assigned2->topology_version == 71 && assigned2->route_version == 5,
                "assignment2 stamps layout/route epoch");

    // ⑧ 两客户端各自发 CELL_CREATE_ENTITY → 各自网关 → world → cell。
    c1.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(1001), session1)));
    c2.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(1002), session2)));
    // 互不覆盖：两会话各自解析到各自玩家（旧单键槽位时 1/2 相撞）。
    TEST_ASSERT(wait_until(
                    [&] { return cell.resolveIntentPlayer(session1) == 1001; }, 8000),
                "cell spawned player 1001 (g1 session)");
    TEST_ASSERT(wait_until(
                    [&] { return cell.resolveIntentPlayer(session2) == 1002; }, 8000),
                "cell spawned player 1002 (g2 session)");

    // ⑨ base 锚点各自 Online + 各自绑定所属网关。
    const auto avatar1 = base.store->findAvatar(1001);
    const auto avatar2 = base.store->findAvatar(1002);
    TEST_ASSERT(avatar1 != nullptr && avatar2 != nullptr, "both avatars loaded at base");
    TEST_ASSERT(avatar1->state() == apollo::game::base::AvatarState::Online &&
                avatar2->state() == apollo::game::base::AvatarState::Online,
                "both avatars online after chain");
    TEST_ASSERT(avatar1->session_binding().is_bound() &&
                avatar2->session_binding().is_bound(),
                "both session bindings written");
    TEST_ASSERT(avatar1->session_binding().session_id == session1 &&
                avatar1->session_binding().gateway_id == 77,
                "binding1 carries g1 session/gateway id");
    TEST_ASSERT(avatar2->session_binding().session_id == session2 &&
                avatar2->session_binding().gateway_id == 88,
                "binding2 carries g2 session/gateway id");

    g1.stop();
    g2.stop();
    cell.stop();
    world->stop();
    login.stop();
    return true;
}

// ---- D14 #4 远端销毁回执（L134 两 cell 回收确认批）----
// World 管理面裸帧客户端（963 cell_retire_request → 966 回执）：release 只在
// 过期/缩容路径触发，测试须经真实 963 缩容命令释放 Space（World 状态单写者
// 在 M1 泵线程——不直调 beginCellRetire）。自泵线程 + 966 按 request_id 收账。
struct WorldControlClient {
    std::unique_ptr<netlink::RouteTable> table;
    std::unique_ptr<netlink::LinkManager> manager;
    std::atomic<bool> running{false};
    std::thread pump;
    std::mutex replies_mutex;
    std::map<std::uint64_t, bool> retire_accepted;  // request_id → accepted

    bool start(std::uint16_t world_port) {
        table = std::make_unique<netlink::RouteTable>();
        std::ifstream in(kRouteJson, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (!netlink::load_route_table_json(buffer.str(), *table)) {
            return false;
        }
        netlink::LinkManagerConfig mc;
        mc.self_name = "e2e-control";
        mc.route_version = netlink::route_version_from_hash(table->schema_hash());
        manager = std::make_unique<netlink::LinkManager>(mc);
        netlink::EndpointConfig ep;
        ep.name = "world";
        ep.host = "127.0.0.1";
        ep.port = world_port;
        ep.role = netlink::EndpointRole::kActive;
        if (!manager->add_endpoint(ep)) {
            return false;
        }
        auto& dispatcher = manager->link("world")->dispatcher();
        dispatcher.set_route_table(table.get());
        dispatcher.allow_domain("internal");
        dispatcher.set_handler(
            id_of(MsgId::cell_retire_reply),
            [this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
                std::vector<wire::FieldValue> fields;
                if (!wire::decode_payload(id_of(MsgId::cell_retire_reply), data, size,
                                          fields)
                    || fields.size() != 6) {
                    return;
                }
                const auto request_id =
                    static_cast<std::uint64_t>(fields[0].integer);
                std::lock_guard<std::mutex> lock(replies_mutex);
                retire_accepted[request_id] = fields[1].integer != 0;
            });
        running.store(true, std::memory_order_release);
        pump = std::thread([this] {
            const auto now_ms = [] {
                return static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
            };
            while (running.load(std::memory_order_acquire)) {
                manager->pump(now_ms());
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });
        return true;
    }

    bool wait_up() const {
        for (int i = 0; i < 3000; i += 2) {
            if (manager != nullptr
                && manager->link("world")->state() == netlink::LinkState::kUp) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    bool send_retire(std::uint64_t request_id, std::int32_t op,
                     std::uint64_t cell_id) {
        auto* link = manager ? manager->link("world") : nullptr;
        if (link == nullptr) {
            return false;
        }
        std::vector<wire::FieldValue> f(3);
        f[0].integer = static_cast<std::int64_t>(request_id);
        f[1].integer = op;
        f[2].integer = static_cast<std::int64_t>(cell_id);
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::cell_retire_request), f, payload)) {
            return false;
        }
        link->send(id_of(MsgId::cell_retire_request), payload.data(), payload.size(),
                   netlink::SendClass::kReliable);
        return true;
    }

    bool accepted(std::uint64_t request_id) {
        std::lock_guard<std::mutex> lock(replies_mutex);
        return retire_accepted.count(request_id) != 0;
    }

    void stop() {
        if (!running.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (pump.joinable()) {
            pump.join();
        }
        manager.reset();
        table.reset();
    }
};

// D14 真实多 cell 分配批（L134 首切片）：真实链路两 cell 确定性分空间与
// 路由隔离——两个真 CellApp（cell-21 声明能力 0x1、cell-22 声明 0x2）经
// 960/961 注册，World assignSpaces {501:0x1, 503:0x2} 按能力位全覆盖裁决
// 归属（非首就绪独占——不合格 cell 不落到缺能力的 Space 上凑数），网关按
// 玩家目录落点空间路由到归属 cell，另一 cell 零接收（跨 cell 不误路由）。
// 断言：501→cell-21、503→cell-22（World 权威归属表）；玩家 1001 输入到达
// cell-21、1002 输入到达 cell-22；cell-21 无 1002、cell-22 无 1001（隔离）；
// 两玩家各自 base 锚点在线（单 cell 模拟不受影响，D14 #5）。
bool login_chain_two_cell_space_routing() {
    TemporaryDirectory files;

    // ① base：PlayerStore M1 面 + 播种 player1/player2。
    StoreHost base(base_store_config(files.path), 0, 0, 0, 0);
    TEST_ASSERT(base.store->m1_listen_port() != 0, "base face did not listen");
    TEST_ASSERT(base.store->adoptPlayer(1001, "", 0, 0, 2), "seed player 1001 profile");
    TEST_ASSERT(base.store->adoptPlayer(1002, "", 0, 0, 2), "seed player 1002 profile");

    // ② login：准入面 + base 客户端面 + 内建 OnlineDirectory。初始空间 501
    //（玩家 1001 落 501；1002 落点由本测试按 503 覆写目录——D14 场景前提
    //「玩家进入指定 Space」，登录全局 initialSpaceId 仅为落点缺省，目录是
    // World 准入权威、落点按玩家独立）。
    auto lconfig = login_config();
    lconfig.initialSpaceId = 501;
    lconfig.worldStoreM1Host = "127.0.0.1";
    lconfig.worldStoreM1Port = base.store->m1_listen_port();
    login::LoginServer login(lconfig);
    login.start();
    TEST_ASSERT(login.admissionPort() != 0, "login admission face up");
    TEST_ASSERT(login.onlineDirectory() != nullptr, "login online directory up");
    TEST_ASSERT(wait_until([&] { return login.baseActivationReady(); }, 3000),
                "login base activation link up");

    // ③ world：Space 分配池按能力位（501 需 0x1、503 需 0x2）确定性分空间。
    world_app::WorldAppConfig wconfig;
    wconfig.worldId = 1;
    wconfig.host = "127.0.0.1";
    wconfig.m1Port = 0;
    wconfig.m1RouteJson = kRouteJson;
    wconfig.tickRateMs = 20;
    // 池含临时 Instance 504:instance（需 0x2，D14 设计 §2.2「504 归 cell-22」）
    wconfig.assignSpaces = {{501, 0x1}, {503, 0x2}, {504, 0x2}};
    auto world = std::make_unique<world_app::WorldApp>(wconfig);
    world->start();
    TEST_ASSERT(world->isRunning(), "world starts");

    // ④ 两 cell 注册（cell-21 声明 0x1、cell-22 声明 0x2）→ World 按能力归属。
    cell_test::CellHost cell21;
    cell21.config = wconfig;
    cell21.config.declaredCapabilities = 0x1;
    TEST_ASSERT(cell21.start(world->m1Link().listenPort(), 21, 0, &world->m1Link()),
                "cell-21 host starts");
    cell21.set_world_link(&world->m1Link());
    cell21.pump_until_registered();
    TEST_ASSERT(cell21.registered(), "cell-21 registered with world");

    cell_test::CellHost cell22;
    cell22.config = wconfig;
    cell22.config.declaredCapabilities = 0x2;
    TEST_ASSERT(cell22.start(world->m1Link().listenPort(), 22, 0, &world->m1Link()),
                "cell-22 host starts");
    cell22.set_world_link(&world->m1Link());
    cell22.pump_until_registered();
    TEST_ASSERT(cell22.registered(), "cell-22 registered with world");

    // ⑤ 权威归属：501→cell-21、503/504→cell-22（能力位裁决，非首就绪独占——
    // cell-21 无 0x2 不落 503/504，cell-22 无 0x1 不落 501）。
    TEST_ASSERT(wait_until([&] {
                    const auto* a501 = world->find_space_assignment(501);
                    const auto* a503 = world->find_space_assignment(503);
                    const auto* a504 = world->find_space_assignment(504);
                    return a501 != nullptr && a501->cell_id == 21
                           && a503 != nullptr && a503->cell_id == 22
                           && a504 != nullptr && a504->cell_id == 22;
                }, 5000),
                "world assigns 501->21 and 503/504->22 by capability");

    // ⑥ 登录登记表投影（L120 既有注入口径：World 未直连本 login 的 968 拨号，
    // 端点取各 cell 真实 M1 监听端点——L122 同源）。
    const auto inject = [&](std::uint64_t space_id, std::uint64_t cell_id,
                            std::uint16_t port) {
        login::SpacePositionProjection proj;
        proj.world_id = 1;
        proj.space_id = space_id;
        proj.cell_id = cell_id;
        proj.assignment_version = 5;
        proj.topology_version = 71;
        proj.management_epoch = 3;
        proj.ready = true;
        proj.cell_host = "127.0.0.1";
        proj.cell_port = port;
        return login.positionProjectionRegistry()->apply_projection(proj, 0)
               == login::ProjectionVerdict::kApplied;
    };
    TEST_ASSERT(inject(501, 21, cell21.listen_port()), "501 projection injected");
    TEST_ASSERT(inject(503, 22, cell22.listen_port()), "503 projection injected");

    // ⑦ gateway：login 准入 + base 激活 + world M1 三面全接 + 目录 M1 路由面。
    // directoryM1Port 必须接 login 内建 OnlineDirectory 的 M1 控制面
    // （listenPort 读回真实端点）——缺省 0 = 不装配路由客户端，网关首意图
    // 懒解析 fetchRouteSnapshot 得 nullopt → buildDefaultRoute() 空间 0，
    // 真实按玩家空间的选路（D14 #1/#2）将退化为首就绪兜底而失真。
    auto gconfig = gateway_config(world->m1Link().listenPort(),
                                  login.admissionPort(), base.store->m1_listen_port());
    gconfig.directoryM1Host = "127.0.0.1";
    gconfig.directoryM1Port = login.onlineDirectory()->listenPort();
    TEST_ASSERT(gconfig.directoryM1Port != 0, "gateway route client wired to directory");
    gateway::GatewayServer gateway(gconfig);
    gateway.start();

    // ⑧ 两客户端连 gateway → PING 建会话。
    TcpTestClient c1;
    TcpTestClient c2;
    TEST_ASSERT(c1.connect("127.0.0.1", gconfig.port), "client1 connects");
    TEST_ASSERT(c2.connect("127.0.0.1", gconfig.port), "client2 connects");
    protocol::Ping ping;
    ping.timestamp = 1;
    c1.sendAll(frame_bytes(1, 0, protocol::MessageCodec::encode(ping, 0)));
    std::vector<std::uint8_t> reply1;
    TEST_ASSERT(c1.recvLinkFrame(reply1), "ping pong received on c1");
    const auto session1 = header_of(reply1).sessionId;
    c2.sendAll(frame_bytes(1, 0, protocol::MessageCodec::encode(ping, 0)));
    std::vector<std::uint8_t> reply2;
    TEST_ASSERT(c2.recvLinkFrame(reply2), "ping pong received on c2");
    const auto session2 = header_of(reply2).sessionId;
    TEST_ASSERT(session1 != 0 && session2 != 0, "both gateway sessions created");

    // ⑨ login 端认证签发两票（player1→1001，player2→1002）。
    protocol::LoginRequest loginReq1;
    loginReq1.username = "player1";
    loginReq1.password = "pass1";
    const auto loginResp1 = login.dispatchRequest(
        protocol::MessageCodec::encode(loginReq1, 0));
    TEST_ASSERT(loginResp1.size() > sizeof(protocol::MessageHeader), "login1 response");
    protocol::LoginResponse parsedLogin1;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(loginResp1.begin() + sizeof(protocol::MessageHeader),
                                  loginResp1.end()), parsedLogin1);
    TEST_ASSERT(parsedLogin1.success && parsedLogin1.playerId == 1001,
                "login1 accepted for 1001");
    protocol::LoginRequest loginReq2;
    loginReq2.username = "player2";
    loginReq2.password = "pass2";
    const auto loginResp2 = login.dispatchRequest(
        protocol::MessageCodec::encode(loginReq2, 0));
    TEST_ASSERT(loginResp2.size() > sizeof(protocol::MessageHeader), "login2 response");
    protocol::LoginResponse parsedLogin2;
    protocol::MessageCodec::decodeBody(
        std::vector<std::uint8_t>(loginResp2.begin() + sizeof(protocol::MessageHeader),
                                  loginResp2.end()), parsedLogin2);
    TEST_ASSERT(parsedLogin2.success && parsedLogin2.playerId == 1002,
                "login2 accepted for 1002");
    TEST_ASSERT(!parsedLogin1.loginTicket.empty() && !parsedLogin2.loginTicket.empty(),
                "both login tickets issued");

    // ⑩ 玩家 1002 的目录落点覆写为 503（D14 场景前提「玩家进入指定 Space」
    //——登录全局 initialSpaceId 只给 1001 的 501）。覆写须在 1002 首个模拟
    // 输入前——网关 ensureRouteSnapshot 懒解析（首个 world 意图）即取 503。
    // 不设 cell_id：登录流程落点亦不戳 cell_id，路由按权威空间归属（space_id
    // → 归属 cell）裁决，本测试即验证该裁决面。
    auto a2 = login.onlineDirectory()->resolveWorldAssignment(1002);
    TEST_ASSERT(a2.has_value(), "1002 assignment recorded");
    a2->space_id = 503;
    a2->cell_host = "127.0.0.1";
    a2->cell_port = cell22.listen_port();
    TEST_ASSERT(login.onlineDirectory()->assignWorld(1002, *a2),
                "1002 re-assigned to space 503");

    // ⑪ 网关准入（login M1 真校验 + base 激活 + 目录 World 准入）。
    TEST_ASSERT(wait_until([&] { return gateway.baseActivationReady(); }, 3000),
                "base activation link up");
    TEST_ASSERT(gateway.authenticateSession(session1, 1001, parsedLogin1.loginTicket),
                "g1 auth accepted for 1001");
    TEST_ASSERT(gateway.authenticateSession(session2, 1002, parsedLogin2.loginTicket),
                "g2 auth accepted for 1002");

    // ⑫ 落点空间各归其所：1001→501、1002→503（目录按玩家独立，非全局）。
    const auto assigned1 = login.onlineDirectory()->resolveWorldAssignment(1001);
    const auto assigned2 = login.onlineDirectory()->resolveWorldAssignment(1002);
    TEST_ASSERT(assigned1.has_value() && assigned1->space_id == 501,
                "1001 assigned to space 501");
    TEST_ASSERT(assigned2.has_value() && assigned2->space_id == 503,
                "1002 assigned to space 503");

    // ⑬ 两客户端各自发 CELL_CREATE_ENTITY → gateway → world → 归属 cell。
    // 落点空间随请求（1001→501、1002→503）——选路按目录落点空间（网关→World
    // 按 space 路由到归属 cell），实体入归属空间真实 scene（503 占用中是
    // D14 #4「占用不销毁」前提）。
    c1.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(1001, 501), session1)));
    c2.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(1002, 503), session2)));

    // ⑭ 断言：1001 到达 cell-21、1002 到达 cell-22（选路正确，D14 #1/#2）。
    TEST_ASSERT(wait_until(
                    [&] { return cell21.resolveIntentPlayer(session1) == 1001; }, 8000),
                "cell-21 spawned player 1001");
    TEST_ASSERT(wait_until(
                    [&] { return cell22.resolveIntentPlayer(session2) == 1002; }, 8000),
                "cell-22 spawned player 1002");
    // ⑮ 跨 cell 隔离：cell-21 无 1002、cell-22 无 1001（不误路由，D14 #3）。
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    TEST_ASSERT(cell21.resolveIntentPlayer(session2) == 0,
                "cell-21 did not receive 1002 (isolation)");
    TEST_ASSERT(cell22.resolveIntentPlayer(session1) == 0,
                "cell-22 did not receive 1001 (isolation)");

    // ⑯ 两玩家各自 base 锚点在线（同 cell 单空间模拟不受影响，D14 #5）。
    const auto avatar1 = base.store->findAvatar(1001);
    const auto avatar2 = base.store->findAvatar(1002);
    TEST_ASSERT(avatar1 != nullptr && avatar2 != nullptr, "both avatars loaded at base");
    TEST_ASSERT(avatar1->state() == apollo::game::base::AvatarState::Online &&
                avatar2->state() == apollo::game::base::AvatarState::Online,
                "both avatars online after chain");

    // ⑰ D14 #4：释放 504 → cell-22 确认远端销毁（销毁回执，L134 两 cell 回收
    // 确认批）。经真实 963 缩容命令触发释放（World 状态单写者在 M1 泵线程，
    // 不直调 beginCellRetire）：releaseCellSpaces → 962 ready=0 撤销标记 →
    // cell-22 remove_space 销毁 504 → 969 cell_space_reclaim_done 回执 → World
    // 记账即确认面。503 占用中（1002 avatar 在位）拒绝销毁保持路由、无回执
    //——确认账只落 504（销毁的是腾空的临时 Instance）。
    WorldControlClient ctl;
    TEST_ASSERT(ctl.start(world->m1Link().listenPort()), "world control client up");
    TEST_ASSERT(ctl.wait_up(), "world control client linked");
    constexpr std::uint64_t kRetireReq = 4242;
    TEST_ASSERT(ctl.send_retire(kRetireReq, 1, 22), "retire begin frame sent");
    TEST_ASSERT(wait_until([&] { return ctl.accepted(kRetireReq); }, 5000),
                "world accepts retire begin for cell-22");
    TEST_ASSERT(wait_until([&] {
                    const auto* conf = world->find_reclaim_confirmation(504);
                    return conf != nullptr && conf->cell_id == 22;
                }, 8000),
                "cell-22 confirms remote destroy of released 504");
    TEST_ASSERT(world->find_reclaim_confirmation(503) == nullptr,
                "occupied 503 not destroyed (no receipt)");
    ctl.stop();

    gateway.stop();
    cell21.stop();
    cell22.stop();
    world->stop();
    login.stop();
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "login chain e2e tests: SKIPPED (link tcp/m1 base posix only)\n";
    return 0;
#else
    const struct { const char* name; bool (*fn)(); } cases[] = {
        {"login_chain_creates_cell_entity", login_chain_creates_cell_entity},
        {"login_chain_two_gateway_concurrent", login_chain_two_gateway_concurrent},
        {"login_chain_two_cell_space_routing", login_chain_two_cell_space_routing},
    };
    int failed = 0;
    for (const auto& c : cases) {
        std::cout << "[RUN ] " << c.name << std::endl;
        if (!c.fn()) {
            ++failed;
            std::cerr << "[FAIL] " << c.name << std::endl;
        } else {
            std::cout << "[PASS] " << c.name << std::endl;
        }
    }
    if (failed == 0) {
        std::cout << "LoginChainE2eTests: all "
                  << (sizeof(cases) / sizeof(cases[0])) << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "LoginChainE2eTests: " << failed << " case(s) failed" << std::endl;
    return 1;
#endif
}
