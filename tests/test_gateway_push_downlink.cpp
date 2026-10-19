// 网关服务端主动下行测试（L51 游戏 WS/WSS → Lua/场景接线与下行第一锤）：
// World 939 client_deliver → Gateway sendToClient → 真实客户端 socket。
//
// 既有 egress 级用例（world_drain/update_continuity）只用 ClientFace 汇流点
// 证明下行到达 egress——本批从真实 TCP/WS 客户端走完整个游戏帧/会话/意图
// 路径：PING 建会话 → 准入绑玩家 → CELL_CREATE_ENTITY 造玩家实体 →
// PARTY_CREATE_REQUEST → World 经 M1 939 回推 PARTY_RESULT → 网关按会话号
// 真投真实 socket。覆盖：
//   1. TCP 明文客户端收到 World 主动推送（帧头 sessionId = 本会话号）；
//   2. WS 客户端收到同一路推送（升级 /game → masked binary → 服务端 binary）；
//   3. 推送按会话寻址：非队长会话收不到 PARTY_RESULT（多会话隔离）。
// 下行证据走线缆可见面（客户端 socket 真收到协议全帧），非日志非计数。
// Windows：link TCP/TLS 底座不进 M1 批，用例本体空转。

#include "gateway/gateway_server.hpp"
#include "world_app/world_app.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
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

    bool start(std::uint32_t world_id) {
        config.worldId = world_id;
        config.host = "127.0.0.1";
        config.m1Port = 0;
        config.m1RouteJson = kRouteJson;
        config.tickRateMs = 20;
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
