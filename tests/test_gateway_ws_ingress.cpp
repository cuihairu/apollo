// gateway WS/WSS 入口真接线测试（L45 批③；localhost 实 socket）。
//
// 覆盖：
//   1. ws 升级 round-trip：GET /game → 101（含算出的 Accept）→ masked
//      binary 帧（载荷 = link Frame）→ observer Opened+Packet；send →
//      无掩码 binary 帧回包（载荷 = link Frame）；
//   2. 路径不命中 → 400 no such route + 断连，无任何 observer 事件；
//   3. 非法升级（POST）→ 400 invalid websocket upgrade；
//   4. 垃圾字节流（无头部终止，超 8KiB 上限）→ 400；
//   5. ws ping → pong 回执；对端 close → close echo + NormalClose 事件；
//   6. text 帧（契约外载荷）→ TransportError 断连；
//   7. wss round-trip：TLS 底座上同一套升级与帧语义（自签夹具）；
//   8. 扇入号段：TCP(基址 0) + WS(基址 1<<40) 并装，号互斥、send 各回各；
//   9. WSS 配置缺凭据 → GatewayServer 装配即抛（不静默降级成明文）；
//  10. GatewayServer 全链 e2e：config.wsPort 起 WS 监听，真 ws 客户端
//      PING（netlink 帧）→ 分发 → PONG 回包（ws binary 帧）；
//  11. 有界积压：升级后业务连接出站残余超上限 → SlowConsumer 踢线
//      （客户端不读，8 MiB 灌压）。
//
// Windows：link TCP/TLS 底座不进 M1 批（POSIX only）——用例本体空转。

#include "gateway/gateway_server.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/ingress/ingress_fan_in.hpp"
#include "gateway/ingress/tcp_client_ingress_server.hpp"
#include "gateway/ingress/ws_client_ingress_server.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "tls_test_support.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
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
using gateway::ConnectionID;
using gateway::DisconnectReason;

enum class EventKind { Opened, Packet, Closed };

struct RecordedEvent {
    EventKind kind{EventKind::Closed};
    ConnectionID connectionId{0};
    gateway::ClientEndpoint endpoint;
    DisconnectReason reason{DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

class RecordingObserver final : public gateway::IClientIngressObserver {
public:
    void onConnectionOpened(ConnectionID connectionId,
                            const gateway::ClientEndpoint& endpoint) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Opened, connectionId, endpoint,
                           DisconnectReason::NormalClose, {}});
    }

    void onPacketReceived(ConnectionID connectionId,
                          std::span<const std::uint8_t> payload) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Packet, connectionId, {},
                           DisconnectReason::NormalClose,
                           {payload.begin(), payload.end()}});
    }

    void onConnectionClosed(ConnectionID connectionId, DisconnectReason reason) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Closed, connectionId, {}, reason, {}});
    }

    void onConnectionIdle(ConnectionID) override {}

    std::vector<RecordedEvent> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_;
    }

    std::size_t count(EventKind kind) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t n = 0;
        for (const auto& event : events_) {
            if (event.kind == kind) {
                ++n;
            }
        }
        return n;
    }

private:
    mutable std::mutex mutex_;
    std::vector<RecordedEvent> events_;
};

#if !defined(_WIN32)

// macOS CI 裸自旋必 flake：重试循环每拍 sleep ≥2ms。
const auto kTick = std::chrono::milliseconds(2);

template <typename Pred>
bool pump_until(gateway::ClientIngressServer& ingress, Pred pred, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        ingress.pump();
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(kTick);
    }
    ingress.pump();
    return pred();
}

// 泵固定时长（无 observer 事件的回执面：pong/close echo/400 已在泵内写出）。
void pump_rounds(gateway::ClientIngressServer& ingress, int durationMs) {
    pump_until(ingress, [] { return false; }, durationMs);
}

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

// ---- ws 测试客户端（阻塞 socket；独立实现，不复用被测代码收发路径）----

const char* kClientKey = "dGhlIHNhbXBsZSBub25jZQ==";

