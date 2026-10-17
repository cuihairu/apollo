#pragma once

#include "gateway/config.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/ingress/client_packet_dispatcher.hpp"
#include "gateway/ingress/gateway_connection_registry.hpp"
#include "gateway/ingress/session_admission_service.hpp"
#include "gateway/m1_egress.hpp"
#include "gateway/m1_route_client.hpp"
#include "gateway/session_manager.hpp"
#include "apollo/core/metrics/metric_registry.h"
#include <memory>
#include <optional>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace gateway {

// 消息路由器
class MessageRouter {
public:
    explicit MessageRouter(const GatewayConfig& config);
    ~MessageRouter();

    // 启动路由器
    void start();

    // 停止路由器
    void stop();

    // 宿主泵一拍（M1 出口收发/应答回收推进；机制零内部线程——gateway
    // 泵线程 acceptNewConnections 循环调用）
    void pump();

    // 转发消息到 WorldApp（M1 出口：legacy 世界帧 → 契约 wire 前递）
    void forwardToWorld(SessionID sessionId, const RouteSnapshot& routeSnapshot,
                        const std::vector<uint8_t>& message);

    // 转发消息到 BaseApp（nng 双 Channel 摘除，P4-4 批②：落地面 =
    // m1Egress degraded 同口径；真实 base 面挂账 P4-4 ③ zone M1 受理面）
    void forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message);

    RouteSnapshot buildDefaultRoute() const;

    // M1 出口观测面（降级时为空）
    M1Egress* egress() const { return m1Egress_.get(); }

private:
    GatewayConfig config_;

    // cell 面（2b-iii：world nng 通道池退役，M1 出口单端点直连）
    std::unique_ptr<M1Egress> m1Egress_;

    std::atomic<bool> running_{false};
};

// Gateway 服务器
class GatewayServer : private IClientIngressObserver {
public:
    explicit GatewayServer(const GatewayConfig& config);
    ~GatewayServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

    // 实际监听端口（port 0 = 内核分配，测试与编排面经此读回）
    std::uint16_t listenPort() const;

    // 消息路由器观测面（测试与编排面经此读 M1 出口计数）
    MessageRouter* messageRouter() const { return messageRouter_.get(); }

    SessionID createPendingSession(const std::string& clientIP, uint16_t clientPort);
    SessionID createPendingSession(const ClientEndpoint& endpoint);
    bool authenticateSession(SessionID sessionId, PlayerID playerId, const std::string& loginTicket);
    std::optional<ClientEndpoint> findClientEndpoint(SessionID sessionId) const;
    std::size_t getRegisteredConnectionCount() const;

private:
    void onConnectionOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) override;
    void onPacketReceived(ConnectionID connectionId, std::span<const std::uint8_t> payload) override;
    void onConnectionClosed(ConnectionID connectionId, DisconnectReason reason) override;
    void onConnectionIdle(ConnectionID connectionId) override;

    // 客户端连接处理
    void acceptNewConnections();

    // 处理客户端消息
    void handleClientMessage(SessionID sessionId, const std::vector<uint8_t>& message);

    // 心跳检查线程
    void heartbeatCheckLoop();

    // metrics admin 通道（泵拍源端渲染消费面）：gauge 会话/应答关联水位
    //（pendingAcks 仅限宿主泵线程读——m1_egress 线程纪律）、counter 绝对值
    // 增量推进（counter_advance_to）
    void recordMetrics();

    // 处理客户端断开
    void onClientDisconnect(SessionID sessionId, bool normalClose);

    // 发送消息给客户端
    void sendToClient(SessionID sessionId, const std::vector<uint8_t>& message);

    RouteSnapshot ensureRouteSnapshot(SessionID sessionId);
    std::optional<RouteSnapshot> fetchRouteSnapshot(SessionID sessionId);

    GatewayConfig config_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::unique_ptr<ClientIngressServer> ingressServer_;
    std::unique_ptr<ClientPacketDispatcher> packetDispatcher_;
    std::unique_ptr<GatewayConnectionRegistry> connectionRegistry_;
    std::unique_ptr<SessionAdmissionService> admissionService_;
    std::unique_ptr<MessageRouter> messageRouter_;

    // M1 路由解析客户端面（P4-4 批④；directoryM1Port = 0 = 不装配，路由
    // 解析缺席 → buildDefaultRoute 兜底）
    std::unique_ptr<M1RouteClient> m1RouteClient_;

    // TCP 监听器归 ingress 真入口（link TcpListener），本类不持裸 fd

    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::thread heartbeatThread_;
    std::vector<std::thread> workerThreads_;

    // metrics admin 通道计数面（引用在注册表存续期恒稳定——注册表头注；
    // last*_ 为绝对值→Counter 增量推进的影子读数，泵线程独占）
    apollo::core::metrics::Counter& gatewayIntentsSentTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_sent_total")};
    apollo::core::metrics::Counter& gatewayIntentsAckedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_acked_total")};
    apollo::core::metrics::Counter& gatewayIntentsFailedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_failed_total")};
    apollo::core::metrics::Counter& gatewayIntentsTimedOutTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_timed_out_total")};
    apollo::core::metrics::Counter& gatewayIntentsUnforwardableTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_unforwardable_total")};
    apollo::core::metrics::Counter& gatewayIntentsDroppedTotal_{
        apollo::core::metrics::MetricRegistry::instance().counter("gateway_intents_dropped_total")};
    std::uint64_t lastIntentsSent_{0};
    std::uint64_t lastIntentsAcked_{0};
    std::uint64_t lastIntentsFailed_{0};
    std::uint64_t lastIntentsTimedOut_{0};
    std::uint64_t lastIntentsUnforwardable_{0};
    std::uint64_t lastIntentsDropped_{0};
};

} // namespace gateway
