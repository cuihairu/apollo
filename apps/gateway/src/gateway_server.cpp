#include "gateway/gateway_server.hpp"
#include "gateway/ingress/ingress_fan_in.hpp"
#include "gateway/ingress/tcp_client_ingress_server.hpp"
#include "gateway/ingress/ws_client_ingress_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/tls/tls_transport.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>

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

#if !defined(_WIN32)
namespace {

// 客户端入口多监听装配（L45 批③）：TCP 明文主入口 + 可选 WS/WSS 子入口，
// 号段互斥分区（TCP 0 / WS 1<<40 / WSS 2<<40）。wsPort/wssPort 均 0 =
// 单 TCP 入口原形态（不套扇入）。WSS 凭据缺失/装载失败即装配拒绝（凭据
// 是显式部署集成，net-abstraction §5.9——不静默降级成明文）；监听绑定
// 失败归 start 面（同 TCP 口径，响亮失败）。
struct AssembledIngress {
    std::unique_ptr<apollo::net::tls::TlsServerContext> tlsContext;
    std::unique_ptr<ClientIngressServer> server;
};

AssembledIngress assemble_client_ingress(const GatewayConfig& config) {
    const std::size_t max_conns = static_cast<std::size_t>(config.maxConnections);
    std::vector<IngressFanInBinding> bindings;
    bindings.push_back(IngressFanInBinding{
        makeTcpClientIngressServer(config.host, config.port, max_conns, 0), 0});

    std::unique_ptr<apollo::net::tls::TlsServerContext> tls_context;
    if (config.wsPort != 0) {
        WsIngressConfig ws;
        ws.host = config.host;
        ws.port = config.wsPort;
        ws.path = config.wsPath;
        ws.maxConnections = max_conns;
        ws.connectionIdBase = 1 * kIngressFanInIdStride;
        bindings.push_back(IngressFanInBinding{
            makeWsClientIngressServer(ws), ws.connectionIdBase});
    }
    if (config.wssPort != 0) {
        if (config.tlsCertPath.empty() || config.tlsKeyPath.empty()) {
            throw std::runtime_error(
                "wss ingress enabled but tls cert/key not configured (--tls-cert/--tls-key)");
        }
        tls_context = std::make_unique<apollo::net::tls::TlsServerContext>();
        const auto loaded = tls_context->load(config.tlsCertPath, config.tlsKeyPath);
        if (!loaded.ok) {
            throw std::runtime_error("wss tls context load failed: " + loaded.error);
        }
        WsIngressConfig wss;
        wss.host = config.host;
        wss.port = config.wssPort;
        wss.path = config.wsPath;
        wss.tls = tls_context.get();
        wss.maxConnections = max_conns;
        wss.connectionIdBase = 2 * kIngressFanInIdStride;
        bindings.push_back(IngressFanInBinding{
            makeWsClientIngressServer(wss), wss.connectionIdBase});
    }

    if (bindings.size() == 1) {
        return {nullptr, std::move(bindings[0].server)};
    }
    return {std::move(tls_context), makeIngressFanIn(std::move(bindings))};
}

} // namespace
#endif // !_WIN32

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
    egressConfig.host = config_.worldM1Host;
    egressConfig.port = config_.worldM1Port;
    egressConfig.route_json = config_.worldM1RouteJson;
    m1Egress_ = makeM1Egress(std::move(egressConfig));
    if (!m1Egress_->start()) {
        glog().warning("gateway WorldApp M1 egress degraded: route table unavailable ("
                       + config_.worldM1RouteJson + "), WorldApp forwarding disabled");
        m1Egress_.reset();
    }

    if (m1Egress_ && deliverToClient_) {
        setDeliverToClient(deliverToClient_);
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

void MessageRouter::setDeliverToClient(
    std::function<void(SessionID, const std::vector<uint8_t>&)> fn) {
    // 保留宿主在 start 前设置的出口，重启路由器后也需重新装配。
    deliverToClient_ = std::move(fn);
    // 939 下行出口：M1 出口解字段 → 本回调（宿主泵线程）→ 入口投递队列。
    // egress 降级时闭包不装——World 回带只落 observability 计数。
    if (!m1Egress_) {
        return;
    }
    m1Egress_->setClientDeliver(
        [fn = deliverToClient_](std::uint64_t session_id, std::uint32_t /*gateway_id*/,
                             const std::vector<std::uint8_t>& payload) {
            if (fn) fn(session_id, payload);
        });
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
    routeSnapshot.worldServerUrl = config_.worldUrl;
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
        config.loginM1Host, config.loginM1Port, config.worldM1RouteJson);
    if (!linkService) {
        glog().warning("gateway login M1 admission degraded: route table unavailable ("
                       + config.worldM1RouteJson + "), admission degrades tolerant");
        return makeDefaultSessionAdmissionService();
    }
    return linkService;
}

} // namespace

