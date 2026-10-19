// gateway 入口真接线测试（link 帧定界 + TCP 底座，localhost 实 socket）。
//
// 覆盖：
//   1. 帧 round-trip：入站帧载荷原样达 observer；send 经出站帧原样达客户端；
//   2. 对端 EOF → NormalClose 断开事件；
//   3. 载荷尾校验破坏 → fatal → TransportError 断连；
//   4. control 帧（InterServerLink 专用）→ 契约外消息断连；
//   5. close 经投递队列收线（泵外线程调用面）；
//   6. 未知连接 send 容忍（不崩不炸）；
//   7. 满载拒收（超出 maxConnections 的入站在 accept 即关）；
//   8. 慢读踢线（有界积压：出站残余超上限 → SlowConsumer 断开）；
//   9. 投递队列有界（顶格丢最旧，帧号 drain 侧分配无序列缝）；
//   10. GatewayServer 全链 e2e：真 TCP PING → 分发 → PONG 回包。
//
// Windows：link TCP 底座不进 M1 批（POSIX only）——TCP 用例本体空转，
// 仅保留工厂烟囱（null 桩回退可启停）。

#include "gateway/gateway_server.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/ingress/tcp_client_ingress_server.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
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

enum class EventKind { Opened, Packet, Closed };

struct RecordedEvent {
    EventKind kind{EventKind::Closed};
    gateway::ConnectionID connectionId{0};
    gateway::ClientEndpoint endpoint;
    gateway::DisconnectReason reason{gateway::DisconnectReason::TransportError};
    std::vector<std::uint8_t> payload;
};

class RecordingObserver final : public gateway::IClientIngressObserver {
public:
    void onConnectionOpened(gateway::ConnectionID connectionId,
                            const gateway::ClientEndpoint& endpoint) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Opened, connectionId, endpoint,
                           gateway::DisconnectReason::NormalClose, {}});
    }

    void onPacketReceived(gateway::ConnectionID connectionId,
                          std::span<const std::uint8_t> payload) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Packet, connectionId, {},
                           gateway::DisconnectReason::NormalClose,
                           {payload.begin(), payload.end()}});
    }

    void onConnectionClosed(gateway::ConnectionID connectionId,
                            gateway::DisconnectReason reason) override {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back({EventKind::Closed, connectionId, {}, reason, {}});
    }

    void onConnectionIdle(gateway::ConnectionID) override {}

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

// 阻塞式测试客户端（独立实现，不复用被测代码的收发路径）。
class TestClient {
public:
    ~TestClient() { close(); }

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

    // 收一帧（内部用 link parser 拆流——帧格式属被测契约面，拆帧工具复用）
    bool recvFrame(std::vector<std::uint8_t>& payload) {
        apollo::net::link::FrameParser parser;
        std::uint8_t buf[4096];
        for (;;) {
            const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
            if (n <= 0) {
                return false;
            }
            auto outcome = parser.feed(buf, static_cast<std::size_t>(n));
            if (outcome.fatal || outcome.frames.empty()) {
                return false;
            }
            payload = std::move(outcome.frames.front().payload);
            return true;
        }
    }

    // 等连接被对端关闭（EOF/错误），有数据到达也返回 false 并丢弃
    bool waitEof(int timeoutMs) {
        pollfd pfd{fd_, POLLIN, 0};
        if (::poll(&pfd, 1, timeoutMs) <= 0) {
            return false;
        }
        std::uint8_t buf[64];
        const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
        return n <= 0;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // 底层描述符（多帧连续收取面用——recvFrame 单帧语义会丢中间缓冲）。
    int fd() const { return fd_; }

    // 排空至 EOF（慢读踢线用例：踢线前已灌入大站出站残留，单帧读法读不到
    // EOF——必须连读到对端 FIN/RST）。
    bool drainToEof(int timeoutMs) {
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            pollfd pfd{fd_, POLLIN, 0};
            if (::poll(&pfd, 1, 50) <= 0) {
                continue;
            }
            std::uint8_t buf[65536];
            const ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
            if (n > 0) {
                continue;
            }
            if (n < 0 && errno == EINTR) {
                continue;
            }
            return true;  // EOF 或读错误（RST）都算收到断开
        }
        return false;
    }

private:
    int fd_ = -1;
};

