#include "apollo/net/tls/tls_transport.hpp"

#if !defined(_WIN32)

#include "apollo/net/link/tcp_transport.hpp"

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <arpa/inet.h>
#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>

namespace apollo::net::tls {

namespace {

int set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 对端地址：与 tcp_transport.cpp 同源的端点捕获（fd 级一次性读数；
// 失败留空，传输语义不受影响）。
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

// 握手态传输：read/write 两个方向都在推进 SSL_accept（缝无握手专用
// 入口，宿主泵只能经这两条缝步进）。
class TlsTransport final : public link::ILinkTransport {
public:
    TlsTransport(int fd, SSL_CTX* ctx, std::uint32_t handshake_timeout_ms)
        : fd_(fd), deadline_(std::chrono::steady_clock::now() +
                             std::chrono::milliseconds(handshake_timeout_ms)),
          has_deadline_(handshake_timeout_ms > 0) {
        set_nonblocking(fd_);
        int one = 1;
        ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
        read_peer_endpoint(fd_, peer_address_, peer_port_);
        ssl_ = SSL_new(ctx);
        if (ssl_ != nullptr) {
            SSL_set_fd(ssl_, fd_);
            // 链路层余量重试语义要求「已发余量重试时换缓冲指针 + 允许
            // 部分写」——两个模式都开，否则 OpenSSL 报 bad write retry。
            SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
                                   SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        }
        if (ssl_ == nullptr) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    ~TlsTransport() override { close(); }

    int read(std::uint8_t* buf, std::size_t cap) override {
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
        const int n = SSL_read(ssl_, buf, static_cast<int>(cap));
        if (n > 0) {
            return n;
        }
        switch (SSL_get_error(ssl_, n)) {
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

    int write(const std::uint8_t* data, std::size_t size) override {
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
        const int n = SSL_write(ssl_, data, static_cast<int>(size));
        if (n > 0) {
            return n;  // 可短写——余量由链路层重试
        }
        return (SSL_get_error(ssl_, n) == SSL_ERROR_WANT_READ ||
                SSL_get_error(ssl_, n) == SSL_ERROR_WANT_WRITE)
                   ? 0
                   : -1;
    }

    void close() override {
        if (ssl_ != nullptr) {
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    bool closed() const override { return ssl_ == nullptr; }

    std::string peerAddress() const override { return peer_address_; }
    std::uint16_t peerPort() const override { return peer_port_; }

private:
    // false = fatal（已作废连接）。
    bool step_handshake() {
        if (handshake_done_) {
            return true;
        }
        if (has_deadline_ && std::chrono::steady_clock::now() >= deadline_) {
            return fail();
        }
        ERR_clear_error();
        const int rc = SSL_accept(ssl_);
        if (rc == 1) {
            handshake_done_ = true;
            return true;
        }
        const int err = SSL_get_error(ssl_, rc);
        switch (err) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
                return true;  // 未 fatal，宿主下个泵周期再步进
            case SSL_ERROR_SYSCALL:
                return (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                           ? true
                           : fail();
            default:
                return fail();  // 协议错/对端断：不重试，断连记账归上层
        }
    }

    bool fail() {
        close();
        return false;
    }

    int fd_ = -1;
    SSL* ssl_ = nullptr;
    bool handshake_done_ = false;
    bool has_deadline_ = false;
    std::chrono::steady_clock::time_point deadline_{};
    std::string peer_address_;
    std::uint16_t peer_port_ = 0;
};

}  // namespace

// ---- TlsServerContext ----

TlsServerContext::TlsServerContext() {
    ctx_ = SSL_CTX_new(TLS_server_method());
}

TlsServerContext::~TlsServerContext() {
    if (ctx_ != nullptr) {
        SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
        ctx_ = nullptr;
    }
}

TlsServerContext::LoadResult TlsServerContext::load(const std::string& cert_path,
                                                    const std::string& key_path) {
    LoadResult out;
    // 先装进临时上下文，全部通过再替换既有装载——失败不动已装载凭据。
    SSL_CTX* fresh = SSL_CTX_new(TLS_server_method());
    if (fresh == nullptr) {
        out.error = "tls context unavailable";
        return out;
    }
    ERR_clear_error();
    if (SSL_CTX_use_certificate_chain_file(fresh, cert_path.c_str()) != 1) {
        out.error = "certificate chain load failed: " + cert_path;
        SSL_CTX_free(fresh);
        return out;
    }
    ERR_clear_error();
    if (SSL_CTX_use_PrivateKey_file(fresh, key_path.c_str(), SSL_FILETYPE_PEM) != 1) {
        out.error = "private key load failed: " + key_path;
        SSL_CTX_free(fresh);
        return out;
    }
    ERR_clear_error();
    // 钥证匹配单独校验：装载成功但不匹配是静默配置错误，必须当场判死。
    if (SSL_CTX_check_private_key(fresh) != 1) {
        out.error = "certificate and private key do not match";
        SSL_CTX_free(fresh);
        return out;
    }
    SSL_CTX_free(static_cast<SSL_CTX*>(ctx_));
    ctx_ = fresh;
    loaded_ = true;
    out.ok = true;
    return out;
}

// ---- TlsListener ----

TlsListener::~TlsListener() {
    close();
}

bool TlsListener::start(TlsServerContext& ctx, const std::string& host, std::uint16_t port) {
    close();
    if (!ctx.loaded()) {
        return false;
    }
    if (!link::detail::listen_socket(host, port, fd_, port_)) {
        close();
        return false;
    }
    ctx_ = ctx.ctx_;
    return true;
}

std::unique_ptr<link::ILinkTransport> TlsListener::accept() {
    if (fd_ < 0 || ctx_ == nullptr) {
        return nullptr;
    }
    const int fd = link::detail::accept_one(fd_);
    if (fd < 0) {
        return nullptr;
    }
    return std::make_unique<TlsTransport>(fd, static_cast<SSL_CTX*>(ctx_),
                                          handshake_timeout_ms_);
}

void TlsListener::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    port_ = 0;
}

}  // namespace apollo::net::tls

#endif  // !_WIN32
