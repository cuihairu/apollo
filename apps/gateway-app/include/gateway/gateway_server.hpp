#pragma once

#include "gateway/config.hpp"
#include "gateway/ingress/client_ingress_server.hpp"
#include "gateway/ingress/client_packet_dispatcher.hpp"
#include "gateway/ingress/gateway_connection_registry.hpp"
#include "gateway/ingress/session_admission_service.hpp"
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

    // 转发消息到 CellApp
    void forwardToWorld(SessionID sessionId, const RouteSnapshot& routeSnapshot,
                        const std::vector<uint8_t>& message);

    // 转发消息到 BaseApp
    void forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message);

    // 转发消息到 ChatApp
    void forwardToChatApp(SessionID sessionId, const std::vector<uint8_t>& message);

    RouteSnapshot buildDefaultRoute() const;

private:
    GatewayConfig config_;

    // 后端服务连接
    std::unique_ptr<apollo::net::protocol::Channel> loginAppClient_;
    std::unique_ptr<apollo::net::protocol::Channel> baseAppClient_;
    std::unique_ptr<apollo::net::protocol::Channel> chatAppClient_;

    // WorldHost 池
    struct WorldNodeInfo {
        std::string url;
        std::unique_ptr<apollo::net::protocol::Channel> client;
        int load;  // 负载计数
        std::atomic<bool> available;
    };
    std::vector<std::unique_ptr<WorldNodeInfo>> worldNodes_;

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
    std::unique_ptr<SessionAdmissionService> admissionService_;
    std::unique_ptr<MessageRouter> messageRouter_;
    std::unique_ptr<apollo::protocol::RpcClient> baseAppRouteClient_;
    std::unique_ptr<apollo::protocol::RpcClient> loginAppClient_;

    // TCP 监听器 (简化版)
    int listenSocket_ = -1;

    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::thread heartbeatThread_;
    std::vector<std::thread> workerThreads_;
};

} // namespace gateway
