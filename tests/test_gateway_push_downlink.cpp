// 网关服务端主动下行测试（L51 游戏 WS/WSS → 下行/广播/Lua 消费全链）：
// World 939 client_deliver → Gateway sendToClient → 真实客户端 socket。
//
// 既有 egress 级用例（world_drain/update_continuity）只用 ClientFace 汇流点
// 证明下行到达 egress——本批从真实 TCP/WS 客户端走完整个游戏帧/会话/意图
// 路径：PING 建会话 → 准入绑玩家 → CELL_CREATE_ENTITY 造玩家实体 →
// PARTY_CREATE_REQUEST → World 经 M1 939 回推 PARTY_RESULT → 网关按会话号
// 真投真实 socket。覆盖：
//   1. TCP 明文客户端收到 World 主动推送（帧头 sessionId = 本会话号）；
//   2. WS 客户端收到同一路推送（升级 /game → masked binary → 服务端 binary）；
//   3. 推送按会话寻址：非队长会话收不到 PARTY_RESULT（多会话隔离）；
//   4. AOI 移动广播只达视野内会话（逐 viewer 重编帧头；视野外零帧）；
//   5. 同一移动广播经 WS 视野内客户端复现（跨传输）；
//   6. 技能伤害广播：零会话号帧头按 viewer 重编（战斗下发面）；
//   7. 客户端消息 → Lua 契约路由消费 → world 模块挂推送源（脚本源帧回环）；
//   8. 上行重复/乱序规则：重放/旧序帧闸外（负窗零帧）、跳空容忍、零序直通。
// 下行证据走线缆可见面（客户端 socket 真收到协议全帧），非日志非计数。
// Windows：link TCP/TLS 底座不进 M1 批，用例本体空转。

#include "gateway/gateway_server.hpp"
#include "world_app/world_app.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo_contract.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
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
namespace ws = apollo::net::ws;
namespace protocol = apollo::protocol;
using world_app::WorldAppConfig;

#if !defined(_WIN32)

// macOS CI 裸自旋必 flake：重试循环每拍 sleep ≥2ms。
const auto kTick = std::chrono::milliseconds(2);

const char* kRouteJson = APOLLO_TEST_ROUTE_JSON;

std::vector<std::uint8_t> frame_bytes(std::uint32_t seq, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    netlink::append_frame(out, seq, flags, payload);
    return out;
}

// 空闲端口探测（GatewayConfig 层 0 = 监听关闭，测试需要真实端口）。
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

// ---- TCP 测试客户端（阻塞 socket；独立实现，不复用被测代码收发路径）----

class TcpTestClient {
public:
    ~TcpTestClient() { close(); }

    bool connect(const std::string& host, std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;  // 200ms 单收上限：下行轮询拍
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
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    // 读一个 link 帧，回载荷（协议全帧）。
    bool recvLinkFrame(std::vector<std::uint8_t>& payload) {
        for (;;) {
            if (!pending_.empty()) {
                payload = std::move(pending_.front());
                pending_.pop_front();
                return true;
            }
            if (!fill()) {
                return false;
            }
            const auto outcome = parser_.feed(buf_.data(), buf_.size());
            if (outcome.fatal) {
                return false;
            }
            buf_.clear();
            for (auto& frame : outcome.frames) {
                pending_.push_back(std::move(frame.payload));
            }
        }
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    bool fill() {
        std::uint8_t chunk[4096];
        const ssize_t n = ::recv(fd_, chunk, sizeof chunk, 0);
        if (n <= 0) {
            return false;
        }
        buf_.insert(buf_.end(), chunk, chunk + n);
        return true;
    }

    int fd_ = -1;
    std::vector<std::uint8_t> buf_;
    netlink::FrameParser parser_;
    std::deque<std::vector<std::uint8_t>> pending_;
};

// ---- ws 测试客户端（与 ws 入口测试同款最小实现）----

const char* kClientKey = "dGhlIHNhbXBsZSBub25jZQ==";

std::vector<std::uint8_t> upgrade_request(const std::string& path) {
    const std::string req = "GET " + path + " HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Key: " + std::string(kClientKey) + "\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "\r\n";
    return {req.begin(), req.end()};
}

void append_masked_frame(std::vector<std::uint8_t>& out, ws::OpCode opcode, bool fin,
                         const std::uint8_t* payload, std::size_t size, std::uint8_t mask_key) {
    out.push_back((fin ? 0x80 : 0x00) | static_cast<std::uint8_t>(opcode));
    if (size < 126) {
        out.push_back(0x80 | static_cast<std::uint8_t>(size));
    } else if (size <= 0xFFFF) {
        out.push_back(0x80 | 126);
        out.push_back(static_cast<std::uint8_t>(size >> 8));
        out.push_back(static_cast<std::uint8_t>(size & 0xFF));
    } else {
        out.push_back(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.push_back(static_cast<std::uint8_t>((size >> shift) & 0xFF));
        }
    }
    for (int i = 0; i < 4; ++i) {
        out.push_back(mask_key);
    }
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(payload[i] ^ mask_key);
    }
}

class WsTestClient {
public:
    ~WsTestClient() { close(); }

