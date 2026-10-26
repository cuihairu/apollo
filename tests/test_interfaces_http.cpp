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
//  12. 端口冲突 → start() 抛（绑定失败显形，不静默）；
//  13. 平台回调受理链（L62 批⑮）：cap 顶格拒收、告警线滞回、单拍消费
//      预算、FIFO 序、业务抛异常不断链、滞留过期丢弃计数；
//  14. 回调路由真接线：入队 202 / 顶格 429，drain 业务恰见受理的请求、
//      体逐字节保真；
//  15. 原始帧保留（验签保真面）：raw_request = 请求行 + 原始大小写头 +
//      恰取声明体长（管道尾字节不吞），body 含多字节/NUL 逐字节保真；
//  16. 请求超时预算：首字节起 T 内未完成 → 408 + 断连，完整请求不受误伤。
//  17. 回调验签 gate：HMAC-SHA256 绑定 raw_request；正例入链 202，
//      缺签/坏签/改体拒绝且链不受理（声明体长不符到不了 handler，见 18）。
//  18. 验签 Reason 矩阵：8 个拒绝原因各归因一次（SignatureHeaderMissing
//      由 17 覆盖），含 Content-Length 溢出防护、方案前缀大小写不敏感、
//      签名头行剥离口径、密钥未配置。
//
// Windows：link TCP/TLS 底座不进 M1 批（POSIX only）——用例本体空转。

#include "interfaces/callback_chain.hpp"
#include "interfaces/callback_verifier.hpp"
#include "interfaces/http_ingress.hpp"
#include "apollo/utils/crypto.h"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"
#include "tls_test_support.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <optional>
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
using interfaces::CallbackChain;
using interfaces::CallbackChainConfig;
using interfaces::CallbackRequest;
using interfaces::CallbackVerifier;
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

std::string unsigned_callback_frame(const std::string& body) {
    // 不给「声明体长 ≠ 实际体长」留口：入口按声明体长恰读该字节数才派发，
    // 线上造不出体长不符的完整请求（偏大则挂到超时，偏小则体被截断）。
    return "POST /callbacks HTTP/1.1\r\n"
           "Host: localhost\r\n"
           "Content-Length: " +
           std::to_string(body.size()) + "\r\n\r\n" + body;
}

std::string signed_callback_frame(const std::string& body, const std::string& secret) {
    std::string frame = unsigned_callback_frame(body);
    const std::string mac =
        apollo::utils::HMAC::sha256(secret, frame.data(), frame.size());
    const std::string separator("\r\n\r\n");
    const std::size_t at = frame.rfind(separator);
    if (at == std::string::npos) {
        throw std::runtime_error("callback test frame malformed");
    }
    return frame.substr(0, at + 2) + "X-Signature: hmac-sha256=" + mac +
           frame.substr(at);
}