std::vector<std::uint8_t> upgrade_request(const std::string& method, const std::string& path) {
    const std::string req = std::string(method) + " " + path + " HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Key: " + std::string(kClientKey) + "\r\n"
                            "Sec-WebSocket-Version: 13\r\n"
                            "\r\n";
    return {req.begin(), req.end()};
}

// 客户端 → 服务端帧（必须掩码）。
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

constexpr char kHeaderTerm[] = "\r\n\r\n";

class WsTestClient {
public:
    ~WsTestClient() { close(); }

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

    // 读 HTTP 应答（头至 \r\n\r\n + Content-Length 指定的 body）。
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
        const std::string head(buf_.begin(), terminator + 4);
        std::size_t body_len = 0;
        const auto marker = head.find("Content-Length:");
        if (marker != std::string::npos) {
            body_len = static_cast<std::size_t>(
                std::strtoull(head.c_str() + marker + 15, nullptr, 10));
        }
        while (buf_.size() < head.size() + body_len) {
            if (!fill()) {
                return false;
            }
        }
        response = head + std::string(
                             buf_.begin() + static_cast<long>(head.size()),
                             buf_.begin() + static_cast<long>(head.size() + body_len));
        buf_.erase(buf_.begin(),
                   buf_.begin() + static_cast<long>(head.size() + body_len));
        return true;
    }

    // 读一个服务端帧（无掩码）。
    bool recvWsFrame(std::uint8_t& opcode, std::vector<std::uint8_t>& payload) {
        for (;;) {
            if (buf_.size() >= 2) {
                const std::uint8_t b0 = buf_[0];
                const std::uint8_t b1 = buf_[1];
                if ((b1 & 0x80) != 0) {
                    return false;  // 服务端帧不掩码——掩码位即坏
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

    bool waitEof(int timeoutMs) {
        pollfd pfd{fd_, POLLIN, 0};
        if (::poll(&pfd, 1, timeoutMs) <= 0) {
            return false;
        }
        std::uint8_t scratch[64];
        const ssize_t n = ::recv(fd_, scratch, sizeof scratch, 0);
        return n <= 0;
    }

    // 底层描述符（慢读踢线收线面：泵 + 大块排空交替用）。
    int fd() const { return fd_; }

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
};

std::unique_ptr<gateway::ClientIngressServer> make_game_ws_ingress(
    RecordingObserver& observer, apollo::net::tls::TlsServerContext* tls = nullptr) {
    gateway::WsIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.path = "/game";
    config.tls = tls;
    auto ingress = gateway::makeWsClientIngressServer(config);
    ingress->setObserver(&observer);
    ingress->start();
    return ingress;
}

// ---- 1. ws 升级 round-trip ----

bool test_ws_upgrade_roundtrip() {
    std::cout << "Running: test_ws_upgrade_roundtrip..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/game"));

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened after upgrade");
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "101 response received");
    TEST_ASSERT(response.find("101") != std::string::npos, "switching protocols");
    TEST_ASSERT(response.find("Sec-WebSocket-Accept: "
                              + ws::compute_accept_key(kClientKey)) != std::string::npos,
                "accept key computed");

    // PING 协议包（MessageHeader+body）入 link 帧入 ws binary 帧。
    apollo::protocol::Ping ping;
    ping.timestamp = 4242;
    const auto packet = apollo::protocol::MessageCodec::encode(ping, 0);
    const auto link_frame = frame_bytes(1, 0, packet);
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true, link_frame.data(),
                        link_frame.size(), 0x3c);
    client.sendAll(ws_frame);

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Packet) == 1; },
                           2000),
                "packet reached observer");
    auto events = observer.snapshot();
    TEST_ASSERT(events.size() == 2, "opened + packet events");
    TEST_ASSERT(events[1].payload == packet, "netlink payload intact");
    const auto connection_id = events[0].connectionId;

    // 出站：send → 无掩码 binary 帧，载荷 = link Frame。
    const std::vector<std::uint8_t> reply{0xAA, 0xBB, 0xCC};
    ingress->send(connection_id, reply);
    pump_rounds(*ingress, 100);
    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> frame_payload;
    TEST_ASSERT(client.recvWsFrame(opcode, frame_payload), "ws frame received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kBinary),
                "reply is binary frame");
    netlink::FrameParser net_parser;
    const auto net_outcome = net_parser.feed(frame_payload);
    TEST_ASSERT(!net_outcome.fatal && net_outcome.frames.size() == 1, "link frame intact");
    TEST_ASSERT(net_outcome.frames[0].payload == reply, "payload intact");

    client.close();
    TEST_ASSERT(pump_until(*ingress,
                           [&] { return observer.count(EventKind::Closed) == 1; }, 2000),
                "closed on eof");
    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 2. 路径不命中 → 400 + 断连 + 无事件 ----

