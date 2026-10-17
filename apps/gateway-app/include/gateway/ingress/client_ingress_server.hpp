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

enum class DisconnectReason {
    NormalClose,
    IdleTimeout,
    AdmissionRejected,
    TransportError,
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
