#include "interfaces/http_ingress.hpp"

#if !defined(_WIN32)

#include "apollo/core/log/log_manager.h"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <span>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace interfaces {

namespace netlink = apollo::net::link;
namespace netws = apollo::net::ws;

namespace {

apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("interfaces_http");
}

// 预算面（头文件纪律的落实处）。
inline constexpr std::size_t kMaxHeaderBytes = 8 * 1024;
inline constexpr std::size_t kMaxBodyBytes = 1024 * 1024;

// 头部终止与行分隔的模式（显式长度——std::search 不收字符串字面量）。
constexpr char kHeaderTerm[] = "\r\n\r\n";
constexpr char kLineBreak[] = "\r\n";

inline std::string to_lower(std::string_view text) {
    std::string out(text);
    for (auto& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

inline std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == ' ' || text[end - 1] == '\t')) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

const char* status_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 202: return "Accepted";
        case 400: return "Bad Request";
        case 408: return "Request Timeout";
        case 404: return "Not Found";
        case 413: return "Payload Too Large";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

// HTTP 应答字节（Content-Length / Connection: close 统一补）。
std::vector<std::uint8_t> render_response(int status,
                                          const std::vector<std::pair<std::string, std::string>>& extra_headers,
                                          const std::vector<std::uint8_t>& body) {
    std::string head = "HTTP/1.1 " + std::to_string(status) + " "
                       + status_reason(status) + "\r\n";
    for (const auto& [name, value] : extra_headers) {
        head += name + ": " + value + "\r\n";
    }
    head += "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "Connection: close\r\n"
            "\r\n";
    std::vector<std::uint8_t> out(head.begin(), head.end());
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::string header_value(const HttpRequest& request, const std::string& name) {
    for (const auto& [header_name, value] : request.headers) {
        if (header_name == name) {
            return value;
        }
    }
    return {};
}

// 升级判据（ws::is_valid_upgrade 消费 UpgradeRequest——由本入口解析面填）。
bool is_valid_upgrade_request(const HttpRequest& request) {
    netws::UpgradeRequest upgrade;
    upgrade.method = request.method;
    upgrade.path = request.path;
    upgrade.key = header_value(request, "sec-websocket-key");
    upgrade.version = std::atoi(header_value(request, "sec-websocket-version").c_str());
    for (const auto& [name, value] : request.headers) {
        if (name == "upgrade") {
            upgrade.upgrade_websocket = to_lower(value).find("websocket") != std::string::npos;
        } else if (name == "connection") {
            const std::string lowered = to_lower(value);
            for (std::size_t start = 0; start <= lowered.size();) {
                const std::size_t comma = lowered.find(',', start);
                const std::string_view token(lowered.data() + start,
                                             (comma == std::string::npos ? lowered.size()
                                                                         : comma) - start);
                if (trim(token) == "upgrade") {
                    upgrade.connection_upgrade = true;
                }
                start = comma == std::string::npos ? lowered.size() + 1 : comma + 1;
            }
        }
    }
    return netws::is_valid_upgrade(upgrade);
}

// 泵线程内先缓冲后派发（observer 回调不得重入泵状态机）。
struct ObserverEvent {
    enum class Kind { kHttpHandled, kWsOpened, kWsBinary, kWsClosed };

    Kind kind{Kind::kWsClosed};
    ConnectionID connectionId{0};
    ClientEndpoint endpoint;
    DisconnectReason reason{DisconnectReason::TransportError};
    int status{0};
    HttpRequest request;
    std::vector<std::uint8_t> payload;
};

struct HttpConnection {
    ConnectionID id{0};
    ClientEndpoint endpoint;
    std::unique_ptr<netlink::ILinkTransport> transport;

    enum class Phase { kRequest, kWs, kClosing };
    Phase phase{Phase::kRequest};

    std::vector<std::uint8_t> rx;        // 请求头/体累积（泵线程独占）
    std::vector<std::uint8_t> tx;        // 出站残余，泵线程独占
    std::vector<std::uint8_t> netframe;  // ws echo 组帧中转
    std::size_t body_need{0};            // Content-Length 目标体长
    bool body_declared{false};           // 有 Content-Length（区别于无体请求）
    HttpRequest request;                 // 头解析结果（body 阶段续填）
    netws::FrameParser frames;           // ws 专用路由生命周期
    std::uint32_t stalledPumps{0};       // closing 且残余写不尽的泵计数
    // 请求超时预算（requestTimeout > 0 才生效）：首请求字节到达武装，绝对
    // 截止——T 内未完成即 408。TLS 握手期 read 返 0，不会误武装（首字节 =
    // 首个应用层字节）。
    bool requestDeadlineArmed{false};
    std::chrono::steady_clock::time_point requestDeadline{};
};

// 请求行 + 头部解析（失败 = 400；body 字节留在 rx）。
bool parse_request_head(HttpConnection& conn, std::size_t head_size) {
    const std::string_view raw(reinterpret_cast<const char*>(conn.rx.data()), head_size);
    const std::size_t line_end = raw.find(kLineBreak, 0, 2);
    if (line_end == std::string_view::npos) {
        return false;
    }
    const std::string_view first_line = raw.substr(0, line_end);
    const std::size_t first_space = first_line.find(' ');
    const std::size_t second_space = first_line.find(' ', first_space == std::string_view::npos ? 0 : first_space + 1);
    if (first_space == std::string_view::npos || second_space == std::string_view::npos ||
        second_space == first_line.size() - 1) {
        return false;
    }
    conn.request = HttpRequest{};
    conn.request.method = std::string(first_line.substr(0, first_space));
    conn.request.path = std::string(first_line.substr(first_space + 1,
                                                      second_space - first_space - 1));

    const std::string_view headers = raw.substr(line_end + 2);
    std::size_t offset = 0;
    while (offset < headers.size()) {
        const std::size_t nl = headers.find(kLineBreak, offset, 2);
        if (nl == std::string_view::npos) {
            break;
        }
        const std::string_view line = headers.substr(offset, nl - offset);
        offset = nl + 2;
        if (line.empty()) {
            continue;
        }
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            return false;
        }
        conn.request.headers.emplace_back(to_lower(line.substr(0, colon)),
                                          trim(line.substr(colon + 1)));
    }
    return true;
}