template <typename Pred>
bool pump_until(gateway::ClientIngressServer& ingress, Pred pred, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now()
                          + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        ingress.pump();
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ingress.pump();
    return pred();
}

std::vector<std::uint8_t> frame_bytes(std::uint32_t seq, std::uint8_t flags,
                                      const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    apollo::net::link::append_frame(out, seq, flags, payload);
    return out;
}

// ---- 1. 帧 round-trip：入站载荷原样达 observer；出站帧原样达客户端 ----
bool test_ingress_frame_roundtrip() {
    std::cout << "Running: test_ingress_frame_roundtrip..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    TEST_ASSERT(ingress != nullptr, "ingress factory non-null");
    ingress->setObserver(&observer);
    ingress->start();
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");

    const std::vector<std::uint8_t> payload{0x01, 0x02, 0x03, 0x04};
    client.sendAll(frame_bytes(1, 0, payload));

    const auto inbound = payload;
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Packet) == 1; },
                           2000),
                "packet reached observer");
    auto events = observer.snapshot();
    TEST_ASSERT(events.size() >= 2, "opened + packet events");
    TEST_ASSERT(events[0].kind == EventKind::Opened, "opened first");
    TEST_ASSERT(events[0].endpoint.address == "127.0.0.1", "peer address captured");
    TEST_ASSERT(events[0].endpoint.port != 0, "peer port captured");
    TEST_ASSERT(events[1].kind == EventKind::Packet, "packet second");
    TEST_ASSERT(events[1].payload == inbound, "payload intact");
    const auto connectionId = events[0].connectionId;

    const std::vector<std::uint8_t> outbound{0xAA, 0xBB, 0xCC};
    ingress->send(connectionId, outbound);
    TEST_ASSERT(pump_until(*ingress, [&] { return true; }, 0), "pumped once");
    std::vector<std::uint8_t> received;
    TEST_ASSERT(client.recvFrame(received), "client received a frame");
    TEST_ASSERT(received == outbound, "outbound payload intact");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_eof_disconnect() {
    std::cout << "Running: test_ingress_eof_disconnect..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");

    client.close();
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "closed after EOF");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == gateway::DisconnectReason::NormalClose,
                "EOF is normal close");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_corrupt_payload_fatal() {
    std::cout << "Running: test_ingress_corrupt_payload_fatal..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");

    auto bytes = frame_bytes(1, 0, {0x11, 0x22, 0x33});
    bytes[bytes.size() - 5] ^= 0xFF;  // 破坏载荷尾字节 → pcrc 不过 → fatal
    client.sendAll(bytes);

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "fatal closed");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == gateway::DisconnectReason::TransportError,
                "fatal is transport error");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_control_frame_rejected() {
    std::cout << "Running: test_ingress_control_frame_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");

    // control 帧是 InterServerLink 专用——客户端发出即契约外消息
    client.sendAll(frame_bytes(1, apollo::net::link::kFlagControl, {0x01}));

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "closed");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == gateway::DisconnectReason::TransportError,
                "contract-violating frame is transport error");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_close_from_foreign_thread() {
    std::cout << "Running: test_ingress_close_from_foreign_thread..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");
    const auto connectionId = observer.snapshot().front().connectionId;

    std::thread closer([&] { ingress->close(connectionId,
                                           gateway::DisconnectReason::AdmissionRejected); });
    closer.join();

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           2000),
                "closed via handoff");
    TEST_ASSERT(observer.snapshot().back().reason
                    == gateway::DisconnectReason::AdmissionRejected,
                "reason preserved through handoff");
    TEST_ASSERT(client.waitEof(2000), "client socket closed by server");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_send_unknown_connection_dropped() {
    std::cout << "Running: test_ingress_send_unknown_connection_dropped..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    const std::vector<std::uint8_t> stray{0x01, 0x02};
    ingress->send(9999, stray);
    ingress->close(9999, gateway::DisconnectReason::NormalClose);
    ingress->pump();
    TEST_ASSERT(observer.snapshot().empty(), "no events for unknown connection");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ingress_capacity_cap() {
    std::cout << "Running: test_ingress_capacity_cap..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 1);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient first;
    TestClient second;
    TEST_ASSERT(first.connect("127.0.0.1", ingress->listenPort()), "first connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "first admitted");

    TEST_ASSERT(second.connect("127.0.0.1", ingress->listenPort()), "second connected");
    pump_until(*ingress, [] { return false; }, 200);  // 泵几轮：cap 拒收在 accept 面
    TEST_ASSERT(observer.count(EventKind::Opened) == 1, "second not admitted");
    TEST_ASSERT(second.waitEof(2000), "second socket closed at accept");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 9. 有界积压：慢读踢线（出站残余超上限 → SlowConsumer 断开）----
bool test_ingress_slow_consumer_kicked() {
    std::cout << "Running: test_ingress_slow_consumer_kicked..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");
    const auto connectionId = observer.snapshot().front().connectionId;

    // 客户端不读：8 MiB 出站灌压（32×256 KiB）远超内核双侧缓冲与
    // kMaxTxBacklogBytes=2 MiB 上限——flush 余量滞留 tx，超限即踢
    //（SlowConsumer；会话保留窗承接重连续传属上层语义，此处验收踢线事实）。
    const std::vector<std::uint8_t> big(256 * 1024, 0x7C);
    for (int i = 0; i < 32; ++i) {
        ingress->send(connectionId, big);
    }
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Closed) == 1; },
                           8000),
                "slow consumer kicked closed");
    const auto events = observer.snapshot();
    TEST_ASSERT(events.back().reason == gateway::DisconnectReason::SlowConsumer,
                "close reason is slow consumer");
    TEST_ASSERT(client.drainToEof(2000), "client sees EOF after kick");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 10. 有界积压：投递队列顶格丢最旧（帧号 drain 侧分配，无序列缝）----
