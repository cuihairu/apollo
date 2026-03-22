#include "login/login_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace login {

//==============================================================================
// Authenticator 实现
//==============================================================================

Authenticator::Authenticator(const LoginConfig& config)
    : config_(config) {
    initTestUsers();
}

void Authenticator::initTestUsers() {
    // 创建测试用户
    UserInfo user1;
    user1.playerId = 1001;
    user1.username = "player1";
    user1.passwordHash = "pass1";  // 简化，实际应该用 hash
    user1.lastLoginMs = 0;
    user1.createdAtMs = 0;

    users_["player1"] = user1;

    UserInfo user2;
    user2.playerId = 1002;
    user2.username = "player2";
    user2.passwordHash = "pass2";
    user2.lastLoginMs = 0;
    user2.createdAtMs = 0;

    users_["player2"] = user2;
}

bool Authenticator::authenticate(const std::string& username, const std::string& password,
                                  PlayerID& outPlayerId, std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否被锁定
    if (isLockedOut(username)) {
        errorMessage = "Account locked due to too many failed attempts";
        return false;
    }

    // 查找用户
    auto it = users_.find(username);
    if (it == users_.end()) {
        errorMessage = "Invalid username or password";
        recordAttempt(username, false);
        return false;
    }

    // 验证密码
    if (it->second.passwordHash != password) {
        errorMessage = "Invalid username or password";
        recordAttempt(username, false);
        return false;
    }

    // 登录成功
    outPlayerId = it->second.playerId;
    it->second.lastLoginMs = getCurrentTimeMs();
    recordAttempt(username, true);

    return true;
}

bool Authenticator::isLockedOut(const std::string& username) {
    auto it = loginAttempts_.find(username);
    if (it == loginAttempts_.end()) {
        return false;
    }

    const auto& attempt = it->second;
    if (attempt.attempts >= config_.maxLoginAttempts) {
        int64_t now = getCurrentTimeMs();
        if (now < attempt.lockoutUntilMs) {
            return true;
        }
        // 锁定已过期，重置
        loginAttempts_.erase(it);
    }

    return false;
}

void Authenticator::recordAttempt(const std::string& username, bool success) {
    auto it = loginAttempts_.find(username);
    int64_t now = getCurrentTimeMs();

    if (success) {
        // 成功登录，清除记录
        if (it != loginAttempts_.end()) {
            loginAttempts_.erase(it);
        }
    } else {
        // 失败登录
        if (it == loginAttempts_.end()) {
            LoginAttempt attempt;
            attempt.attempts = 1;
            attempt.lastAttemptMs = now;
            attempt.lockoutUntilMs = 0;
            loginAttempts_[username] = attempt;
        } else {
            it->second.attempts++;
            it->second.lastAttemptMs = now;

            if (it->second.attempts >= config_.maxLoginAttempts) {
                it->second.lockoutUntilMs = now + config_.lockoutDurationMs;
            }
        }
    }
}

int64_t Authenticator::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

//==============================================================================
// GatewayAllocator 实现
//==============================================================================

GatewayAllocator::GatewayAllocator(const LoginConfig& config)
    : config_(config) {
    // 初始化网关列表
    int index = 0;
    for (const auto& url : config_.gatewayUrls) {
        GatewayInfo info;
        info.url = url;
        info.connections = 0;
        info.lastUpdateMs = 0;
        gateways_.push_back(info);
        index++;
    }
}

std::string GatewayAllocator::selectGateway() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (gateways_.empty()) {
        return "";
    }

    // 选择连接数最少的网关
    auto it = std::min_element(gateways_.begin(), gateways_.end(),
        [](const GatewayInfo& a, const GatewayInfo& b) {
            return a.connections < b.connections;
        });

    return it->url;
}

void GatewayAllocator::updateGatewayLoad(const std::string& gatewayUrl, int connections) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& gateway : gateways_) {
        if (gateway.url == gatewayUrl) {
            gateway.connections = connections;
            gateway.lastUpdateMs = getCurrentTimeMs();
            break;
        }
    }
}

int64_t GatewayAllocator::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

//==============================================================================
// SessionManager 实现
//==============================================================================

SessionManager::SessionManager(const LoginConfig& config)
    : config_(config) {
}

SessionID SessionManager::createSession(
    PlayerID playerId,
    const std::string& gatewayUrl,
    std::string& outLoginTicket
) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionID sessionId = nextSessionId_++;

    SessionInfo info;
    info.sessionId = sessionId;
    info.playerId = playerId;
    info.gatewayUrl = gatewayUrl;
    info.loginTicket = generateLoginTicket(playerId, sessionId);
    info.createdAtMs = getCurrentTimeMs();
    info.lastAccessMs = info.createdAtMs;

    sessions_[sessionId] = info;
    tickets_[info.loginTicket] = sessionId;
    outLoginTicket = info.loginTicket;

    return sessionId;
}