bool test_ws_wrong_path_rejected() {
    std::cout << "Running: test_ws_wrong_path_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/admin"));

    pump_rounds(*ingress, 300);  // 400 在泵内写出后即拆线
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("400") != std::string::npos, "400 status");
    TEST_ASSERT(response.find("no such route") != std::string::npos, "reason body");
    TEST_ASSERT(client.waitEof(2000), "connection closed after reject");
    TEST_ASSERT(observer.snapshot().empty(), "no observer events for rejected upgrade");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 3. 非法升级（POST）→ 400 ----

bool test_ws_invalid_upgrade_rejected() {
    std::cout << "Running: test_ws_invalid_upgrade_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("POST", "/game"));

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("400") != std::string::npos, "400 status");
    TEST_ASSERT(response.find("invalid websocket upgrade") != std::string::npos,
                "reason body");
    TEST_ASSERT(client.waitEof(2000), "connection closed after reject");
    TEST_ASSERT(observer.snapshot().empty(), "no observer events");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 4. 垃圾字节流（超头部上限）→ 400 ----

bool test_ws_garbage_rejected() {
    std::cout << "Running: test_ws_garbage_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    std::vector<std::uint8_t> garbage(9 * 1024, 'x');  // 超握手头部 8KiB 上限
    client.sendAll(garbage);

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("400") != std::string::npos, "400 status");
    TEST_ASSERT(observer.snapshot().empty(), "no observer events");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 5. ws ping → pong；close → echo + NormalClose ----

bool test_ws_ping_close_control() {
    std::cout << "Running: test_ws_ping_close_control..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/game"));
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response) && response.find("101") != std::string::npos,
                "upgraded");

    // ping → pong（原载荷无掩码回）。
    const std::uint8_t hb[] = "hb";
    std::vector<std::uint8_t> ping_frame;
    append_masked_frame(ping_frame, ws::OpCode::kPing, true, hb, 2, 0x55);
    client.sendAll(ping_frame);
    pump_rounds(*ingress, 100);
    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(client.recvWsFrame(opcode, payload), "pong received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kPong), "pong opcode");
    TEST_ASSERT(payload.size() == 2 && payload[0] == 'h' && payload[1] == 'b',
                "ping payload echoed");

    // close(1000) → echo close + NormalClose 事件。
    const std::uint8_t close_code[] = {0x03, 0xE8};
    std::vector<std::uint8_t> close_frame;
    append_masked_frame(close_frame, ws::OpCode::kClose, true, close_code, 2, 0x66);
    client.sendAll(close_frame);
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "closed event fired");
    TEST_ASSERT(client.recvWsFrame(opcode, payload), "close echo received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kClose), "close opcode");
    TEST_ASSERT(payload.size() == 2 && payload[0] == 0x03 && payload[1] == 0xE8,
                "close code echoed");
    const auto closed = observer.snapshot();
    TEST_ASSERT(closed.back().reason == DisconnectReason::NormalClose, "normal close reason");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 6. text 帧 = 契约外载荷 → TransportError 断连 ----

bool test_ws_text_frame_rejected() {
    std::cout << "Running: test_ws_text_frame_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/game"));
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");

    const std::uint8_t text[] = "hello";
    std::vector<std::uint8_t> text_frame;
    append_masked_frame(text_frame, ws::OpCode::kText, true, text, 5, 0x77);
    client.sendAll(text_frame);

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "closed event fired");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == DisconnectReason::TransportError,
                "text frame is transport error");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 7. wss round-trip（TLS 底座上同一套语义）----