    bool connect(const std::string& host, std::uint16_t port) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            return false;
        }
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 200000;  // 200ms 单收上限
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
                if (n < 0 && errno == EINTR) {
                    continue;
                }
                return;
            }
            off += static_cast<std::size_t>(n);
        }
    }

    bool recvHttpResponse(std::string& response) {
        while (buf_.size() < 16 * 1024) {
            const auto terminator = std::search(buf_.begin(), buf_.end(),
                                                kHeaderTerm, kHeaderTerm + 4);
            if (terminator != buf_.end()) {
                break;
            }
            if (!fill()) {
                return false;
            }
        }
        if (buf_.size() >= 16 * 1024) {
            return false;
        }
        const auto terminator = std::search(buf_.begin(), buf_.end(),
                                            kHeaderTerm, kHeaderTerm + 4);
        response.assign(buf_.begin(), terminator + 4);
        buf_.erase(buf_.begin(), terminator + 4);
        return true;
    }

    bool recvWsFrame(std::uint8_t& opcode, std::vector<std::uint8_t>& payload) {
        for (;;) {
            if (buf_.size() >= 2) {
                const std::uint8_t b0 = buf_[0];
                const std::uint8_t b1 = buf_[1];
                if ((b1 & 0x80) != 0) {
                    return false;  // 服务端帧不掩码
                }
                const std::size_t len7 = b1 & 0x7F;
                if ((len7 == 126 && buf_.size() < 4) ||
                    (len7 == 127 && buf_.size() < 10)) {
                    if (!fill()) {
                        return false;
                    }
                    continue;
                }
                std::size_t header = 2;
                std::size_t len = len7;
                if (len7 == 126) {
                    len = (static_cast<std::size_t>(buf_[2]) << 8) | buf_[3];
                    header = 4;
                } else if (len7 == 127) {
                    len = 0;
                    for (int i = 0; i < 8; ++i) {
                        len = (len << 8) | buf_[2 + i];
                    }
                    header = 10;
                }
                if (buf_.size() >= header + len) {
                    opcode = b0 & 0x0F;
                    payload.assign(buf_.begin() + static_cast<long>(header),
                                   buf_.begin() + static_cast<long>(header + len));
                    buf_.erase(buf_.begin(), buf_.begin() + static_cast<long>(header + len));
                    return true;
                }
            }
            if (!fill()) {
                return false;
            }
        }
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    static constexpr char kHeaderTerm[] = "\r\n\r\n";

    bool fill() {
        std::uint8_t chunk[4096];
        const ssize_t n = ::recv(fd_, chunk, sizeof chunk, 0);
        if (n <= 0) {
            return false;
        }
        buf_.insert(buf_.end(), chunk, chunk + n);
        return true;
    }

    int fd_ = -1;
    std::vector<std::uint8_t> buf_;
};

// ---- 协议消息帮手 ----

std::vector<std::uint8_t> ping_packet(std::uint64_t session_id) {
    protocol::Ping ping;
    ping.timestamp = 777;
    return protocol::MessageCodec::encode(ping, session_id);
}

// 控序协议帧：encode 产物改写协议头 sequence（重复/乱序闸面在协议头序号，
// 链路层帧号不参与判定）。
std::vector<std::uint8_t> with_sequence(const std::vector<std::uint8_t>& frame,
                                        std::uint64_t sequence) {
    auto header = protocol::MessageCodec::parseHeader(frame.data(), frame.size());
    header.sequence = sequence;
    auto out = protocol::MessageCodec::encodeHeader(header);
    out.insert(out.end(), frame.begin() + sizeof(protocol::MessageHeader), frame.end());
    return out;
}

protocol::MessageHeader header_of(const std::vector<std::uint8_t>& protocol_frame) {
    return protocol::MessageCodec::parseHeader(protocol_frame.data(), protocol_frame.size());
}

protocol::CellCreateEntity make_player_spawn(std::uint64_t player_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = player_id;  // P0-2：player 身份 = 创建请求实体号
    msg.entityType = protocol::EntityType::PLAYER;
    msg.spaceId = 0;  // 默认 scene
    return msg;
}

// 落点显式的玩家实体（AOI 视野关系由进场位置决定：view_radius 默认 20）。
protocol::CellCreateEntity make_player_spawn_at(std::uint64_t player_id,
                                                float x, float y, float z) {
    auto msg = make_player_spawn(player_id);
    msg.position = {x, y, z};
    return msg;
}

protocol::CellEntityMove make_move(std::uint64_t entity_id, float x, float y, float z) {
    protocol::CellEntityMove msg;
    msg.entityId = entity_id;
    msg.oldPos = {0.0f, 0.0f, 0.0f};
    msg.newPos = {x, y, z};
    return msg;
}

