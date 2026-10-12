#include "gateway/gateway_server.hpp"
#include "gateway/ingress/tcp_client_ingress_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include <chrono>
#include <iostream>
#include <sstream>

namespace gateway {

namespace {

// 库面日志出口：cat=gateway_server（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& glog() {
    return *apollo::core::log::global_log_manager().createLogger("gateway_server");
}

std::vector<uint8_t> encodeDisconnectMessage(SessionID sessionId, PlayerID playerId, bool normalClose) {
    apollo::protocol::GatewayClientDisconnect message;
    message.sessionId = sessionId;
    message.playerId = playerId;
    message.normalClose = normalClose;
    return apollo::protocol::MessageCodec::encode(message);
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

//==============================================================================
// MessageRouter 实现
//==============================================================================

MessageRouter::MessageRouter(const GatewayConfig& config)
    : config_(config) {
}

MessageRouter::~MessageRouter() {
    stop();
}

void MessageRouter::start() {
    if (running_) return;

    // cell 面换装 M1 出口（2b-iii）：路由表缺位 = 出口降级（警告 + 空件，
    // world 前递落地），起进程不拦——处方①同口径。cell 不在线不在此刻判断，
    // active 端点自带退避重连。
    M1EgressConfig egressConfig;
    egressConfig.host = config_.cellM1Host;
    egressConfig.port = config_.cellM1Port;
    egressConfig.route_json = config_.cellM1RouteJson;
    m1Egress_ = makeM1Egress(std::move(egressConfig));
    if (!m1Egress_->start()) {
        glog().warning("gateway cell M1 egress degraded: route table unavailable ("
                       + config_.cellM1RouteJson + "), world forwarding disabled");
        m1Egress_.reset();
    }

    running_ = true;
}

void MessageRouter::stop() {
    running_ = false;
    if (m1Egress_) {
        m1Egress_->stop();
        m1Egress_.reset();
    }
}

void MessageRouter::pump() {
    if (!m1Egress_) {
        return;
    }
    m1Egress_->pump(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch())
                        .count());
}

void MessageRouter::forwardToWorld(
    SessionID sessionId,
    const RouteSnapshot& routeSnapshot,
    const std::vector<uint8_t>& message
) {
    (void)routeSnapshot;  // M1 单 cell 端点：路由快照仅存档，选择面随多 cell 批回归
    if (!m1Egress_) {
        glog().warning("Gateway dropped world message for session "
                       + std::to_string(sessionId) + " (M1 egress degraded)");
        return;
    }
    if (!m1Egress_->forwardIntent(sessionId, message)) {
        // 不可前递（非五族意图——如断连通知，cell 感知面随契约后续批收口）
        glog().info("Gateway dropped unforwardable world message for session "
                    + std::to_string(sessionId));
    }
}

void MessageRouter::forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message) {
    // nng 双 Channel 摘除（P4-4 批②，ADR-011 ChatApp 同款）：桩上死面，
    // 从未有帧过线；base 面的真实路由 = zone M1 受理面（挂账 P4-4 ③，
    // 随 login↔base 链整批立项）。落地面 = m1Egress degraded 同口径。
    (void)message;
    glog().info("Gateway dropped base message for session "
                + std::to_string(sessionId) + " (no base forwarding face)");
}

RouteSnapshot MessageRouter::buildDefaultRoute() const {
    RouteSnapshot routeSnapshot;
    routeSnapshot.worldId = 1;
    routeSnapshot.mapId = 1;
    routeSnapshot.instanceId = 1;
    routeSnapshot.spaceId = 1;
    routeSnapshot.routeVersion = 1;
    // 路由名目留存（路由解析面与快照消费方仍按 url 标认 world 落点）；
    // M1 出口单端点直连，不据其选择——选择面随多 cell 批回归。
    routeSnapshot.worldServerUrl = config_.cellAppUrl;
    return routeSnapshot;
}

//==============================================================================
// GatewayServer 实现
//==============================================================================

namespace {

// 准入传输选装（P4-4 批①）：loginM1Port 非零 = link M1 面（拨 login 准入
// 受理面）；装配缺位降级（处方①）= 退化容忍面，起进程不拦。
std::unique_ptr<SessionAdmissionService> makeAdmissionService(const GatewayConfig& config) {
    if (config.loginM1Port == 0) {
        return makeDefaultSessionAdmissionService();
    }
    auto linkService = makeLinkSessionAdmissionService(
        config.loginM1Host, config.loginM1Port, config.cellM1RouteJson);
    if (!linkService) {
        glog().warning("gateway login M1 admission degraded: route table unavailable ("
                       + config.cellM1RouteJson + "), admission degrades tolerant");
        return makeDefaultSessionAdmissionService();
    }
    return linkService;
}

} // namespace

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , sessionManager_(std::make_unique<SessionManager>())
    , ingressServer_(makeTcpClientIngressServer(
          config_.host, config_.port, static_cast<std::size_t>(config_.maxConnections)))
    , packetDispatcher_(std::make_unique<ClientPacketDispatcher>())
    , connectionRegistry_(std::make_unique<GatewayConnectionRegistry>())
    , admissionService_(makeAdmissionService(config))
    , messageRouter_(std::make_unique<MessageRouter>(config)) {
}

