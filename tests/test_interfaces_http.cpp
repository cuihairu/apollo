// interfaces HTTP/HTTPS 入口真接线测试（L45 批④；localhost 实 socket）。
//
// 覆盖：
//   1. GET /health → 200（Content-Length + Connection: close，应答尽即拆）；
//   2. POST /echo → 体原样回显；
//   3. handler 抛异常 → 500（入口不随 handler 崩）；
//   4. ws 升级 round-trip：GET /ws → 101（含算出的 Accept）→ masked
//      binary echo、ping→pong、close→echo + WsClosed 事件；
//   5. 升级路由上的普通 GET → 400（不降级）+ 断连 + 零 ws 事件；
//   6. HTTP 路由上的合法升级 → 400（http 路由不受理升级）；
//   7. 未注册路径 → 404；
//   8. 垃圾字节流（无头部终止，超 8KiB 上限）→ 431；
//   9. 体超 1 MiB 预算 → 413（头齐即拒，不等体到齐）；
//  10. HTTPS round-trip：TLS 底座上同一套请求语义（自签夹具）；
//  11. 并行监听 = 独立实例 + 互斥号段（基址 0 / 1<<40）；
//  12. 端口冲突 → start() 抛（绑定失败显形，不静默）。
//
// Windows：link TCP/TLS 底座不进 M1 批（POSIX only）——用例本体空转。

#include "interfaces/http_ingress.hpp"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"
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

namespace ws = apollo::net::ws;
using interfaces::ClientEndpoint;
using interfaces::ConnectionID;
using interfaces::DisconnectReason;
using interfaces::HttpIngressConfig;
using interfaces::HttpIngressServer;
using interfaces::HttpRequest;
using interfaces::HttpResponse;

enum class EventKind { HttpHandled, WsOpened, WsBinary, WsClosed };

struct RecordedEvent {
    EventKind kind{EventKind::WsClosed};
    ConnectionID connectionId{0};
    ClientEndpoint endpoint;
    DisconnectReason reason{DisconnectReason::TransportError};
    int status{0};
    std::string method;
    std::string path;
    std::vector<std::uint8_t> payload;
};

class RecordingObserver final : public interfaces::IHttpIngressObserver {
public:
    void onHttpHandled(ConnectionID connectionId, const HttpRequest& request,
                       int status) override {
        std::lock_guard<std::mutex> lock(mutex_);
        RecordedEvent event;
        event.kind = EventKind::HttpHandled;
        event.connectionId = connectionId;
        event.status = status;
        event.method = request.method;
        event.path = request.path;
        events_.push_back(std::move(event));
    }