// 收到的协议全帧（会话号 + 类型 + body），下行断言事实源。
struct ProtocolFrame {
    std::uint64_t session_id;
    std::uint16_t type;
    std::vector<std::uint8_t> body;
};

bool wait_until(const std::function<bool()>& pred, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(kTick);
    }
    return pred();
}

// 等玩家实体出栏（spawn intent 在 tick 边界消费；resolveIntentPlayer 非零 =
// 会话已绑玩家，场景 Avatar 已入 AOI）。
bool wait_player_spawned(world_app::WorldApp& world, std::uint64_t session_id,
                         std::uint64_t player_id, int timeout_ms) {
    return wait_until(
        [&world, session_id, player_id] {
            return world.resolveIntentPlayer(session_id) == player_id;
        },
        timeout_ms);
}

// 从 TCP 客户端收帧直到命中目标类型/会话（其间帧记入 sink）；超时 false。
// 200ms 单收超时 = 轮询拍；重试循环每拍 sleep ≥2ms（macOS CI 纪律）。
bool wait_tcp_frame(TcpTestClient& client, std::uint16_t want_type,
                    std::uint64_t want_session, ProtocolFrame& out,
                    std::vector<ProtocolFrame>& sink, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        std::vector<std::uint8_t> payload;
        if (client.recvLinkFrame(payload)) {
            const auto h = header_of(payload);
            ProtocolFrame f{h.sessionId, h.type,
                            {payload.begin() + sizeof(protocol::MessageHeader),
                             payload.end()}};
            if (f.type == want_type && f.session_id == want_session) {
                out = std::move(f);
                return true;
            }
            sink.push_back(std::move(f));
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(kTick);
    }
}

// 从 WS 客户端收帧直到命中目标类型/会话（跳过控制帧与异构帧）；超时 false。
bool wait_ws_frame(WsTestClient& ws, std::uint16_t want_type,
                   std::uint64_t want_session, ProtocolFrame& out,
                   std::vector<ProtocolFrame>& sink, int timeout_ms) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        std::uint8_t opcode = 0;
        std::vector<std::uint8_t> payload;
        if (ws.recvWsFrame(opcode, payload)) {
            if (opcode != static_cast<std::uint8_t>(ws::OpCode::kBinary)) {
                continue;  // 控制帧（ping/pong/close）不下断言面
            }
            netlink::FrameParser parser;
            const auto outcome = parser.feed(payload);
            if (outcome.fatal || outcome.frames.empty()) {
                continue;
            }
            const auto& link_frame = outcome.frames[0].payload;
            const auto h = header_of(link_frame);
            ProtocolFrame f{h.sessionId, h.type,
                            {link_frame.begin() + sizeof(protocol::MessageHeader),
                             link_frame.end()}};
            if (f.type == want_type && f.session_id == want_session) {
                out = std::move(f);
                return true;
            }
            sink.push_back(std::move(f));
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(kTick);
    }
}

// ---- 服务端壳 ----

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-gateway-push-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// WorldApp（m1Port 0 内核分配；档案宿主已迁 base，此面不触）。
struct WorldEnd {
    TemporaryDirectory files;
    WorldAppConfig config;
    std::unique_ptr<world_app::WorldApp> world;
    // 脚本源（非空 = 落临时文件并接进启动装载序 handler → 契约）。
    std::string scriptHandlersSrc;
    std::string scriptContractSrc;

    bool start(std::uint32_t world_id) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        config.tickRateMs = 20;
        if (!scriptHandlersSrc.empty() || !scriptContractSrc.empty()) {
            // TemporaryDirectory 只算路径不建目录——落盘前显式建（ofstream
            // 静默失败面：目录缺失时 f<< 为 no-op，读端只会拿到 nullopt）。
            std::error_code ec;
            std::filesystem::create_directories(files.path, ec);
        }
        if (!scriptHandlersSrc.empty()) {
            const auto path = files.path / "handlers.lua";
            std::ofstream f(path, std::ios::binary);
            f << scriptHandlersSrc;
            f.close();
            config.scriptHandersFile = path.string();
        }
        if (!scriptContractSrc.empty()) {
            const auto path = files.path / "contract.lua";
            std::ofstream f(path, std::ios::binary);
            f << scriptContractSrc;
            f.close();
            config.scriptContractFile = path.string();
        }
        try {
            world = std::make_unique<world_app::WorldApp>(config);
            world->start();
        } catch (const std::exception& e) {
            std::cout << "  world start failed: " << e.what() << std::endl;
            return false;
        }
        return world->m1Link().listenPort() != 0;
    }

    void stop() {
        if (world) {
            world->stop();
            world.reset();
        }
    }
};

// 网关配置：世界 M1 真端点 + 可选 WS 入口；base/login/directory 面全关
//（准入默认放行，激活面缺席 = 旧语义，不干扰下行路径）。
gateway::GatewayConfig gateway_config(std::uint16_t world_m1_port,
                                      std::uint16_t ws_port) {
    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = probe_free_port();
    config.wsPort = ws_port;
    config.sessionTimeoutMs = 600000;         // 防心跳超时干扰
    config.heartbeatCheckIntervalMs = 50;
    config.controlM1Port = 0;
    config.worldM1Host = "127.0.0.1";
    config.worldM1Port = world_m1_port;
    config.worldM1RouteJson = kRouteJson;
    config.baseM1Port = 0;                    // 激活面关：准入过即绑玩家
    return config;
}

