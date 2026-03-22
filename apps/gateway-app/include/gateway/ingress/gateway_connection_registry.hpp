#pragma once

#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/session_manager.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace gateway {

enum class GatewayConnectionState {
    Open,
    Idle,
    Closed,
};

struct GatewayConnection {
    ConnectionID connectionId{0};
    ClientEndpoint remoteEndpoint;
    GatewayConnectionState state{GatewayConnectionState::Open};
    SessionID sessionId{0};
    std::int64_t openedAtMs{0};
    std::int64_t lastSeenAtMs{0};
};

class GatewayConnectionRegistry {
public:
    GatewayConnectionRegistry();
    ~GatewayConnectionRegistry() = default;

    ConnectionID registerConnection(const ClientEndpoint& endpoint);
    bool attachSession(ConnectionID connectionId, SessionID sessionId);
    bool touch(ConnectionID connectionId);
    bool markIdle(ConnectionID connectionId);
    bool markClosed(ConnectionID connectionId);
    bool removeConnection(ConnectionID connectionId);

    std::shared_ptr<GatewayConnection> getConnection(ConnectionID connectionId) const;
    std::shared_ptr<GatewayConnection> getConnectionBySession(SessionID sessionId) const;
    std::optional<ClientEndpoint> findEndpointBySession(SessionID sessionId) const;
    std::vector<std::shared_ptr<GatewayConnection>> getAllConnections() const;
    std::size_t getConnectionCount() const;

private:
    std::int64_t currentTimeMs() const;

    mutable std::mutex mutex_;
    std::unordered_map<ConnectionID, std::shared_ptr<GatewayConnection>> connections_;
    std::unordered_map<SessionID, ConnectionID> sessionToConnection_;
    std::atomic<ConnectionID> nextConnectionId_{1};
};

} // namespace gateway