void install_callback_route(HttpIngressServer& server, CallbackChain& chain,
                            const CallbackVerifier& verifier) {
    server.setHttpRoute("/callbacks", [&chain, &verifier](const HttpRequest& request) {
        HttpResponse response;
        const auto verified = verifier.verify(request);
        if (!verified.valid) {
            response.status = verified.status;
            response.reason = verified.status == 401 ? "Unauthorized" : "Forbidden";
            const std::string body =
                verified.status == 401 ? "callback signature mismatch\n"
                                       : "callback signature malformed\n";
            response.body.assign(body.begin(), body.end());
            return response;
        }
        const auto result = chain.push(interfaces::snapshot(request));
        response.status = result.accepted ? 202 : 429;
        response.reason = result.accepted ? "Accepted" : "Too Many Requests";
        const std::string body =
            result.accepted ? "accepted\n" : "callback backlog full\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
}

// 单元级验签矩阵夹具：raw_request / body / headers 三面与入口解析产物同形，
// 每个负例只动一面，结论即归因到那一处。
struct CallbackFixture {
    HttpRequest request;
    std::string digest;  // 过验签 gate 的正确摘要（对无签名头行的帧算得）
};

CallbackFixture callback_fixture(const std::string& body, const std::string& secret) {
    const std::string frame_no_sig = "POST /callbacks HTTP/1.1\r\n"
                                     "Host: localhost\r\n"
                                     "Content-Length: " + std::to_string(body.size()) +
                                     "\r\n\r\n" + body;
    const std::string digest = apollo::utils::HMAC::sha256(secret, frame_no_sig.data(),
                                                           frame_no_sig.size());
    const std::string separator("\r\n\r\n");
    const std::size_t at = frame_no_sig.rfind(separator);
    const std::string raw = frame_no_sig.substr(0, at + 2) +
                            "X-Signature: hmac-sha256=" + digest + frame_no_sig.substr(at);

    HttpRequest request;
    request.method = "POST";
    request.path = "/callbacks";
    request.headers.emplace_back("content-length", std::to_string(body.size()));
    request.headers.emplace_back("x-signature", "hmac-sha256=" + digest);
    request.body.assign(body.begin(), body.end());
    request.raw_request.assign(raw.begin(), raw.end());
    return {request, digest};
}

// 显式字节构造：字面量从 const char* 初始化会在首个 NUL 处截断成空体，
// 而验签用例要的是含 NUL 的二进制体（空体让 body.back() 越界成 UB）。
std::string binary_callback_body() {
    return std::string{'\x00', static_cast<char>(0xff), '-', 'b', 'i', 'n',
                       'a', 'r', 'y', ' ', 'c', 'a', 'l', 'l', 'b', 'a',
                       'c', 'k'};
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
    // 响应晚 HttpHandled 一拍到端（同 wss 101 竞态）：泵到响应体真正可见再
    // 断言——校验不弱化，只除掉同拍竞态。
    TEST_ASSERT(pump_until(*ingress,
                           [&] {
                               drain_tls(stream);
                               const std::string response(
                                   reinterpret_cast<const char*>(stream.data()),
                                   stream.size());
                               return response.find("200") != std::string::npos
                                      && response.find("{\"status\":\"ok\"}\n")
                                             != std::string::npos;
                           },
                           5000),
                "response captured over tls stream");
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

// ---- 13. 有界单写者回调链（单元：cap/滞回/拒收/单拍预算/抛异常/过期）----

bool test_callback_chain_bounded_single_writer() {
    std::cout << "Running: test_callback_chain_bounded_single_writer..." << std::endl;

    CallbackChainConfig config;
    config.cap = 4;
    config.high_watermark = 3;
    config.max_per_pump = 2;
    config.expire_after = std::chrono::milliseconds(50);
    CallbackChain chain(config);

    auto make = [](const std::string& path) {
        HttpRequest request;
        request.method = "POST";
        request.path = path;
        const std::string body = "body-of-" + path;
        request.body.assign(body.begin(), body.end());
        return interfaces::snapshot(request);
    };

    // cap 4 / 告警线 3：第三条越线告警一次，第四条不重复（滞回）。
    TEST_ASSERT(chain.push(make("/a")).accepted, "first accepted");
    const auto second = chain.push(make("/b"));
    TEST_ASSERT(second.accepted && !second.high_watermark, "below line no alarm");
    const auto third = chain.push(make("/c"));
    TEST_ASSERT(third.accepted && third.high_watermark, "third crosses line, alarm once");
    const auto fourth = chain.push(make("/d"));
    TEST_ASSERT(fourth.accepted && !fourth.high_watermark, "no repeat alarm (hysteresis)");
    TEST_ASSERT(fourth.id == 4, "sequential ids");
    const auto fifth = chain.push(make("/e"));
    TEST_ASSERT(!fifth.accepted && fifth.id == 0, "cap reject assigns no id");
    TEST_ASSERT(chain.rejected_count() == 1, "rejection counted");
    TEST_ASSERT(chain.size() == 4, "four queued");

    // 单拍消费预算 2：两拍各消费 2 条，FIFO 序 + 受理号递增。
    std::vector<std::string> order;
    std::vector<std::uint64_t> ids;
    const auto business = [&](const CallbackRequest& callback) {
        order.push_back(callback.path);
        ids.push_back(callback.id);
    };
    const auto drain_one = chain.drain(business);
    TEST_ASSERT(drain_one.executed == 2 && chain.size() == 2, "budget 2 per pump");
    const auto drain_two = chain.drain(business);
    TEST_ASSERT(drain_two.executed == 2 && chain.size() == 0, "second pump drains rest");
    TEST_ASSERT(order == std::vector<std::string>({"/a", "/b", "/c", "/d"}),
                "fifo order");
    TEST_ASSERT(ids == std::vector<std::uint64_t>({1, 2, 3, 4}), "ids ascending");
    TEST_ASSERT(chain.executed_count() == 4, "executed counter");

    // 滞回：排空回落告警线以下后，重新越线再告警一次。
    chain.push(make("/x"));
    chain.push(make("/y"));
    TEST_ASSERT(chain.push(make("/z")).high_watermark, "alarm re-arms after fall");

    // 业务抛异常：计 failed，链不断（同拍余量继续，余项下拍补）。
    const auto fail_on_x = [&](const CallbackRequest& callback) {
        if (callback.path == "/x") {
            throw std::runtime_error("boom on x");
        }
        business(callback);
    };
    const auto throwing_stats = chain.drain(fail_on_x);
    TEST_ASSERT(throwing_stats.failed == 1, "throw counted failed");
    TEST_ASSERT(throwing_stats.executed == 1, "same-pump budget continues after throw");
    TEST_ASSERT(chain.failed_count() == 1, "failed counter");
    TEST_ASSERT(chain.size() == 1, "tail still queued");
    const auto tail = chain.drain(business);
    TEST_ASSERT(tail.executed == 1 && tail.failed == 0, "tail drained cleanly");

    // 滞留过期：迟到的回调不执行，丢弃并计数（渠道侧重试补齐）。
    chain.push(make("/stale"));
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    const auto stale_stats = chain.drain(business);
    TEST_ASSERT(stale_stats.expired == 1 && stale_stats.executed == 0,
                "stale dropped");
    TEST_ASSERT(chain.expired_count() == 1, "expired counter");
    TEST_ASSERT(chain.size() == 0, "nothing left");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 14. 回调路由真接线：入队 202 / 顶格 429 / drain 体保真 ----

bool test_http_callback_route_202_and_429() {
    std::cout << "Running: test_http_callback_route_202_and_429..." << std::endl;

    RecordingObserver observer;
    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    auto ingress = interfaces::makeHttpIngressServer(config);

    CallbackChainConfig chain_config;
    chain_config.cap = 2;
    chain_config.high_watermark = 2;
    chain_config.max_per_pump = 8;
    CallbackChain chain(chain_config);
    const CallbackVerifier verifier("test-secret");
    install_callback_route(*ingress, chain, verifier);
    ingress->setObserver(&observer);
    ingress->start();
    TEST_ASSERT(ingress->listenPort() != 0, "port assigned");

    // 入口每应答一连接（Connection: close）——一请求一连接三连发。
    std::string response;
    auto post = [&](const std::string& body) {
        response.clear();
        HttpTestClient client;
        if (!client.connect("127.0.0.1", ingress->listenPort())) {
            return false;
        }
        const std::string frame = signed_callback_frame(body, "test-secret");
        client.sendAll({frame.begin(), frame.end()});
        return pump_until(*ingress,
                          [&] { return client.recvHttpResponse(response); }, 2000);
    };
    TEST_ASSERT(post("payload-one"), "first response");
    TEST_ASSERT(response.find("202") != std::string::npos, "first accepted 202");
    TEST_ASSERT(post("payload-two"), "second response");
    TEST_ASSERT(response.find("202") != std::string::npos, "second accepted 202");
    TEST_ASSERT(post("payload-three"), "third response");
    TEST_ASSERT(response.find("429") != std::string::npos, "third backpressured 429");

    TEST_ASSERT(chain.size() == 2, "two queued");
    TEST_ASSERT(chain.rejected_count() == 1, "one rejected");

    // drain：业务恰见受理的两条，FIFO 序 + 体逐字节（验签输入保真）。
    std::vector<std::vector<std::uint8_t>> bodies;
    const auto stats = chain.drain(
        [&](const CallbackRequest& callback) { bodies.push_back(callback.body); });
    TEST_ASSERT(stats.executed == 2, "business saw exactly two");
    const std::vector<std::uint8_t> one{'p', 'a', 'y', 'l', 'o', 'a', 'd', '-', 'o', 'n', 'e'};
    const std::vector<std::uint8_t> two{'p', 'a', 'y', 'l', 'o', 'a', 'd', '-', 't', 'w', 'o'};
    TEST_ASSERT(bodies.size() == 2 && bodies[0] == one, "first body verbatim");
    TEST_ASSERT(bodies[1] == two, "second body verbatim");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 15. 原始帧保留（验签保真面）：raw_request 逐字节、头名原大小写、
//          体恰取声明长度（管道尾字节不吞）----

bool test_raw_request_retained_verbatim() {
    std::cout << "Running: test_raw_request_retained_verbatim..." << std::endl;

    RecordingObserver observer;
    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    auto ingress = interfaces::makeHttpIngressServer(config);

    CallbackChain chain;
    ingress->setHttpRoute("/callbacks", [&chain](const HttpRequest& request) {
        HttpResponse response;
        const auto result = chain.push(interfaces::snapshot(request));
        response.status = result.accepted ? 202 : 429;
        response.reason = result.accepted ? "Accepted" : "Too Many Requests";
        const std::string body =
            result.accepted ? "accepted\n" : "callback backlog full\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
    ingress->setObserver(&observer);
    ingress->start();
    TEST_ASSERT(ingress->listenPort() != 0, "port assigned");

    // 二进制体（含多字节 UTF-8 与 NUL，6 字节声明）；同一次 write 附带
    // 管道尾字节——raw/body 均不得吞入（验签按声明体长算哈希）。
    const std::vector<std::uint8_t> payload{0x70, 0x61, 0xC3, 0xA9, 0x79, 0x00};
    std::vector<std::uint8_t> wire;
    const auto push_str = [&wire](const std::string& text) {
        wire.insert(wire.end(), text.begin(), text.end());
    };
    push_str("POST /callbacks HTTP/1.1\r\n"
             "Host: localhost\r\n"
             "X-Signature: abc123DEF\r\n"
             "Content-Length: 6\r\n"
             "\r\n");
    wire.insert(wire.end(), payload.begin(), payload.end());
    const std::vector<std::uint8_t> expected_raw = wire;  // 请求行+头+体（原大小写）
    const std::vector<std::uint8_t> trailing{'T', 'R', 'A', 'I', 'L'};
    wire.insert(wire.end(), trailing.begin(), trailing.end());

    HttpTestClient client;
    TEST_ASSERT(client.connect("127.0.0.1", ingress->listenPort()), "client connected");
    client.sendAll(wire);
    std::string response;
    TEST_ASSERT(pump_until(*ingress, [&] { return client.recvHttpResponse(response); },
                           2000),
                "response received");
    TEST_ASSERT(response.find("202") != std::string::npos, "accepted 202");

    std::optional<CallbackRequest> captured;
    const auto stats = chain.drain(
        [&](const CallbackRequest& callback) { captured = callback; });
    TEST_ASSERT(stats.executed == 1, "one callback drained");
    TEST_ASSERT(captured.has_value(), "captured");
    TEST_ASSERT(captured->raw_request == expected_raw,
                "raw frame verbatim (head + declared body only)");
    TEST_ASSERT(captured->body == payload, "body bytes verbatim");
    bool signature_header_kept = false;
    for (const auto& [name, value] : captured->headers) {
        if (name == "x-signature" && value == "abc123DEF") {
            signature_header_kept = true;
        }
    }
    TEST_ASSERT(signature_header_kept, "parsed header lowercased, value intact");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 17. 回调验签 gate：正例入链，缺签/坏签/体长不符不入链 ----

bool test_callback_signature_gate() {
    std::cout << "Running: test_callback_signature_gate..." << std::endl;

    RecordingObserver observer;
    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    auto ingress = interfaces::makeHttpIngressServer(config);

    CallbackChainConfig chain_config;
    chain_config.cap = 2;
    CallbackChain chain(chain_config);
    const CallbackVerifier verifier("channel-secret");
    install_callback_route(*ingress, chain, verifier);
    ingress->setObserver(&observer);
    ingress->start();
    TEST_ASSERT(ingress->listenPort() != 0, "port assigned");

    const std::string body = binary_callback_body();
    const std::string valid = signed_callback_frame(body, "channel-secret");
    std::string response;
    auto post = [&](const std::string& frame) {
        response.clear();
        HttpTestClient client;
        if (!client.connect("127.0.0.1", ingress->listenPort())) {
            return false;
        }
        client.sendAll({frame.begin(), frame.end()});
        return pump_until(*ingress,
                          [&] { return client.recvHttpResponse(response); }, 2000);
    };

    TEST_ASSERT(post(valid), "valid response received");
    TEST_ASSERT(response.find("202") != std::string::npos, "valid signature accepted");

    std::string tampered = valid;
    const std::size_t scheme_at = tampered.find("hmac-sha256=");
    TEST_ASSERT(scheme_at != std::string::npos && scheme_at + 12 < tampered.size(),
                "signature digest found");
    tampered[scheme_at + 12] = tampered[scheme_at + 12] == '0' ? '1' : '0';
    TEST_ASSERT(post(tampered), "tampered response received");
    TEST_ASSERT(response.find("401") != std::string::npos, "tampered signature rejected");

    TEST_ASSERT(post(unsigned_callback_frame(body)), "missing response received");
    TEST_ASSERT(response.find("401") != std::string::npos, "missing signature rejected");

    std::string malformed = unsigned_callback_frame(body);
    malformed.insert(malformed.rfind("\r\n\r\n"),
                     "\r\nX-Signature: hmac-sha256=not-hex");
    TEST_ASSERT(post(malformed), "malformed response received");
    TEST_ASSERT(response.find("403") != std::string::npos, "malformed rejected");

    // 体字节被改、Content-Length 与签名头原样 → 401：验签哈希覆盖声明体长。
    std::string body_tampered = valid;
    body_tampered.back() = static_cast<char>(body_tampered.back() ^ 0x01);
    TEST_ASSERT(post(body_tampered), "body-tamper response received");
    TEST_ASSERT(response.find("401") != std::string::npos, "body tamper rejected");

    // 声明体长 ≠ 实际体长的负例只能走单测面（见 18）：入口按声明体长恰读该
    // 字节数才派发给 handler——声明偏大则请求永不完成（挂到 requestTimeout →
    // 408，见 16），声明偏小则体被截到声明长度，两种线上形态都到不了验签的
    // Content-Length 比对（旧例在这里挂死等一个不会来的字节，故移除）。

    TEST_ASSERT(chain.size() == 1, "only valid callback accepted");
    TEST_ASSERT(chain.rejected_count() == 0, "security rejects bypass the chain");
    std::string captured_raw;
    const auto stats = chain.drain([&](const CallbackRequest& callback) {
        captured_raw.assign(callback.raw_request.begin(), callback.raw_request.end());
    });
    TEST_ASSERT(stats.executed == 1, "one verified callback drained");
    TEST_ASSERT(captured_raw == valid, "drained raw frame preserved");

    ingress->stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 18. 验签 Reason 矩阵：8 个拒绝原因各归因一次，不混判
//      （SignatureHeaderMissing 走真链路，见 17）----

bool test_callback_verifier_reason_matrix() {
    std::cout << "Running: test_callback_verifier_reason_matrix..." << std::endl;

    const std::string body = binary_callback_body();
    const CallbackVerifier verifier("channel-secret");
    const auto fx = callback_fixture(body, "channel-secret");
    TEST_ASSERT(verifier.verify(fx.request).valid, "fixture self-consistent");

    // 摘要定长 + 定长比较：合法摘要过 gate。
    const auto ok = verifier.verify(fx.request);
    TEST_ASSERT(ok.valid && ok.status == 202, "valid accepted 202");

    // 摘要与体不同步（body 与 raw 同字节翻转 = 线上改包）→ SignatureMismatch 401。
    HttpRequest body_tampered = fx.request;
    body_tampered.body.back() = static_cast<std::uint8_t>(body_tampered.body.back() ^ 0x01);
    body_tampered.raw_request.back() =
        static_cast<std::uint8_t>(body_tampered.raw_request.back() ^ 0x01);
    const auto tampered = verifier.verify(body_tampered);
    TEST_ASSERT(!tampered.valid && tampered.status == 401 &&
                    tampered.reason == CallbackVerifier::Reason::SignatureMismatch,
                "body tamper -> SignatureMismatch 401");

    // 密钥不对（同一请求给别的 secret）→ SignatureMismatch 401。
    const CallbackVerifier wrong_secret("other-secret");
    const auto wrong = wrong_secret.verify(fx.request);
    TEST_ASSERT(!wrong.valid && wrong.status == 401 &&
                    wrong.reason == CallbackVerifier::Reason::SignatureMismatch,
                "wrong secret -> SignatureMismatch 401");

    // 按「含签名头行的整帧」算摘要（渠道未按去签名头行口径）→ SignatureMismatch
    // 401：证明验签输入确实是剥离 x-signature 行的帧。
    const std::string frame_no_sig("POST /callbacks HTTP/1.1\r\n"
                                   "Host: localhost\r\n"
                                   "Content-Length: " + std::to_string(body.size()) +
                                   "\r\n\r\n" + body);
    const std::string separator("\r\n\r\n");
    const std::size_t sep_at = frame_no_sig.rfind(separator);
    const std::string frame_with_sig = frame_no_sig.substr(0, sep_at + 2) +
                                       "X-Signature: hmac-sha256=" +
                                       std::string(64, '0') +
                                       frame_no_sig.substr(sep_at);
    HttpRequest header_included = fx.request;
    std::string raw_with_sig(frame_with_sig.begin(), frame_with_sig.end());
    header_included.raw_request.assign(raw_with_sig.begin(), raw_with_sig.end());
    for (auto& [name, value] : header_included.headers) {
        if (name == "x-signature") {
            value = "hmac-sha256=" + apollo::utils::HMAC::sha256(
                                          "channel-secret", raw_with_sig.data(),
                                          raw_with_sig.size());
        }
    }
    const auto header_included_result = verifier.verify(header_included);
    TEST_ASSERT(!header_included_result.valid && header_included_result.status == 401 &&
                    header_included_result.reason ==
                        CallbackVerifier::Reason::SignatureMismatch,
                "signature over header-bearing frame rejected");

    // 方案前缀大小写不敏感（渠道客户端常见写法）→ 仍受理。
    HttpRequest upper_scheme = fx.request;
    for (auto& [name, value] : upper_scheme.headers) {
        if (name == "x-signature") {
            value = "HMAC-SHA256=" + fx.digest;
        }
    }
    std::string upper_raw(upper_scheme.raw_request.begin(),
                          upper_scheme.raw_request.end());
    upper_raw.replace(upper_raw.find("hmac-sha256="), 12, "HMAC-SHA256=");
    upper_scheme.raw_request.assign(upper_raw.begin(), upper_raw.end());
    TEST_ASSERT(verifier.verify(upper_scheme).valid,
                "uppercase scheme prefix accepted");

    // 重复签名头（歧义签名面）→ SignatureHeaderAmbiguous 403。
    HttpRequest ambiguous = fx.request;
    ambiguous.headers.emplace_back("x-signature", "hmac-sha256=" + fx.digest);
    const auto ambiguous_result = verifier.verify(ambiguous);
    TEST_ASSERT(!ambiguous_result.valid && ambiguous_result.status == 403 &&
                    ambiguous_result.reason ==
                        CallbackVerifier::Reason::SignatureHeaderAmbiguous,
                "duplicate signature header -> ambiguous 403");

    // 摘要非 64 位十六进制 → SignatureMalformed 403。
    HttpRequest short_digest = fx.request;
    for (auto& [name, value] : short_digest.headers) {
        if (name == "x-signature") {
            value = "hmac-sha256=" + fx.digest.substr(0, 63);
        }
    }
    const auto short_result = verifier.verify(short_digest);
    TEST_ASSERT(!short_result.valid && short_result.status == 403 &&
                    short_result.reason == CallbackVerifier::Reason::SignatureMalformed,
                "63-char digest -> SignatureMalformed 403");

    // 方案前缀不对 → SignatureHeaderMalformed 403。
    HttpRequest bad_scheme = fx.request;
    for (auto& [name, value] : bad_scheme.headers) {
        if (name == "x-signature") {
            value = "sha256=" + fx.digest;
        }
    }
    const auto bad_scheme_result = verifier.verify(bad_scheme);
    TEST_ASSERT(!bad_scheme_result.valid && bad_scheme_result.status == 403 &&
                    bad_scheme_result.reason ==
                        CallbackVerifier::Reason::SignatureHeaderMalformed,
                "wrong scheme -> SignatureHeaderMalformed 403");

    // Content-Length 缺失 → ContentLengthMissing 403。
    HttpRequest no_length = fx.request;
    for (auto it = no_length.headers.begin(); it != no_length.headers.end(); ++it) {
        if (it->first == "content-length") {
            no_length.headers.erase(it);
            break;
        }
    }
    const auto no_length_result = verifier.verify(no_length);
    TEST_ASSERT(!no_length_result.valid && no_length_result.status == 403 &&
                    no_length_result.reason == CallbackVerifier::Reason::ContentLengthMissing,
                "missing content-length -> ContentLengthMissing 403");

    // 声明体长超 size_t 表示域（30 位）→ 判 malformed 而非回绕命中 →
    // ContentLengthMismatch 403（溢出防护，见 exact_size）。
    HttpRequest huge_length = fx.request;
    for (auto& [name, value] : huge_length.headers) {
        if (name == "content-length") {
            value.assign(30, '9');
        }
    }
    const auto huge_result = verifier.verify(huge_length);
    TEST_ASSERT(!huge_result.valid && huge_result.status == 403 &&
                    huge_result.reason == CallbackVerifier::Reason::ContentLengthMismatch,
                "oversized content-length -> ContentLengthMismatch 403");

    // raw 面与 body 面不一致（尾字节不是 body）→ RawFrameMismatch 403。
    HttpRequest raw_diverged = fx.request;
    raw_diverged.raw_request.back() =
        static_cast<std::uint8_t>(raw_diverged.raw_request.back() ^ 0x01);
    const auto raw_result = verifier.verify(raw_diverged);
    TEST_ASSERT(!raw_result.valid && raw_result.status == 403 &&
                    raw_result.reason == CallbackVerifier::Reason::RawFrameMismatch,
                "raw/body divergence -> RawFrameMismatch 403");

    // raw 面为空 → RawFrameMismatch 403（无原始帧即无验签输入）。
    HttpRequest no_raw = fx.request;
    no_raw.raw_request.clear();
    const auto no_raw_result = verifier.verify(no_raw);
    TEST_ASSERT(!no_raw_result.valid && no_raw_result.status == 403 &&
                    no_raw_result.reason == CallbackVerifier::Reason::RawFrameMismatch,
                "empty raw frame -> RawFrameMismatch 403");

    // 密钥未配置 → SecretNotConfigured 403（不受理任何回调）。
    const CallbackVerifier no_secret("");
    const auto no_secret_result = no_secret.verify(fx.request);
    TEST_ASSERT(!no_secret_result.valid && no_secret_result.status == 403 &&
                    no_secret_result.reason ==
                        CallbackVerifier::Reason::SecretNotConfigured,
                "unset secret -> SecretNotConfigured 403");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 16. 请求超时预算：首字节起 T 内未完成 → 408 + 断连；完整请求不受误伤 ----

bool test_request_timeout_408() {
    std::cout << "Running: test_request_timeout_408..." << std::endl;

    RecordingObserver observer;
    HttpIngressConfig config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.requestTimeout = std::chrono::milliseconds(300);
    auto ingress = interfaces::makeHttpIngressServer(config);
    ingress->setHttpRoute("/health", [](const HttpRequest&) {
        HttpResponse response;
        const std::string body = "{\"status\":\"ok\"}\n";
        response.body.assign(body.begin(), body.end());
        return response;
    });
    ingress->setObserver(&observer);
    ingress->start();
    TEST_ASSERT(ingress->listenPort() != 0, "port assigned");

    // 慢客户端：头齐 + 声明体长 5，体不续传 → 300ms 截止 408 + 断连。
    HttpTestClient slow;
    TEST_ASSERT(slow.connect("127.0.0.1", ingress->listenPort()), "slow connected");
    const std::string partial = "POST /health HTTP/1.1\r\n"
                                "Host: localhost\r\n"
                                "Content-Length: 5\r\n"
                                "\r\n";
    slow.sendAll({partial.begin(), partial.end()});
    pump_rounds(*ingress, 700);  // 超 300ms 截止：408 生成并写出（拍间隔 ≥2ms）
    std::string response;
    TEST_ASSERT(slow.recvHttpResponse(response), "408 response received");
    TEST_ASSERT(response.find("408") != std::string::npos, "408 status");
    TEST_ASSERT(response.find("request timeout") != std::string::npos,
                "reason body");
    TEST_ASSERT(slow.waitEof(2000), "connection closed after timeout");

    // 同一入口的完整请求不受预算误伤（快客户端照常 200，零假阳性）。
    HttpTestClient fast;
    TEST_ASSERT(fast.connect("127.0.0.1", ingress->listenPort()), "fast connected");
    fast.sendAll(plain_request("GET", "/health"));
    std::string fast_response;
    TEST_ASSERT(pump_until(*ingress,
                           [&] { return fast.recvHttpResponse(fast_response); },
                           2000),
                "fast response received");
    TEST_ASSERT(fast_response.find("200") != std::string::npos, "no false positive");

    ingress->stop();
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
        {"callback_chain_bounded_single_writer", test_callback_chain_bounded_single_writer},
        {"http_callback_route_202_and_429", test_http_callback_route_202_and_429},
        {"raw_request_retained_verbatim", test_raw_request_retained_verbatim},
        {"request_timeout_408", test_request_timeout_408},
        {"callback_signature_gate", test_callback_signature_gate},
        {"callback_verifier_reason_matrix", test_callback_verifier_reason_matrix},
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
