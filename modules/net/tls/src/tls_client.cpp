#include "apollo/net/tls/tls_client.hpp"

#if !defined(_WIN32)

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace apollo::net::tls {

namespace {

int set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 对端地址：与服务端底座同源的端点捕获（fd 级一次性读数；失败留空，
// 传输语义不受影响）。
void read_peer_endpoint(int fd, std::string& address, std::uint16_t& port) {
    ::sockaddr_storage ss{};
    ::socklen_t slen = sizeof ss;
    if (::getpeername(fd, reinterpret_cast<::sockaddr*>(&ss), &slen) != 0) {
        return;
    }
    char text[INET6_ADDRSTRLEN] = {0};
    if (ss.ss_family == AF_INET) {
        const auto* in4 = reinterpret_cast<const ::sockaddr_in*>(&ss);
        port = ntohs(in4->sin_port);
        if (::inet_ntop(AF_INET, &in4->sin_addr, text, sizeof text) != nullptr) {
            address = text;
        }
    } else if (ss.ss_family == AF_INET6) {
        const auto* in6 = reinterpret_cast<const ::sockaddr_in6*>(&ss);
        port = ntohs(in6->sin6_port);
        if (::inet_ntop(AF_INET6, &in6->sin6_addr, text, sizeof text) != nullptr) {
            address = text;
        }
    }
}

}  // namespace

// ---- TlsClientContext ----

TlsClientContext::TlsClientContext() {
    ctx_ = SSL_CTX_new(TLS_client_method());
    // 验签纪律：默认强校验（链 + 主机名，主机名在传输侧经 SSL_set1_host
    // 绑定）。本地联调显式 setVerifyEnabled(false) 才降级。
    SSL_CTX_set_verify(static_cast<SSL_CTX*>(ctx_), SSL_VERIFY_PEER, nullptr);
}

TlsClientContext::~TlsClientContext() {
    if (ctx_ != nullptr) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

TlsClientContext::LoadResult TlsClientContext::loadCaFile(const std::string& ca_path) {
    LoadResult out;
    // 先装进临时上下文，全部通过再替换既有装载——失败不动已装载凭据。
    SSL_CTX* fresh = SSL_CTX_new(TLS_client_method());
    if (fresh == nullptr) {
        out.error = "tls context unavailable";
        return out;
    }
    SSL_CTX_set_verify(fresh, SSL_VERIFY_PEER, nullptr);
    ERR_clear_error();
    if (SSL_CTX_load_verify_locations(fresh, ca_path.c_str(), nullptr) != 1) {
        out.error = "ca chain load failed: " + ca_path;
        SSL_CTX_free(fresh);
        return out;
    }
    SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
    ctx_ = fresh;
    loaded_ = true;
    out.ok = true;
    return out;
}

// ---- TlsClientTransport ----

TlsClientTransport::TlsClientTransport(int fd, TlsClientContext& ctx,
                                       const std::string& host,
                                       std::uint32_t handshake_timeout_ms)
    : fd_(fd), deadline_(std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(handshake_timeout_ms)),
      has_deadline_(handshake_timeout_ms > 0) {
    set_nonblocking(fd_);
    int one = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    read_peer_endpoint(fd_, peer_address_, peer_port_);
    if (ctx.ctx_ == nullptr) {
        failure_reason_ = "tls client context unavailable";
        ::close(fd_);
        fd_ = -1;
        return;
    }
    ssl_ = SSL_new(static_cast<SSL_CTX*>(ctx.ctx_));
    if (ssl_ == nullptr) {
        failure_reason_ = "tls session allocation failed";
        ::close(fd_);
        fd_ = -1;
        return;
    }
    SSL_set_fd(static_cast<SSL*>(ssl_), fd_);
    // 链路层余量重试语义要求「已发余量重试时换缓冲指针 + 允许部分写」
    // （与服务端底座同款双模式）。
    SSL_set_mode(static_cast<SSL*>(ssl_), SSL_MODE_ENABLE_PARTIAL_WRITE |
                                              SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    if (!ctx.verify_enabled_) {
        // 本地联调显式降级：链与主机名都不校验。
        SSL_set_verify(static_cast<SSL*>(ssl_), SSL_VERIFY_NONE, nullptr);
    } else if (!host.empty()) {
        // 主机名绑定：同时是 SNI 与证书 subject（CN/SAN）校验目标——
        // 不匹配即验签失败（X509_V_ERR_HOSTNAME_MISMATCH）。
        if (SSL_set1_host(static_cast<SSL*>(ssl_), host.c_str()) != 1) {
            failure_reason_ = "hostname bind failed: " + host;
            SSL_free(static_cast<SSL*>(ssl_));
            ssl_ = nullptr;
            ::close(fd_);
            fd_ = -1;
        }
    }
}

TlsClientTransport::~TlsClientTransport() {
    close();
}

int TlsClientTransport::read(std::uint8_t* buf, std::size_t cap) {
    if (ssl_ == nullptr) {
        return -1;
    }
    if (!handshake_done_) {
        if (!step_handshake()) {
            return -1;
        }
        if (!handshake_done_) {
            return 0;  // 握手未完：暂无明文
        }
    }
    ERR_clear_error();  // 只按返回码分流，不消费 OpenSSL 错误队列
    const int n = SSL_read(static_cast<SSL*>(ssl_), buf, static_cast<int>(cap));
    if (n > 0) {
        return n;
    }
    switch (SSL_get_error(static_cast<SSL*>(ssl_), n)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return 0;
        case SSL_ERROR_SYSCALL:
            // EAGAIN 归暂无（个别版本未归入 WANT_READ）；其余归 fatal。
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        default:
            return -1;  // close_notify / 协议错 / 对端断
    }
}

int TlsClientTransport::write(const std::uint8_t* data, std::size_t size) {
    if (ssl_ == nullptr) {
        return -1;
    }
    if (!handshake_done_) {
        if (!step_handshake()) {
            return -1;
        }
        if (!handshake_done_) {
            return 0;  // 握手未完：本批留到下个泵周期
        }
    }
    if (size == 0) {
        return 0;
    }
    ERR_clear_error();
    const int n = SSL_write(static_cast<SSL*>(ssl_), data, static_cast<int>(size));
    if (n > 0) {
        return n;  // 可短写——余量由链路层重试
    }
    return (SSL_get_error(static_cast<SSL*>(ssl_), n) == SSL_ERROR_WANT_READ ||
            SSL_get_error(static_cast<SSL*>(ssl_), n) == SSL_ERROR_WANT_WRITE)
               ? 0
               : -1;
}

void TlsClientTransport::close() {
    if (ssl_ != nullptr) {
        SSL_free(static_cast<SSL*>(ssl_));
        ssl_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// false = fatal（已作废连接）。
bool TlsClientTransport::step_handshake() {
    if (handshake_done_) {
        return true;
    }
    if (has_deadline_ && std::chrono::steady_clock::now() >= deadline_) {
        failure_reason_ = "tls handshake deadline exceeded";
        return fail();
    }
    ERR_clear_error();
    const int rc = SSL_connect(static_cast<SSL*>(ssl_));
    if (rc == 1) {
        handshake_done_ = true;
        verify_result_ = 0;
        return true;
    }
    const int err = SSL_get_error(static_cast<SSL*>(ssl_), rc);
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return true;  // 未 fatal，宿主下个泵周期再步进
        case SSL_ERROR_SYSCALL:
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                return true;
            }
            failure_reason_ = "tls handshake connection lost";
            return fail();
        case SSL_ERROR_SSL:
            // 验签失败走这条：先记 X509 验签码再判死（机制不吞错）。
            verify_result_ =
                static_cast<std::uint32_t>(SSL_get_verify_result(static_cast<SSL*>(ssl_)));
            failure_reason_ = "tls verification failed: " + std::string(
                                  X509_verify_cert_error_string(
                                      static_cast<long>(verify_result_)));
            return fail();
        default:
            failure_reason_ = "tls handshake failed";
            return fail();
    }
}

bool TlsClientTransport::fail() {
    close();
    return false;
}

// ---- link 管理器 dial 装配工厂 ----

std::function<std::unique_ptr<link::ILinkTransport>(int, const link::EndpointConfig&)>
make_tls_dial_factory(TlsClientContext& ctx, std::uint32_t handshake_timeout_ms) {
    return [&ctx, handshake_timeout_ms](
               int fd, const link::EndpointConfig& ep) -> std::unique_ptr<link::ILinkTransport> {
        // 验签目标 = 端点 host（拨号地址即期望身份；见头注 IP 字面量纪律）。
        return std::make_unique<TlsClientTransport>(fd, ctx, ep.host, handshake_timeout_ms);
    };
}

}  // namespace apollo::net::tls

#endif  // !_WIN32