class HttpIngressImpl final : public HttpIngressServer {
public:
    explicit HttpIngressImpl(HttpIngressConfig config)
        : config_(std::move(config)), nextConnectionId_(config.connectionIdBase + 1) {}

    void start() override {
        if (!config_.tls) {
            if (!tcpListener_.start(config_.host, config_.port)) {
                throw std::runtime_error("Failed to bind http ingress on "
                                         + config_.host + ":" + std::to_string(config_.port));
            }
        } else {
            if (!tlsListener_.start(*config_.tls, config_.host, config_.port)) {
                throw std::runtime_error("Failed to bind https ingress on "
                                         + config_.host + ":" + std::to_string(config_.port)
                                         + " (tls context not loaded or bind refused)");
            }
        }
        started_ = true;
        glog().info("http ingress listening on " + config_.host + ":"
                    + std::to_string(listenPort())
                    + (config_.tls ? " (https)" : " (http)"));
    }

    void stop() override {
        if (!started_) {
            return;
        }
        close_listener();
        for (auto& [id, conn] : connections_) {
            conn->transport->close();
            if (conn->phase == HttpConnection::Phase::kWs) {
                ObserverEvent event;
                event.kind = ObserverEvent::Kind::kWsClosed;
                event.connectionId = id;
                event.reason = DisconnectReason::NormalClose;
                events_.push_back(std::move(event));
            }
        }
        connections_.clear();
        started_ = false;
        fire_events();
    }

    void pump() override {
        if (!started_) {
            return;
        }
        accept_new();
        io_round();
        reap_closed();
        fire_events();
    }

    std::uint16_t listenPort() const override {
        return started_ ? (config_.tls ? tlsListener_.port() : tcpListener_.port()) : 0;
    }

    void beginDrain() override {
        if (!started_ || draining_) {
            return;
        }
        draining_ = true;
        close_listener();
        glog().info("http ingress drain begun: listener closed, " +
                    std::to_string(activeConnections()) + " existing connections served");
    }

    std::size_t activeConnections() const override {
        std::size_t n = 0;
        for (const auto& [id, conn] : connections_) {
            if (conn->phase != HttpConnection::Phase::kClosing) {
                ++n;
            }
        }
        return n;
    }

    std::vector<ConnectionID> connectionIds() const override {
        std::vector<ConnectionID> ids;
        for (const auto& [id, conn] : connections_) {
            if (conn->phase != HttpConnection::Phase::kClosing) {
                ids.push_back(id);
            }
        }
        return ids;
    }

    void setHttpRoute(std::string path, HttpHandler handler) override {
        http_routes_[std::move(path)] = std::move(handler);
    }

