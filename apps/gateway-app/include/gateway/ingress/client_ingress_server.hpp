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

    virtual void send(ConnectionID connectionId, std::span<const std::uint8_t> payload) = 0;
    virtual void close(ConnectionID connectionId, DisconnectReason reason) = 0;

    virtual void setObserver(IClientIngressObserver* observer) = 0;
};

std::unique_ptr<ClientIngressServer> makeNullClientIngressServer();

} // namespace gateway