// 建 TCP 会话：连网关 → PING → 取 PONG 头回显会话号 → 准入绑玩家。
bool tcp_establish(TcpTestClient& client, gateway::GatewayServer& server,
                   std::uint16_t port, std::uint64_t player_id,
                   std::uint64_t& session_id) {
    if (!client.connect("127.0.0.1", port)) {
        return false;
    }
    client.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    if (!client.recvLinkFrame(reply)) {
        return false;
    }
    session_id = header_of(reply).sessionId;
    if (session_id == 0) {
        return false;
    }
    return server.authenticateSession(session_id, player_id, "ticket");
}

// 建 WS 会话：升级 /game → PING（link 帧包协议帧）→ PONG → 准入绑玩家。
bool ws_establish(WsTestClient& ws, gateway::GatewayServer& server,
                  std::uint16_t port, std::uint64_t player_id,
                  std::uint64_t& session_id) {
    if (!ws.connect("127.0.0.1", port)) {
        return false;
    }
    ws.sendAll(upgrade_request("/game"));
    std::string response;
    if (!ws.recvHttpResponse(response) ||
        response.find("101") == std::string::npos) {
        return false;
    }
    const auto link_frame = frame_bytes(1, 0, ping_packet(0));
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true,
                        link_frame.data(), link_frame.size(), 0x33);
    ws.sendAll(ws_frame);
    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> payload;
    if (!ws.recvWsFrame(opcode, payload) ||
        opcode != static_cast<std::uint8_t>(ws::OpCode::kBinary)) {
        return false;
    }
    netlink::FrameParser parser;
    const auto outcome = parser.feed(payload);
    if (outcome.fatal || outcome.frames.empty()) {
        return false;
    }
    session_id = header_of(outcome.frames[0].payload).sessionId;
    if (session_id == 0) {
        return false;
    }
    return server.authenticateSession(session_id, player_id, "ticket");
}

// 单玩家建队链路：造玩家实体（实体号 = 玩家号）→ 建队请求（帧头会话号 = 本会话）。
void send_spawn_and_party(TcpTestClient& client, std::uint64_t player_id,
                          std::uint64_t session_id) {
    client.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(player_id), session_id)));
    client.sendAll(frame_bytes(3, 0,
        protocol::MessageCodec::encode(protocol::PartyCreateRequest{}, session_id)));
}

// ---- 1. TCP 客户端收到 World 主动推送 ----

bool test_tcp_push_downlink() {
    std::cout << "Running: test_tcp_push_downlink..." << std::endl;
    WorldEnd world;
    TEST_ASSERT(world.start(44), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    TcpTestClient client;
    std::uint64_t sid = 0;
    TEST_ASSERT(tcp_establish(client, server, server.listenPort(), 9001, sid),
                "tcp session established + authenticated");
    send_spawn_and_party(client, 9001, sid);

    ProtocolFrame out;
    std::vector<ProtocolFrame> sink;
    TEST_ASSERT(wait_tcp_frame(client, static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT),
                               sid, out, sink, 8000),
                "world party result pushed to tcp client");
    const auto result = protocol::MessageCodec::decodeBody<protocol::PartyResult>(out.body);
    TEST_ASSERT(result.op == static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                    result.code == 0 && result.partyId != 0,
                "party create accepted, party id allocated");

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 2. WS 客户端收到同一路 World 推送 ----

bool test_ws_push_downlink() {
    std::cout << "Running: test_ws_push_downlink..." << std::endl;
    const auto ws_port = probe_free_port();
    TEST_ASSERT(ws_port != 0, "free ws port probed");
    WorldEnd world;
    TEST_ASSERT(world.start(45), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), ws_port));
    server.start();
    TEST_ASSERT(server.wsListenPort() != 0, "ws ingress listening");

    WsTestClient ws;
    std::uint64_t sid = 0;
    TEST_ASSERT(ws_establish(ws, server, server.wsListenPort(), 9002, sid),
                "ws session established + authenticated");

    const auto spawn = frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(9002), sid));
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true,
                        spawn.data(), spawn.size(), 0x44);
    ws.sendAll(ws_frame);
    const auto party = frame_bytes(3, 0,
        protocol::MessageCodec::encode(protocol::PartyCreateRequest{}, sid));
    ws_frame.clear();
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true,
                        party.data(), party.size(), 0x55);
    ws.sendAll(ws_frame);

    ProtocolFrame out;
    std::vector<ProtocolFrame> sink;
    TEST_ASSERT(wait_ws_frame(ws, static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT),
                              sid, out, sink, 8000),
                "world party result pushed to ws client");
    const auto result = protocol::MessageCodec::decodeBody<protocol::PartyResult>(out.body);
    TEST_ASSERT(result.op == static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                    result.code == 0 && result.partyId != 0,
                "party create accepted over ws");

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 3. 推送按会话寻址：非队长会话收不到 PARTY_RESULT ----