    void setWsRoute(std::string path) override { ws_routes_.insert(std::move(path)); }

    void setObserver(IHttpIngressObserver* observer) override { observer_ = observer; }

private:
    void close_listener() {
        if (config_.tls) {
            tlsListener_.close();
        } else {
            tcpListener_.close();
        }
    }

    void accept_new() {
        if (draining_) {
            return;
        }
        for (;;) {
            std::unique_ptr<netlink::ILinkTransport> transport =
                config_.tls ? tlsListener_.accept() : tcpListener_.accept();
            if (!transport) {
                break;
            }
            if (config_.maxConnections != 0 && connections_.size() >= config_.maxConnections) {
                glog().warning("http ingress at capacity, dropping incoming connection");
                transport->close();
                continue;
            }
            auto conn = std::make_unique<HttpConnection>();
            conn->endpoint = ClientEndpoint{transport->peerAddress(), transport->peerPort()};
            conn->transport = std::move(transport);
            conn->id = nextConnectionId_++;
            connections_.emplace(conn->id, std::move(conn));
        }
    }

    void io_round() {
        const auto now = std::chrono::steady_clock::now();
        for (auto& [id, conn] : connections_) {
            if (conn->phase == HttpConnection::Phase::kClosing) {
                // 应答/回执写尽即拆；滞留有强拆兜底（对端不读 socket）。
                if (!conn->tx.empty()) {
                    flush_tx(*conn);
                    if (!conn->tx.empty() && ++conn->stalledPumps > 500) {
                        glog().warning("http closing reply stalled, dropping connection");
                        conn->tx.clear();
                    }
                }
                continue;
            }
            // 请求超时预算：首字节起 T 内未完成（头不齐/体不齐）→ 408 断连，
            // 挂死的半请求不吃并发预算（渠道侧重试补齐）。
            if (conn->phase == HttpConnection::Phase::kRequest && conn->requestDeadlineArmed &&
                config_.requestTimeout.count() > 0 && now > conn->requestDeadline) {
                reply_simple(*conn, 408, "request timeout");
                continue;
            }
            flush_tx(*conn);
            if (conn->phase == HttpConnection::Phase::kClosing) {
                continue;
            }
            read_io(*conn);
            if (!conn->tx.empty()) {
                flush_tx(*conn);
            }
        }
    }

    void flush_tx(HttpConnection& conn) {
        while (!conn.tx.empty()) {
            const int n = conn.transport->write(conn.tx.data(), conn.tx.size());
            if (n < 0) {
                drop_connection(conn);
                return;
            }
            if (n == 0) {
                return;  // 暂不可写：余量留到下个泵周期
            }
            conn.tx.erase(conn.tx.begin(), conn.tx.begin() + n);
        }
    }

    void read_io(HttpConnection& conn) {
        std::uint8_t buf[4096];
        for (;;) {
            const int n = conn.transport->read(buf, sizeof buf);
            if (n > 0) {
                if (conn.phase == HttpConnection::Phase::kWs) {
                    feed_ws_bytes(conn, std::span<const std::uint8_t>(
                                            buf, static_cast<std::size_t>(n)));
                } else {
                    if (!conn.requestDeadlineArmed) {
                        conn.requestDeadlineArmed = true;
                        conn.requestDeadline =
                            std::chrono::steady_clock::now() + config_.requestTimeout;
                    }
                    conn.rx.insert(conn.rx.end(), buf, buf + n);
                    if (conn.phase == HttpConnection::Phase::kRequest) {
                        step_request(conn);
                    }
                }
                if (conn.phase == HttpConnection::Phase::kClosing) {
                    return;
                }
                if (static_cast<std::size_t>(n) < sizeof buf) {
                    return;
                }
                continue;
            }
            if (n == 0) {
                return;  // 暂无数据（含 TLS 握手在跑）
            }
            // -1：对端关闭与传输错误同形。
            if (conn.phase == HttpConnection::Phase::kWs) {
                ObserverEvent event;
                event.kind = ObserverEvent::Kind::kWsClosed;
                event.connectionId = conn.id;
                event.reason = DisconnectReason::NormalClose;
                events_.push_back(std::move(event));
            }
            drop_connection(conn);
            return;
        }
    }