bool test_ingress_handoff_cap_drops_oldest() {
    std::cout << "Running: test_ingress_handoff_cap_drops_oldest..." << std::endl;

    RecordingObserver observer;
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    ingress->setObserver(&observer);
    ingress->start();

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "connected");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::Opened) == 1; },
                           2000),
                "opened");
    const auto connectionId = observer.snapshot().front().connectionId;

    // 不泵连投 kMaxHandoffOps+50 个带序 payload（本地序 4 字节）：最旧 50 个
    // 被弃，保号连续——收到帧数恰为 cap，首帧序 = 50、尾帧序 = cap+49。
    constexpr std::size_t kSent = gateway::kMaxHandoffOps + 50;
    for (std::uint32_t i = 0; i < kSent; ++i) {
        std::vector<std::uint8_t> payload(4);
        std::memcpy(payload.data(), &i, 4);
        ingress->send(connectionId, payload);
    }

    TEST_ASSERT(pump_until(*ingress, [&] { return true; }, 0), "pumped once");

    // 连收 kMaxHandoffOps 帧（独立 parser 持续喂流，不丢中间缓冲）。
    apollo::net::link::FrameParser parser;
    std::vector<std::uint32_t> indices;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (indices.size() < gateway::kMaxHandoffOps &&
           std::chrono::steady_clock::now() < deadline) {
        pollfd pfd{client.fd(), POLLIN, 0};
        if (::poll(&pfd, 1, 50) <= 0) {
            continue;
        }
        std::uint8_t buf[8192];
        const ssize_t n = ::recv(client.fd(), buf, sizeof buf, 0);
        if (n <= 0) {
            break;
        }
        const auto outcome = parser.feed(buf, static_cast<std::size_t>(n));
        TEST_ASSERT(!outcome.fatal, "frame stream intact after oldest-drop");
        for (const auto& frame : outcome.frames) {
            std::uint32_t v = 0;
            if (frame.payload.size() >= 4) {
                std::memcpy(&v, frame.payload.data(), 4);
            }
            indices.push_back(v);
        }
    }
    TEST_ASSERT(indices.size() == gateway::kMaxHandoffOps, "exactly cap ops delivered");
    TEST_ASSERT(indices.front() == 50, "oldest 50 ops dropped first");
    TEST_ASSERT(indices.back() == static_cast<std::uint32_t>(kSent) - 1, "newest op kept");

    ingress->stop();
    client.close();
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_server_ping_pong_e2e() {
    std::cout << "Running: test_gateway_server_ping_pong_e2e..." << std::endl;

    gateway::GatewayConfig config;
    config.host = "127.0.0.1";
    config.port = 0;  // 内核分配，listenPort() 读回
    config.sessionTimeoutMs = 60000;
    config.heartbeatCheckIntervalMs = 1000;

    gateway::GatewayServer server(config);
    server.start();
    TEST_ASSERT(server.listenPort() != 0, "gateway listening");

    TestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", server.listenPort()), "client connected");

    apollo::protocol::Ping ping;
    ping.timestamp = 12345;
    const auto packet = apollo::protocol::MessageCodec::encode(ping, 0);
    client.sendAll(frame_bytes(1, 0, packet));

    std::vector<std::uint8_t> received;
    TEST_ASSERT(client.recvFrame(received), "pong received");
    TEST_ASSERT(received.size() > sizeof(apollo::protocol::MessageHeader),
                "pong has header + body");
    apollo::protocol::MessageHeader header{};
    std::memcpy(&header, received.data(), sizeof header);
    TEST_ASSERT(header.type == static_cast<std::uint16_t>(apollo::protocol::MessageType::PONG),
                "reply is PONG");
    const std::vector<std::uint8_t> body(received.begin()
                                             + static_cast<long>(sizeof header),
                                         received.end());
    const auto pong = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::Pong>(body);
    TEST_ASSERT(pong.timestamp == 12345, "timestamp echoed");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "one connection registered");

    server.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#endif // !_WIN32

