#pragma once

#include "gateway/config.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/ingress/client_packet_dispatcher.hpp"
#include "gateway/ingress/gateway_connection_registry.hpp"
#include "gateway/ingress/session_admission_service.hpp"
#include "gateway/m1_egress.hpp"
#include "gateway/session_manager.hpp"
#include "apollo/net/protocol/channel.hpp"
#include "apollo/protocol/socket.hpp"
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

    // 转发消息到 CellApp（M1 出口：legacy 世界帧 → 契约 wire 前递）
    void forwardToWorld(SessionID sessionId, const RouteSnapshot& routeSnapshot,
                        const std::vector<uint8_t>& message);

    // 转发消息到 BaseApp
    void forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message);

    RouteSnapshot buildDefaultRoute() const;

    // M1 出口观测面（降级时为空）
    M1Egress* egress() const { return m1Egress_.get(); }

private:
    GatewayConfig config_;

    // 后端服务连接（后端缺位时为空件——启动期降级，P3-4 处方①）
    std::unique_ptr<apollo::net::protocol::Channel> loginAppClient_;
    std::unique_ptr<apollo::net::protocol::Channel> baseAppClient_;

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
    // 声明序必须与构造初始化列表一致：admissionService_ 依赖 loginAppClient_.get()，
    // 置于其后，否则读未构造成员（UB，macos Debug 实测爆：准入拿空指针误拒）。
    std::unique_ptr<apollo::protocol::RpcClient> baseAppRouteClient_;
    std::unique_ptr<apollo::protocol::RpcClient> loginAppClient_;
    std::unique_ptr<SessionAdmissionService> admissionService_;
    std::unique_ptr<MessageRouter> messageRouter_;

    // TCP 监听器归 ingress 真入口（link TcpListener），本类不持裸 fd

    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::thread heartbeatThread_;
    std::vector<std::thread> workerThreads_;
};

} // namespace gateway
