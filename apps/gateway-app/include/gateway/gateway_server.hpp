#pragma once

#include "gateway/config.hpp"
#include "gateway/session_manager.hpp"
#include "apollo/protocol/socket.hpp"
#include <memory>
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
    void forwardToCellApp(SessionID sessionId, const std::vector<uint8_t>& message);

    // 转发消息到 BaseApp
    void forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message);

    // 转发消息到 ChatApp
    void forwardToChatApp(SessionID sessionId, const std::vector<uint8_t>& message);

    // 获取最优 CellApp
    std::string getBestCellApp();

private:
    GatewayConfig config_;

    // 后端服务连接
    std::unique_ptr<protocol::ReqSocket> loginAppClient_;
    std::unique_ptr<protocol::ReqSocket> baseAppClient_;
    std::unique_ptr<protocol::ReqSocket> chatAppClient_;

    // CellApp 池
    struct CellAppInfo {
        std::string url;
        std::unique_ptr<protocol::ReqSocket> client;
        int load;  // 负载计数
        std::atomic<bool> available;
    };
    std::vector<std::unique_ptr<CellAppInfo>> cellApps_;

    std::atomic<bool> running_{false};
};

// Gateway 服务器
class GatewayServer {
public:
    explicit GatewayServer(const GatewayConfig& config);
    ~GatewayServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

private:
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

    GatewayConfig config_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::unique_ptr<MessageRouter> messageRouter_;

    // TCP 监听器 (简化版)
    int listenSocket_ = -1;

    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::thread heartbeatThread_;
    std::vector<std::thread> workerThreads_;
};

} // namespace gateway
