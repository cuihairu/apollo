#include "gateway/ingress/gateway_connection_registry.hpp"

#include <chrono>

namespace gateway {

GatewayConnectionRegistry::GatewayConnectionRegistry() = default;

ConnectionID GatewayConnectionRegistry::registerConnection(const ClientEndpoint& endpoint) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto connectionId = nextConnectionId_++;
    auto connection = std::make_shared<GatewayConnection>();
    connection->connectionId = connectionId;
    connection->remoteEndpoint = endpoint;
    connection->state = GatewayConnectionState::Open;
    connection->openedAtMs = currentTimeMs();
    connection->lastSeenAtMs = connection->openedAtMs;

    connections_[connectionId] = connection;
    return connectionId;
}

bool GatewayConnectionRegistry::attachSession(ConnectionID connectionId, SessionID sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    if (it->second->sessionId != 0) {
        sessionToConnection_.erase(it->second->sessionId);
    }

    it->second->sessionId = sessionId;
    it->second->lastSeenAtMs = currentTimeMs();
    sessionToConnection_[sessionId] = connectionId;
    return true;
}

bool GatewayConnectionRegistry::touch(ConnectionID connectionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    it->second->lastSeenAtMs = currentTimeMs();
    if (it->second->state != GatewayConnectionState::Closed) {
        it->second->state = GatewayConnectionState::Open;
    }
    return true;
}

bool GatewayConnectionRegistry::markIdle(ConnectionID connectionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    it->second->state = GatewayConnectionState::Idle;
    it->second->lastSeenAtMs = currentTimeMs();
    return true;
}

bool GatewayConnectionRegistry::markClosed(ConnectionID connectionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    it->second->state = GatewayConnectionState::Closed;
    it->second->lastSeenAtMs = currentTimeMs();
    return true;
}

bool GatewayConnectionRegistry::removeConnection(ConnectionID connectionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    if (it == connections_.end()) {
        return false;
    }

    if (it->second->sessionId != 0) {
        sessionToConnection_.erase(it->second->sessionId);
    }

    connections_.erase(it);
    return true;
}

std::shared_ptr<GatewayConnection> GatewayConnectionRegistry::getConnection(ConnectionID connectionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = connections_.find(connectionId);
    return it != connections_.end() ? it->second : nullptr;
}

std::shared_ptr<GatewayConnection> GatewayConnectionRegistry::getConnectionBySession(SessionID sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto sessionIt = sessionToConnection_.find(sessionId);
    if (sessionIt == sessionToConnection_.end()) {
        return nullptr;
    }

    const auto connectionIt = connections_.find(sessionIt->second);
    return connectionIt != connections_.end() ? connectionIt->second : nullptr;
}

std::optional<ClientEndpoint> GatewayConnectionRegistry::findEndpointBySession(SessionID sessionId) const {
    const auto connection = getConnectionBySession(sessionId);
    if (!connection) {
        return std::nullopt;
    }
    return connection->remoteEndpoint;
}

std::vector<std::shared_ptr<GatewayConnection>> GatewayConnectionRegistry::getAllConnections() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<GatewayConnection>> result;
    result.reserve(connections_.size());
    for (const auto& [_, connection] : connections_) {
        result.push_back(connection);
    }
    return result;
}

std::size_t GatewayConnectionRegistry::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_.size();
}

std::int64_t GatewayConnectionRegistry::currentTimeMs() const {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

} // namespace gateway