namespace {

// 单调毫秒（排空期限钟——受理点（handleDrainRequest）与推进（drain_pump）
// 都在泵线程，同钟无竞）。
std::uint64_t steady_now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , sessionManager_(std::make_unique<SessionManager>())
    , packetDispatcher_(std::make_unique<ClientPacketDispatcher>())
    , connectionRegistry_(std::make_unique<GatewayConnectionRegistry>())
    , admissionService_(makeAdmissionService(config))
    , messageRouter_(std::make_unique<MessageRouter>(config)) {
    // 多监听装配在构造体（凭据装载失败即此处抛出）：tlsContext_ 先于
    // ingressServer_ 声明 → 后于其析构（监听器持有其 SSL_CTX）。
#if !defined(_WIN32)
    auto assembled = assemble_client_ingress(config_);
    tlsContext_ = std::move(assembled.tlsContext);
    ingressServer_ = std::move(assembled.server);
#else
    ingressServer_ = makeTcpClientIngressServer(
        config_.host, config_.port, static_cast<std::size_t>(config_.maxConnections));
#endif
}

GatewayServer::~GatewayServer() {
    stop();
}

void GatewayServer::start() {
    if (running_) return;

    // 客户端帧下行出口（P2 真实下行；start 前装配）：
    // World 939 回带 → M1 出口解字段 → sendToClient（会话在即真投）。
    messageRouter_->setDeliverToClient(
        [this](SessionID sessionId, const std::vector<uint8_t>& message) {
            sendToClient(sessionId, message);
        });

    // 启动消息路由器
    messageRouter_->start();
    ingressServer_->setObserver(this);
    // 监听 socket 归 ingress 真入口（link TcpListener）——bind 失败即 throw，
    // 与旧裸 socket 块同契约。
    ingressServer_->start();

    // M1 路由解析客户端面（P4-4 批④）：directoryM1Port 非零 = 拨 manager
    // 控制面；装配失败降级（警告 + 不拦启动），路由解析缺席 → 默认路由兜底。
    if (config_.directoryM1Port != 0) {
        M1RouteClientConfig routeConfig;
        routeConfig.host = config_.directoryM1Host;
        routeConfig.port = config_.directoryM1Port;
        routeConfig.route_json = config_.worldM1RouteJson;
        m1RouteClient_ = makeM1RouteClient(std::move(routeConfig));
        if (m1RouteClient_) {
            glog().info("Gateway M1 route client dialing " + config_.directoryM1Host
                        + ":" + std::to_string(config_.directoryM1Port));
        } else {
            glog().warning("gateway M1 route client degraded: route table unavailable ("
                           + config_.worldM1RouteJson + "), default route stays");
        }
    }

    // Base M1 激活面（L53 登录串联第三环）：baseM1Port 非零 = 拨 Base
    // PlayerStore M1 受理面，准入通过后激活锚点 + 绑定会话；装配失败降级
    // （警告 + 认证退容忍面——面缺席 ≠ 会话非法，准入面装配降级同口径）。
    if (config_.baseM1Port != 0) {
        M1BaseClientConfig baseConfig;
        baseConfig.host = config_.baseM1Host;
        baseConfig.port = config_.baseM1Port;
        baseConfig.route_json = config_.worldM1RouteJson;
        baseClient_ = makeM1BaseClient(std::move(baseConfig));
        if (baseClient_) {
            glog().info("Gateway M1 base client dialing " + config_.baseM1Host + ":"
                        + std::to_string(config_.baseM1Port));
        } else {
            glog().warning("gateway M1 base client degraded: route table unavailable ("
                           + config_.worldM1RouteJson + "), base activation off");
        }
    }

    // 排空控制受理面（P4 排空批）：装配失败降级（警告 + ops 不可达，客户端
    // 服务不受影响——出口降级同口径）。
    {
        GatewayControlConfig controlConfig;
        controlConfig.host = config_.host;
        controlConfig.port = config_.controlM1Port;
        controlConfig.route_json = config_.worldM1RouteJson;
        controlServer_ = makeGatewayControlServer(
            std::move(controlConfig), [this](const DrainControlRequest& request) {
                return handleDrainRequest(request);
            });
        if (!controlServer_->start()) {
            glog().warning("gateway drain control face degraded: bind/route failed ("
                           + config_.worldM1RouteJson + "), drain ops unreachable");
            controlServer_.reset();
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
    if (controlServer_) {
        controlServer_->stop();
        controlServer_.reset();
    }
    if (m1RouteClient_) {
        m1RouteClient_->stop();
        m1RouteClient_.reset();
    }
    if (baseClient_) {
        baseClient_->stop();
        baseClient_.reset();
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

    auto sessionId = connection->sessionId;
    bool haveHeader = false;
    apollo::protocol::MessageHeader header{};
    if (payload.size() >= sizeof(apollo::protocol::MessageHeader)) {
        // 会话连续（L53）：上行帧头携异地会话号 = 重连恢复尝试（客户端记
        // 住旧会话号，新连接首包带上）。信封残缺/魔数不符 = 按无恢复尝试
        // 处理（分发器另行按 Invalid 落地），入口不因畸形头抛穿。
        try {
            header = apollo::protocol::MessageCodec::parseHeader(payload.data(), payload.size());
            haveHeader = true;
            if (header.sessionId != 0 && header.sessionId != sessionId) {
                tryResumeSession(sessionId, header.sessionId, connectionId);
                const auto rebound = connectionRegistry_->getConnection(connectionId);
                if (rebound) {
                    sessionId = rebound->sessionId;
                }
            }
        } catch (const std::exception&) {
            // 畸形信封：不进恢复逻辑与序号闸（分发器另行按 Invalid 落地）
        }
    }

    // 上行重复/乱序规则（L51）：恢复解析后按最终会话过序号闸——重放/重复
    // 帧（≤ 水位）不再分发到业务（计数在 SessionManager）；乱序空档按流
    // 传输语义容忍。恢复重绑发生在闸前，故旧会话号的旧序重放落回原会话
    // 水位上判定。
    if (haveHeader && !sessionManager_->acceptUplinkSequence(sessionId, header.sequence)) {
        return;
    }

    handleClientMessage(
        sessionId,
        std::vector<std::uint8_t>(payload.begin(), payload.end())
    );
}

void GatewayServer::tryResumeSession(SessionID freshSessionId, SessionID retainedSessionId,
                                     ConnectionID connectionId) {
    const auto fresh = sessionManager_->getSession(freshSessionId);
    if (!fresh || fresh->playerId != 0 || freshSessionId == retainedSessionId) {
        return;  // 本连接已认证（防抢线）或同号回显：非恢复语义
    }

    if (!sessionManager_->resumeSession(retainedSessionId, connectionId,
                                        config_.sessionRetentionMs)) {
        glog().info("Gateway session resume rejected for session "
                    + std::to_string(retainedSessionId) +
                    " (not retained, window expired, or already resumed)");
        return;  // 落回 fresh 会话 = 完整重登路径
    }

    sessionManager_->removeSession(freshSessionId);
    connectionRegistry_->attachSession(connectionId, retainedSessionId);

    const auto resumed = sessionManager_->getSession(retainedSessionId);
    glog().info("Gateway resumed session " + std::to_string(retainedSessionId) +
                " for player " + std::to_string(resumed ? resumed->playerId : 0) +
                " on new connection " + std::to_string(connectionId));
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

        // 会话连续（L53）：保留窗到期销毁——延迟的断线通知此时才下发后端。
        for (auto sessionId : sessionManager_->checkRetentions(config_.sessionRetentionMs)) {
            glog().info("Session " + std::to_string(sessionId)
                        + " reconnect window expired, disconnecting...");
            destroySessionAndNotify(sessionId, false);
        }
    }
}

void GatewayServer::acceptNewConnections() {
    // 宿主泵循环（net M1 缝纪律：机制零内部线程）：accept/收帧/发帧/M1 出口
    // 推进全在此线程过拍，经 ingress 投递队列与 send/close 跨线程面解耦。
    while (running_) {
        const auto now_ms = steady_now_ms();
        ingressServer_->pump();
        messageRouter_->pump();
        if (controlServer_) {
            controlServer_->pump(now_ms);
        }
        drain_pump(now_ms);
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

std::optional<ClientEndpoint> GatewayServer::findClientEndpoint(SessionID sessionId) const {
    if (!connectionRegistry_) {
        return std::nullopt;
    }
    return connectionRegistry_->findEndpointBySession(sessionId);
}

std::size_t GatewayServer::getRegisteredConnectionCount() const {
    return connectionRegistry_ ? connectionRegistry_->getConnectionCount() : 0;
}

std::size_t GatewayServer::getSessionCount() const {
    return sessionManager_ ? sessionManager_->getSessionCount() : 0;
}

// 实际监听端口（port 0 = 内核分配，测试与编排面经此读回）。
std::uint16_t GatewayServer::listenPort() const {
    return ingressServer_ ? ingressServer_->listenPort() : 0;
}

std::uint16_t GatewayServer::wsListenPort() const {
#if !defined(_WIN32)
    // 装配序（assemble_client_ingress）：TCP 0，WS 1。
    if (config_.wsPort == 0 || !ingressServer_) return 0;
    const auto ports = ingressServer_->listenPorts();
    return ports.size() > 1 ? ports[1] : 0;
#else
    return 0;
#endif
}

std::uint16_t GatewayServer::wssListenPort() const {
#if !defined(_WIN32)
    // 装配序：TCP 0，[WS]，WSS——按配置跳缺位。
    if (config_.wssPort == 0 || !ingressServer_) return 0;
    const auto ports = ingressServer_->listenPorts();
    const std::size_t idx = 1 + (config_.wsPort != 0 ? 1 : 0);
    return ports.size() > idx ? ports[idx] : 0;
#else
    return 0;
#endif
}

void GatewayServer::onClientDisconnect(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    // 会话连续（L53 断线重连）：已认证会话掉线进保留窗——连接拆掉、会话
    // 留存（playerId/路由快照/会话号原样），后端断线通知延到窗到期才下发
    // （窗内恢复即无通知；Zone 侧 907/908 resume 握手随 world 链路批接线，
    // 本批为 gateway 本地面）。未认证会话无可保留（重连即全新会话），
    // retention=0 关窗回旧语义（掉线即销毁）。
    if (config_.sessionRetentionMs > 0) {
        const auto connectionId = session->connectionId;  // 先取：mark 后置 0
        if (sessionManager_->markDisconnected(sessionId)) {
            glog().info("Session " + std::to_string(sessionId)
                        + " disconnected, retained for reconnect window");
            if (connectionId != 0) {
                if (ingressServer_) {
                    ingressServer_->close(connectionId,
                                          normalClose ? DisconnectReason::NormalClose
                                                      : DisconnectReason::IdleTimeout);
                }
                connectionRegistry_->markClosed(connectionId);
                connectionRegistry_->removeConnection(connectionId);
            }
            return;
        }
    }

    destroySessionAndNotify(sessionId, normalClose);
}

void GatewayServer::destroySessionAndNotify(SessionID sessionId, bool normalClose) {
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

DrainStatus GatewayServer::handleDrainRequest(const DrainControlRequest& request) {
    switch (request.op) {
        case 1: {  // begin：关监听停新连接 + 起期限（幂等：已排水期重入只收紧期限）
            if (drain_state_.load(std::memory_order_acquire) == kDrainServing) {
                ingressServer_->beginDrain();
                drain_state_.store(kDrainDraining, std::memory_order_release);
                glog().info("gateway drain begun: new connections refused, "
                            "existing connections served");
            }
            if (request.deadline_s != 0) {
                const auto deadline_ms =
                    steady_now_ms()
                    + static_cast<std::uint64_t>(request.deadline_s) * 1000U;
                drain_deadline_ms_ = drain_deadline_ms_ == 0
                                         ? deadline_ms
                                         : std::min(drain_deadline_ms_, deadline_ms);
            }
            break;
        }
        case 2:
            break;  // status：直落快照
        default:
            return DrainStatus{false, drainState(), 0,
                               "unknown op " + std::to_string(request.op)};
    }
    DrainStatus st;
    st.ok = true;
    st.state = drainState();
    st.active_connections = ingressServer_->activeConnections();
    if (st.state == kDrainDrained) {
        st.detail = drain_detail_;
    } else if (st.state == kDrainDraining) {
        st.detail = "draining: " + std::to_string(st.active_connections)
                    + " active connections served";
    } else {
        st.detail = "serving";
    }
    return st;
}

void GatewayServer::drain_pump(std::uint64_t now_ms) {
    if (drain_state_.load(std::memory_order_acquire) != kDrainDraining) {
        return;
    }
    if (ingressServer_->activeConnections() == 0) {
        enter_drained("drained: all client connections left");
        return;
    }
    if (drain_deadline_ms_ != 0 && now_ms >= drain_deadline_ms_) {
        // 强停：期限到仍在线——全量收线（close 经投递队列，泵线程落 socket）
        // 后落终态。强停结果 = 在途客户端工作丢弃，客户端按断线重连面恢复；
        // 数量显形在日志与 status detail（不静默）。
        const auto ids = ingressServer_->connectionIds();
        for (const auto id : ids) {
            ingressServer_->close(id, DisconnectReason::NormalClose);
        }
        enter_drained("drained: deadline reached, " + std::to_string(ids.size())
                      + " connections force-closed");
    }
}

void GatewayServer::enter_drained(const std::string& detail) {
    drain_detail_ = detail;
    drain_state_.store(kDrainDrained, std::memory_order_release);
    glog().info("gateway " + detail);
}

} // namespace gateway