GatewayServer::~GatewayServer() {
    stop();
}

void GatewayServer::start() {
    if (running_) return;

    // 启动消息路由器
    messageRouter_->start();
    ingressServer_->setObserver(this);
    // 监听 socket 归 ingress 真入口（link TcpListener）——bind 失败即 throw，
    // 与旧裸 socket 块同契约。
    ingressServer_->start();

    // M1 路由解析客户端面（P4-4 批④）：baseAppMgrM1Port 非零 = 拨 manager
    // 控制面；装配失败降级（警告 + 不拦启动），路由解析缺席 → 默认路由兜底。
    if (config_.baseAppMgrM1Port != 0) {
        M1RouteClientConfig routeConfig;
        routeConfig.host = config_.baseAppMgrM1Host;
        routeConfig.port = config_.baseAppMgrM1Port;
        routeConfig.route_json = config_.cellM1RouteJson;
        m1RouteClient_ = makeM1RouteClient(std::move(routeConfig));
        if (m1RouteClient_) {
            glog().info("Gateway M1 route client dialing " + config_.baseAppMgrM1Host
                        + ":" + std::to_string(config_.baseAppMgrM1Port));
        } else {
            glog().warning("gateway M1 route client degraded: route table unavailable ("
                           + config_.cellM1RouteJson + "), default route stays");
        }
    }

    running_ = true;

    acceptThread_ = std::thread(&GatewayServer::acceptNewConnections, this);

    // 启动心跳检查线程
    heartbeatThread_ = std::thread(&GatewayServer::heartbeatCheckLoop, this);

    glog().info("Gateway server listening on " + config_.host + ":"
                + std::to_string(config_.port));
}

void GatewayServer::stop() {
    running_ = false;

    // 心跳线程先收线：其超时路径经 send/close 投递队列与入口交互，且
    // onClientDisconnect 会触 forwardToWorld/forwardToBaseApp——路由器
    // （M1 出口/后端通道）与入口都得等它停驱后再拆。
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    // 泵线程停驱后才拆路由器与入口（TcpClientIngressServer::stop 契约：
    // 连接表泵线程独占，跨线程并发拆表不在契约内；M1 出口链路面同纪律，
    // forwardToWorld 的调用方全在上述两线程内）。
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    messageRouter_->stop();
    if (m1RouteClient_) {
        m1RouteClient_->stop();
        m1RouteClient_.reset();
    }
    if (ingressServer_) {
        ingressServer_->stop();
    }

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

    // 准入面最后收口（全部调用线程已 join——in-flight admit 先于泵停驱）。
    if (admissionService_) {
        admissionService_->shutdown();
    }
}

void GatewayServer::onConnectionOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) {
    // 以 ingress 分配的连接号登记（只 attach 不 register 是桩时代死代码
    // 的暗坑——首包即被 registry 查空落地）。
    connectionRegistry_->registerConnection(connectionId, endpoint);
    const auto sessionId = sessionManager_->createSession(endpoint.address, endpoint.port, connectionId);
    connectionRegistry_->attachSession(connectionId, sessionId);
}

void GatewayServer::onPacketReceived(ConnectionID connectionId, std::span<const std::uint8_t> payload) {
    connectionRegistry_->touch(connectionId);

    const auto connection = connectionRegistry_->getConnection(connectionId);
    if (!connection || connection->sessionId == 0) {
        return;
    }

    handleClientMessage(
        connection->sessionId,
        std::vector<std::uint8_t>(payload.begin(), payload.end())
    );
}

void GatewayServer::onConnectionClosed(ConnectionID connectionId, DisconnectReason reason) {
    const auto connection = connectionRegistry_->getConnection(connectionId);
    if (!connection) {
        return;
    }

    if (connection->sessionId != 0) {
        onClientDisconnect(connection->sessionId, reason == DisconnectReason::NormalClose);
        return;
    }

    connectionRegistry_->markClosed(connectionId);
    connectionRegistry_->removeConnection(connectionId);
}

