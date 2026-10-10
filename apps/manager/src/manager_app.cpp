#include "manager/manager_app.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace manager {

ManagerApp::ManagerApp(uint16_t port, std::string host)
    : host_(std::move(host))
    , port_(port) {
    // 目录事件面（P1-2）：事件落日志（可观察）；P3-1 增量②起追加监听链
    // （跨进程镜像 publisher 由进程壳经 set_directory_event_listener 接入）
    directory_.set_event_sink([this](const apollo::game::session::PlayerDirectory::Event& event) {
        static const char* kKindNames[] = {"SessionUp", "SessionDown", "SessionMoved",
                                           "SessionKicked"};
        apollo::core::log::global_log_manager()
            .createLogger("directory")
            ->info(std::string(kKindNames[static_cast<int>(event.kind)])
                   + " player=" + std::to_string(event.player_id) + " epoch="
                   + std::to_string(event.anchor_epoch) + " zone="
                   + std::to_string(event.zone_id) + " reason="
                   + std::to_string(event.reason));
        if (directory_listener_) {
            directory_listener_(event);
        }
    });
}

void ManagerApp::set_directory_event_listener(
    apollo::game::session::PlayerDirectory::EventSink listener) {
    directory_listener_ = std::move(listener);
}

void ManagerApp::set_admission_gate(std::function<bool()> gate) {
    admission_gate_ = std::move(gate);
}

std::size_t ManagerApp::intake_directory_full_report(
    const std::vector<apollo::game::session::MirrorEntry>& sessions) {
    return directory_.intake_full_report(sessions);
}

std::size_t ManagerApp::suspend_zone_sessions(std::uint32_t zone_id,
                                              std::uint64_t now_tick,
                                              std::uint64_t window_ticks) {
    return directory_.mark_suspended_by_zone(zone_id, now_tick, window_ticks);
}

std::size_t ManagerApp::suspend_gateway_sessions(std::uint32_t gateway_id,
                                                 std::uint64_t now_tick,
                                                 std::uint64_t window_ticks) {
    return directory_.mark_suspended_by_gateway(gateway_id, now_tick, window_ticks);
}

std::size_t ManagerApp::sweep_suspended(std::uint64_t now_tick) {
    return directory_.sweep(now_tick);
}

ManagerApp::~ManagerApp() {
    stop();
}

bool ManagerApp::assignWorld(
    PlayerID playerId,
    const apollo::game::session::WorldAssignment& assignment
) {
    if (playerId == 0) {
        return false;
    }
    // 恢复相位排他（P3-1 增量③）：闸门关闭期拒新落点
    if (admission_gate_ && !admission_gate_()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(assignmentsMutex_);
        assignments_[playerId] = assignment;
    }
    // 目录事件面（P1-2）：落点迁移产 SessionMoved；玩家尚无条目时目录
    // 无动作（SessionUp 由 bindSession 登记锚点承担）
    directory_.moved(playerId, assignment);
    return true;
}

bool ManagerApp::clearWorldAssignment(PlayerID playerId) {
    std::lock_guard<std::mutex> lock(assignmentsMutex_);
    return assignments_.erase(playerId) > 0;
}

bool ManagerApp::bindSession(PlayerID playerId, const apollo::game::session::SessionBinding& binding) {
    if (playerId == 0 || binding.session_id == 0) {
        return false;
    }
    // 恢复相位排他（P3-1 增量③）：闸门关闭期拒新会话锚点
    if (admission_gate_ && !admission_gate_()) {
        return false;
    }
    sessionLocator_.bind(playerId, binding);

    // 目录登记锚点（P1-2）：条目级真值 + SessionUp 事件；同账号已有条目
    // 时目录内完成顶号（SessionKicked 事件面）
    apollo::game::session::WorldAssignment assignment;
    {
        std::lock_guard<std::mutex> lock(assignmentsMutex_);
        const auto it = assignments_.find(playerId);
        if (it != assignments_.end()) {
            assignment = it->second;
        }
    }
    directory_.session_up(playerId, binding, assignment);
    return true;
}

bool ManagerApp::unbindSession(protocol::SessionID sessionId) {
    const auto playerId = sessionLocator_.find_player_by_session(sessionId);
    if (!playerId.has_value()) {
        return false;
    }
    sessionLocator_.unbind_session(sessionId);
    sessionLocator_.unbind_player(*playerId);
    directory_.session_down(*playerId);
    return true;
}

bool ManagerApp::reconcileDirectory(const std::vector<PlayerID>& reported_online) const {
    return directory_.reconcile(reported_online);
}

std::optional<PlayerID> ManagerApp::findPlayerBySession(protocol::SessionID sessionId) const {
    return sessionLocator_.find_player_by_session(sessionId);
}

std::optional<apollo::game::session::WorldAssignment> ManagerApp::resolveWorldAssignment(
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

std::optional<apollo::game::session::SessionBinding> ManagerApp::resolveSessionBinding(
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

void ManagerApp::start() {
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

    apollo::core::log::global_log_manager()
        .createLogger("manager")
        ->info("ManagerApp listening on " + host_ + ":" + std::to_string(port_));
}

void ManagerApp::stop() {
    running_ = false;
    if (server_) {
        server_->stop();
    }
    server_.reset();
}

std::vector<uint8_t> ManagerApp::handlePlayerAssignWorldRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> ManagerApp::handlePlayerResolveRouteRequest(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> ManagerApp::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

int64_t ManagerApp::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace manager
