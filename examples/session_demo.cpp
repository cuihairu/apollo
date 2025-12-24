/**
 * @file session_demo.cpp
 * @brief 会话管理示例
 *
 * 演示如何使用 SessionManager 管理网络连接
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <functional>
#include <shared_mutex>
#include <cstring>

namespace apollo {
namespace utils {

/// 简单的 ID 池实现
class IdPool {
public:
    IdPool(uint32_t minId, uint32_t maxId)
        : minId_(minId), maxId_(maxId), nextId_(minId) {}

    uint32_t allocate() {
        if (nextId_ > maxId_) {
            return UINT32_MAX;
        }
        return nextId_++;
    }

    void release(uint32_t id) {
        // 简化实现，不实际回收
    }

private:
    uint32_t minId_;
    uint32_t maxId_;
    uint32_t nextId_;
};

} // namespace utils

namespace net {

using utils::IdPool;

/// 会话状态
enum class SessionState : uint8_t {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Authenticated = 3,
    Closing = 4
};

/// 会话事件
enum class SessionEvent : uint8_t {
    Connected = 1,
    Disconnected = 2,
    DataReceived = 3,
    DataSent = 4,
    Error = 5,
    Timeout = 6,
    Authenticated = 7
};

/// 会话信息
struct SessionInfo {
    uint64_t sessionId;
    uint64_t playerId;
    std::string remoteAddress;
    uint16_t remotePort;
    SessionState state;
    uint64_t connectTime;
    uint64_t lastActiveTime;
    uint64_t totalRecvBytes;
    uint64_t totalSentBytes;
    uint32_t recvPackets;
    uint32_t sentPackets;

    SessionInfo()
        : sessionId(0), playerId(0), remotePort(0)
        , state(SessionState::Disconnected)
        , connectTime(0), lastActiveTime(0)
        , totalRecvBytes(0), totalSentBytes(0)
        , recvPackets(0), sentPackets(0) {}
};

/// 会话监听器
class ISessionListener {
public:
    virtual ~ISessionListener() = default;
    virtual void onEvent(SessionEvent event, uint64_t sessionId,
                        const void* data = nullptr, size_t length = 0) {}
};

/// 连接接口
class IConnection {
public:
    virtual ~IConnection() = default;
    virtual bool isConnected() const = 0;
    virtual bool send(const void* data, size_t length) = 0;
    virtual int receive(void* buffer, size_t size) = 0;
    virtual void close() = 0;
    virtual std::string getRemoteAddress() const = 0;
    virtual uint16_t getRemotePort() const = 0;
};

/// 模拟连接
class MockConnection : public IConnection {
public:
    MockConnection(const std::string& addr, uint16_t port)
        : address_(addr), port_(port), connected_(true) {}

    bool isConnected() const override { return connected_; }
    bool send(const void* data, size_t length) override {
        if (!connected_) return false;
        sentBytes_ += length;
        sentPackets_++;
        return true;
    }
    int receive(void* buffer, size_t size) override {
        if (!connected_) return -1;
        // 模拟没有数据
        return 0;
    }
    void close() override { connected_ = false; }
    std::string getRemoteAddress() const override { return address_; }
    uint16_t getRemotePort() const override { return port_; }

    uint64_t getSentBytes() const { return sentBytes_; }
    uint32_t getSentPackets() const { return sentPackets_; }

private:
    std::string address_;
    uint16_t port_;
    bool connected_;
    uint64_t sentBytes_ = 0;
    uint32_t sentPackets_ = 0;
};

/// 会话接口
class ISession {
public:
    virtual ~ISession() = default;
    virtual uint64_t getSessionId() const = 0;
    virtual uint64_t getPlayerId() const = 0;
    virtual void setPlayerId(uint64_t playerId) = 0;
    virtual SessionState getState() const = 0;
    virtual bool send(const void* data, size_t length) = 0;
    virtual void close(int reason = 0) = 0;
    virtual const SessionInfo& getInfo() const = 0;
};

/// 会话实现
class Session : public ISession {
public:
    Session(uint64_t sessionId, std::unique_ptr<IConnection> connection,
            ISessionListener* listener)
        : sessionId_(sessionId)
        , connection_(std::move(connection))
        , listener_(listener)
        , state_(SessionState::Connected) {

        info_.sessionId = sessionId_;
        info_.state = state_;
        info_.connectTime = getCurrentTimeMs();
        info_.lastActiveTime = info_.connectTime;
        info_.remoteAddress = connection_->getRemoteAddress();
        info_.remotePort = connection_->getRemotePort();
    }

    uint64_t getSessionId() const override { return sessionId_; }
    uint64_t getPlayerId() const override { return info_.playerId; }
    void setPlayerId(uint64_t playerId) override {
        info_.playerId = playerId;
        state_ = SessionState::Authenticated;
        info_.state = state_;
        if (listener_) {
            listener_->onEvent(SessionEvent::Authenticated, sessionId_);
        }
    }
    SessionState getState() const override { return state_; }

    bool send(const void* data, size_t length) override {
        if (state_ == SessionState::Disconnected ||
            state_ == SessionState::Closing) {
            return false;
        }

        if (connection_->send(data, length)) {
            info_.totalSentBytes += length;
            info_.sentPackets++;
            info_.lastActiveTime = getCurrentTimeMs();
            return true;
        }
        return false;
    }

    void close(int reason) override {
        if (state_ == SessionState::Disconnected) return;
        state_ = SessionState::Disconnected;
        connection_->close();
    }

    const SessionInfo& getInfo() const override { return info_; }

private:
    static uint64_t getCurrentTimeMs() {
        return std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
    }

    uint64_t sessionId_;
    std::unique_ptr<IConnection> connection_;
    ISessionListener* listener_;
    SessionState state_;
    SessionInfo info_;
};

/// 会话管理器
class SessionManager : public ISessionListener {
public:
    static SessionManager& instance() {
        static SessionManager instance;
        return instance;
    }

    bool initialize(size_t maxSessions = 1000) {
        maxSessions_ = maxSessions;
        sessionIdPool_ = std::make_unique<IdPool>(1, maxSessions);
        return true;
    }

    void shutdown() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        sessions_.clear();
        playerIdToSessionId_.clear();
    }

    ISession* createSession(std::unique_ptr<IConnection> connection) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (sessions_.size() >= maxSessions_) {
            return nullptr;
        }

        uint64_t sessionId = sessionIdPool_->allocate();
        if (sessionId == UINT32_MAX) {
            return nullptr;
        }

        auto session = std::make_unique<Session>(sessionId, std::move(connection), this);
        ISession* ptr = session.get();
        sessions_[sessionId] = std::move(session);

        if (listener_) {
            listener_->onEvent(SessionEvent::Connected, sessionId);
        }

        return ptr;
    }

    ISession* getSession(uint64_t sessionId) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        return (it != sessions_.end()) ? it->second.get() : nullptr;
    }

    ISession* getSessionByPlayerId(uint64_t playerId) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = playerIdToSessionId_.find(playerId);
        if (it == playerIdToSessionId_.end()) {
            return nullptr;
        }
        return getSession(it->second);
    }

    void closeSession(uint64_t sessionId, int reason = 0) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = sessions_.find(sessionId);
        if (it == sessions_.end()) return;

        // 移除玩家映射
        uint64_t playerId = it->second->getPlayerId();
        if (playerId != 0) {
            playerIdToSessionId_.erase(playerId);
        }

        it->second->close(reason);

        if (listener_) {
            listener_->onEvent(SessionEvent::Disconnected, sessionId);
        }

        sessions_.erase(it);
    }

    void closeSessionByPlayerId(uint64_t playerId, int reason = 0) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = playerIdToSessionId_.find(playerId);
        if (it == playerIdToSessionId_.end()) return;
        lock.unlock();
        closeSession(it->second, reason);
    }

    size_t getSessionCount() const {
        return sessions_.size();
    }

    std::vector<uint64_t> getAllSessionIds() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<uint64_t> result;
        result.reserve(sessions_.size());
        for (const auto& pair : sessions_) {
            result.push_back(pair.first);
        }
        return result;
    }

    bool kickPlayer(uint64_t playerId, int reason = 0) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = playerIdToSessionId_.find(playerId);
        if (it == playerIdToSessionId_.end()) {
            return false;
        }
        uint64_t sessionId = it->second;
        lock.unlock();
        closeSession(sessionId, reason);
        return true;
    }

    size_t broadcast(const void* data, size_t length, uint64_t exceptSessionId = 0) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        size_t sentCount = 0;
        for (const auto& pair : sessions_) {
            if (pair.first != exceptSessionId) {
                if (pair.second->send(data, length)) {
                    sentCount++;
                }
            }
        }
        return sentCount;
    }

    void setListener(ISessionListener* listener) { listener_ = listener; }

    // ISessionListener 实现
    void onEvent(SessionEvent event, uint64_t sessionId,
                const void* data, size_t length) override {
        if (listener_) {
            listener_->onEvent(event, sessionId, data, length);
        }

        if (event == SessionEvent::Authenticated) {
            auto* session = getSession(sessionId);
            if (session) {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                playerIdToSessionId_[session->getPlayerId()] = sessionId;
            }
        }
    }

private:
    SessionManager() = default;
    ~SessionManager() = default;

    std::unique_ptr<IdPool> sessionIdPool_;
    std::unordered_map<uint64_t, std::unique_ptr<Session>> sessions_;
    std::unordered_map<uint64_t, uint64_t> playerIdToSessionId_;
    size_t maxSessions_ = 1000;
    ISessionListener* listener_ = nullptr;
    mutable std::shared_mutex mutex_;
};

} // namespace net
} // namespace apollo

//==============================================================================
// 会话事件监听器
//==============================================================================

class GameSessionListener : public apollo::net::ISessionListener {
public:
    void onEvent(apollo::net::SessionEvent event, uint64_t sessionId,
                const void* data, size_t length) override {
        (void)data;
        (void)length;
        switch (event) {
            case apollo::net::SessionEvent::Connected:
                std::cout << "  [Event] Session " << sessionId << " connected" << std::endl;
                break;
            case apollo::net::SessionEvent::Disconnected:
                std::cout << "  [Event] Session " << sessionId << " disconnected" << std::endl;
                break;
            case apollo::net::SessionEvent::Authenticated:
                std::cout << "  [Event] Session " << sessionId << " authenticated" << std::endl;
                break;
            case apollo::net::SessionEvent::DataReceived:
                break;
            default:
                break;
        }
    }
};

//==============================================================================
// 示例场景
//==============================================================================

void example1_CreateSessions() {
    std::cout << "\n=== Example 1: Create Sessions ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();
    manager.initialize(100);

    GameSessionListener listener;
    manager.setListener(&listener);

    // 创建多个会话（模拟玩家连接）
    for (int i = 0; i < 5; ++i) {
        auto addr = "192.168.1." + std::to_string(100 + i);
        auto connection = std::make_unique<apollo::net::MockConnection>(addr, 12345);
        manager.createSession(std::move(connection));
    }

    std::cout << "  Current sessions: " << manager.getSessionCount() << std::endl;
}

void example2_AuthenticatePlayers() {
    std::cout << "\n=== Example 2: Authenticate Players ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();

    auto sessionIds = manager.getAllSessionIds();

    uint64_t playerIds[] = {1001, 1002, 1003, 1004, 1005};

    for (size_t i = 0; i < sessionIds.size() && i < 5; ++i) {
        auto* session = manager.getSession(sessionIds[i]);
        if (session) {
            session->setPlayerId(playerIds[i]);
            std::cout << "  Session " << sessionIds[i]
                      << " -> Player " << playerIds[i] << std::endl;
        }
    }
}

void example3_GetSessionByPlayer() {
    std::cout << "\n=== Example 3: Get Session by Player ID ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();

    auto* session = manager.getSessionByPlayerId(1003);
    if (session) {
        const auto& info = session->getInfo();
        std::cout << "  Found session for player 1003:" << std::endl;
        std::cout << "    Session ID: " << info.sessionId << std::endl;
        std::cout << "    Remote: " << info.remoteAddress << ":" << info.remotePort << std::endl;
        std::cout << "    State: " << static_cast<int>(info.state) << std::endl;
    }
}

void example4_BroadcastMessage() {
    std::cout << "\n=== Example 4: Broadcast Message ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();

    const char* message = "Hello from server!";
    size_t sent = manager.broadcast(message, std::strlen(message));

    std::cout << "  Broadcast sent to " << sent << " sessions" << std::endl;

    // 查看统计
    auto ids = manager.getAllSessionIds();
    for (auto id : ids) {
        auto* session = manager.getSession(id);
        if (session) {
            const auto& info = session->getInfo();
            std::cout << "    Session " << id
                      << ": " << info.sentPackets << " packets sent, "
                      << info.totalSentBytes << " bytes" << std::endl;
        }
    }
}

void example5_KickPlayer() {
    std::cout << "\n=== Example 5: Kick Player ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();

    std::cout << "  Before kick: " << manager.getSessionCount() << " sessions" << std::endl;

    if (manager.kickPlayer(1002)) {
        std::cout << "  Player 1002 kicked successfully" << std::endl;
    }

    std::cout << "  After kick: " << manager.getSessionCount() << " sessions" << std::endl;
}

void example6_Cleanup() {
    std::cout << "\n=== Example 6: Cleanup ===" << std::endl;

    auto& manager = apollo::net::SessionManager::instance();

    std::cout << "  Final session count: " << manager.getSessionCount() << std::endl;

    manager.shutdown();

    std::cout << "  After shutdown: " << manager.getSessionCount() << " sessions" << std::endl;
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Session Manager Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    example1_CreateSessions();
    example2_AuthenticatePlayers();
    example3_GetSessionByPlayer();
    example4_BroadcastMessage();
    example5_KickPlayer();
    example6_Cleanup();

    std::cout << "\n=== Demo Complete ===" << std::endl;

    return 0;
}
