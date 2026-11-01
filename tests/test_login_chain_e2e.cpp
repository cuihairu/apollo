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
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iostream>
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

#if !defined(_WIN32)

const auto kTick = std::chrono::milliseconds(2);

const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

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

protocol::CellCreateEntity make_player_spawn(std::uint64_t player_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = player_id;
    msg.entityType = protocol::EntityType::PLAYER;
    msg.spaceId = 0;
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
                                      std::uint16_t base_m1_port) {
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
    config.gatewayId = 77;
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

#endif // !_WIN32

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "login chain e2e tests: SKIPPED (link tcp/m1 base posix only)\n";
    return 0;
#else
    const struct { const char* name; bool (*fn)(); } cases[] = {
        {"login_chain_creates_cell_entity", login_chain_creates_cell_entity},
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