bool test_wss_roundtrip() {
    std::cout << "Running: test_wss_roundtrip..." << std::endl;

    const std::string dir = "/tmp/apollo-ws-ingress-tests-" + std::to_string(::getpid());
    TEST_ASSERT(::system(("mkdir -p " + dir).c_str()) == 0, "cert dir created");
    const auto certs = tls_test_support::generate_self_signed(dir, "wss", "localhost");
    TEST_ASSERT(certs.error.empty(), "self-signed cert generated");

    apollo::net::tls::TlsServerContext tls_context;
    const auto loaded = tls_context.load(certs.cert_path, certs.key_path);
    TEST_ASSERT(loaded.ok, "tls context loaded");

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer, &tls_context);
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    tls_test_support::TlsClient client;
    std::string error;
    TEST_ASSERT(client.start("127.0.0.1", ingress->listenPort(), error), "tls client started");

    // TLS 客户端读尽非阻塞缓冲（泵循环里每拍调用）。
    std::vector<std::uint8_t> stream;
    auto drain_tls = [&](std::vector<std::uint8_t>& sink) {
        std::uint8_t chunk[4096];
        for (;;) {
            const int n = client.read(chunk, sizeof chunk);
            if (n <= 0) {
                break;
            }
            sink.insert(sink.end(), chunk, chunk + n);
        }
    };

    // 双侧步进至 TLS 握手完成（服务端 SSL_accept 在入口泵内隐式步进）。
    const auto connected = pump_until(*ingress, [&] {
        return client.step() == tls_test_support::TlsClient::Step::kConnected;
    }, 5000);
    TEST_ASSERT(connected, "tls handshake completed");

    // 升级请求（TLS 短写由泵循环消化）。
    const auto request = upgrade_request("GET", "/game");
    {
        std::size_t off = 0;
        const auto sent = pump_until(*ingress, [&] {
            while (off < request.size()) {
                const int n = client.write(request.data() + off, request.size() - off);
                if (n <= 0) {
                    break;
                }
                off += static_cast<std::size_t>(n);
            }
            return off == request.size();
        }, 5000);
        TEST_ASSERT(sent, "upgrade request sent over tls");
    }

    const auto upgraded = pump_until(*ingress, [&] {
        drain_tls(stream);
        return observer.count(EventKind::Opened) == 1;
    }, 5000);
    TEST_ASSERT(upgraded, "opened over wss");
    const std::string head(reinterpret_cast<const char*>(stream.data()),
                           std::min<std::size_t>(stream.size(), 400));
    TEST_ASSERT(head.find("101") != std::string::npos, "101 seen in tls stream");

    // 业务帧：netlink 帧（PING 包）入 masked binary。
    apollo::protocol::Ping ping;
    ping.timestamp = 777;
    const auto packet = apollo::protocol::MessageCodec::encode(ping, 0);
    const auto link_frame = frame_bytes(1, 0, packet);
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true, link_frame.data(),
                        link_frame.size(), 0x9a);
    {
        std::size_t off = 0;
        const auto sent = pump_until(*ingress, [&] {
            while (off < ws_frame.size()) {
                const int n = client.write(ws_frame.data() + off, ws_frame.size() - off);
                if (n <= 0) {
                    break;
                }
                off += static_cast<std::size_t>(n);
            }
            return off == ws_frame.size();
        }, 5000);
        TEST_ASSERT(sent, "ws frame sent over tls");
    }

    const auto packet_seen = pump_until(*ingress, [&] {
        drain_tls(stream);
        return observer.count(EventKind::Packet) == 1;
    }, 5000);
    TEST_ASSERT(packet_seen, "ping packet observed");
    auto events = observer.snapshot();
    TEST_ASSERT(events[1].payload == packet, "netlink payload intact over wss");

    // 出站回包：send → tls 流里追加 ws binary 帧。
    const std::vector<std::uint8_t> reply{0x01, 0x02};
    ingress->send(events[0].connectionId, reply);
    const auto replied = pump_until(*ingress, [&] {
        drain_tls(stream);
        return stream.size() >= head.size() + 2 + 6 + 2 + 4;  // 101 头 + ws 帧 + 链路帧
    }, 5000);
    TEST_ASSERT(replied, "reply received over tls");

    // 出站流 = 101 应答 + ws binary 帧：剥头后解帧。
    const auto header_end = head.find("\r\n\r\n");
    TEST_ASSERT(header_end != std::string::npos, "101 header terminator");
    std::vector<std::uint8_t> ws_bytes(stream.begin() + static_cast<long>(header_end) + 4,
                                       stream.end());
    TEST_ASSERT(ws_bytes.size() >= 4, "ws frame bytes present");
    TEST_ASSERT(ws_bytes[0] == 0x82, "binary frame opcode");
    const std::size_t ws_len = ws_bytes[1] & 0x7F;
    TEST_ASSERT(ws_bytes.size() >= 2 + ws_len, "ws frame complete");
    netlink::FrameParser net_parser;
    const auto net_outcome = net_parser.feed(ws_bytes.data() + 2, ws_len);
    TEST_ASSERT(!net_outcome.fatal && net_outcome.frames.size() == 1, "link frame intact");
    TEST_ASSERT(net_outcome.frames[0].payload == reply, "payload intact over wss");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 8. 扇入号段：TCP + WS 并装 ----

