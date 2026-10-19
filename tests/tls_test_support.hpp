#pragma once

// TLS 测试支撑（自签证书生成 + 非阻塞测试客户端）。
//
// 只服务测试面：证书经 OpenSSL API 现场自签落盘（不依赖 openssl CLI 的
// 版本旗标差异，不内置任何仓库凭据——机制层不生成证书，这里生成的是
// 测试夹具）。客户端用 TLS_client_method 且不校验链——证书链/主机名校验
// 是客户端验收（L49），底座测试只证服务端装载与传输语义。

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstddef>
#include <cstdint>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace tls_test_support {

// ---- 自签证书夹具 ----

struct CertPair {
    std::string cert_path;
    std::string key_path;
    std::string error;  // 空 = 成功
};

inline bool write_pem(const std::string& path, int (*writer)(void*, FILE*), void* obj) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = writer(obj, f) == 1;
    std::fclose(f);
    return ok;
}

// 自签 RSA-2048 证书写 PEM 对（CN=cn）。任一步失败即整体失败并带原因。
inline CertPair generate_self_signed(const std::string& dir, const std::string& stem,
                                     const std::string& cn) {
    CertPair out;
    out.cert_path = dir + "/" + stem + ".crt.pem";
    out.key_path = dir + "/" + stem + ".key.pem";

    EVP_PKEY* pkey = nullptr;
    X509* x509 = nullptr;
    auto fail = [&](const char* why) {
        out.error = why;
        EVP_PKEY_free(pkey);
        X509_free(x509);
        return out;
    };

    EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (kctx == nullptr || EVP_PKEY_keygen_init(kctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0 ||
        EVP_PKEY_keygen(kctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        return fail("rsa keygen failed");
    }
    EVP_PKEY_CTX_free(kctx);

    x509 = X509_new();
    if (x509 == nullptr || X509_set_version(x509, 2) != 1 ||
        ASN1_INTEGER_set(X509_get_serialNumber(x509), 1) != 1) {
        return fail("x509 base failed");
    }
    X509_NAME* name = X509_get_subject_name(x509);
    if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                   reinterpret_cast<const unsigned char*>(cn.c_str()), -1,
                                   -1, 0) != 1 ||
        X509_set_issuer_name(x509, name) != 1 ||
        X509_gmtime_adj(X509_getm_notBefore(x509), -60) == nullptr ||
        X509_gmtime_adj(X509_getm_notAfter(x509), 3600) == nullptr ||
        X509_set_pubkey(x509, pkey) != 1 ||
        X509_sign(x509, pkey, EVP_sha256()) == 0) {
        return fail("x509 build/sign failed");
    }

    auto cert_writer = [](void* obj, FILE* f) { return PEM_write_X509(f, static_cast<X509*>(obj)); };
    auto key_writer = [](void* obj, FILE* f) {
        return PEM_write_PrivateKey(f, static_cast<EVP_PKEY*>(obj), nullptr, nullptr, 0, nullptr,
                                    nullptr);
    };
    if (!write_pem(out.cert_path, cert_writer, x509) ||
        !write_pem(out.key_path, key_writer, pkey)) {
        return fail("pem write failed");
    }
    X509_free(x509);
    EVP_PKEY_free(pkey);
    return out;
}

// ---- 非阻塞 TLS 测试客户端 ----
//
// 服务端是宿主泵步进握手（机制零内部线程），客户端若走阻塞
// SSL_connect 会在等服务端航班时死锁——客户端同样按 WANT_READ/
// WANT_WRITE 步进，由测试循环交替推两侧。

class TlsClient {
public:
    ~TlsClient() { close(); }