bool test_ingress_factory_smoke() {
    std::cout << "Running: test_ingress_factory_smoke..." << std::endl;

    // 双平台工厂可用：POSIX 返回真入口，Windows 退回 null 桩——启停均无异常。
    auto ingress = gateway::makeTcpClientIngressServer("127.0.0.1", 0, 0);
    TEST_ASSERT(ingress != nullptr, "factory returns an implementation");
    ingress->start();
    ingress->pump();
    (void)ingress->listenPort();
    ingress->stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    int failed = 0;
    if (!test_ingress_factory_smoke()) ++failed;
#if !defined(_WIN32)
    if (!test_ingress_frame_roundtrip()) ++failed;
    if (!test_ingress_eof_disconnect()) ++failed;
    if (!test_ingress_corrupt_payload_fatal()) ++failed;
    if (!test_ingress_control_frame_rejected()) ++failed;
    if (!test_ingress_close_from_foreign_thread()) ++failed;
    if (!test_ingress_send_unknown_connection_dropped()) ++failed;
    if (!test_ingress_capacity_cap()) ++failed;
    if (!test_ingress_slow_consumer_kicked()) ++failed;
    if (!test_ingress_handoff_cap_drops_oldest()) ++failed;
    if (!test_gateway_server_ping_pong_e2e()) ++failed;
#endif
    if (failed == 0) {
        std::cout << "ALL INGRESS TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " ingress test(s) failed" << std::endl;
    return 1;
}
