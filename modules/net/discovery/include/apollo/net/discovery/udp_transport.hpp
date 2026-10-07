#pragma once

// UDP 传输件（G-1 批 C——库层延伸）。
//
// 批次分界（对批 B「UDP 归进程壳批」口径的收窄）：传输**库件**先行落本层
// （批 C），Machined 进程壳批只做守护主循环接线。本文件只给最小 socket
// 面——无事件循环、无线程，主循环归守护进程壳（与 discovery.hpp 单线程
// 驱动纪律同源）。
//
//   - UdpBeaconTransport：进程侧 BeaconTransport 的 UDP 实现——无连接
//     sendto，每包自带目的端点（discovery.hpp 的目录端点模型）；
//   - UdpFeed：目录侧收包口——bind 到端点（SO_REUSEADDR 支持守护重启），
//     非阻塞 try_receive 供守护主循环轮询，收满即喂 Registry::on_packet。
//
// 口径：端点为 IPv4 点分字面量（骨架期不做主机名解析——解析归部署配置层）；
// 超长/不足 32B 的报文静默丢弃（wire 纪律：单包定长，长度不符即非本协议）。

#include "discovery.hpp"

#include <cstdint>
#include <string>

namespace apollo::net::discovery {

// 变长 datagram 单包上限（send_bytes/变长 try_receive 通用守卫；目录镜像面
// 最大 snapshot part 远低于此）
inline constexpr std::size_t kMaxDatagramSize = 16384;

// Beacon 侧 UDP 传输（sendto 语义：无需 bind，内核分配临时源端口）
class UdpBeaconTransport final : public BeaconTransport {
public:
    UdpBeaconTransport() = default;
    ~UdpBeaconTransport() override;

    UdpBeaconTransport(const UdpBeaconTransport&) = delete;
    UdpBeaconTransport& operator=(const UdpBeaconTransport&) = delete;

    // 打开发送 socket；重复调用幂等（已开即 true）。false = socket 创建失败
    [[nodiscard]] bool open();
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept {
        return handle_ != kInvalidHandle;
    }

    // BeaconTransport：发送一条 wire 报文；false = 未 open / 地址非法 / sendto 失败
    [[nodiscard]] bool send_to(const std::string& host, std::uint16_t port,
                               const std::uint8_t (&buf)[kWireSize]) override;

    // 变长报文发送（G-1 收尾批增量②：目录镜像面 datagram——同 sendto 语义，
    // 长度上限 kMaxDatagramSize）。false = 未 open / 地址非法 / 超限 / 失败
    [[nodiscard]] bool send_bytes(const std::string& host, std::uint16_t port,
                                  const std::uint8_t* data, std::size_t len);

    // 开启 SO_BROADCAST（广播发现层：Query 发往定向广播地址，如
    // 127.255.255.255 / 子网广播）。幂等；false = 未 open / setsockopt 失败
    [[nodiscard]] bool enable_broadcast();

private:
    static constexpr std::intptr_t kInvalidHandle = -1;
    std::intptr_t handle_ = kInvalidHandle;  // POSIX fd / Windows SOCKET（cpp 内转型）
};

// Registry 侧收包口（bind + 非阻塞轮询）
class UdpFeed final {
public:
    UdpFeed() = default;
    ~UdpFeed();

    UdpFeed(const UdpFeed&) = delete;
    UdpFeed& operator=(const UdpFeed&) = delete;

    // 绑定到 bind_host:bind_port（bind_port=0 = 内核分配临时端口，测试用）；
    // SO_REUSEADDR 开启（守护重启同端口）。false = bind 失败
    [[nodiscard]] bool open(std::uint16_t bind_port,
                            const std::string& bind_host = "0.0.0.0");
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept {
        return handle_ != kInvalidHandle;
    }

    // 实际绑定端口（open(0) 后非零；未 open 恒 0）
    [[nodiscard]] std::uint16_t port() const noexcept {
        return bound_port_;
    }

    // 非阻塞取一条报文：返回 false = 暂无数据（EWOULDBLOCK）或长度 ≠ 32B
    // （非本协议报文，静默丢——调用方勿区分，下一轮询再试）
    [[nodiscard]] bool try_receive(std::uint8_t (&buf)[kWireSize]);

    // 同上，并捕获发送方端点（发现层应答路由用：Advertise 回执到
    // Query.service_port@sender_host）
    [[nodiscard]] bool try_receive(std::uint8_t (&buf)[kWireSize],
                                   std::string& sender_host,
                                   std::uint16_t& sender_port);

    // 变长报文收包（G-1 收尾批增量②：目录镜像面 datagram）。cap = 调用方
    // 缓冲容量（须 > 本方协议最大报文——恰等 cap 的报文按疑似截断丢弃，
    // 超长静默丢不污染后续）。命中时 len_out = 实际长度。
    [[nodiscard]] bool try_receive(std::uint8_t* buf, std::size_t cap,
                                   std::size_t& len_out);

    // 同上，并捕获发送方端点
    [[nodiscard]] bool try_receive(std::uint8_t* buf, std::size_t cap,
                                   std::size_t& len_out,
                                   std::string& sender_host,
                                   std::uint16_t& sender_port);

private:
    static constexpr std::intptr_t kInvalidHandle = -1;
    std::intptr_t handle_ = kInvalidHandle;
    std::uint16_t bound_port_ = 0;
};

} // namespace apollo::net::discovery