    void onWsOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) override {
        std::lock_guard<std::mutex> lock(mutex_);
        RecordedEvent event;
        event.kind = EventKind::WsOpened;
        event.connectionId = connectionId;
        event.endpoint = endpoint;
        events_.push_back(std::move(event));
    }

    void onWsBinary(ConnectionID connectionId,
                    std::span<const std::uint8_t> payload) override {
        std::lock_guard<std::mutex> lock(mutex_);
        RecordedEvent event;
        event.kind = EventKind::WsBinary;
        event.connectionId = connectionId;
        event.payload.assign(payload.begin(), payload.end());
        events_.push_back(std::move(event));
    }

    void onWsClosed(ConnectionID connectionId, DisconnectReason reason) override {
        std::lock_guard<std::mutex> lock(mutex_);
        RecordedEvent event;
        event.kind = EventKind::WsClosed;
        event.connectionId = connectionId;
        event.reason = reason;
        events_.push_back(std::move(event));
    }

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
bool pump_until(HttpIngressServer& ingress, Pred pred, int timeoutMs) {
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

void pump_rounds(HttpIngressServer& ingress, int durationMs) {
    pump_until(ingress, [] { return false; }, durationMs);
}

// 演示路由装配（与 apps/interfaces main 同面；专用升级路由 /ws）。
void install_demo_routes(HttpIngressServer& server) {
    server.setHttpRoute("/health", [](const HttpRequest&) {
        HttpResponse response;
        const std::string body = "{\"status\":\"ok\"}\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
    server.setHttpRoute("/echo", [](const HttpRequest& request) {
        HttpResponse response;
        response.body = request.body;
        return response;
    });
    server.setHttpRoute("/boom", [](const HttpRequest&) -> HttpResponse {
        throw std::runtime_error("handler exploded");
    });
    server.setWsRoute("/ws");
}

std::unique_ptr<HttpIngressServer> make_ingress(
    RecordingObserver& observer, apollo::net::tls::TlsServerContext* tls = nullptr,
    std::uint64_t connection_id_base = 0) {
    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;  // 内核分配；listenPort() 读实际端口
    config.tls = tls;
    config.connectionIdBase = connection_id_base;
    auto ingress = interfaces::makeHttpIngressServer(config);
    install_demo_routes(*ingress);
    ingress->setObserver(&observer);
    ingress->start();
    return ingress;
}

// ---- HTTP/ws 测试客户端（阻塞 socket；独立实现，不复用被测收发路径）----

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

std::vector<std::uint8_t> plain_request(const std::string& method, const std::string& path,
                                        const std::string& body = "") {
    std::string req = std::string(method) + " " + path + " HTTP/1.1\r\n"
                      "Host: localhost\r\n";
    if (!body.empty()) {
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    req += "\r\n" + body;
    return {req.begin(), req.end()};
}

// 客户端 → 服务端帧（必须掩码）。
void append_masked_frame(std::vector<std::uint8_t>& out, ws::OpCode opcode,
                         const std::vector<std::uint8_t>& payload, std::uint8_t mask_key) {
    out.push_back(0x80 | static_cast<std::uint8_t>(opcode));
    const std::size_t size = payload.size();
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

class HttpTestClient {
public:
    ~HttpTestClient() { close(); }

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

// ---- 1. GET /health → 200 ----

bool test_http_get_health() {
    std::cout << "Running: test_http_get_health..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(plain_request("GET", "/health"));

    std::string response;
    TEST_ASSERT(pump_until(*ingress, [&] { return client.recvHttpResponse(response); },
                           2000),
                "response received");
    TEST_ASSERT(response.find("200") != std::string::npos, "200 status");
    TEST_ASSERT(response.find("Connection: close") != std::string::npos,
                "close declared");
    TEST_ASSERT(response.rfind("{\"status\":\"ok\"}\n") == response.size() - 16,
                "health body");
    TEST_ASSERT(client.waitEof(2000), "connection closed after response");

    TEST_ASSERT(pump_until(*ingress,
                           [&] { return observer.count(EventKind::HttpHandled) == 1; },
                           2000),
                "handled event");
    const auto events = observer.snapshot();
    TEST_ASSERT(events[0].status == 200, "status recorded");
    TEST_ASSERT(events[0].path == "/health", "path recorded");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 2. POST /echo → 体回显 ----

bool test_http_post_echo() {
    std::cout << "Running: test_http_post_echo..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    const std::string body = "echo-payload-123";
    client.sendAll(plain_request("POST", "/echo", body));

    std::string response;
    TEST_ASSERT(pump_until(*ingress, [&] { return client.recvHttpResponse(response); },
                           2000),
                "response received");
    TEST_ASSERT(response.find("200") != std::string::npos, "200 status");
    TEST_ASSERT(response.rfind(body) == response.size() - body.size(), "body echoed");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 3. handler 抛异常 → 500 ----

bool test_handler_throw_500() {
    std::cout << "Running: test_handler_throw_500..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(plain_request("GET", "/boom"));

    std::string response;
    TEST_ASSERT(pump_until(*ingress, [&] { return client.recvHttpResponse(response); },
                           2000),
                "response received");
    TEST_ASSERT(response.find("500") != std::string::npos, "500 status");
    TEST_ASSERT(response.find("handler error") != std::string::npos, "error body");

    TEST_ASSERT(pump_until(*ingress,
                           [&] { return observer.count(EventKind::HttpHandled) == 1; },
                           2000),
                "handled event");
    TEST_ASSERT(observer.snapshot()[0].status == 500, "500 recorded");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 4. ws 升级 round-trip（echo 生命周期 + 控制帧回执）----

bool test_ws_upgrade_on_ws_route() {
    std::cout << "Running: test_ws_upgrade_on_ws_route..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/ws"));

    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::WsOpened) == 1; },
                           2000),
                "opened after upgrade");
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "101 response received");
    TEST_ASSERT(response.find("101") != std::string::npos, "switching protocols");
    TEST_ASSERT(response.find("Sec-WebSocket-Accept: "
                              + ws::compute_accept_key(kClientKey)) != std::string::npos,
                "accept key computed");
    TEST_ASSERT(observer.snapshot()[0].connectionId == 1, "id from base 0");

    // binary echo：专用路由生命周期不进 HTTP handler。
    const std::vector<std::uint8_t> payload{'h', 'e', 'l', 'l', 'o'};
    std::vector<std::uint8_t> frame;
    append_masked_frame(frame, ws::OpCode::kBinary, payload, 0x3c);
    client.sendAll(frame);
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::WsBinary) == 1; },
                           2000),
                "binary reached observer");
    std::uint8_t opcode = 0;
    std::vector<std::uint8_t> echoed;
    TEST_ASSERT(client.recvWsFrame(opcode, echoed), "echo frame received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kBinary),
                "echo is binary");
    TEST_ASSERT(echoed == payload, "echo payload intact");

    // ping → pong 回执（原载荷）。
    const std::vector<std::uint8_t> ping_data{'a', 'b'};
    std::vector<std::uint8_t> ping_frame;
    append_masked_frame(ping_frame, ws::OpCode::kPing, ping_data, 0x11);
    client.sendAll(ping_frame);
    pump_rounds(*ingress, 100);  // 回执在泵内写出（本进程无独立发送线程）
    TEST_ASSERT(client.recvWsFrame(opcode, echoed), "pong received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kPong), "pong opcode");
    TEST_ASSERT(echoed == ping_data, "pong payload intact");

    // close → close echo + WsClosed。
    const std::vector<std::uint8_t> close_data{0x03, 0xE8};  // code 1000
    std::vector<std::uint8_t> close_frame;
    append_masked_frame(close_frame, ws::OpCode::kClose, close_data, 0x22);
    client.sendAll(close_frame);
    pump_rounds(*ingress, 100);
    TEST_ASSERT(client.recvWsFrame(opcode, echoed), "close echo received");
    TEST_ASSERT(opcode == static_cast<std::uint8_t>(ws::OpCode::kClose), "close opcode");
    TEST_ASSERT(echoed == close_data, "close code echoed");
    TEST_ASSERT(pump_until(*ingress, [&] { return observer.count(EventKind::WsClosed) == 1; },
                           2000),
                "closed event");
    TEST_ASSERT(observer.snapshot().back().reason == DisconnectReason::NormalClose,
                "normal close");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 5. 升级路由上的普通 GET → 400（不降级）----

bool test_plain_get_on_ws_route_rejected() {
    std::cout << "Running: test_plain_get_on_ws_route_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(plain_request("GET", "/ws"));

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("400") != std::string::npos, "400 status");
    TEST_ASSERT(response.find("upgrade route requires websocket upgrade") != std::string::npos,
                "reason body");
    TEST_ASSERT(client.waitEof(2000), "connection closed after reject");

    TEST_ASSERT(observer.count(EventKind::WsOpened) == 0, "no ws opened");
    TEST_ASSERT(observer.count(EventKind::WsBinary) == 0, "no ws frames");
    TEST_ASSERT(observer.count(EventKind::HttpHandled) == 1, "http handled recorded");
    TEST_ASSERT(observer.snapshot()[0].status == 400, "400 recorded");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 6. HTTP 路由上的合法升级 → 400 ----

bool test_ws_upgrade_on_http_route_rejected() {
    std::cout << "Running: test_ws_upgrade_on_http_route_rejected..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(upgrade_request("GET", "/health"));

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("400") != std::string::npos, "400 status");
    TEST_ASSERT(response.find("websocket upgrade not allowed on http route") != std::string::npos,
                "reason body");
    TEST_ASSERT(client.waitEof(2000), "connection closed after reject");
    TEST_ASSERT(observer.count(EventKind::WsOpened) == 0, "no ws lifecycle on http route");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 7. 未注册路径 → 404 ----

bool test_unknown_path_404() {
    std::cout << "Running: test_unknown_path_404..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(plain_request("GET", "/nope"));

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "response received");
    TEST_ASSERT(response.find("404") != std::string::npos, "404 status");
    TEST_ASSERT(response.find("no such route") != std::string::npos, "reason body");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 8. 垃圾字节流（超头部上限）→ 431 ----

bool test_header_too_large_431() {
    std::cout << "Running: test_header_too_large_431..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    std::vector<std::uint8_t> garbage(9 * 1024, 'x');  // 超 8KiB 头部预算
    client.sendAll(garbage);

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("431") != std::string::npos, "431 status");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 9. 体超 1 MiB 预算 → 413（头齐即拒）----

bool test_body_too_large_413() {
    std::cout << "Running: test_body_too_large_413..." << std::endl;

    RecordingObserver observer;
    auto ingress = make_ingress(observer);

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    // 只声明超预算体长（不实发 1MiB+1 字节——拒收在头齐时即应发生）。
    const std::string req = "POST /echo HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Content-Length: 1048577\r\n"
                            "\r\n";
    client.sendAll({req.begin(), req.end()});

    pump_rounds(*ingress, 300);
    std::string response;
    TEST_ASSERT(client.recvHttpResponse(response), "reject response received");
    TEST_ASSERT(response.find("413") != std::string::npos, "413 status");
    TEST_ASSERT(client.waitEof(2000), "connection closed after reject");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 10. HTTPS round-trip（TLS 底座同一套语义）----

bool test_https_roundtrip() {
    std::cout << "Running: test_https_roundtrip..." << std::endl;

    const std::string dir = "/tmp/apollo-interfaces-tests-" + std::to_string(::getpid());
    TEST_ASSERT(::system(("mkdir -p " + dir).c_str()) == 0, "cert dir created");
    const auto certs = tls_test_support::generate_self_signed(dir, "https", "localhost");
    TEST_ASSERT(certs.error.empty(), "self-signed cert generated");

    apollo::net::tls::TlsServerContext tls_context;
    const auto loaded = tls_context.load(certs.cert_path, certs.key_path);
    TEST_ASSERT(loaded.ok, "tls context loaded");

    RecordingObserver observer;
    auto ingress = make_ingress(observer, &tls_context);
    TEST_ASSERT(ingress->listenPort() != 0, "ephemeral port assigned");

    tls_test_support::TlsClient client;
    std::string error;
    TEST_ASSERT(client.start("127.0.0.1", ingress->listenPort(), error), "tls client started");

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

    const auto request = plain_request("GET", "/health");
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
        TEST_ASSERT(sent, "request sent over tls");
    }

    const auto handled = pump_until(*ingress, [&] {
        drain_tls(stream);
        return observer.count(EventKind::HttpHandled) == 1;
    }, 5000);
    TEST_ASSERT(handled, "handled over https");
    const std::string response(reinterpret_cast<const char*>(stream.data()), stream.size());
    TEST_ASSERT(response.find("200") != std::string::npos, "200 seen in tls stream");
    TEST_ASSERT(response.find("Connection: close") != std::string::npos, "close declared");
    TEST_ASSERT(response.find("{\"status\":\"ok\"}\n") != std::string::npos, "health body");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 11. 并行监听 = 独立实例 + 互斥号段 ----

bool test_parallel_listeners_disjoint_ids() {
    std::cout << "Running: test_parallel_listeners_disjoint_ids..." << std::endl;

    RecordingObserver observer_a;
    RecordingObserver observer_b;
    constexpr std::uint64_t kIdStride = std::uint64_t{1} << 40;
    auto ingress_a = make_ingress(observer_a, nullptr, 0);
    auto ingress_b = make_ingress(observer_b, nullptr, kIdStride);
    TEST_ASSERT(ingress_a->listenPort() != ingress_b->listenPort(),
                "distinct ephemeral ports");

    HttpTestClient client_a;
    TEST_ASSERT(client_a.connect("127.0.0.1", ingress_a->listenPort()), "client a connected");
    client_a.sendAll(plain_request("GET", "/health"));
    HttpTestClient client_b;
    TEST_ASSERT(client_b.connect("127.0.0.1", ingress_b->listenPort()), "client b connected");
    client_b.sendAll(plain_request("GET", "/health"));

    TEST_ASSERT(pump_until(*ingress_a,
                           [&] { return observer_a.count(EventKind::HttpHandled) == 1; },
                           2000),
                "a handled");
    TEST_ASSERT(pump_until(*ingress_b,
                           [&] { return observer_b.count(EventKind::HttpHandled) == 1; },
                           2000),
                "b handled");
    const auto id_a = observer_a.snapshot()[0].connectionId;
    const auto id_b = observer_b.snapshot()[0].connectionId;
    TEST_ASSERT(id_a == 1, "a id from base 0");
    TEST_ASSERT(id_b == kIdStride + 1, "b id from base 1<<40");

    ingress_a->stop();
    ingress_b->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 12. 端口冲突 → start() 抛 ----

bool test_port_conflict_start_throws() {
    std::cout << "Running: test_port_conflict_start_throws..." << std::endl;

    RecordingObserver observer;
    auto first = make_ingress(observer);
    const auto port = first->listenPort();
    TEST_ASSERT(port != 0, "first listening");

    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    auto second = interfaces::makeHttpIngressServer(config);
    bool threw = false;
    try {
        second->start();
    } catch (const std::exception&) {
        threw = true;
    }
    TEST_ASSERT(threw, "bind conflict throws");

    // 首实例不受冲突影响，继续正常服务。
    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", port), "first still accepting");
    client.sendAll(plain_request("GET", "/health"));
    std::string response;
    TEST_ASSERT(pump_until(*first, [&] { return client.recvHttpResponse(response); }, 2000),
                "first still serving");

    first->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

#endif // !_WIN32

} // namespace

int main() {
#if !defined(_WIN32)
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"http_get_health", test_http_get_health},
        {"http_post_echo", test_http_post_echo},
        {"handler_throw_500", test_handler_throw_500},
        {"ws_upgrade_on_ws_route", test_ws_upgrade_on_ws_route},
        {"plain_get_on_ws_route_rejected", test_plain_get_on_ws_route_rejected},
        {"ws_upgrade_on_http_route_rejected", test_ws_upgrade_on_http_route_rejected},
        {"unknown_path_404", test_unknown_path_404},
        {"header_too_large_431", test_header_too_large_431},
        {"body_too_large_413", test_body_too_large_413},
        {"https_roundtrip", test_https_roundtrip},
        {"parallel_listeners_disjoint_ids", test_parallel_listeners_disjoint_ids},
        {"port_conflict_start_throws", test_port_conflict_start_throws},
    };
    int failures = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            ++failures;
        }
    }
    if (failures != 0) {
        std::cerr << failures << "/" << sizeof(cases) / sizeof(cases[0])
                  << " cases FAILED" << std::endl;
        return 1;
    }
    std::cout << "All " << sizeof(cases) / sizeof(cases[0]) << " cases passed." << std::endl;
    return 0;
#else
    std::cout << "interfaces http ingress tests: skipped on Windows (POSIX only)" << std::endl;
    return 0;
#endif
}
