#include "baseappmgr/baseappmgr.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace baseappmgr {

BaseAppMgr::BaseAppMgr(uint16_t port, std::string host)
    : host_(std::move(host))
    , port_(port) {
}

BaseAppMgr::~BaseAppMgr() {
    stop();
}

bool BaseAppMgr::assignWorld(
    PlayerID playerId,
    const apollo::game::session::WorldAssignment& assignment
) {
    if (playerId == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    assignments_[playerId] = assignment;
    return true;
}

bool BaseAppMgr::clearWorldAssignment(PlayerID playerId) {
    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    return assignments_.erase(playerId) > 0;
}

bool BaseAppMgr::bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding) {
    if (playerId == 0 || binding.session_id == 0) {
        return false;
    }
    sessionLocator_.bind(playerId, binding);
    return true;
}

bool BaseAppMgr::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_.find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }
    sessionLocator_.unbind_session(sessionId);
    sessionLocator_.unbind_player(*playerId);
    return true;
}

std::optional<PlayerID> BaseAppMgr::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_.find_player_by_session(sessionId);
}

std::optional<apollo::game::session::WorldAssignment> BaseAppMgr::resolveWorldAssignment(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (playerId == 0 && sessionId != 0) {
        const auto resolved = sessionLocator_.find_player_by_session(sessionId);
        if (!resolved.has_value()) {
            return std::nullopt;
        }
        playerId = *resolved;
    }

    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    const auto it = assignments_.find(playerId);
    if (it == assignments_.end() || !it->second.is_assigned()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<apollo::game::session::SessionBinding> BaseAppMgr::resolveSessionBinding(
    PlayerID playerId,
    protocol::SessionID sessionId
) const {
    if (sessionId != 0) {
        const auto resolved = sessionLocator_.find_player_by_session(sessionId);
        if (resolved.has_value()) {
            const auto binding = sessionLocator_.find_by_player(*resolved);
            if (binding.has_value()) {
                return binding;
            }
        }
    }

    if (playerId == 0) {
        return std::nullopt;
    }

    return sessionLocator_.find_by_player(playerId);
}

void BaseAppMgr::start() {
    if (running_) return;

    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(host_, port_)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        auto header = protocol::MessageCodec::parseHeader(data);
        auto msgType = static_cast<protocol::MessageType>(header.type);

        switch (msgType) {
            case protocol::MessageType::PLAYER_ASSIGN_WORLD_REQUEST:
                return handlePlayerAssignWorldRequest(data);

            case protocol::MessageType::PLAYER_RESOLVE_ROUTE_REQUEST:
                return handlePlayerResolveRouteRequest(data);

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    running_ = true;
    server_->start();

    std::cout << "BaseAppMgr listening on " << host_ << ":" << port_ << std::endl;
}

void BaseAppMgr::stop() {
    running_ = false;
    if (server_) {
        server_->stop();
    }
    server_.reset();
}

std::vector<uint8_t> BaseAppMgr::handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto assignReq = protocol::MessageCodec::decodeBody<protocol::PlayerAssignWorldRequest>(bodyData);

    protocol::PlayerAssignWorldResponse response;
    response.success = false;
    response.routeVersion = assignReq.routeVersion;

    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = assignReq.worldId;
    assignment.map_id = assignReq.mapId;
    assignment.instance_id = assignReq.instanceId;
    assignment.space_id = assignReq.spaceId;
    assignment.route_version = assignReq.routeVersion;

    if (assignWorld(assignReq.playerId, assignment)) {
        response.success = true;
    } else {
        response.errorMessage = "Failed to assign player world";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseAppMgr::handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    const auto resolveReq = protocol::MessageCodec::decodeBody<protocol::PlayerResolveRouteRequest>(bodyData);

    protocol::PlayerResolveRouteResponse response;
    response.success = false;
    response.playerId = resolveReq.playerId;
    response.sessionId = resolveReq.sessionId;

    const auto resolvedPlayerId = resolveReq.playerId != 0
        ? std::optional<PlayerID>(resolveReq.playerId)
        : findPlayerBySession(resolveReq.sessionId);

    if (!resolvedPlayerId.has_value()) {
        response.errorMessage = "Player not found for route resolution";
        return protocol::MessageCodec::encode(response, header.sessionId);
    }

    response.playerId = *resolvedPlayerId;

    const auto binding = resolveSessionBinding(*resolvedPlayerId, resolveReq.sessionId);
    const auto assignment = resolveWorldAssignment(*resolvedPlayerId, resolveReq.sessionId);
    if (!binding.has_value() || !assignment.has_value()) {
        response.errorMessage = "Player route is not ready";
        return protocol::MessageCodec::encode(response, header.sessionId);
    }

    response.success = true;
    response.sessionId = binding->session_id;
    response.gatewayId = binding->gateway_id;
    response.gatewayAddr = binding->gateway_addr;
    response.worldId = assignment->world_id;
    response.mapId = assignment->map_id;
    response.instanceId = assignment->instance_id;
    response.spaceId = assignment->space_id;
    response.routeVersion = assignment->route_version;

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseAppMgr::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

int64_t BaseAppMgr::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace baseappmgr
