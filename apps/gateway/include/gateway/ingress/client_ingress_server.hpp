#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace gateway {

using ConnectionID = std::uint64_t;

struct ClientEndpoint {
    std::string address;
    std::uint16_t port{0};
};

// 有界积压（L51，TCP/WS 客户端入口共用）：单连接出站残余字节上限——写不尽
// 且超限 = 对端不读，踢线。2×kMaxPayload：足纳一帧最大载荷在途 + 一帧余量。
inline constexpr std::size_t kMaxTxBacklogBytes = 2 * 1024 * 1024;

// 有界积压（L51）：跨线程投递队列 op 数上限——send 侧生产快过泵消费时顶格
// 丢最旧（帧号在 pump drain 侧才分配，丢 op 不产生序列缝）。与受理队列
// kDefaultCap 同量级。
inline constexpr std::size_t kMaxHandoffOps = 1024;

enum class DisconnectReason {
    NormalClose,
    IdleTimeout,
    AdmissionRejected,
    TransportError,
    // 慢读踢线（L51 有界积压面）：出站积压超限 = 对端不读——断连保内存，
    // 会话保留窗承接重连续传（非 normal 断开，session resume 语义同传输错）。
    SlowConsumer,
};

class IClientIngressObserver {
public:
    virtual ~IClientIngressObserver() = default;

    virtual void onConnectionOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) = 0;
    virtual void onPacketReceived(ConnectionID connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void onConnectionClosed(ConnectionID connectionId, DisconnectReason reason) = 0;
    virtual void onConnectionIdle(ConnectionID connectionId) = 0;
};

class ClientIngressServer {
public:
    virtual ~ClientIngressServer() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    // 宿主泵驱动（net M1 缝纪律同源：机制零内部线程）：accept/收帧/发帧
    // 全在此推进。null 桩空实现——无传输可泵。
    virtual void pump() {}

    // 实际监听端口（port 0 = 内核分配时经此读回；未启动返回 0）。
    virtual std::uint16_t listenPort() const { return 0; }

    // 全部监听端口（装配序；扇入返回各子入口，单入口返回自身）。
    // port 0 = 内核分配时经此读回；未启动的监听器记 0。
    virtual std::vector<std::uint16_t> listenPorts() const { return {listenPort()}; }

    // 排空面（P4 排空批）：停止接受新连接（关监听），既有连接照常服务。
    // 幂等；null 桩空实现。
    virtual void beginDrain() {}

    // 活跃连接数（排空裁决面：非 closing 的在册连接数）。
    virtual std::size_t activeConnections() const { return 0; }

    // 在册连接号快照（排空强停收线面）。
    virtual std::vector<ConnectionID> connectionIds() const { return {}; }

    virtual void send(ConnectionID connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void close(ConnectionID connectionId, DisconnectReason reason) = 0;

    virtual void setObserver(IClientIngressObserver* observer) = 0;
};

std::unique_ptr<ClientIngressServer> makeNullClientIngressServer();

} // namespace gateway
