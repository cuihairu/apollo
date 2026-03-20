#include "gateway/gateway_server.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace gateway {

namespace netproto = apollo::net::protocol;

namespace {

std::unique_ptr<netproto::Channel> connectBackendChannel(const std::string& url) {
    auto channel = std::make_unique<netproto::Channel>();
    if (!channel->connect(url)) {
        throw std::runtime_error("Failed to connect backend channel: " + url);
    }
    return channel;
}

std::vector<uint8_t> encodeDisconnectMessage(SessionID sessionId, PlayerID playerId, bool normalClose) {
    std::ostringstream stream;
    stream << "gateway_client_disconnect"
           << "|sessionId=" << sessionId
           << "|playerId=" << playerId
           << "|normalClose=" << (normalClose ? 1 : 0);
    const auto payload = stream.str();
    return std::vector<uint8_t>(payload.begin(), payload.end());
}

bool sendPayload(netproto::Channel* channel, const std::vector<uint8_t>& message) {
    if (channel == nullptr || !channel->isConnected()) {
        return false;
    }

    return channel->send(netproto::Message(message));
}

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

    // 连接 LoginApp
    loginAppClient_ = connectBackendChannel(config_.loginAppUrl);

    // 连接 BaseApp
    baseAppClient_ = connectBackendChannel(config_.baseAppUrl);

    // 连接 ChatApp
    chatAppClient_ = connectBackendChannel(config_.chatAppUrl);

    // 初始化 CellApp 池
    auto cellApp = std::make_unique<CellAppInfo>();
    cellApp->url = config_.cellAppUrl;
    cellApp->client = connectBackendChannel(cellApp->url);
    cellApp->load = 0;
    cellApp->available = cellApp->client->isConnected();

    cellApps_.push_back(std::move(cellApp));

    running_ = true;
}

void MessageRouter::stop() {
    running_ = false;
    loginAppClient_.reset();
    baseAppClient_.reset();
    chatAppClient_.reset();
    cellApps_.clear();
}

void MessageRouter::forwardToCellApp(SessionID sessionId, const std::vector<uint8_t>& message) {
    // 获取会话信息
    // 这里简化处理，实际应该从 sessionManager 获取
    // 然后根据 assignedCellApp 转发

    // 使用负载最低的 CellApp
    auto cellAppUrl = getBestCellApp();

    for (auto& cellApp : cellApps_) {
        if (cellApp->url == cellAppUrl && cellApp->available) {
            try {
                if (!sendPayload(cellApp->client.get(), message)) {
                    throw std::runtime_error("send failed");
                }
                cellApp->load++;
            } catch (...) {
                cellApp->available = false;
            }
            break;
        }
    }
}

void MessageRouter::forwardToBaseApp(SessionID sessionId, const std::vector<uint8_t>& message) {
    (void)sessionId;
    if (baseAppClient_) {
        if (!sendPayload(baseAppClient_.get(), message)) {
            std::cerr << "Failed to forward to BaseApp" << std::endl;
        }
    }
}

void MessageRouter::forwardToChatApp(SessionID sessionId, const std::vector<uint8_t>& message) {
    (void)sessionId;
    if (chatAppClient_) {
        if (!sendPayload(chatAppClient_.get(), message)) {
            std::cerr << "Failed to forward to ChatApp" << std::endl;
        }
    }
}

std::string MessageRouter::getBestCellApp() {
    // 查找负载最低的 CellApp
    auto it = std::min_element(cellApps_.begin(), cellApps_.end(),
        [](const auto& a, const auto& b) {
            return a->load < b->load;
        });

    if (it != cellApps_.end() && (*it)->available) {
        return (*it)->url;
    }

    // 默认返回第一个
    if (!cellApps_.empty()) {
        return cellApps_[0]->url;
    }

    return "";
}

//==============================================================================
// GatewayServer 实现
//==============================================================================

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , sessionManager_(std::make_unique<SessionManager>())
    , messageRouter_(std::make_unique<MessageRouter>(config)) {
}

GatewayServer::~GatewayServer() {
    stop();
}

void GatewayServer::start() {
    if (running_) return;

#ifdef _WIN32
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("Failed to initialize Winsock");
    }
#endif

    // 创建监听 socket
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    // 设置 SO_REUSEADDR
    int opt = 1;
#ifdef _WIN32
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // 绑定
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(config_.port);

    if (bind(listenSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
#ifdef _WIN32
        closesocket(listenSocket_);
#else
        close(listenSocket_);
#endif
        throw std::runtime_error("Failed to bind socket");
    }

    // 监听
    if (listen(listenSocket_, SOMAXCONN) < 0) {
#ifdef _WIN32
        closesocket(listenSocket_);
#else
        close(listenSocket_);
#endif
        throw std::runtime_error("Failed to listen");
    }

    // 启动消息路由器
    messageRouter_->start();

    running_ = true;

    // 启动心跳检查线程
    heartbeatThread_ = std::thread(&GatewayServer::heartbeatCheckLoop, this);

    std::cout << "Gateway server listening on " << config_.host << ":" << config_.port << std::endl;
}

void GatewayServer::stop() {
    running_ = false;

    if (listenSocket_ >= 0) {
#ifdef _WIN32
        closesocket(listenSocket_);
#else
        close(listenSocket_);
#endif
        listenSocket_ = -1;
    }

    messageRouter_->stop();

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

#ifdef _WIN32
    WSACleanup();
#endif
}

void GatewayServer::heartbeatCheckLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeatCheckIntervalMs));

        // 检查超时会话
        auto timeoutSessions = sessionManager_->checkTimeouts(config_.sessionTimeoutMs);

        for (auto sessionId : timeoutSessions) {
            std::cout << "Session " << sessionId << " timeout, disconnecting..." << std::endl;
            onClientDisconnect(sessionId, false);
        }
    }
}

void GatewayServer::onClientDisconnect(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    std::cout << "Client " << session->clientIP << ":" << session->clientPort
              << " disconnected (" << (normalClose ? "normal" : "timeout") << ")" << std::endl;

    // 通知后端服务
    auto data = encodeDisconnectMessage(sessionId, session->playerId, normalClose);

    if (session->state == SessionState::IN_GAME) {
        messageRouter_->forwardToCellApp(sessionId, data);
        messageRouter_->forwardToBaseApp(sessionId, data);
    }

    // 移除会话
    sessionManager_->removeSession(sessionId);
}

void GatewayServer::sendToClient(SessionID sessionId, const std::vector<uint8_t>& message) {
    // 实际实现需要维护 socket 连接映射
    // 这里简化处理
}

} // namespace gateway