bool test_push_session_addressed() {
    std::cout << "Running: test_push_session_addressed..." << std::endl;
    WorldEnd world;
    TEST_ASSERT(world.start(46), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    // A = 队长会话：造实体 + 建队。
    TcpTestClient leader;
    std::uint64_t sid_a = 0;
    TEST_ASSERT(tcp_establish(leader, server, server.listenPort(), 9011, sid_a),
                "leader session established + authenticated");
    send_spawn_and_party(leader, 9011, sid_a);

    // B = 旁观会话：只造实体，不建队。
    TcpTestClient observer;
    std::uint64_t sid_b = 0;
    TEST_ASSERT(tcp_establish(observer, server, server.listenPort(), 9012, sid_b),
                "observer session established + authenticated");
    observer.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn(9012), sid_b)));

    ProtocolFrame out_a;
    std::vector<ProtocolFrame> sink_a;
    TEST_ASSERT(wait_tcp_frame(leader, static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT),
                               sid_a, out_a, sink_a, 8000),
                "leader receives its own party result");
    const auto result = protocol::MessageCodec::decodeBody<protocol::PartyResult>(out_a.body);
    TEST_ASSERT(result.op == static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
                    result.code == 0,
                "leader party create accepted");

    // B 窗内不得见 PARTY_RESULT（隔离证明走线缆可见面）。
    ProtocolFrame out_b;
    std::vector<ProtocolFrame> sink_b;
    TEST_ASSERT(!wait_tcp_frame(observer,
                                static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT),
                                sid_b, out_b, sink_b, 1500),
                "observer receives no party result");
    for (const auto& f : sink_b) {
        TEST_ASSERT(f.type != static_cast<std::uint16_t>(protocol::MessageType::PARTY_RESULT),
                    "no party result of any session reached observer");
    }

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 4. AOI 移动广播只达视野内会话（缆线可见：真客户端收帧） ----

bool test_aoi_broadcast_to_viewers() {
    std::cout << "Running: test_aoi_broadcast_to_viewers..." << std::endl;
    WorldEnd world;
    TEST_ASSERT(world.start(47), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    // mover 原点；near 视野内 (1,0,0)；far 视野外 (500,0,0)（world 默认
    // 视野半径 200，SceneAoi 类默认 20 不适用）。
    TcpTestClient mover;
    std::uint64_t sid_mover = 0;
    TEST_ASSERT(tcp_establish(mover, server, server.listenPort(), 9201, sid_mover),
                "mover session established");
    mover.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9201, 0.0f, 0.0f, 0.0f), sid_mover)));

    TcpTestClient near_c;
    std::uint64_t sid_near = 0;
    TEST_ASSERT(tcp_establish(near_c, server, server.listenPort(), 9202, sid_near),
                "near viewer session established");
    near_c.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9202, 1.0f, 0.0f, 0.0f), sid_near)));

    TcpTestClient far_c;
    std::uint64_t sid_far = 0;
    TEST_ASSERT(tcp_establish(far_c, server, server.listenPort(), 9203, sid_far),
                "far client session established");
    far_c.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9203, 500.0f, 0.0f, 0.0f), sid_far)));

    // 三玩家全出栏再动——AOI 视野关系随 scene.enter 建立。
    TEST_ASSERT(wait_player_spawned(*world.world, sid_mover, 9201, 8000), "mover spawned");
    TEST_ASSERT(wait_player_spawned(*world.world, sid_near, 9202, 8000), "near viewer spawned");
    TEST_ASSERT(wait_player_spawned(*world.world, sid_far, 9203, 8000), "far client spawned");

    // 清先导帧（spawn 无响应，但为 far 负例断言做隔离）。
    {
        std::vector<std::uint8_t> scratch;
        while (mover.recvLinkFrame(scratch)) {}
        while (near_c.recvLinkFrame(scratch)) {}
        while (far_c.recvLinkFrame(scratch)) {}
    }

    // mover 移动：world 广播给视野内所有玩家（含自身）。
    mover.sendAll(frame_bytes(3, 0,
        protocol::MessageCodec::encode(make_move(9201, 3.0f, 0.0f, 0.0f), sid_mover)));

    ProtocolFrame out_near;
    std::vector<ProtocolFrame> sink_near;
    TEST_ASSERT(wait_tcp_frame(near_c, static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE),
                               sid_near, out_near, sink_near, 8000),
                "near viewer receives re-addressed move broadcast");
    const auto move = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(out_near.body);
    TEST_ASSERT(move.entityId == 9201 && move.newPos.x == 3.0f && move.newPos.y == 0.0f,
                "move body intact for near viewer (entity + new position)");

    // 己方回传（服务端权威：mover 知道自己动了）。
    ProtocolFrame out_self;
    std::vector<ProtocolFrame> sink_self;
    TEST_ASSERT(wait_tcp_frame(mover, static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE),
                               sid_mover, out_self, sink_self, 8000),
                "mover receives its own move echo");

    // 视野外客户端不得收到任何移动广播。
    ProtocolFrame out_far;
    std::vector<ProtocolFrame> sink_far;
    TEST_ASSERT(!wait_tcp_frame(far_c, static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE),
                                sid_far, out_far, sink_far, 1500),
                "far client receives no move broadcast");
    for (const auto& f : sink_far) {
        TEST_ASSERT(f.type != static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE),
                    "no move frame of any session reached far client");
    }

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 5. WS 视野内客户端收到同一路移动广播（跨传输） ----

