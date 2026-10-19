// gateway 会话连续测试（L53 断线重连：保留窗内跨连接/跨传输恢复同一会话）。
//
// 覆盖：
//   1. TCP 掉线保留已认证会话（连接计数归零、会话仍在册）；
//   2. 窗内新 TCP 连接首包携旧会话号上行即恢复：PONG 回执头 sessionId
//      与旧号一致，连接/会话计数各归 1，同号继续走心跳；
//   3. 保留窗到期销毁（会话计数归零），窗后携旧号重连得新会话号（不恢复）；
//   4. 未绑定玩家的会话掉线立即销毁（无可保留，重连即全新会话）；
//   5. sessionRetentionMs=0 关窗回旧语义（掉线即销毁、旧号不恢复）；
//   6. TCP 掉线后经 WS 重连恢复同一会话（跨传输正向）；
//   7. WS 掉线后经 TCP 重连恢复同一会话（跨传输反向）。
//
// 恢复判据走线缆可见面：上行 PING 帧头 sessionId = 客户端记住的旧会话号，
// 服务端 PONG 回执头带当前会话号——恢复成功回执同号，落回全新会话则回执
// 新号。Windows：link TCP/TLS 底座不进 M1 批，用例本体空转。

#include "gateway/gateway_server.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
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

#if !defined(_WIN32)

// macOS CI 裸自旋必 flake：重试循环每拍 sleep ≥2ms。
const auto kTick = std::chrono::milliseconds(2);

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
        tv.tv_sec = 10;  // 单收上限：CI 挂死守门
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
        tv.tv_sec = 10;
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
    apollo::protocol::Ping ping;
    ping.timestamp = 777;
    return apollo::protocol::MessageCodec::encode(ping, session_id);
}

apollo::protocol::MessageHeader header_of(const std::vector<std::uint8_t>& protocol_frame) {
    return apollo::protocol::MessageCodec::parseHeader(
        protocol_frame.data(), protocol_frame.size());
}

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

// 阶段一公用件：建立已认证会话并异常掉线，回旧会话号。
// retention 全组共用小窗 + 快检查拍；sessionTimeoutMs 拉满防心跳超时干扰。
struct RetainFixture {
    gateway::GatewayConfig config;
    gateway::SessionID session_id = 0;
};

gateway::GatewayConfig base_config(std::int64_t retention_ms) {
    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.sessionRetentionMs = retention_ms;
    config.sessionTimeoutMs = 600000;      // 心跳超时不干扰（保留窗单独裁决）
    config.heartbeatCheckIntervalMs = 50;  // 到期销毁的检查拍
    config.controlM1Port = 0;              // 排空控制面不开（测试无 ops 面）
    return config;
}

// ---- 1. TCP 掉线保留已认证会话 ----

