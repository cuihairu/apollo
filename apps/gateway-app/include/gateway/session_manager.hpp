#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <vector>

namespace gateway {

using SessionID = uint64_t;
using PlayerID = uint64_t;

// 会话状态
enum class SessionState {
    CONNECTING,     // 连接中
    AUTHENTICATED,  // 已认证
    IN_GAME,        // 游戏中
    DISCONNECTING,  // 断开中
    DISCONNECTED    // 已断开
};

// 客户端连接信息
struct ClientConnection {
    SessionID sessionId;
    PlayerID playerId;
    std::string clientIP;
    uint16_t clientPort;
    SessionState state;
    int64_t lastHeartbeatMs;

    // 后端服务分配
    std::string assignedCellApp;  // 分配的 CellApp 地址
};

// 会话管理器
class SessionManager {
public:
    SessionManager();
    ~SessionManager() = default;

    // 创建新会话
    SessionID createSession(const std::string& clientIP, uint16_t clientPort);

    // 获取会话
    std::shared_ptr<ClientConnection> getSession(SessionID sessionId);

    // 移除会话
    void removeSession(SessionID sessionId);

    // 绑定玩家
    void bindPlayer(SessionID sessionId, PlayerID playerId);

    // 设置会话状态
    void setState(SessionID sessionId, SessionState state);

    // 更新心跳
    void updateHeartbeat(SessionID sessionId);

    // 分配 CellApp
    void assignCellApp(SessionID sessionId, const std::string& cellAppUrl);

    // 获取所有会话
    std::vector<std::shared_ptr<ClientConnection>> getAllSessions();

    // 获取会话数量
    size_t getSessionCount() const;

    // 检查超时会话
    std::vector<SessionID> checkTimeouts(int timeoutMs);

    // 是否存在会话
    bool hasSession(SessionID sessionId) const;

private:
    int64_t getCurrentTimeMs() const;

    mutable std::mutex mutex_;
    std::unordered_map<SessionID, std::shared_ptr<ClientConnection>> sessions_;
    std::atomic<uint64_t> nextSessionId_{1};
};

} // namespace gateway
