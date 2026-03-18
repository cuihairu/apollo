#include "login/login_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

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

SessionID SessionManager::createSession(PlayerID playerId, const std::string& gatewayUrl) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionID sessionId = nextSessionId_++;

    SessionInfo info;
    info.sessionId = sessionId;
    info.playerId = playerId;
    info.gatewayUrl = gatewayUrl;
    info.createdAtMs = getCurrentTimeMs();
    info.lastAccessMs = info.createdAtMs;

    sessions_[sessionId] = info;

    return sessionId;
}

bool SessionManager::validateSession(SessionID sessionId, PlayerID& outPlayerId,
                                      std::string& outGatewayUrl) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
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
    sessions_.erase(sessionId);
}

void SessionManager::cleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = getCurrentTimeMs();
    int64_t expiryMs = config_.sessionTimeoutMs;

    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.lastAccessMs > expiryMs) {
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

//==============================================================================
// LoginServer 实现
//==============================================================================

LoginServer::LoginServer(const LoginConfig& config)
    : config_(config)
    , authenticator_(std::make_unique<Authenticator>(config))
    , gatewayAllocator_(std::make_unique<GatewayAllocator>(config))
    , sessionManager_(std::make_unique<SessionManager>(config)) {
}

LoginServer::~LoginServer() {
    stop();
}

void LoginServer::start() {
    if (running_) return;

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
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    server_->start();

    // 启动会话清理线程
    cleanupThread_ = std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(60));
            sessionManager_->cleanupExpiredSessions();
        }
    });

    running_ = true;

    std::cout << "Login server listening on " << config_.host << ":" << config_.port << std::endl;
}

void LoginServer::stop() {
    running_ = false;
    server_.stop();
    server_.reset();

    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
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
        response.success = true;
        response.playerId = playerId;

        // 选择网关
        response.gatewayHost = "127.0.0.1";  // 解析 URL
        response.gatewayPort = 8888;
        response.gatewayHost = gatewayAllocator_->selectGateway();

        // 创建会话
        response.sessionId = sessionManager_->createSession(playerId, response.gatewayHost);

        std::cout << "Player " << loginReq.username << " (ID: " << playerId
                  << ") logged in, session: " << response.sessionId << std::endl;
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
    if (sessionManager_->validateSession(assignReq.sessionId, playerId, gatewayUrl)) {
        response.success = true;

        // 解析 gateway URL
        // 简化处理
        response.gatewayHost = "127.0.0.1";
        response.gatewayPort = 8888;
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

} // namespace login
