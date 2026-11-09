#include "gateway/gateway_server.hpp"
#include "gateway_server_detail.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include <chrono>
#include <span>
#include <string>
#include <vector>

//==============================================================================
// 客户端帧受理面（gateway_server 拆分单元：帧分发/准入受理/Base 激活绑定/
// 下行出口与路由快照解析）
//==============================================================================

namespace gateway {

namespace {

// 库面日志出口（server_detail 单定义，本单元转发——定义在 gateway_server.cpp）
apollo::core::log::Logger& glog() {
    return server_detail::glog();
}

std::string routeLabel(const RouteSnapshot& routeSnapshot) {
    std::ostringstream stream;
    stream << "world=" << routeSnapshot.worldId
           << ", instance=" << routeSnapshot.instanceId
           << ", space=" << routeSnapshot.spaceId
           << ", version=" << routeSnapshot.routeVersion
           << ", url=" << routeSnapshot.worldServerUrl;
    return stream.str();
}
constexpr std::uint32_t kM1RouteTimeoutMs = 500;  // 路由解析同步等待上限
} // namespace

void GatewayServer::handleClientMessage(SessionID sessionId, const std::vector<uint8_t>& message) {
    if (!packetDispatcher_) {
        return;
    }

    const auto dispatch = packetDispatcher_->dispatch(
        std::span<const std::uint8_t>(message.data(), message.size()));

    if (dispatch.kind == ClientPacketKind::Invalid) {
        glog().warning("Gateway dropped malformed client message for session "
                       + std::to_string(sessionId));
        return;
    }

    switch (dispatch.kind) {
        case ClientPacketKind::Heartbeat: {
            sessionManager_->updateHeartbeat(sessionId);

            const auto body = std::vector<uint8_t>(
                message.begin() + sizeof(apollo::protocol::MessageHeader),
                message.end());
            const auto ping = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::Ping>(body);
            apollo::protocol::Pong pong;
            pong.timestamp = ping.timestamp;
            sendToClient(sessionId, apollo::protocol::MessageCodec::encode(pong, sessionId));
            return;
        }

        case ClientPacketKind::Login:
            // 客户端准入（登录串联入口帧）：凭票据换会话绑定。GATEWAY_ASSIGN
            // 是 login-flow 里 gateway 端的唯一客户端准入词汇——LOGIN_REQUEST
            // 归 login 进程，不经 gateway。
            handleClientAuth(sessionId, dispatch, message);
            return;

        case ClientPacketKind::Chat:
            // ChatApp 摘除（P3-4 处方③）：fleet 无 chat 进程，聊天包显式落地
            // 不再挂幽灵后端连接（chat 下游随聊天玩法批重建）
            if (sessionManager_->hasSession(sessionId)) {
                glog().warning("Gateway dropped chat message for session "
                               + std::to_string(sessionId)
                               + " (chat backend not wired)");
            }
            return;

        case ClientPacketKind::World: {
            auto session = sessionManager_->getSession(sessionId);
            if (!session || session->playerId == 0) {
                ++unauthenticatedWorldDrops_;
                glog().warning(
                    "Gateway rejected unauthenticated world message for session "
                    + std::to_string(sessionId));
                return;
            }
            const auto routeSnapshot = ensureRouteSnapshot(sessionId);
            if (!routeSnapshot.isAssigned()) {
                glog().error("Gateway cannot route world message for session "
                             + std::to_string(sessionId));
                return;
            }
            messageRouter_->forwardToWorld(sessionId, routeSnapshot, message);
            return;
        }

        case ClientPacketKind::Base:
            if (auto session = sessionManager_->getSession(sessionId);
                !session || session->playerId == 0) {
                ++unauthenticatedBaseDrops_;
                glog().warning(
                    "Gateway rejected unauthenticated base message for session "
                    + std::to_string(sessionId));
                return;
            }
            messageRouter_->forwardToBaseApp(sessionId, message);
            return;

        case ClientPacketKind::Invalid:
            return;
    }
}

// 客户端准入受理（登录串联入口帧；宿主泵线程调用点）。GATEWAY_ASSIGN_
// REQUEST{playerId, sessionId, loginTicket} → 准入面裁决（login M1 准入或
// 容忍面）→ Base 激活链（面在场时）→ bindPlayer。回执 GATEWAY_ASSIGN_
// RESPONSE 按本会话号寻址下发；准入口径与 authenticateSession 完全一致，
// 本帧只是把进程内调用点换成线缆可见面。
void GatewayServer::handleClientAuth(SessionID sessionId,
                                     const ClientPacketDispatchResult& dispatch,
                                     const std::vector<uint8_t>& message) {
    ++authRequests_;

    const auto session = sessionManager_->getSession(sessionId);
    if (!session) {
        ++authRejected_;
        return;  // 会话已销毁：无处寻址，静默落地
    }
    if (dispatch.messageType != apollo::protocol::MessageType::GATEWAY_ASSIGN_REQUEST) {
        ++authRejected_;
        // LOGIN_REQUEST 等登录族帧归 login 进程；gateway 端不受理（回执
        // success=false 携原因，客户端不得误以为已准入）。
        apollo::protocol::GatewayAssignResponse decline;
        decline.success = false;
        decline.gatewayHost = config_.host;
        decline.gatewayPort = listenPort();
        decline.errorMessage = "gateway accepts GATEWAY_ASSIGN_REQUEST only (login handled by login app)";
        sendToClient(sessionId, apollo::protocol::MessageCodec::encode(decline, sessionId));
        return;
    }
    if (session->playerId != 0) {
        ++authRejected_;
        glog().warning("Gateway auth rejected for session " + std::to_string(sessionId) +
                       " (already bound to player " + std::to_string(session->playerId) + ")");
        apollo::protocol::GatewayAssignResponse decline;
        decline.success = false;
        decline.gatewayHost = config_.host;
        decline.gatewayPort = listenPort();
        decline.errorMessage = "session already authenticated";
        sendToClient(sessionId, apollo::protocol::MessageCodec::encode(decline, sessionId));
        return;
    }

    apollo::protocol::GatewayAssignRequest request;
    try {
        const auto body = std::vector<uint8_t>(
            message.begin() + sizeof(apollo::protocol::MessageHeader), message.end());
        request = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::GatewayAssignRequest>(body);
    } catch (const std::exception&) {
        ++authRejected_;
        glog().warning("Gateway auth dropped malformed assign frame for session "
                       + std::to_string(sessionId));
        apollo::protocol::GatewayAssignResponse decline;
        decline.success = false;
        decline.gatewayHost = config_.host;
        decline.gatewayPort = listenPort();
        decline.errorMessage = "malformed assign request";
        sendToClient(sessionId, apollo::protocol::MessageCodec::encode(decline, sessionId));
        return;
    }

    // 准入裁决（login M1 准入面或容忍面）+ Base 激活链（面在场时）+
    // bindPlayer——口径与 authenticateSession 单点一致。
    const bool accepted = authenticateSession(sessionId, request.playerId, request.loginTicket);

    apollo::protocol::GatewayAssignResponse response;
    response.success = accepted;
    response.gatewayHost = config_.host;
    response.gatewayPort = listenPort();
    response.token = request.loginTicket;
    if (!accepted) {
        ++authRejected_;
        response.errorMessage = "admission rejected";
        glog().warning("Gateway auth rejected for session " + std::to_string(sessionId) +
                       " player " + std::to_string(request.playerId));
    } else {
        ++authAccepted_;
        glog().info("Gateway authenticated session " + std::to_string(sessionId) +
                    " for player " + std::to_string(request.playerId));
    }
    sendToClient(sessionId, apollo::protocol::MessageCodec::encode(response, sessionId));
}

SessionID GatewayServer::createPendingSession(const std::string& clientIP, uint16_t clientPort) {
    return createPendingSession(ClientEndpoint{clientIP, clientPort});
}

SessionID GatewayServer::createPendingSession(const ClientEndpoint& endpoint) {
    const auto connectionId = connectionRegistry_->registerConnection(endpoint);
    const auto sessionId = sessionManager_->createSession(endpoint.address, endpoint.port, connectionId);
    connectionRegistry_->attachSession(connectionId, sessionId);
    return sessionId;
}

ConnectionID GatewayServer::sessionIdToConnectionId(SessionID sessionId) const {
    const auto connection = connectionRegistry_->getConnectionBySession(sessionId);
    return connection ? connection->connectionId : 0;
}

bool GatewayServer::authenticateSession(
    SessionID sessionId,
    PlayerID playerId,
    const std::string& loginTicket
) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) {
        return false;
    }

    if (!admissionService_) {
        return false;
    }

    const auto admission = admissionService_->admit({sessionId, playerId, loginTicket});
    if (!admission.accepted) {
        return false;
    }

    // 登录串联第三环（L53）：Base 经 database 激活——面在场（baseM1Port
    // ≠ 0 且装配成）= 激活/绑定不成即拒绝（无权威确认不放行）；面缺席 =
    // 退容忍（准入过即绑玩家，旧语义）。
    if (baseClient_ && !activateAndBindAtBase(sessionId, playerId)) {
        return false;
    }

    sessionManager_->bindPlayer(sessionId, playerId);
    return true;
}