bool test_fan_in_id_spaces() {
    std::cout << "Running: test_fan_in_id_spaces..." << std::endl;

    RecordingObserver observer;
    const std::uint64_t ws_base = 1 * gateway::kIngressFanInIdStride;

    // 号段重叠装配即抛。
    {
        std::vector<gateway::IngressFanInBinding> bad;
        bad.push_back(gateway::IngressFanInBinding{
            gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0), 0});
        bad.push_back(gateway::IngressFanInBinding{
            gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0), 0});
        bool threw = false;
        try {
            (void)gateway::makeIngressFanIn(std::move(bad));
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        TEST_ASSERT(threw, "overlapping id ranges rejected");
    }

    std::vector<gateway::IngressFanInBinding> bindings;
    bindings.push_back(gateway::IngressFanInBinding{
        gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0), 0});
    gateway::WsIngressConfig ws_config;
    ws_config.host = "127.0.0.1";
    ws_config.port = 0;
    ws_config.path = "/game";
    ws_config.connectionIdBase = ws_base;
    bindings.push_back(gateway::IngressFanInBinding{
        gateway::makeWsClientIngressServer(ws_config), ws_base});
    auto fan_in = gateway::makeIngressFanIn(std::move(bindings));
    fan_in->setObserver(&observer);
    fan_in->start();
    const auto ports = fan_in->listenPorts();
    TEST_ASSERT(ports.size() == 2 && ports[0] != 0 && ports[1] != 0, "two listeners up");

    WsTestClient ws_client;
    TEST_ASSERT(ws_client.connect("127.0.0.1", ports[1]), "ws client connected");
    ws_client.sendAll(upgrade_request("GET", "/game"));

    // TCP 客户端（裸 link 帧）。
    int tcp_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT(tcp_fd >= 0, "tcp socket");
    timeval tv{};
    tv.tv_sec = 10;
    ::setsockopt(tcp_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    ::sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ports[0]);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    TEST_ASSERT(::connect(tcp_fd, reinterpret_cast<::sockaddr*>(&addr), sizeof addr) == 0,
                "tcp connected");
    const std::vector<std::uint8_t> tcp_payload{0x11};
    {
        auto out = frame_bytes(1, 0, tcp_payload);
        std::size_t off = 0;
        while (off < out.size()) {
            const ssize_t n = ::send(tcp_fd, out.data() + off, out.size() - off, 0);
            if (n <= 0) break;
            off += static_cast<std::size_t>(n);
        }
    }

    TEST_ASSERT(pump_until(*fan_in, [&] {
        return observer.count(EventKind::Opened) == 2 &&
               observer.count(EventKind::Packet) == 1;
    }, 2000), "both connections admitted");
    {
        std::string response;
        TEST_ASSERT(ws_client.recvHttpResponse(response) && response.find("101") != std::string::npos,
                    "ws upgraded");
    }

    const auto events = observer.snapshot();
    ConnectionID tcp_id = 0;
    ConnectionID ws_id = 0;
    for (const auto& event : events) {
        if (event.kind == EventKind::Opened) {
            if (event.connectionId > ws_base) {
                ws_id = event.connectionId;
            } else {
                tcp_id = event.connectionId;
            }
        }
    }
    TEST_ASSERT(tcp_id == 1, "tcp id in base segment");
    TEST_ASSERT(ws_id == ws_base + 1, "ws id in ws segment");
    bool tcp_packet_seen = false;
    for (const auto& event : events) {
        if (event.kind == EventKind::Packet) {
            tcp_packet_seen = event.payload == tcp_payload && event.connectionId == tcp_id;
        }
    }
    TEST_ASSERT(tcp_packet_seen, "tcp packet via fan-in");

    // 出站路由：各回各的监听。
    fan_in->send(tcp_id, tcp_payload);
    fan_in->send(ws_id, tcp_payload);
    pump_rounds(*fan_in, 100);
    {
        std::uint8_t chunk[256];
        const ssize_t n = ::recv(tcp_fd, chunk, sizeof chunk, 0);
        TEST_ASSERT(n > 0, "tcp reply arrived");
        netlink::FrameParser tcp_net;
        const auto outcome = tcp_net.feed(chunk, static_cast<std::size_t>(n));
        TEST_ASSERT(!outcome.fatal && outcome.frames.size() == 1 &&
                    outcome.frames[0].payload == tcp_payload,
                    "tcp reply routed to tcp listener");
    }
    {
        std::uint8_t opcode = 0;
        std::vector<std::uint8_t> payload;
        TEST_ASSERT(ws_client.recvWsFrame(opcode, payload), "ws reply arrived");
        TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kBinary),
                    "ws reply is binary frame");
        netlink::FrameParser ws_net;
        const auto outcome = ws_net.feed(payload);
        TEST_ASSERT(!outcome.fatal && outcome.frames.size() == 1 &&
                    outcome.frames[0].payload == tcp_payload,
                    "ws reply routed to ws listener");
    }

    // 越界号容忍（迟到操作面：不崩不炸）。
    fan_in->send(ws_base + 500, {});
    fan_in->close(99999999999ULL, DisconnectReason::NormalClose);

    ::close(tcp_fd);
    ws_client.close();
    fan_in->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 9. WSS 缺凭据 → 装配即抛 ----