void GatewayServer::onConnectionIdle(ConnectionID connectionId) {
    connectionRegistry_->markIdle(connectionId);
}

void GatewayServer::heartbeatCheckLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatCheckIntervalMs));

        // 检查超时会话
        auto timeoutSessions = sessionManager_->checkTimeouts(config_.sessionTimeoutMs);

        for (auto sessionId : timeoutSessions) {
            glog().info("Session " + std::to_string(sessionId)
                        + " timeout, disconnecting...");
            onClientDisconnect(sessionId, false);
        }
    }
}

void GatewayServer::acceptNewConnections() {
    // 宿主泵循环（net M1 缝纪律：机制零内部线程）：accept/收帧/发帧/M1 出口
    // 推进全在此线程过拍，经 ingress 投递队列与 send/close 跨线程面解耦。
    while (running_) {
        ingressServer_->pump();
        messageRouter_->pump();
        recordMetrics();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void GatewayServer::recordMetrics() {
    auto& registry = apollo::core::metrics::MetricRegistry::instance();
    registry.gauge("gateway_sessions")
        .store(static_cast<std::int64_t>(sessionManager_->getSessionCount()));
    if (auto* egress = messageRouter_->egress(); egress != nullptr) {
        // pending 关联账面仅限宿主泵线程读（m1_egress 线程纪律）——本拍合法
        registry.gauge("gateway_pending_acks")
            .store(static_cast<std::int64_t>(egress->pendingAcks()));
        // 出口计数为进程期只增原子——增量推进（Prometheus rate() 消费面）
        counter_advance_to(gatewayIntentsSentTotal_, lastIntentsSent_,
                           egress->sentIntents());
        counter_advance_to(gatewayIntentsAckedTotal_, lastIntentsAcked_,
                           egress->ackedIntents());
        counter_advance_to(gatewayIntentsFailedTotal_, lastIntentsFailed_,
                           egress->failedAcks());
        counter_advance_to(gatewayIntentsTimedOutTotal_, lastIntentsTimedOut_,
                           egress->timedOutIntents());
        counter_advance_to(gatewayIntentsUnforwardableTotal_, lastIntentsUnforwardable_,
                           egress->unforwardableFrames());
        counter_advance_to(gatewayIntentsDroppedTotal_, lastIntentsDropped_,
                           egress->droppedSends());
    }
}

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
            messageRouter_->forwardToBaseApp(sessionId, message);
            return;

        case ClientPacketKind::Invalid:
            return;
    }
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

    sessionManager_->bindPlayer(sessionId, playerId);
    return true;
}

std::optional<ClientEndpoint> GatewayServer::findClientEndpoint(SessionID sessionId) const {
    if (!connectionRegistry_) {
        return std::nullopt;
    }
    return connectionRegistry_->findEndpointBySession(sessionId);
}

std::size_t GatewayServer::getRegisteredConnectionCount() const {
    return connectionRegistry_ ? connectionRegistry_->getConnectionCount() : 0;
}

// 实际监听端口（port 0 = 内核分配，测试与编排面经此读回）。
std::uint16_t GatewayServer::listenPort() const {
    return ingressServer_ ? ingressServer_->listenPort() : 0;
}

void GatewayServer::onClientDisconnect(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    const auto endpoint = connectionRegistry_->findEndpointBySession(sessionId);
    const auto clientAddress = endpoint.has_value() ? endpoint->address : session->clientIP;
    const auto clientPort = endpoint.has_value() ? endpoint->port : session->clientPort;

    glog().info("Client " + clientAddress + ":" + std::to_string(clientPort)
                + " disconnected (" + (normalClose ? "normal" : "timeout") + ")");

    // 通知后端服务
    auto data = encodeDisconnectMessage(sessionId, session->playerId, normalClose);

    if (session->state == SessionState::IN_GAME) {
        messageRouter_->forwardToWorld(sessionId, session->routeSnapshot, data);
        messageRouter_->forwardToBaseApp(sessionId, data);
    }

    if (session->connectionId != 0) {
        // 会话没了，TCP 连接也须真断（心跳超时路径尤甚——不断则成僵尸
        // 连接，包包落地）；对已断连接是迟到操作，入口侧落地丢弃。
        if (ingressServer_) {
            ingressServer_->close(session->connectionId,
                                  normalClose ? DisconnectReason::NormalClose
                                              : DisconnectReason::IdleTimeout);
        }
        connectionRegistry_->markClosed(session->connectionId);
        connectionRegistry_->removeConnection(session->connectionId);
    }

    // 移除会话
    sessionManager_->removeSession(sessionId);
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
                        routeSnapshot.worldServerUrl = config_.cellAppUrl;
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
