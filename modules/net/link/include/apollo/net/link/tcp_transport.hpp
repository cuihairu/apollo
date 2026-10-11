#pragma once

// TCP 底座（net M1 内核 · 增量 5；net-abstraction §5.7 连接器——Mercury
// TCPConnectionOpener 蓝本）。
//
// 三件机制，全部零内部线程（宿主泵驱动，transport.hpp 缝纪律同源）：
//   TcpTransport  已连 fd 的非阻塞字节流（ILinkTransport 实现）
//   TcpConnector  出站连接状态机：非阻塞 connect + POLLOUT + 一次性超时；
//                 失败按 errno 分类成类型化原因，机制不带重试
//                 （ECONNREFUSED/ENETUNREACH/EHOSTUNREACH → kNoSuchPort、
//                 超时 → kTimedOut；重试归 Link 管理器——策略不在机制里）
//   TcpListener   入站接受：bind/listen + 非阻塞 accept（M1 接受面无
//                 鉴权前移——鉴权在握手层 route_version/epoch 校验）
//
// M1 交付面：POSIX only（_WIN32 下本 TU 为空——Windows 底座不进 M1 内核批）。
// 不做：证书/TLS（§5.9 归客户端段）、SO_REUSEPORT 多进程分流量、
// IPv6 双栈显式用例（getaddrinfo 取首结果，本机测试全走 127.0.0.1）。

#include "apollo/net/link/transport.hpp"

#include <cstdint>
#include <memory>
#include <string>

#if !defined(_WIN32)

namespace apollo::net::link {

// 已连 TCP fd 的缝实现。fd 归本类所有（析构即关）。
class TcpTransport : public ILinkTransport {
public:
    // 接管裸 fd（须为已连 socket；本类置 O_NONBLOCK）。
    explicit TcpTransport(int fd);
    ~TcpTransport() override;

    int read(std::uint8_t* buf, std::size_t cap) override;
    int write(const std::uint8_t* data, std::size_t size) override;
    void close() override;
    bool closed() const override { return fd_ < 0; }

    // 对端地址（构造时 getpeername 一次性捕获；入站接受面据此生成
    // 连接端点——fd 级读数不随生命周期变，取不到时为空串/0）。
    std::string peerAddress() const override { return peer_address_; }
    std::uint16_t peerPort() const override { return peer_port_; }

private:
    int fd_;
    std::string peer_address_;
    std::uint16_t peer_port_ = 0;
};

// 连接失败分类（§5.7：ECONNREFUSED/ENETUNREACH → NO_SUCH_PORT、
// ETIMEDOUT → TIMER_EXPIRED 的 apollo 命名；类型化原因，不代掷猜测）。
enum class ConnectFailure {
    kNoSuchPort,   // 对端端口无人听 / 路由不可达
    kTimedOut,     // 连接超时（一次性定时器到期）
    kSystemError,  // 其余 errno / 名字解析失败
};

enum class ConnectState { kIdle, kConnecting, kConnected, kFailed };

// 出站连接状态机（Mercury TCPConnectionOpener 形态：机制不重试——失败即
// 类型化回调，重连策略归 Link 管理器）。用法：
//   start(host, port, timeout_ms, now_ms) → pump(now_ms) 循环直至
//   kConnected（take_transport 取走）/ kFailed（failure() 看原因）。
class TcpConnector {
public:
    TcpConnector() = default;
    ~TcpConnector();

    TcpConnector(const TcpConnector&) = delete;
    TcpConnector& operator=(const TcpConnector&) = delete;

    bool start(const std::string& host, std::uint16_t port, std::uint32_t timeout_ms,
               std::uint64_t now_ms);
    void cancel();

    enum class PumpResult { kInProgress, kConnected, kFailed };
    PumpResult pump(std::uint64_t now_ms);

    std::unique_ptr<ILinkTransport> take_transport();
    ConnectFailure failure() const { return failure_; }
    ConnectState state() const { return state_; }

private:
    void classify_failure(int err);

    int fd_ = -1;
    ConnectState state_ = ConnectState::kIdle;
    ConnectFailure failure_ = ConnectFailure::kSystemError;
    std::uint64_t deadline_ms_ = 0;
};

// 入站接受面。port 0 = 内核分配（start 后 port() 读实际端口——测试用）。
class TcpListener {
public:
    TcpListener() = default;
    ~TcpListener();

    TcpListener(const TcpListener&) = delete;
    TcpListener& operator=(const TcpListener&) = delete;

    bool start(const std::string& host, std::uint16_t port);
    // 非阻塞 accept：暂无 → nullptr（accept 面不区分错误细节——M1 内核
    // 正确性优先，连接级错误在握手/收发面显形）。
    std::unique_ptr<ILinkTransport> accept();
    std::uint16_t port() const { return port_; }
    void close();
    bool listening() const { return fd_ >= 0; }

private:
    int fd_ = -1;
    std::uint16_t port_ = 0;
};

}  // namespace apollo::net::link

#endif  // !_WIN32