bool GatewayServer::activateAndBindAtBase(SessionID sessionId, PlayerID playerId) {
    namespace protocol = apollo::protocol;
    const std::chrono::milliseconds kTimeoutMs(500);  // 同步等待上限（准入面同值）

    // ① 激活：database 装载 Anchor（PlayerStore activatePlayer）。
    protocol::PlayerActivateRequest activate;
    activate.playerId = playerId;
    auto reply = baseClient_->call(protocol::MessageCodec::encode(activate, sessionId),
                                   kTimeoutMs);
    if (!reply) {
        glog().warning("Base activation unreachable for player "
                       + std::to_string(playerId));
        return false;
    }
    try {
        if (reply->size() < sizeof(protocol::MessageHeader)) {
            return false;
        }
        const auto header = protocol::MessageCodec::parseHeader(*reply);
        if (static_cast<protocol::MessageType>(header.type) !=
            protocol::MessageType::PLAYER_ACTIVATE_RESPONSE) {
            return false;
        }
        protocol::PlayerActivateResponse parsed;
        const std::vector<std::uint8_t> body(reply->begin() + sizeof(protocol::MessageHeader),
                                             reply->end());
        protocol::MessageCodec::decodeBody(body, parsed);
        if (!parsed.success) {
            glog().warning("Base activation rejected for player "
                           + std::to_string(playerId) + ": " + parsed.errorMessage);
            return false;
        }
    } catch (const std::exception&) {
        return false;  // 畸形回执 = 无权威确认
    }

    // ② 绑定：会话号 + 网关身份写入 SessionBinding（下行通道落点）。
    protocol::PlayerBindSessionRequest bind;
    bind.playerId = playerId;
    bind.sessionId = sessionId;
    bind.gatewayId = config_.gatewayId;
    bind.gatewayAddr = config_.host + ":" + std::to_string(listenPort());
    reply = baseClient_->call(protocol::MessageCodec::encode(bind, sessionId), kTimeoutMs);
    if (!reply) {
        glog().warning("Base session bind unreachable for session "
                       + std::to_string(sessionId));
        return false;
    }
    try {
        if (reply->size() < sizeof(protocol::MessageHeader)) {
            return false;
        }
        const auto header = protocol::MessageCodec::parseHeader(*reply);
        if (static_cast<protocol::MessageType>(header.type) !=
            protocol::MessageType::PLAYER_BIND_SESSION_RESPONSE) {
            return false;
        }
        protocol::PlayerBindSessionResponse parsed;
        const std::vector<std::uint8_t> body(reply->begin() + sizeof(protocol::MessageHeader),
                                             reply->end());
        protocol::MessageCodec::decodeBody(body, parsed);
        if (!parsed.success) {
            glog().warning("Base session bind rejected for session "
                           + std::to_string(sessionId) + ": " + parsed.errorMessage);
            return false;
        }
    } catch (const std::exception&) {
        return false;
    }

    glog().info("Player " + std::to_string(playerId) + " activated at Base, session "
                + std::to_string(sessionId) + " bound");
    return true;
}