    // 请求阶段状态推进：头 →（ws 路由 | 体 → 应答）。
    void step_request(HttpConnection& conn) {
        const auto terminator = std::search(conn.rx.begin(), conn.rx.end(),
                                            kHeaderTerm, kHeaderTerm + 4);
        if (terminator == conn.rx.end()) {
            if (conn.rx.size() > kMaxHeaderBytes) {
                reply_simple(conn, 431, "request header exceeds budget");
            }
            return;  // 头未齐（或应答已入途）
        }
        const std::size_t head_size =
            static_cast<std::size_t>(terminator - conn.rx.begin()) + 4;
        if (!parse_request_head(conn, head_size)) {
            reply_simple(conn, 400, "malformed request");
            return;
        }

        // WS 专用路由：升级判据过了才受理（独立生命周期，不进 http handler；
        // 升级路由上的普通 HTTP 请求不降级——400）。
        if (ws_routes_.count(conn.request.path) != 0) {
            if (!is_valid_upgrade_request(conn.request)) {
                reply_simple(conn, 400, "upgrade route requires websocket upgrade");
                return;
            }
            const auto accept_reply =
                netws::make_accept_response(header_value(conn.request, "sec-websocket-key"));
            conn.tx.insert(conn.tx.end(), accept_reply.begin(), accept_reply.end());
            conn.rx.clear();
            conn.phase = HttpConnection::Phase::kWs;
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kWsOpened;
            event.connectionId = conn.id;
            event.endpoint = conn.endpoint;
            events_.push_back(std::move(event));
            return;
        }

        const auto handler = http_routes_.find(conn.request.path);
        if (handler == http_routes_.end()) {
            reply_simple(conn, 404, "no such route");
            return;
        }
        // HTTP 路由不受理升级（todo L45 路由隔离：合法升级请求打在 http
        // 路由上 = 400，不进 handler；非法残缺的请求仍走普通语义）。
        if (is_valid_upgrade_request(conn.request)) {
            reply_simple(conn, 400, "websocket upgrade not allowed on http route");
            return;
        }
        const auto content_length = header_value(conn.request, "content-length");
        if (!content_length.empty()) {
            const auto parsed = std::strtoull(content_length.c_str(), nullptr, 10);
            if (parsed > kMaxBodyBytes) {
                reply_simple(conn, 413, "body exceeds budget");
                return;
            }
            conn.body_need = static_cast<std::size_t>(parsed);
            conn.body_declared = true;
        }
        if (conn.rx.size() - head_size >= conn.body_need) {
            finish_request(conn, head_size);
        }
        // 体未齐：留在 kRequest 等下个读拍续推。
    }

    void finish_request(HttpConnection& conn, std::size_t head_size) {
        const std::size_t available = conn.rx.size() - head_size;
        // 体恰取声明的 body_need 字节（无声明则无体）：管道残余尾字节不吞进
        // body/raw——渠道按声明体长算的验签哈希，多吞一个字节 = 必败。raw
        // 保留头 + 声明体长的原始帧（原始大小写头只在 raw 面有）。
        const std::size_t body_len = conn.body_declared
            ? std::min(available, conn.body_need)
            : 0;
        conn.request.raw_request.assign(
            conn.rx.begin(), conn.rx.begin() + static_cast<long>(head_size + body_len));
        conn.request.body.assign(
            conn.rx.begin() + static_cast<long>(head_size),
            conn.rx.begin() + static_cast<long>(head_size + body_len));
        conn.rx.clear();
        HttpResponse response;
        try {
            response = http_routes_[conn.request.path](conn.request);
        } catch (const std::exception& e) {
            glog().warning("http handler threw: " + std::string(e.what()));
            reply_simple(conn, 500, "handler error");
            return;
        }
        const auto status = response.status;
        const auto rendered = render_response(status, response.headers, response.body);
        conn.tx.insert(conn.tx.end(), rendered.begin(), rendered.end());
        conn.phase = HttpConnection::Phase::kClosing;
        ObserverEvent event;
        event.kind = ObserverEvent::Kind::kHttpHandled;
        event.connectionId = conn.id;
        event.status = status;
        event.request = conn.request;
        events_.push_back(std::move(event));
    }

    // 预算/路由拒绝与骨架应答共用面（reason 作 body；记 kHttpHandled）。
    void reply_simple(HttpConnection& conn, int status, const char* body) {
        const std::string text(body);
        const std::vector<std::uint8_t> body_bytes(text.begin(), text.end());
        const auto rendered = render_response(status, {}, body_bytes);
        conn.tx.insert(conn.tx.end(), rendered.begin(), rendered.end());
        conn.phase = HttpConnection::Phase::kClosing;
        ObserverEvent event;
        event.kind = ObserverEvent::Kind::kHttpHandled;
        event.connectionId = conn.id;
        event.status = status;
        event.request = conn.request;
        events_.push_back(std::move(event));
    }