bool SessionManager::validateSession(
    SessionID sessionId,
    const std::string& loginTicket,
    PlayerID& outPlayerId,
    std::string& outGatewayUrl
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }

    if (loginTicket.empty() || it->second.loginTicket != loginTicket) {
        return false;
    }

    const auto ticketIt = tickets_.find(loginTicket);
    if (ticketIt == tickets_.end() || ticketIt->second != sessionId) {
        return false;
    }

    const auto& info = it->second;
    int64_t now = getCurrentTimeMs();

    // 检查是否过期
    if (now - info.lastAccessMs > config_.sessionTimeoutMs) {
        sessions_.erase(it);
        return false;
    }

    // 更新访问时间
    const_cast<SessionInfo&>(info).lastAccessMs = now;

    outPlayerId = info.playerId;
    outGatewayUrl = info.gatewayUrl;

    return true;
}

void SessionManager::removeSession(SessionID sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        tickets_.erase(it->second.loginTicket);
        sessions_.erase(it);
    }
}

void SessionManager::cleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = getCurrentTimeMs();
    int64_t expiryMs = config_.sessionTimeoutMs;

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.lastAccessMs > expiryMs) {
            tickets_.erase(it->second.loginTicket);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

int64_t SessionManager::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string SessionManager::generateLoginTicket(PlayerID playerId, SessionID sessionId) const {
    static constexpr char hex[] = "0123456789abcdef";

    std::uint64_t seed = static_cast<std::uint64_t>(getCurrentTimeMs())
        ^ (playerId * 0x9E3779B97F4A7C15ULL)
        ^ (sessionId * 0xC2B2AE3D27D4EB4FULL);

    std::string ticket;
    ticket.reserve(32);
    for (int i = 0; i < 32; ++i) {
        seed ^= (seed >> 12);
        seed ^= (seed << 25);
        seed ^= (seed >> 27);
        const auto value = static_cast<std::uint8_t>((seed * 0x2545F4914F6CDD1DULL) & 0x0F);
        ticket.push_back(hex[value]);
    }
    return ticket;
}

//==============================================================================
// LoginServer 实现
//==============================================================================

LoginServer::LoginServer(const LoginConfig& config)
    : config_(config)
    , authenticator_(std::make_unique<Authenticator>(config))
    , gatewayAllocator_(std::make_unique<GatewayAllocator>(config))
    , sessionManager_(std::make_unique<SessionManager>(config))
    , baseAppClient_(std::make_unique<protocol::RpcClient>(config.baseAppUrl)) {
}

LoginServer::~LoginServer() {
    stop();
}

void LoginServer::start() {
    if (running_) return;

    baseAppClient_->connect();

    // 创建 RPC 服务器
    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(config_.host, config_.port)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        // 解析消息头
        auto header = protocol::MessageCodec::parseHeader(data);
        auto msgType = static_cast<protocol::MessageType>(header.type);

        switch (msgType) {
            case protocol::MessageType::LOGIN_REQUEST:
                return handleLoginRequest(data);

            case protocol::MessageType::GATEWAY_ASSIGN_REQUEST:
                return handleGatewayAssignRequest(data);

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR_MESSAGE);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    running_ = true;

    server_->start();

    // 启动会话清理线程
    cleanupThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            sessionManager_->cleanupExpiredSessions();
        }
    });

    std::cout << "Login server listening on " << config_.host << ":" << config_.port << std::endl;
}

void LoginServer::stop() {
    running_ = false;
    if (server_) {
        server_->stop();
        server_.reset();
    }

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    if (baseAppClient_) {
        baseAppClient_->disconnect();
    }
}

