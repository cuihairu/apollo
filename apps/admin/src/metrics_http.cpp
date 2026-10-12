#include "admin/metrics_http.hpp"

#if !defined(_WIN32)

#include "apollo/net/link/tcp_transport.hpp"

#include <cstring>
#include <sstream>

namespace apollo::admin {

namespace {

// 读完整响应文本（getline 语义逐行走，无内部状态——响应无长度上限时靠
// Content-Length 一次性断定，实现面不解析 header，读到 EOF 即结束）
constexpr std::size_t kReadChunk = 4096;

}  // namespace

MetricsHttp::MetricsHttp(MetricsHttpConfig config, MetricsHub& hub)
    : config_(std::move(config)), hub_(hub) {}

bool MetricsHttp::start() {
    if (listener_.listening()) {
        return true;
    }
    return listener_.start(config_.host, config_.port);
}

void MetricsHttp::stop() {
    listener_.close();
    conns_.clear();
}

std::uint16_t MetricsHttp::port() const { return listener_.port(); }

std::size_t MetricsHttp::connections() const { return conns_.size(); }

std::uint64_t MetricsHttp::served() const {
    return served_.load(std::memory_order_relaxed);
}

std::uint64_t MetricsHttp::refused() const {
    return refused_.load(std::memory_order_relaxed);
}

std::uint64_t MetricsHttp::aborted() const {
    return aborted_.load(std::memory_order_relaxed);
}

void MetricsHttp::accept_new() {
    for (;;) {
        auto transport = listener_.accept();
        if (transport == nullptr) {
            break;  // 本拍无新连接
        }
        if (conns_.size() >= config_.max_connections) {
            ++refused_;  // 超上限拒收计数（transport 析构即关 fd）
            continue;
        }
        Conn conn;
        conn.transport = std::move(transport);
        conn.deadline_ms = 0;  // 宿主 pump 拍内立刻注时钟
        conns_.push_back(std::move(conn));
    }
}

void MetricsHttp::pump(std::uint64_t now_ms) {
    if (!listener_.listening()) {
        return;
    }
    accept_new();

    for (std::size_t i = 0; i < conns_.size();) {
        Conn& conn = conns_[i];
        if (conn.deadline_ms == 0) {
            conn.deadline_ms = now_ms;  // 入表即注时钟
        }
        if (now_ms - conn.deadline_ms > config_.write_timeout_ms) {
            ++aborted_;  // 超时未收齐/未排空即弃（防慢消费钉住连接槽）
            conn.transport->close();
            conns_.erase(conns_.begin() + i);
            continue;
        }
        if (!conn.queued) {
            if (!read_and_maybe_answer(conn, now_ms)) {
                ++aborted_;  // 读侧 <0 = 对端关闭/错误即断连
                conn.transport->close();
                conns_.erase(conns_.begin() + i);
                continue;
            }
            if (!conn.queued) {
                ++i;  // 请求头未收齐，下拍再来
                continue;
            }
            continue;  // 本拍刚生成响应——下拍推进写出（出站盒按拍推进）
        }
        if (!write_out(conn)) {
            ++aborted_;  // 写出错 = 对端已关
            conn.transport->close();
            conns_.erase(conns_.begin() + i);
            continue;
        }
        if (conn.sent >= conn.response.size()) {
            conn.transport->close();
            ++served_;
            conns_.erase(conns_.begin() + i);
            continue;
        }
        ++i;
    }
}

bool MetricsHttp::read_and_maybe_answer(Conn& conn, std::uint64_t now_ms) {
    std::uint8_t buf[kReadChunk];
    for (;;) {
        const int n = conn.transport->read(buf, sizeof buf);
        if (n < 0) {
            return false;  // 对端关闭/错误（TCP 缝契约）
        }
        if (n == 0) {
            break;  // 本拍无数据（EAGAIN，非断连）
        }
        conn.recv.append(reinterpret_cast<const char*>(buf),
                         static_cast<std::size_t>(n));
    }
    if (conn.queued) {
        return true;
    }
    if (conn.recv.find("\r\n\r\n") == std::string::npos) {
        return true;  // 请求头未收齐
    }
    conn.recv.erase(conn.recv.find("\r\n\r\n") + 4);  // 单响应语义，余量弃
    const std::string body = hub_.render(now_ms);
    std::ostringstream out;
    if (conn.recv.rfind("GET /metrics", 0) == 0) {
        out << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/plain; version=0.0.4\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;
    } else {
        const std::string msg = "Not Found\r\n";
        out << "HTTP/1.1 404 Not Found\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Content-Length: " << msg.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << msg;
    }
    conn.response = out.str();
    conn.queued = true;
    return true;
}

bool MetricsHttp::write_out(Conn& conn) {
    const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(
                                    conn.response.data()) +
                                conn.sent;
    const std::size_t rest = conn.response.size() - conn.sent;
    const int n = conn.transport->write(data, rest);
    if (n < 0) {
        return false;  // 写失败（断管）
    }
    conn.sent += static_cast<std::size_t>(n);  // 短写：余量下拍续推
    return true;
}

}  // namespace apollo::admin

#endif  // !_WIN32