bool test_wss_without_certs_throws() {
    std::cout << "Running: test_wss_without_certs_throws..." << std::endl;

    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.wssPort = 8443;  // 未绑定（ctor 不监听），只考装配校验
    bool threw = false;
    try {
        gateway::GatewayServer server(config);
        (void)server;
    } catch (const std::runtime_error& e) {
        threw = std::string(e.what()).find("tls") != std::string::npos;
    }
    TEST_ASSERT(threw, "wss without certs rejected at assembly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 10. GatewayServer 全链 e2e：WS 上 PING → PONG ----

// ---- 11. 有界积压：慢读踢线（升级后业务连接出站残余超上限 → SlowConsumer）----
bool test_ws_slow_consumer_kicked() {
    std::cout << "Running: test_ws_slow_consumer_kicked..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_game_ws_ingress(observer);
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/game"));
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened after upgrade");
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "101 response received");

    const auto connectionId = observer.snapshot().front().connectionId;
    // 客户端不读：8 MiB 业务出站（32×256 KiB）远超内核双侧缓冲与
    // kMaxTxBacklogBytes=2 MiB 上限——flush 余量滞留 tx，超限即踢（TCP 入口
    // 同款纪律；closing 回执的 kStalledTxPumpLimit 兜底不受影响）。
    const std::vector<std::uint8_t> big(256 * 1024, 0x7C);
    for (int i = 0; i < 32; ++i) {
        ingress->send(connectionId, big);
    }
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           8000),
                "slow consumer kicked closed");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == DisconnectReason::SlowConsumer,
                "close reason is slow consumer");
    // 踢线后残余不滞留连接表：closing 期残余走既有停泵兜底（500 拍强拆），
    // 连接最终真断——本批的有界积压与既有滞留兜底串成完整闭环。泵与客户端
    // 大块排空交替（残留 64B 读法读不尽，永碰不到 EOF）。
    bool torn_down = false;
    const auto teardown_deadline = std::chrono::steady_clock::now()
                                   + std::chrono::milliseconds(5000);
    while (std::chrono::steady_clock::now() < teardown_deadline) {
        ingress->pump();
        pollfd pfd{client.fd(), POLLIN, 0};
        if (::poll(&pfd, 1, 1) > 0) {
            std::uint8_t scratch[65536];
            const ssize_t n = ::recv(client.fd(), scratch, sizeof scratch, 0);
            if (n <= 0 && !(n < 0 && errno == EINTR)) {
                torn_down = true;  // EOF 或读错误（RST）都算收到断开
                break;
            }
        }
    }
    TEST_ASSERT(torn_down, "closing backlog force-dropped and connection torn down");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_server_ws_e2e() {
    std::cout << "Running: test_gateway_server_ws_e2e..." << std::endl;

    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.wsPort = probe_free_port();  // GatewayConfig 层 0 = 监听关闭
    config.sessionTimeoutMs = 60000;
    config.heartbeatCheckIntervalMs = 1000;
    TEST_ASSERT(config.wsPort != 0, "free port probed");

    gateway::GatewayServer server(config);
    server.start();
    TEST_ASSERT(server.listenPort() != 0, "tcp ingress listening");
    TEST_ASSERT(server.wsListenPort() != 0, "ws ingress listening");
    TEST_ASSERT(server.wsListenPort() != server.listenPort(), "distinct listeners");

    WsTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", server.wsListenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/game"));
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response) && response.find("101") != std::string::npos,
                "upgraded");

    apollo::protocol::Ping ping;
    ping.timestamp = 12345;
    const auto packet = apollo::protocol::MessageCodec::encode(ping, 0);
    const auto link_frame = frame_bytes(1, 0, packet);
    std::vector<std::uint8_t> ws_frame;
    append_masked_frame(ws_frame, ws::OpCode::kBinary, true, link_frame.data(),
                        link_frame.size(), 0x21);
    client.sendAll(ws_frame);

    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(client.recvWsFrame(opcode, payload), "ws reply received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kBinary), "binary frame");
    netlink::FrameParser net_parser;
    const auto outcome = net_parser.feed(payload);
    TEST_ASSERT(!outcome.fatal && outcome.frames.size() == 1, "link frame intact");
    const auto& body = outcome.frames[0].payload;
    TEST_ASSERT(body.size() > sizeof(apollo::protocol::MessageHeader), "pong has header");
    apollo::protocol::MessageHeader header{};
    std::memcpy(&header, body.data(), sizeof header);
    TEST_ASSERT(header.type ==
                static_cast<std::uint16_t>(apollo::protocol::MessageType::PONG),
                "reply is PONG");
    const std::vector<std::uint8_t> pong_body(
        body.begin() + static_cast<long>(sizeof header), body.end());
    const auto pong = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::Pong>(pong_body);
    TEST_ASSERT(pong.timestamp == 12345, "timestamp echoed");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "one session registered");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#endif // !_WIN32

}  // namespace

int main() {
#if !defined(_WIN32)
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"ws_upgrade_roundtrip", test_ws_upgrade_roundtrip},
        {"ws_wrong_path_rejected", test_ws_wrong_path_rejected},
        {"ws_invalid_upgrade_rejected", test_ws_invalid_upgrade_rejected},
        {"ws_garbage_rejected", test_ws_garbage_rejected},
        {"ws_ping_close_control", test_ws_ping_close_control},
        {"ws_text_frame_rejected", test_ws_text_frame_rejected},
        {"wss_roundtrip", test_wss_roundtrip},
        {"fan_in_id_spaces", test_fan_in_id_spaces},
        {"wss_without_certs_throws", test_wss_without_certs_throws},
        {"gateway_server_ws_e2e", test_gateway_server_ws_e2e},
        {"ws_slow_consumer_kicked", test_ws_slow_consumer_kicked},
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