std::vector<uint8_t> LoginServer::handleLoginRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto loginReq = protocol::MessageCodec::decodeBody<protocol::LoginRequest>(bodyData);

    protocol::LoginResponse response;
    response.success = false;
    response.sessionId = 0;
    response.playerId = 0;

    // 认证
    PlayerID playerId = 0;
    std::string errorMessage;
    if (authenticator_->authenticate(loginReq.username, loginReq.password, playerId, errorMessage)) {
        const auto gatewayUrl = gatewayAllocator_->selectGateway();
        if (gatewayUrl.empty()) {
            response.errorMessage = "No gateway available";
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        std::string gatewayHost;
        uint16_t gatewayPort = 0;
        if (!parseGatewayEndpoint(gatewayUrl, gatewayHost, gatewayPort)) {
            response.errorMessage = "Invalid gateway endpoint: " + gatewayUrl;
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        std::string loginTicket;
        const auto sessionId = sessionManager_->createSession(playerId, gatewayUrl, loginTicket);
        if (!preparePlayerOnline(playerId, sessionId, gatewayUrl, errorMessage)) {
            sessionManager_->removeSession(sessionId);
            response.errorMessage = errorMessage;
            return protocol::MessageCodec::encode(response, header.sessionId);
        }

        response.success = true;
        response.playerId = playerId;
        response.sessionId = sessionId;
        response.gatewayHost = gatewayHost;
        response.gatewayPort = gatewayPort;
        response.loginTicket = loginTicket;

        std::cout << "Player " << loginReq.username << " (ID: " << playerId
                  << ") logged in, session: " << response.sessionId
                  << ", gateway: " << gatewayUrl << std::endl;
    } else {
        response.errorMessage = errorMessage;
        std::cout << "Failed login attempt for " << loginReq.username
                  << ": " << errorMessage << std::endl;
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> LoginServer::handleGatewayAssignRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto assignReq = protocol::MessageCodec::decodeBody<protocol::GatewayAssignRequest>(bodyData);

    protocol::GatewayAssignResponse response;
    response.success = false;

    // 验证会话
    PlayerID playerId = 0;
    std::string gatewayUrl;
    if (sessionManager_->validateSession(assignReq.sessionId, assignReq.loginTicket, playerId, gatewayUrl)) {
        response.success = true;
        response.token = assignReq.loginTicket;
        if (!parseGatewayEndpoint(gatewayUrl, response.gatewayHost, response.gatewayPort)) {
            response.success = false;
            response.errorMessage = "Invalid gateway endpoint";
        }
    } else {
        response.errorMessage = "Invalid or expired session";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> LoginServer::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

bool LoginServer::preparePlayerOnline(
    PlayerID playerId,
    SessionID sessionId,
    const std::string& gatewayUrl,
    std::string& errorMessage
) {
    try {
        protocol::PlayerActivateRequest activateReq;
        activateReq.playerId = playerId;
        const auto activateResp =
            baseAppClient_->call<protocol::PlayerActivateRequest, protocol::PlayerActivateResponse>(
                activateReq, sessionId
            );

        if (!activateResp.success) {
            errorMessage = activateResp.errorMessage.empty()
                ? "BaseApp rejected player activation"
                : activateResp.errorMessage;
            return false;
        }

        protocol::PlayerBindSessionRequest bindReq;
        bindReq.playerId = playerId;
        bindReq.sessionId = sessionId;
        bindReq.gatewayId = 1;
        bindReq.gatewayAddr = gatewayUrl;

        const auto bindResp =
            baseAppClient_->call<protocol::PlayerBindSessionRequest, protocol::PlayerBindSessionResponse>(
                bindReq, sessionId
            );

        if (!bindResp.success) {
            errorMessage = bindResp.errorMessage.empty()
                ? "BaseApp rejected session binding"
                : bindResp.errorMessage;
            return false;
        }

        protocol::PlayerAssignWorldRequest assignReq;
        assignReq.playerId = playerId;
        assignReq.worldId = config_.initialWorldId;
        assignReq.mapId = config_.initialMapId;
        assignReq.instanceId = config_.initialInstanceId;
        assignReq.spaceId = config_.initialSpaceId;
        assignReq.routeVersion = 1;

        const auto assignResp =
            baseAppClient_->call<protocol::PlayerAssignWorldRequest, protocol::PlayerAssignWorldResponse>(
                assignReq, sessionId
            );

        if (!assignResp.success) {
            errorMessage = assignResp.errorMessage.empty()
                ? "BaseApp rejected initial world assignment"
                : assignResp.errorMessage;
            return false;
        }

        return true;
    } catch (const std::exception& ex) {
        // In stub transport mode, RPC may not return a payload yet. Keep login flow usable.
        std::cerr << "Prepare player online degraded for player " << playerId
                  << ": " << ex.what() << std::endl;
        return true;
    }
}

bool LoginServer::parseGatewayEndpoint(
    const std::string& gatewayUrl,
    std::string& outHost,
    uint16_t& outPort
) {
    constexpr const char* scheme = "tcp://";
    std::string endpoint = gatewayUrl;
    if (endpoint.rfind(scheme, 0) == 0) {
        endpoint.erase(0, 6);
    }

    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos) {
        return false;
    }

    outHost = endpoint.substr(0, colon);
    if (outHost.empty()) {
        return false;
    }

    try {
        const auto port = std::stoi(endpoint.substr(colon + 1));
        if (port < 0 || port > 65535) {
            return false;
        }
        outPort = static_cast<uint16_t>(port);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace login
