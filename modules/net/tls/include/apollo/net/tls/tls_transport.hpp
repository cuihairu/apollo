#pragma once

// TLS 底座（net M1 内核的底座替换件；transport.hpp 缝纪律同源——链路
// 内核不含 socket，底座替换 = 换 ILinkTransport 实现。link 内核保持纯
// std，OpenSSL 只进本目标）。
//
// 三件机制，全部零内部线程（宿主泵驱动）：
//   TlsServerContext  PEM 证书链 + 私钥装载（进程内共享；装载失败给人
//                     可读原因——配置错误不是传输错误）
//   TlsListener       bind/listen（复用 link detail 装配面）+ 非阻塞
//                     accept；accept 出的传输在首次 I/O 前完成 SSL_accept
//   TlsTransport      已连 fd 上的 TLS 记录层（ILinkTransport 实现）：
//                     握手隐式步进——read/write 在握手未完时推进
//                     SSL_accept（WANT_READ/WANT_WRITE → 返回 0，宿主
//                     下个泵周期再来），完成后转 SSL_read/SSL_write
//
// 缝语义严格对齐 transport.hpp：read → >0/0/-1；write → ≥0（可短写：
// SSL_MODE_ENABLE_PARTIAL_WRITE + ACCEPT_MOVING_WRITE_BUFFER，与链路层
// 余量重试相容）/ -1；对端 close_notify → -1（缝里对端关闭与错误同形）。
// 握手带可选截止（steady_clock 内部计时，默认 10s，0 = 不限）：挂死的
// 半开握手不占泵周期资源。fatal（截止/协议错/对端断）一律 -1 且连接
// 立即作废。
//
// M1 交付面：POSIX only（_WIN32 下本 TU 为空，TCP 底座同款纪律）。
// 证书/密钥由部署供给，机制不生成、不内置任何凭据（§5.9 客户端安全：
// 凭据与安全传输是显式部署集成）。

#include "apollo/net/link/transport.hpp"
#include "apollo/net/link/link_manager.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#if !defined(_WIN32)

namespace apollo::net::tls {

class TlsServerContext;

// link LinkManagerConfig.accept_factory 装配面（OpenSSL 不进 link 内核的
// 注入缝——消费者把本工厂装进管理器，passive accept 出的裸 fd 即换 TLS
// 服务端记录层，语义与 TlsListener::accept 完全一致）。工厂接管 fd 所有
// 权（含失败路径的关闭责任）。上下文须活得比工厂与全部产出的传输久
// （工厂捕获其地址——与 TlsListener start 捕获 ctx 同纪律）。
std::function<std::unique_ptr<link::ILinkTransport>(int fd, const link::EndpointConfig&)>
make_tls_accept_factory(TlsServerContext& ctx, std::uint32_t handshake_timeout_ms = 10000);

// 服务端 TLS 上下文：装载一次、监听器共享。头不泄 OpenSSL 类型。
class TlsServerContext {
public:
    TlsServerContext();
    ~TlsServerContext();

    TlsServerContext(const TlsServerContext&) = delete;
    TlsServerContext& operator=(const TlsServerContext&) = delete;

    struct LoadResult {
        bool ok = false;
        std::string error;  // 失败原因（文件缺失/解析失败/钥证不匹配）
    };

    // 装载 PEM 证书链与私钥。失败不动既有装载（幂等重装载语义）。
    LoadResult load(const std::string& cert_path, const std::string& key_path);

    bool loaded() const { return loaded_; }

private:
    friend class TlsListener;
    friend std::function<std::unique_ptr<link::ILinkTransport>(int, const link::EndpointConfig&)>
        make_tls_accept_factory(TlsServerContext&, std::uint32_t);
    void* ctx_ = nullptr;  // SSL_CTX*（构造即建，凭据装载成功才可用）
    bool loaded_ = false;  // 装载完成标志（ctx 存在 ≠ 凭据已装）
};

// TLS 底座监听器：bind/listen 语义同 TcpListener（port 0 = 内核分配，
// start 后 port() 读实际端口）。
class TlsListener {
public:
    TlsListener() = default;
    ~TlsListener();

    TlsListener(const TlsListener&) = delete;
    TlsListener& operator=(const TlsListener&) = delete;

    // 握手截止（毫秒；默认 10000，0 = 不限）。start 前设置生效。
    void setHandshakeTimeout(std::uint32_t ms) { handshake_timeout_ms_ = ms; }

    // 上下文未装载即拒绝（false，监听器保持关闭）。
    bool start(TlsServerContext& ctx, const std::string& host, std::uint16_t port);

    // 非阻塞 accept：暂无 → nullptr；每枚返回的传输自带握手状态。
    std::unique_ptr<link::ILinkTransport> accept();

    std::uint16_t port() const { return port_; }
    void close();
    bool listening() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    std::uint16_t port_ = 0;
    void* ctx_ = nullptr;  // SSL_CTX*（start 时从上下文捕获）
    std::uint32_t handshake_timeout_ms_ = 10000;
};

}  // namespace apollo::net::tls

#endif  // !_WIN32