    // ws 专用路由生命周期：binary 帧 = 平台侧事件载荷（骨架 = echo 回执，
    // L50 业务接线替换此面）；控制回执自动（ping→pong、close→echo + 断）。
    void feed_ws_bytes(HttpConnection& conn, std::span<const std::uint8_t> bytes) {
        const auto outcome = conn.frames.feed(bytes.data(), bytes.size());
        if (outcome.fatal) {
            drop_connection(conn);
            return;
        }
        if (!outcome.pending_control.empty()) {
            conn.tx.insert(conn.tx.end(), outcome.pending_control.begin(),
                           outcome.pending_control.end());
        }
        if (outcome.peer_closed) {
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kWsClosed;
            event.connectionId = conn.id;
            event.reason = DisconnectReason::NormalClose;
            events_.push_back(std::move(event));
            conn.phase = HttpConnection::Phase::kClosing;
            return;
        }
        for (auto& message : outcome.messages) {
            if (message.opcode != netws::OpCode::kBinary) {
                drop_connection(conn);  // 专用路由生命周期只走 binary
                return;
            }
            ObserverEvent event;
            event.kind = ObserverEvent::Kind::kWsBinary;
            event.connectionId = conn.id;
            event.payload = message.payload;
            events_.push_back(std::move(event));
            conn.netframe.clear();
            netws::append_frame(conn.netframe, netws::OpCode::kBinary, message.payload);
            conn.tx.insert(conn.tx.end(), conn.netframe.begin(), conn.netframe.end());
        }
    }

    void reap_closed() {
        for (auto it = connections_.begin(); it != connections_.end();) {
            if (it->second->phase == HttpConnection::Phase::kClosing &&
                it->second->tx.empty()) {
                it->second->transport->close();
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void drop_connection(HttpConnection& conn) {
        conn.tx.clear();
        conn.phase = HttpConnection::Phase::kClosing;
    }

    void fire_events() {
        std::vector<ObserverEvent> events;
        events.swap(events_);
        for (auto& event : events) {
            if (observer_ == nullptr) {
                break;
            }
            switch (event.kind) {
                case ObserverEvent::Kind::kHttpHandled:
                    observer_->onHttpHandled(event.connectionId, event.request, event.status);
                    break;
                case ObserverEvent::Kind::kWsOpened:
                    observer_->onWsOpened(event.connectionId, event.endpoint);
                    break;
                case ObserverEvent::Kind::kWsBinary:
                    observer_->onWsBinary(event.connectionId,
                                          std::span<const std::uint8_t>(
                                              event.payload.data(), event.payload.size()));
                    break;
                case ObserverEvent::Kind::kWsClosed:
                    observer_->onWsClosed(event.connectionId, event.reason);
                    break;
            }
        }
    }

    HttpIngressConfig config_;
    netlink::TcpListener tcpListener_;
    apollo::net::tls::TlsListener tlsListener_;
    std::unordered_map<ConnectionID, std::unique_ptr<HttpConnection>> connections_;
    std::unordered_map<std::string, HttpHandler> http_routes_;
    std::unordered_set<std::string> ws_routes_;
    std::vector<ObserverEvent> events_;
    ConnectionID nextConnectionId_;
    IHttpIngressObserver* observer_{nullptr};
    bool started_{false};
    bool draining_{false};
};

} // namespace

std::unique_ptr<HttpIngressServer> makeHttpIngressServer(const HttpIngressConfig& config) {
    return std::make_unique<HttpIngressImpl>(config);
}

} // namespace interfaces

#else // _WIN32

namespace interfaces {

std::unique_ptr<HttpIngressServer> makeHttpIngressServer(const HttpIngressConfig& /*config*/) {
    // link TCP/TLS 底座不进 M1 批（POSIX only）——Windows 退回 null 桩。
    struct NullIngress final : HttpIngressServer {
        void start() override {}
        void stop() override {}
        void setHttpRoute(std::string, HttpHandler) override {}
        void setWsRoute(std::string) override {}
        void setObserver(IHttpIngressObserver*) override {}
    };
    return std::make_unique<NullIngress>();
}

} // namespace interfaces

#endif // !_WIN32