    void close() {
        if (ssl_ != nullptr) {
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (ctx_ != nullptr) {
            SSL_CTX_free(ctx_);
            ctx_ = nullptr;
        }
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    // 非阻塞 connect + 客户端握手第一步。失败返回 false（error 带原因）。
    bool start(const std::string& host, std::uint16_t port, std::string& error) {
        ::addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        ::addrinfo* res = nullptr;
        if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 ||
            res == nullptr) {
            error = "resolve failed";
            return false;
        }
        fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (fd_ < 0) {
            ::freeaddrinfo(res);
            error = "socket failed";
            return false;
        }
        const int flags = ::fcntl(fd_, F_GETFL, 0);
        ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        if (::connect(fd_, res->ai_addr, res->ai_addrlen) != 0 && errno != EINPROGRESS) {
            ::freeaddrinfo(res);
            error = "connect failed";
            return false;
        }
        ::freeaddrinfo(res);

        ctx_ = SSL_CTX_new(TLS_client_method());
        if (ctx_ == nullptr) {
            error = "ssl ctx failed";
            return false;
        }
        // 默认 SSL_VERIFY_NONE = 自签夹具不校验链（底座/入口测试用）；
        // configureVerify 先于本调用则装信任锚并置 SSL_VERIFY_PEER——verify
        // 模式在 SSL_new 时拷进 SSL 对象，故必须先于 SSL_new 落到 ctx 上。
        SSL_CTX_set_verify(ctx_, verify_configured_ ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                           nullptr);
        if (verify_configured_ &&
            SSL_CTX_load_verify_locations(ctx_, trust_anchor_.c_str(), nullptr) != 1) {
            error = "ca chain load failed: " + trust_anchor_;
            return false;
        }
        ssl_ = SSL_new(ctx_);
        if (ssl_ == nullptr || SSL_set_fd(ssl_, fd_) != 1) {
            error = "ssl set fd failed";
            return false;
        }
        if (!verify_host_.empty() && SSL_set1_host(ssl_, verify_host_.c_str()) != 1) {
            error = "verify host bind failed";
            return false;
        }
        return true;
    }

    // ---- 验签面（L61 WSS 验收）：默认关（自签夹具不校验）；装信任锚即开
    //      SSL_VERIFY_PEER，绑主机名即把 SNI 与 subject/CN/SAN 校验都上。
    //      start() 之后调也有效（SSL_new 尚未发起握手）——本测试里总在
    //      start() 返回后、首个 step() 前设置。----

    // 验签配置：信任锚（PEM，自签证书兼作 CA 亦可）+ 主机名（SNI 与
    // subject/CN/SAN 校验同绑）。须在 start() 之前调用——SSL_new 会把
    // ctx 的 verify 模式拷进 SSL 对象，事后改 ctx 静默不生效（验签码照
    // 常计算但握手仍通过，等于没验）。已 start 则返 false，让调用方当场
    // 看见配错顺序，而不是拿到一条没验签的加密通道。
    bool configureVerify(const std::string& ca_pem_path, const std::string& host) {
        if (ssl_ != nullptr) {
            return false;  // 已握手通道不可重配（见上方注释）
        }
        trust_anchor_ = ca_pem_path;
        verify_host_ = host;
        verify_configured_ = true;  // start() 建 ctx 时装锚并置 SSL_VERIFY_PEER
        return true;
    }

    // X509 验签结论：0 = 通过（或未走到验签阶段）；非 0 = 验签错误码。
    std::uint32_t verifyResult() const {
        return ssl_ != nullptr ? static_cast<std::uint32_t>(SSL_get_verify_result(ssl_)) : 0;
    }

    // 最近一次 OpenSSL 错误串（step() 返回 kFailed 后读，供断言「原因可读」）。
    std::string lastError() const {
        char buf[512];
        const unsigned long code = ERR_get_error();
        if (code == 0) {
            return "";
        }
        ERR_error_string_n(code, buf, sizeof buf);
        return buf;
    }

    enum class Step { kInProgress, kConnected, kFailed };

    // 推进一步连接/握手：connect 未完先看 POLLOUT，随后 SSL_connect。
    Step step() {
        if (fd_ < 0 || ssl_ == nullptr) {
            return Step::kFailed;
        }
        if (!socket_connected_) {
            ::pollfd pfd{};
            pfd.fd = fd_;
            pfd.events = POLLOUT;
            if (::poll(&pfd, 1, 0) <= 0) {
                return Step::kInProgress;
            }
            int so_err = 0;
            ::socklen_t len = sizeof so_err;
            ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_err, &len);
            if (so_err != 0) {
                return Step::kFailed;
            }
            socket_connected_ = true;
        }
        ERR_clear_error();
        const int rc = SSL_connect(ssl_);
        if (rc == 1) {
            return Step::kConnected;
        }
        const int err = SSL_get_error(ssl_, rc);
        return (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) ? Step::kInProgress
                                                                          : Step::kFailed;
    }

    // 已连接后的阻塞式短读（测试单步用；WANT_* 视为 0）。返回 >0 字节数，
    // 0 暂无，-1 EOF/错误。
    int read(std::uint8_t* buf, std::size_t cap) {
        ERR_clear_error();
        const int n = SSL_read(ssl_, buf, static_cast<int>(cap));
        if (n > 0) {
            return n;
        }
        const int err = SSL_get_error(ssl_, n);
        return (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) ? 0 : -1;
    }

    int write(const std::uint8_t* data, std::size_t size) {
        ERR_clear_error();
        const int n = SSL_write(ssl_, data, static_cast<int>(size));
        if (n > 0) {
            return n;
        }
        const int err = SSL_get_error(ssl_, n);
        return (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) ? 0 : -1;
    }

    // 原始 fd 逃生口（垃圾握手负例：不经 SSL 层直接 send 非探测字节）。
    int socket_for_raw_probe() const { return fd_; }

private:
    int fd_ = -1;
    SSL_CTX* ctx_ = nullptr;
    SSL* ssl_ = nullptr;
    bool socket_connected_ = false;
    bool verify_configured_ = false;
    std::string trust_anchor_;
    std::string verify_host_;
};

}  // namespace tls_test_support