bool test_aoi_broadcast_to_ws_viewer() {
    std::cout << "Running: test_aoi_broadcast_to_ws_viewer..." << std::endl;
    const auto ws_port = probe_free_port();
    TEST_ASSERT(ws_port != 0, "free ws port probed");
    WorldEnd world;
    TEST_ASSERT(world.start(48), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), ws_port));
    server.start();
    TEST_ASSERT(server.wsListenPort() != 0, "ws ingress listening");

    TcpTestClient mover;
    std::uint64_t sid_mover = 0;
    TEST_ASSERT(tcp_establish(mover, server, server.listenPort(), 9211, sid_mover),
                "tcp mover session established");
    mover.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9211, 0.0f, 0.0f, 0.0f), sid_mover)));

    WsTestClient viewer;
    std::uint64_t sid_viewer = 0;
    TEST_ASSERT(ws_establish(viewer, server, server.wsListenPort(), 9212, sid_viewer),
                "ws viewer session established");
    // WS 上行：link 帧包协议帧，客户端掩码。
    const auto spawn = frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9212, 2.0f, 0.0f, 0.0f), sid_viewer));
    std::vector<std::uint8_t> ws_spawn;
    append_masked_frame(ws_spawn, ws::OpCode::kBinary, true, spawn.data(), spawn.size(), 0x77);
    viewer.sendAll(ws_spawn);

    TEST_ASSERT(wait_player_spawned(*world.world, sid_mover, 9211, 8000), "mover spawned");
    TEST_ASSERT(wait_player_spawned(*world.world, sid_viewer, 9212, 8000), "ws viewer spawned");

    {
        std::vector<std::uint8_t> scratch;
        while (mover.recvLinkFrame(scratch)) {}
        for (int i = 0; i < 8; ++i) {
            std::uint8_t op = 0;
            std::vector<std::uint8_t> p;
            if (!viewer.recvWsFrame(op, p)) {
                break;
            }
        }
    }

    mover.sendAll(frame_bytes(3, 0,
        protocol::MessageCodec::encode(make_move(9211, 5.0f, 0.0f, 0.0f), sid_mover)));

    ProtocolFrame out;
    std::vector<ProtocolFrame> sink;
    TEST_ASSERT(wait_ws_frame(viewer, static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE),
                              sid_viewer, out, sink, 8000),
                "ws viewer receives re-addressed move broadcast");
    const auto move = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(out.body);
    TEST_ASSERT(move.entityId == 9211 && move.newPos.x == 5.0f,
                "move body intact for ws viewer");

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 6. 技能伤害广播：零会话号帧头按 viewer 重编（战斗下发面） ----