void GatewayServer::sendToClient(SessionID sessionId, const std::vector<uint8_t>& message) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session || session->connectionId == 0 || !ingressServer_) {
        glog().info("Gateway sendToClient placeholder for session "
                    + std::to_string(sessionId));
        return;
    }

    ingressServer_->send(session->connectionId, std::span<const std::uint8_t>(message.data(), message.size()));
}

RouteSnapshot GatewayServer::ensureRouteSnapshot(SessionID sessionId) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) {
        return {};
    }

    if (session->routeSnapshot.isAssigned()) {
        return session->routeSnapshot;
    }

    if (const auto resolved = fetchRouteSnapshot(sessionId); resolved.has_value()) {
        sessionManager_->assignRoute(sessionId, *resolved);
        glog().info("Gateway resolved route snapshot for session "
                    + std::to_string(sessionId) + ": " + routeLabel(*resolved));
        return *resolved;
    }

    const auto routeSnapshot = messageRouter_->buildDefaultRoute();
    sessionManager_->assignRoute(sessionId, routeSnapshot);

    glog().info("Gateway assigned route snapshot for session "
                + std::to_string(sessionId) + ": " + routeLabel(routeSnapshot));

    return routeSnapshot;
}

std::optional<RouteSnapshot> GatewayServer::fetchRouteSnapshot(SessionID sessionId) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) {
        return std::nullopt;
    }

    // 路由解析 = manager 控制面 M1 RPC（P4-4 批④接线；批⑤桩路径退役）。
    // 传输缺席/信封残缺 = 路由解析缺席（nullopt → buildDefaultRoute 兜底，
    // 桩时代同缝）。
    if (m1RouteClient_) {
        try {
            apollo::protocol::PlayerResolveRouteRequest request;
            request.playerId = session->playerId;
            request.sessionId = sessionId;
            const auto response = m1RouteClient_->call(
                apollo::protocol::MessageCodec::encode(request, sessionId),
                std::chrono::milliseconds(kM1RouteTimeoutMs));
            if (response) {
                const auto header = apollo::protocol::MessageCodec::parseHeader(*response);
                if (static_cast<apollo::protocol::MessageType>(header.type) ==
                    apollo::protocol::MessageType::PLAYER_RESOLVE_ROUTE_RESPONSE) {
                    apollo::protocol::PlayerResolveRouteResponse parsed;
                    const std::vector<uint8_t> body(
                        response->begin() + sizeof(apollo::protocol::MessageHeader),
                        response->end());
                    apollo::protocol::MessageCodec::decodeBody(body, parsed);
                    if (parsed.success) {
                        RouteSnapshot routeSnapshot;
                        routeSnapshot.worldId = parsed.worldId;
                        routeSnapshot.mapId = parsed.mapId;
                        routeSnapshot.instanceId = parsed.instanceId;
                        routeSnapshot.spaceId = parsed.spaceId;
                        routeSnapshot.routeVersion = parsed.routeVersion;
                        routeSnapshot.worldServerUrl = config_.worldUrl;
                        return routeSnapshot;
                    }
                    return std::nullopt;  // success=false = 路由未备
                }
                // 帧型错位 = 信封残缺 → 兜底
            }
        } catch (const std::exception&) {
            // 响应体残缺 → 兜底
        }
    }
    return std::nullopt;
}

} // namespace gateway
