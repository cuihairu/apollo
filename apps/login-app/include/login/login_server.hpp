#pragma once

#include "apollo/protocol/messages.hpp"
#include "login/config.hpp"
#include "login/m1_admission_server.hpp"
#include "login/m1_base_client.hpp"
#include "online_directory/online_directory.hpp"
#include <condition_variable>
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
    online_directory::OnlineDirectory* onlineDirectory() noexcept { return directory_.get(); }
    std::uint16_t admissionPort() const { return m1Admission_ ? m1Admission_->listenPort() : 0; }
    std::vector<std::uint8_t> dispatchRequest(const std::vector<std::uint8_t>& request);

    // 网关分配请求处理（M1 准入面 handler 直通——MessageCodec 全帧进出，
    // 会话校验逻辑与 M1 面直通零分叉）
    std::vector<uint8_t> handleGatewayAssignRequest(const std::vector<uint8_t>& request);

private:
    // 处理登录请求
    std::vector<uint8_t> handleLoginRequest(const std::vector<uint8_t>& request);

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    bool preparePlayerOnline(PlayerID playerId, SessionID sessionId,
                             const std::string& gatewayUrl, std::string& errorMessage);
    static bool parseGatewayEndpoint(const std::string& gatewayUrl,
                                     std::string& outHost, uint16_t& outPort);

    LoginConfig config_;
    std::unique_ptr<online_directory::OnlineDirectory> directory_;
    std::mutex cleanupMutex_;
    std::condition_variable cleanupWake_;
    std::unique_ptr<Authenticator> authenticator_;
    std::unique_ptr<GatewayAllocator> gatewayAllocator_;
    std::unique_ptr<SessionManager> sessionManager_;
    std::atomic<bool> running_{false};

    std::thread cleanupThread_;

    // M1 准入受理面（P4-4 批①；m1Port = 0 = 不装配）
    std::unique_ptr<M1AdmissionServer> m1Admission_;

    // M1 base 客户端面（P4-4 批③；worldStoreM1Port = 0 = 不装配，RPC 面缺席
    // = 降级缝 throw）
    std::unique_ptr<M1BaseClient> worldStoreClient_;

    // M1 manager 客户端面（P4-4 批④；m1OnlineDirectoryPort = 0 = 不装配，同上）
    std::unique_ptr<M1BaseClient> m1OnlineDirectoryClient_;
};

} // namespace login