bool test_skill_damage_broadcast() {
    std::cout << "Running: test_skill_damage_broadcast..." << std::endl;
    WorldEnd world;
    TEST_ASSERT(world.start(49), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    // caster 与 target 都在视野内（半径 200）；CombatDamage 帧头会话号为 0，
    // 下行必须按各 viewer 会话号重编才可寻址。
    TcpTestClient caster;
    std::uint64_t sid_caster = 0;
    TEST_ASSERT(tcp_establish(caster, server, server.listenPort(), 9301, sid_caster),
                "caster session established");
    caster.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9301, 0.0f, 0.0f, 0.0f), sid_caster)));

    TcpTestClient target;
    std::uint64_t sid_target = 0;
    TEST_ASSERT(tcp_establish(target, server, server.listenPort(), 9302, sid_target),
                "target session established");
    target.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9302, 1.0f, 0.0f, 0.0f), sid_target)));

    TEST_ASSERT(wait_player_spawned(*world.world, sid_caster, 9301, 8000), "caster spawned");
    TEST_ASSERT(wait_player_spawned(*world.world, sid_target, 9302, 8000), "target spawned");
    {
        std::vector<std::uint8_t> scratch;
        while (caster.recvLinkFrame(scratch)) {}
        while (target.recvLinkFrame(scratch)) {}
    }

    // 施放：COMBAT_SKILL_CAST → world 广播 COMBAT_DAMAGE 给目标视野内全员。
    protocol::CombatSkillCast cast;
    cast.casterId = 9301;
    cast.targetId = 9302;
    cast.skillId = 7;
    cast.casterPos = {0.0f, 0.0f, 0.0f};
    cast.targetPos = {1.0f, 0.0f, 0.0f};
    caster.sendAll(frame_bytes(3, 0, protocol::MessageCodec::encode(cast, sid_caster)));

    // 目标收到伤害帧（帧头会话号 = 本会话，body 完整）。
    ProtocolFrame out_target;
    std::vector<ProtocolFrame> sink_target;
    TEST_ASSERT(wait_tcp_frame(target, static_cast<std::uint16_t>(protocol::MessageType::COMBAT_DAMAGE),
                               sid_target, out_target, sink_target, 8000),
                "target receives re-addressed damage broadcast");
    const auto damage = protocol::MessageCodec::decodeBody<protocol::CombatDamage>(out_target.body);
    TEST_ASSERT(damage.targetId == 9302 && damage.sourceId == 9301 && damage.damage == -10,
                "damage body intact for target");

    // 施法者同在目标视野，也收同一路广播（自己的会话号寻址）。
    ProtocolFrame out_caster;
    std::vector<ProtocolFrame> sink_caster;
    TEST_ASSERT(wait_tcp_frame(caster, static_cast<std::uint16_t>(protocol::MessageType::COMBAT_DAMAGE),
                               sid_caster, out_caster, sink_caster, 8000),
                "caster receives the same broadcast addressed to its own session");

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 7. 客户端消息 → Lua 契约路由消费 → world 模块挂推送源（脚本源帧回环）----
// CELL_ENTITY_MOVE 上行 → C++ handler（权威状态）→ dispatch_script_intent 按
// 契约路由带参调 Lua cell_entity_move → 脚本 world.broadcast_viewers 挂源 →
// 逐 viewer 重编帧头走 939 下行。缆线可见面验收「客户端消息到 Lua/场景逻辑」。
bool test_lua_intent_push_loop() {
    WorldEnd world;
    // 契约只覆盖移动一路：路由 926 → 全局函数 cell_entity_move（参数面 =
    // 帧体字段 entity + 前后坐标，typed LuaValue 推栈）。
    world.scriptContractSrc =
        "return { routes = { [" +
        std::to_string(static_cast<int>(apollo::contract::gen::MsgId::cell_entity_move)) +
        "] = 'cell_entity_move' }, write_whitelist = {}, version = 1 }\n";
    world.scriptHandlersSrc =
        "cell_entity_move = function(entity_id, ox, oy, oz, nx, ny, nz)\n"
        "  world.broadcast_viewers(entity_id, 7777, 'lua-push:' .. tostring(nx))\n"
        "end\n";
    TEST_ASSERT(world.start(51), "world started with script + contract");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    // mover 与 viewer 相邻（同在半径 200 视野内）：移动后两者都应收到脚本源
    // 7777 帧（viewer 旁观 + mover 自 echo），帧头会话号各自重编。
    TcpTestClient mover;
    std::uint64_t sid_mover = 0;
    TEST_ASSERT(tcp_establish(mover, server, server.listenPort(), 9401, sid_mover),
                "mover session established");
    mover.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9401, 0.0f, 0.0f, 0.0f), sid_mover)));

    TcpTestClient viewer;
    std::uint64_t sid_viewer = 0;
    TEST_ASSERT(tcp_establish(viewer, server, server.listenPort(), 9402, sid_viewer),
                "viewer session established");
    viewer.sendAll(frame_bytes(2, 0,
        protocol::MessageCodec::encode(make_player_spawn_at(9402, 1.0f, 0.0f, 0.0f), sid_viewer)));

    TEST_ASSERT(wait_player_spawned(*world.world, sid_mover, 9401, 8000), "mover spawned");
    TEST_ASSERT(wait_player_spawned(*world.world, sid_viewer, 9402, 8000), "viewer spawned");
    {
        std::vector<std::uint8_t> scratch;
        while (mover.recvLinkFrame(scratch)) {}
        while (viewer.recvLinkFrame(scratch)) {}
    }

    // 移动上行：C++ handler 先落权威状态，Lua 路由随后带参消费并挂推送源。
    mover.sendAll(frame_bytes(3, 0,
        protocol::MessageCodec::encode(make_move(9401, 3.0f, 0.0f, 0.0f), sid_mover)));

    // viewer 收到脚本源帧：类型 7777、本会话号寻址、body = 'lua-push:' ..
    // tostring(nx)。Lua 5.4 tostring(3.0) = "3.0"，故按前缀匹配。
    ProtocolFrame out_viewer;
    std::vector<ProtocolFrame> sink_viewer;
    TEST_ASSERT(wait_tcp_frame(viewer, 7777, sid_viewer, out_viewer, sink_viewer, 8000),
                "viewer receives lua-sourced broadcast");
    const std::string body_viewer(out_viewer.body.begin(), out_viewer.body.end());
    TEST_ASSERT(body_viewer.rfind("lua-push:3", 0) == 0,
                "lua body carries new-x argument (tostring(3.0) prefix)");

    // mover 自 echo：同一路广播按其会话号寻址到达自己。
    ProtocolFrame out_mover;
    std::vector<ProtocolFrame> sink_mover;
    TEST_ASSERT(wait_tcp_frame(mover, 7777, sid_mover, out_mover, sink_mover, 8000),
                "mover receives its own lua-sourced broadcast");

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 8. 重复/乱序规则：重放帧闸外、gap 容忍、零序直通（缆线可见负例）----
// 闸在网关会话层按协议头 sequence 记水位：≤ 水位 = 重放/重复不分发，
// > 水位（含跳空）= 受理，0 = 无序占位直通不推水位。以 mover 自 echo 的
// AOI 移动广播为「已受理」证据面——重复帧不受理即零帧到达。
bool test_uplink_duplicate_frames_gated() {
    std::cout << "Running: test_uplink_duplicate_frames_gated..." << std::endl;

    WorldEnd world;
    TEST_ASSERT(world.start(52), "world started with M1 port");
    gateway::GatewayServer server(gateway_config(world.world->m1Link().listenPort(), 0));
    server.start();

    TcpTestClient mover;
    std::uint64_t sid = 0;
    TEST_ASSERT(tcp_establish(mover, server, server.listenPort(), 9501, sid),
                "mover session established");

    // nextSequence 为进程全局自增（PING 已占号）——建会话后取新基，测序恒
    // 大于 PING 建立的水位，判定不依赖计数器绝对值。
    const auto base = protocol::MessageCodec::nextSequence();

    const auto spawn = with_sequence(
        protocol::MessageCodec::encode(make_player_spawn_at(9501, 0.0f, 0.0f, 0.0f), sid),
        base + 1);
    mover.sendAll(frame_bytes(2, 0, spawn));
    TEST_ASSERT(wait_player_spawned(*world.world, sid, 9501, 8000),
                "spawn accepted and bound (seq advances watermark)");

    const auto move_a = with_sequence(
        protocol::MessageCodec::encode(make_move(9501, 3.0f, 0.0f, 0.0f), sid), base + 2);
    mover.sendAll(frame_bytes(3, 0, move_a));
    ProtocolFrame out;
    std::vector<ProtocolFrame> sink;
    const auto move_type = static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE);
    TEST_ASSERT(wait_tcp_frame(mover, move_type, sid, out, sink, 8000),
                "move A broadcast to mover");

    // 原帧重放（同序 base+2 ≤ 水位）：闸外——窗内零移动帧。
    mover.sendAll(frame_bytes(3, 0, move_a));
    TEST_ASSERT(!wait_tcp_frame(mover, move_type, sid, out, sink, 500),
                "replayed frame gated out");

    // 跳空前进（base+2 → base+7）：流传输丢帧空洞容忍，受理并广播。
    const auto move_b = with_sequence(
        protocol::MessageCodec::encode(make_move(9501, 4.0f, 0.0f, 0.0f), sid), base + 7);
    mover.sendAll(frame_bytes(3, 0, move_b));
    TEST_ASSERT(wait_tcp_frame(mover, move_type, sid, out, sink, 8000),
                "forward gap tolerated");

    // 旧序重放（base+2 < 水位 base+7）：同闸拦截。
    mover.sendAll(frame_bytes(3, 0, move_a));
    TEST_ASSERT(!wait_tcp_frame(mover, move_type, sid, out, sink, 500),
                "stale replay gated out");

    // 零序占位（无序语义）：两帧均直通受理——零序不推水位，不参与去重。
    for (const auto step : {5.0f, 6.0f}) {
        mover.sendAll(frame_bytes(3, 0,
            with_sequence(protocol::MessageCodec::encode(make_move(9501, step, 0.0f, 0.0f), sid), 0)));
        TEST_ASSERT(wait_tcp_frame(mover, move_type, sid, out, sink, 8000),
                    "zero-sequence frame passes through");
    }

    server.stop();
    world.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#endif  // !_WIN32

}  // namespace

int main() {
#if !defined(_WIN32)
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"tcp_push_downlink", test_tcp_push_downlink},
        {"ws_push_downlink", test_ws_push_downlink},
        {"push_session_addressed", test_push_session_addressed},
        {"aoi_broadcast_to_viewers", test_aoi_broadcast_to_viewers},
        {"aoi_broadcast_to_ws_viewer", test_aoi_broadcast_to_ws_viewer},
        {"skill_damage_broadcast", test_skill_damage_broadcast},
        {"lua_intent_push_loop", test_lua_intent_push_loop},
        {"uplink_duplicate_frames_gated", test_uplink_duplicate_frames_gated},
    };

    int failed = 0;
    for (const auto& case_entry : cases) {
        if (!case_entry.fn()) {
            std::cerr << "FAIL: case " << case_entry.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }
    std::cerr << failed << " test(s) failed" << std::endl;
    return 1;
#else
    std::cout << "skipped: link TCP/TLS transports are POSIX-only in this batch" << std::endl;
    return 0;
#endif
}
