#include "gateway/gateway_server.hpp"
#include "apollo/protocol/messages.hpp"
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
    std::error_code ec;
    if (!channel->connect(url, ec)) {
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

std::string routeLabel(const RouteSnapshot& routeSnapshot) {
    std::ostringstream stream;
    stream << "world=" << routeSnapshot.worldId
           << ", instance=" << routeSnapshot.instanceId
           << ", space=" << routeSnapshot.spaceId
           << ", version=" << routeSnapshot.routeVersion
           << ", url=" << routeSnapshot.worldServerUrl;
    return stream.str();
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

    auto worldNode = std::make_unique<WorldNodeInfo>();
    worldNode->url = config_.cellAppUrl;
    worldNode->client = connectBackendChannel(worldNode->url);
    worldNode->load = 0;
    worldNode->available = worldNode->client->isConnected();

    worldNodes_.push_back(std::move(worldNode));

    running_ = true;
}

void MessageRouter::stop() {
    running_ = false;
    loginAppClient_.reset();
    baseAppClient_.reset();
    chatAppClient_.reset();
    worldNodes_.clear();
}

void MessageRouter::forwardToWorld(
    SessionID sessionId,
    const RouteSnapshot& routeSnapshot,
    const std::vector<uint8_t>& message
) {
    (void)sessionId;
    const auto route = routeSnapshot.isAssigned() ? routeSnapshot : buildDefaultRoute();

    for (auto& worldNode : worldNodes_) {
        if (worldNode->url == route.worldServerUrl && worldNode->available) {
            try {
                if (!sendPayload(worldNode->client.get(), message)) {
                    throw std::runtime_error("send failed");
                }
                worldNode->load++;
            } catch (...) {
                worldNode->available = false;
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

RouteSnapshot MessageRouter::buildDefaultRoute() const {
    RouteSnapshot routeSnapshot;
    routeSnapshot.worldId = 1;
    routeSnapshot.mapId = 1;
    routeSnapshot.instanceId = 1;
    routeSnapshot.spaceId = 1;
    routeSnapshot.routeVersion = 1;

    auto it = std::min_element(worldNodes_.begin(), worldNodes_.end(),
        [](const auto& a, const auto& b) {
            return a->load < b->load;
        });

    if (it != worldNodes_.end() && (*it)->available) {
        routeSnapshot.worldServerUrl = (*it)->url;
        return routeSnapshot;
    }

    if (!worldNodes_.empty()) {
        routeSnapshot.worldServerUrl = worldNodes_[0]->url;
    }

    return routeSnapshot;
}

//==============================================================================
// GatewayServer 实现
//==============================================================================

GatewayServer::GatewayServer(const GatewayConfig& config)
    : config_(config)
    , sessionManager_(std::make_unique<SessionManager>())
    , ingressServer_(makeNullClientIngressServer())
    , packetDispatcher_(std::make_unique<ClientPacketDispatcher>())
    , connectionRegistry_(std::make_unique<GatewayConnectionRegistry>())
    , baseAppRouteClient_(std::make_unique<apollo::protocol::RpcClient>(config.baseAppUrl))
    , loginAppClient_(std::make_unique<apollo::protocol::RpcClient>(config.loginAppUrl))
    , admissionService_(makeDefaultSessionAdmissionService(loginAppClient_.get()))
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
    ingressServer_->setObserver(this);
    ingressServer_->start();
    baseAppRouteClient_->connect();
    loginAppClient_->connect();

    running_ = true;

    acceptThread_ = std::thread(&GatewayServer::acceptNewConnections, this);

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
    if (ingressServer_) {
        ingressServer_->stop();
    }
    if (baseAppRouteClient_) {
        baseAppRouteClient_->disconnect();
    }
    if (loginAppClient_) {
        loginAppClient_->disconnect();
    }

    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
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

void GatewayServer::onConnectionOpened(ConnectionID connectionId, const ClientEndpoint& endpoint) {
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
            std::cout << "Session " << sessionId << " timeout, disconnecting..." << std::endl;
            onClientDisconnect(sessionId, false);
        }
    }
}

void GatewayServer::acceptNewConnections() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // TCP accept and socket/session binding are not wired yet.
        // Keep the loop alive so the server shape matches the intended runtime model.
    }
}

void GatewayServer::handleClientMessage(SessionID sessionId, const std::vector<uint8_t>& message) {
    if (!packetDispatcher_) {
        return;
    }

    const auto dispatch = packetDispatcher_->dispatch(
        std::span<const std::uint8_t>(message.data(), message.size()));

    if (dispatch.kind == ClientPacketKind::Invalid) {
        std::cerr << "Gateway dropped malformed client message for session " << sessionId << std::endl;
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
            if (!sessionManager_->hasSession(sessionId)) {
                return;
            }
            messageRouter_->forwardToChatApp(sessionId, message);
            return;

        case ClientPacketKind::World: {
            auto session = sessionManager_->getSession(sessionId);
            if (!session || session->playerId == 0) {
                std::cerr << "Gateway rejected unauthenticated world message for session "
                          << sessionId << std::endl;
                return;
            }
            const auto routeSnapshot = ensureRouteSnapshot(sessionId);
            if (!routeSnapshot.isAssigned()) {
                std::cerr << "Gateway cannot route world message for session " << sessionId << std::endl;
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

void GatewayServer::onClientDisconnect(SessionID sessionId, bool normalClose) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session) return;

    const auto endpoint = connectionRegistry_->findEndpointBySession(sessionId);
    const auto clientAddress = endpoint.has_value() ? endpoint->address : session->clientIP;
    const auto clientPort = endpoint.has_value() ? endpoint->port : session->clientPort;

    std::cout << "Client " << clientAddress << ":" << clientPort
              << " disconnected (" << (normalClose ? "normal" : "timeout") << ")" << std::endl;

    // 通知后端服务
    auto data = encodeDisconnectMessage(sessionId, session->playerId, normalClose);

    if (session->state == SessionState::IN_GAME) {
        messageRouter_->forwardToWorld(sessionId, session->routeSnapshot, data);
        messageRouter_->forwardToBaseApp(sessionId, data);
    }

    if (session->connectionId != 0) {
        connectionRegistry_->markClosed(session->connectionId);
        connectionRegistry_->removeConnection(session->connectionId);
    }

    // 移除会话
    sessionManager_->removeSession(sessionId);
}

void GatewayServer::sendToClient(SessionID sessionId, const std::vector<uint8_t>& message) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session || session->connectionId == 0 || !ingressServer_) {
        std::cout << "Gateway sendToClient placeholder for session " << sessionId << std::endl;
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
        std::cout << "Gateway resolved route snapshot for session " << sessionId
                  << ": " << routeLabel(*resolved) << std::endl;
        return *resolved;
    }

    const auto routeSnapshot = messageRouter_->buildDefaultRoute();
    sessionManager_->assignRoute(sessionId, routeSnapshot);

    std::cout << "Gateway assigned route snapshot for session " << sessionId
              << ": " << routeLabel(routeSnapshot) << std::endl;

    return routeSnapshot;
}

std::optional<RouteSnapshot> GatewayServer::fetchRouteSnapshot(SessionID sessionId) {
    auto session = sessionManager_->getSession(sessionId);
    if (!session || !baseAppRouteClient_) {
        return std::nullopt;
    }

    try {
        apollo::protocol::PlayerResolveRouteRequest request;
        request.playerId = session->playerId;
        request.sessionId = sessionId;

        const auto response =
            baseAppRouteClient_->call<
                apollo::protocol::PlayerResolveRouteRequest,
                apollo::protocol::PlayerResolveRouteResponse>(request, sessionId);

        if (!response.success) {
            return std::nullopt;
        }

        RouteSnapshot routeSnapshot;
        routeSnapshot.worldId = response.worldId;
        routeSnapshot.mapId = response.mapId;
        routeSnapshot.instanceId = response.instanceId;
        routeSnapshot.spaceId = response.spaceId;
        routeSnapshot.routeVersion = response.routeVersion;
        routeSnapshot.worldServerUrl = config_.cellAppUrl;
        return routeSnapshot;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

} // namespace gateway
