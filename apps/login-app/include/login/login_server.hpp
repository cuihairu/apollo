#pragma once

#include "login/config.hpp"
#include "apollo/protocol/socket.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>

namespace login {

namespace protocol = apollo::protocol;

using PlayerID = uint64_t;
using SessionID = uint64_t;

// 登录尝试记录
struct LoginAttempt {
    int attempts;
    int64_t lastAttemptMs;
    int64_t lockoutUntilMs;
};

// 用户信息
struct UserInfo {
    PlayerID playerId;
    std::string username;
    std::string passwordHash;  // 实际应该是 bcrypt 等
    int64_t lastLoginMs;
    int64_t createdAtMs;
};

// 认证器
class Authenticator {
public:
    explicit Authenticator(const LoginConfig& config);

    // 验证账号密码
    bool authenticate(const std::string& username, const std::string& password,
                     PlayerID& outPlayerId, std::string& errorMessage);

    // 检查是否被锁定
    bool isLockedOut(const std::string& username);

    // 记录登录尝试
    void recordAttempt(const std::string& username, bool success);

private:
    LoginConfig config_;
    mutable std::mutex mutex_;

    // 用户数据库 (简化版，实际应该从 BaseApp 加载)
    std::unordered_map<std::string, UserInfo> users_;

    // 登录尝试记录
    std::unordered_map<std::string, LoginAttempt> loginAttempts_;

    // 初始化测试用户
    void initTestUsers();

    int64_t getCurrentTimeMs() const;
};

// 网关分配器
class GatewayAllocator {
public:
    explicit GatewayAllocator(const LoginConfig& config);

    // 选择最优网关
    std::string selectGateway();

    // 更新网关负载
    void updateGatewayLoad(const std::string& gatewayUrl, int connections);

private:
    LoginConfig config_;
    mutable std::mutex mutex_;

    struct GatewayInfo {
        std::string url;
        int connections;
        int64_t lastUpdateMs;
    };
    std::vector<GatewayInfo> gateways_;

    int64_t getCurrentTimeMs() const;
};

// 会话管理器
class SessionManager {
public:
    explicit SessionManager(const LoginConfig& config);

    // 创建会话
    SessionID createSession(PlayerID playerId, const std::string& gatewayUrl, std::string& outLoginTicket);

    // 验证会话
    bool validateSession(SessionID sessionId, const std::string& loginTicket,
                         PlayerID& outPlayerId, std::string& outGatewayUrl);

    // 移除会话
    void removeSession(SessionID sessionId);

    // 清理过期会话
    void cleanupExpiredSessions();

private:
    LoginConfig config_;
    mutable std::mutex mutex_;

    struct SessionInfo {
        SessionID sessionId;
        PlayerID playerId;
        std::string gatewayUrl;
        std::string loginTicket;
        int64_t createdAtMs;
        int64_t lastAccessMs;
    };
    std::unordered_map<SessionID, SessionInfo> sessions_;
    std::unordered_map<std::string, SessionID> tickets_;

    std::atomic<uint64_t> nextSessionId_{1};

    int64_t getCurrentTimeMs() const;
    std::string generateLoginTicket(PlayerID playerId, SessionID sessionId) const;
};

// LoginApp 服务器
class LoginServer {
public:
    explicit LoginServer(const LoginConfig& config);
    ~LoginServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

private:
    // 处理登录请求
    std::vector<uint8_t> handleLoginRequest(const std::vector<uint8_t>& request);

    // 处理网关分配请求
    std::vector<uint8_t> handleGatewayAssignRequest(const std::vector<uint8_t>& request);

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    bool preparePlayerOnline(PlayerID playerId, SessionID sessionId,
                             const std::string& gatewayUrl, std::string& errorMessage);
    static bool parseGatewayEndpoint(const std::string& gatewayUrl,
                                     std::string& outHost, uint16_t& outPort);

    LoginConfig config_;
    std::unique_ptr<Authenticator> authenticator_;
    std::unique_ptr<GatewayAllocator> gatewayAllocator_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::unique_ptr<protocol::RpcClient> baseAppClient_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};

    std::thread cleanupThread_;
};

} // namespace login