bool test_tcp_drop_retains_authed_session() {
    std::cout << "Running: test_tcp_drop_retains_authed_session..." << std::endl;

    gateway::GatewayServer server(base_config(2000));
    server.start();
    TEST_ASSERT(server.listenPort() != 0, "tcp ingress listening");

    TcpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", server.listenPort()), "client connected");
    client.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(client.recvLinkFrame(reply), "pong received");
    const auto fresh = header_of(reply).sessionId;
    TEST_ASSERT(fresh != 0, "fresh session id echoed");

    TEST_ASSERT(server.authenticateSession(fresh, 900001, "ticket"), "player bound");

    client.close();  // 异常掉线（FIN，无 logout 通道）
    TEST_ASSERT(wait_until([&] { return server.getRegisteredConnectionCount() == 0; }, 2000),
                "connection torn down");
    TEST_ASSERT(server.getSessionCount() == 1, "authed session retained across drop");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 2. 窗内携旧会话号重连恢复同一会话 ----

bool test_tcp_resume_same_session() {
    std::cout << "Running: test_tcp_resume_same_session..." << std::endl;

    gateway::GatewayServer server(base_config(2000));
    server.start();

    TcpTestClient first;
    TEST_ASSERT(first.connect("127.0.0.1", server.listenPort()), "first connected");
    first.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(first.recvLinkFrame(reply), "pong received");
    const auto old_id = header_of(reply).sessionId;
    TEST_ASSERT(old_id != 0, "session id echoed");
    TEST_ASSERT(server.authenticateSession(old_id, 900002, "ticket"), "player bound");

    first.close();
    TEST_ASSERT(wait_until([&] { return server.getRegisteredConnectionCount() == 0; }, 2000),
                "connection torn down");
    TEST_ASSERT(server.getSessionCount() == 1, "session retained");

    // 重连：首包 PING 头携旧会话号 → 恢复，PONG 回执同号。
    TcpTestClient second;
    TEST_ASSERT(second.connect("127.0.0.1", server.listenPort()), "second connected");
    second.sendAll(frame_bytes(1, 0, ping_packet(old_id)));
    TEST_ASSERT(second.recvLinkFrame(reply), "resume pong received");
    TEST_ASSERT(header_of(reply).sessionId == old_id, "resumed same session id");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "one connection registered");
    TEST_ASSERT(server.getSessionCount() == 1, "fresh session discarded, retained rebound");

    // 同号继续心跳：恢复后的会话走原心跳路径。
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    second.sendAll(frame_bytes(2, 0, ping_packet(old_id)));
    TEST_ASSERT(second.recvLinkFrame(reply), "post-resume pong received");
    TEST_ASSERT(header_of(reply).sessionId == old_id, "session id stable after resume");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 3. 保留窗到期销毁，窗后旧号不恢复 ----

bool test_tcp_resume_after_expiry() {
    std::cout << "Running: test_tcp_resume_after_expiry..." << std::endl;

    gateway::GatewayServer server(base_config(300));
    server.start();

    TcpTestClient first;
    TEST_ASSERT(first.connect("127.0.0.1", server.listenPort()), "first connected");
    first.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(first.recvLinkFrame(reply), "pong received");
    const auto old_id = header_of(reply).sessionId;
    TEST_ASSERT(server.authenticateSession(old_id, 900003, "ticket"), "player bound");

    first.close();
    TEST_ASSERT(wait_until([&] { return server.getSessionCount() == 1; }, 2000),
                "session retained initially");

    // 窗到期（retention 300ms + 50ms 检查拍）：延迟的断线销毁。
    TEST_ASSERT(wait_until([&] { return server.getSessionCount() == 0; }, 3000),
                "retention expired, session destroyed");

    // 窗后携旧号重连：恢复被拒，落全新会话。
    TcpTestClient second;
    TEST_ASSERT(second.connect("127.0.0.1", server.listenPort()), "second connected");
    second.sendAll(frame_bytes(1, 0, ping_packet(old_id)));
    TEST_ASSERT(second.recvLinkFrame(reply), "pong received");
    TEST_ASSERT(header_of(reply).sessionId != old_id, "stale id not resumed");
    TEST_ASSERT(header_of(reply).sessionId != 0, "fresh session served");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 4. 未认证会话掉线立即销毁 ----

bool test_tcp_drop_unauthed_destroyed() {
    std::cout << "Running: test_tcp_drop_unauthed_destroyed..." << std::endl;

    gateway::GatewayServer server(base_config(2000));
    server.start();

    TcpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", server.listenPort()), "client connected");
    client.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(client.recvLinkFrame(reply), "pong received");
    TEST_ASSERT(header_of(reply).sessionId != 0, "session id echoed");

    client.close();
    TEST_ASSERT(wait_until([&] { return server.getSessionCount() == 0; }, 2000),
                "unauthed session destroyed immediately");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 5. retention=0 关窗回旧语义 ----

bool test_retention_zero_legacy_destroy() {
    std::cout << "Running: test_retention_zero_legacy_destroy..." << std::endl;

    gateway::GatewayServer server(base_config(0));
    server.start();

    TcpTestClient first;
    TEST_ASSERT(first.connect("127.0.0.1", server.listenPort()), "first connected");
    first.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(first.recvLinkFrame(reply), "pong received");
    const auto old_id = header_of(reply).sessionId;
    TEST_ASSERT(server.authenticateSession(old_id, 900004, "ticket"), "player bound");

    first.close();
    TEST_ASSERT(wait_until([&] { return server.getSessionCount() == 0; }, 2000),
                "session destroyed immediately when window off");

    TcpTestClient second;
    TEST_ASSERT(second.connect("127.0.0.1", server.listenPort()), "second connected");
    second.sendAll(frame_bytes(1, 0, ping_packet(old_id)));
    TEST_ASSERT(second.recvLinkFrame(reply), "pong received");
    TEST_ASSERT(header_of(reply).sessionId != old_id, "stale id not resumed");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 6. TCP 掉线 → WS 重连恢复（跨传输正向）----

bool test_ws_resume_after_tcp_drop() {
    std::cout << "Running: test_ws_resume_after_tcp_drop..." << std::endl;

    auto config = base_config(2000);
    config.wsPort = probe_free_port();
    TEST_ASSERT(config.wsPort != 0, "free port probed");
    gateway::GatewayServer server(config);
    server.start();
    TEST_ASSERT(server.wsListenPort() != 0, "ws ingress listening");

    TcpTestClient first;
    TEST_ASSERT(first.connect("127.0.0.1", server.listenPort()), "tcp connected");
    first.sendAll(frame_bytes(1, 0, ping_packet(0)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(first.recvLinkFrame(reply), "pong received");
    const auto old_id = header_of(reply).sessionId;
    TEST_ASSERT(server.authenticateSession(old_id, 900005, "ticket"), "player bound");

    first.close();
    TEST_ASSERT(wait_until([&] { return server.getRegisteredConnectionCount() == 0; }, 2000),
                "tcp connection torn down");
    TEST_ASSERT(server.getSessionCount() == 1, "session retained");

    // 经 WS 重连：升级 /game → masked binary（载荷 = link Frame 载协议全帧）。
    WsTestClient ws;
    TEST_ASSERT(ws.connect("127.0.0.1", server.wsListenPort()), "ws connected");
    ws.sendAll(upgrade_request("/game"));
    std::string response;
    TEST_ASSERT(ws.recvHttpResponse(response) && response.find("101") != std::string::npos,
                "ws upgraded");
    const auto link_frame = frame_bytes(1, 0, ping_packet(old_id));
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true, link_frame.data(),
                        link_frame.size(), 0x33);
    ws.sendAll(ws_frame);

    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(ws.recvWsFrame(opcode, payload), "ws reply received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kBinary), "binary frame");
    netlink::FrameParser net_parser;
    const auto outcome = net_parser.feed(payload);
    TEST_ASSERT(!outcome.fatal && outcome.frames.size() == 1, "link frame intact");
    TEST_ASSERT(header_of(outcome.frames[0].payload).sessionId == old_id,
                "resumed same session over ws");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "one connection registered");
    TEST_ASSERT(server.getSessionCount() == 1, "one session after resume");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 7. WS 掉线 → TCP 重连恢复（跨传输反向）----

bool test_tcp_resume_after_ws_drop() {
    std::cout << "Running: test_tcp_resume_after_ws_drop..." << std::endl;

    auto config = base_config(2000);
    config.wsPort = probe_free_port();
    TEST_ASSERT(config.wsPort != 0, "free port probed");
    gateway::GatewayServer server(config);
    server.start();

    WsTestClient ws;
    TEST_ASSERT(ws.connect("127.0.0.1", server.wsListenPort()), "ws connected");
    ws.sendAll(upgrade_request("/game"));
    std::string response;
    TEST_ASSERT(ws.recvHttpResponse(response) && response.find("101") != std::string::npos,
                "ws upgraded");
    const auto first_frame = frame_bytes(1, 0, ping_packet(0));
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true, first_frame.data(),
                        first_frame.size(), 0x44);
    ws.sendAll(ws_frame);
    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(ws.recvWsFrame(opcode, payload), "ws reply received");
    netlink::FrameParser net_parser;
    const auto outcome = net_parser.feed(payload);
    TEST_ASSERT(!outcome.fatal && outcome.frames.size() == 1, "link frame intact");
    const auto old_id = header_of(outcome.frames[0].payload).sessionId;
    TEST_ASSERT(old_id != 0, "session id echoed");
    TEST_ASSERT(server.authenticateSession(old_id, 900006, "ticket"), "player bound");

    ws.close();  // TCP FIN 直断（无 ws close 帧）
    TEST_ASSERT(wait_until([&] { return server.getRegisteredConnectionCount() == 0; }, 2000),
                "ws connection torn down");
    TEST_ASSERT(server.getSessionCount() == 1, "session retained");

    // 经 TCP 重连恢复。
    TcpTestClient tcp;
    TEST_ASSERT(tcp.connect("127.0.0.1", server.listenPort()), "tcp connected");
    tcp.sendAll(frame_bytes(1, 0, ping_packet(old_id)));
    std::vector<std::uint8_t> reply;
    TEST_ASSERT(tcp.recvLinkFrame(reply), "resume pong received");
    TEST_ASSERT(header_of(reply).sessionId == old_id, "resumed same session over tcp");

    server.stop();
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
        {"tcp_drop_retains_authed_session", test_tcp_drop_retains_authed_session},
        {"tcp_resume_same_session", test_tcp_resume_same_session},
        {"tcp_resume_after_expiry", test_tcp_resume_after_expiry},
        {"tcp_drop_unauthed_destroyed", test_tcp_drop_unauthed_destroyed},
        {"retention_zero_legacy_destroy", test_retention_zero_legacy_destroy},
        {"ws_resume_after_tcp_drop", test_ws_resume_after_tcp_drop},
        {"tcp_resume_after_ws_drop", test_tcp_resume_after_ws_drop},
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
