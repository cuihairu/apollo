#pragma once

// 客户端 TLS 面（§5.9「凭据与安全传输是显式部署集成」的出站侧）。
//
// 与服务端面对称三件机制，全部零内部线程（宿主泵驱动，ILinkTransport
// 缝纪律同源）：
//   TlsClientContext    PEM 信任锚（CA 链）装载；验签开关显式（默认开）
//   TlsClientTransport  已连 fd 上的 TLS 记录层（ILinkTransport 实现）：
//                       SSL_connect 隐式步进（read/write 在握手未完时
//                       推进），完成后转 SSL_read/SSL_write；缝语义与
//                       服务端 TlsTransport 完全一致（read >0/0/-1，
//                       write ≥0 短写 / -1，对端 close_notify → -1）
//
// 验签纪律（服务端面无对应概念，客户端面才有）：链与主机名是受理前提。
//   证书链不被信任锚覆盖 → 握手 fatal，verifyResult() = X509 验签码，
//   不产出任何已连传输；主机名与证书 subject（CN/SAN）不匹配 → 同样
//   fatal。失败原因 failureReason() 给人可读文本，机制不吞错。
//   本地联调可用 setVerifyEnabled(false) 显式关闭（生产不可关——关则
//   既不放行未验证证书也不做主机名绑定，行为等同明文 socket）。
//
// 不做：出站连接状态机（link::TcpConnector 归 link 模块，Tls 包装
// 归调用方在拿到 fd 后接线）；证书钉扎（pinned fingerprint）；客户端
// 私钥/mTLS（服务端未要求客户端证书）。
//
// M1 交付面：POSIX only（_WIN32 下本 TU 为空，与服务端底座同款纪律）。
// 证书由部署供给，机制不生成、不内置任何凭据。

#include "apollo/net/link/transport.hpp"
#include "apollo/net/link/link_manager.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#if !defined(_WIN32)

namespace apollo::net::tls {

class TlsClientContext;

// link LinkManagerConfig.dial_factory 装配面（OpenSSL 不进 link 内核的
// 注入缝——消费者把本工厂装进管理器，active 拨号 TCP 连上后裸 fd 即换
// TLS 客户端记录层，链 + 主机名验签不过则传输当场作废、链路按断连对待
// 重拨）。验签目标 = 端点 host（LinkManagerConfig 拨号地址；IP 字面量直
// 连时须证书含该 IP SAN 才能过，否则部署侧应改用主机名端点）。上下文须
// 活得比工厂与全部产出的传输久（工厂捕获其地址）。
std::function<std::unique_ptr<link::ILinkTransport>(int fd, const link::EndpointConfig&)>
make_tls_dial_factory(TlsClientContext& ctx, std::uint32_t handshake_timeout_ms = 10000);

// 客户端 TLS 上下文：装载一次、多个监听/连接共享。头不泄 OpenSSL 类型。
class TlsClientContext {
public:
    TlsClientContext();
    ~TlsClientContext();

    TlsClientContext(const TlsClientContext&) = delete;
    TlsClientContext& operator=(const TlsClientContext&) = delete;

    struct LoadResult {
        bool ok = false;
        std::string error;  // 失败原因（文件缺失/解析失败）
    };

    // 装载 PEM 信任锚（CA 链，可含多证书）。失败不动既有装载（幂等
    // 重装载语义，与服务端 load 同口径）。
    LoadResult loadCaFile(const std::string& ca_path);

    // 验签开关（默认开）。关闭仅用于本地联调——生产环境不得关闭：
    // 关闭后链与主机名都不校验，传输等同明文。
    void setVerifyEnabled(bool enabled) { verify_enabled_ = enabled; }
    bool verifyEnabled() const { return verify_enabled_; }

    bool loaded() const { return loaded_; }

private:
    friend class TlsClientTransport;
    friend std::function<std::unique_ptr<link::ILinkTransport>(int, const link::EndpointConfig&)>
        make_tls_dial_factory(TlsClientContext&, std::uint32_t);
    void* ctx_ = nullptr;  // SSL_CTX*（构造即建，凭据装载成功才可用）
    bool loaded_ = false;  // 信任锚装载完成标志
    bool verify_enabled_ = true;
};

// 客户端 TLS 记录层（ILinkTransport 缝实现）。fd 归本类所有（析构即关）。
class TlsClientTransport final : public link::ILinkTransport {
public:
    // 接管已连裸 fd（本类置 O_NONBLOCK）。host = 主机名验签目标
    // （同时作 SNI）；空串 = 只验链、不绑定主机名（部署方显式放弃主机
    // 名绑定时才允许）。
    TlsClientTransport(int fd, TlsClientContext& ctx, const std::string& host,
                       std::uint32_t handshake_timeout_ms = 10000);
    ~TlsClientTransport() override;

    int read(std::uint8_t* buf, std::size_t cap) override;
    int write(const std::uint8_t* data, std::size_t size) override;
    void close() override;
    bool closed() const override { return ssl_ == nullptr; }

    std::string peerAddress() const override { return peer_address_; }
    std::uint16_t peerPort() const override { return peer_port_; }

    // 握手是否完成（宿主拨号状态机判断「可收发」的唯一依据）。
    bool handshakeDone() const { return handshake_done_; }

    // 验签结论：0 = 通过或尚未到验签阶段；非 0 = X509 验签错误码
    // （对端自签且不在信任锚 → X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT；
    // CA 签发链缺中间证书 → X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY；
    // 主机名不匹配 → X509_V_ERR_HOSTNAME_MISMATCH）。
    std::uint32_t verifyResult() const { return verify_result_; }
    // 可读失败原因（验签不过或握手 fatal 后非空）。
    std::string failureReason() const { return failure_reason_; }

private:
    // false = fatal（已作废连接）。
    bool step_handshake();
    bool fail();

    int fd_ = -1;
    void* ssl_ = nullptr;  // SSL*
    bool handshake_done_ = false;
    bool has_deadline_ = false;
    std::uint32_t verify_result_ = 0;
    std::string failure_reason_;
    std::chrono::steady_clock::time_point deadline_{};
    std::string peer_address_;
    std::uint16_t peer_port_ = 0;
};

}  // namespace apollo::net::tls

#endif  // !_WIN32
