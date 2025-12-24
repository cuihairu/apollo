#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include "apollo/utils/id_pool.h"
#include "apollo/utils/loop_buffer.h"
#include "apollo/utils/data_queue.h"

namespace apollo {
namespace net {

class ISession;
class IConnection;

/// 会话状态
enum class SessionState {
    Disconnected = 0,
    Connecting = 1,
    Connected = 2,
    Authenticated = 3,
    Closing = 4
};

/// 会话事件
enum class SessionEvent {
    Connected = 1,
    Disconnected = 2,
    DataReceived = 3,
    DataSent = 4,
    Error = 5,
    Timeout = 6
};

/// 会话信息
struct SessionInfo {
    uint64_t sessionId;
    uint64_t playerId;
    std::string remoteAddress;
    uint16_t remotePort;
    SessionState state;
    uint64_t connectTime;      // 连接时间（毫秒时间戳）
    uint64_t lastActiveTime;   // 最后活跃时间
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

    /// 连接建立
    virtual void onConnected(uint64_t sessionId) {}

    /// 连接断开
    virtual void onDisconnected(uint64_t sessionId, int reason) {}

    /// 数据接收
    virtual void onDataReceived(uint64_t sessionId, const void* data, size_t length) {}

    /// 认证成功
    virtual void onAuthenticated(uint64_t sessionId, uint64_t playerId) {}

    /// 会话超时
    virtual void onTimeout(uint64_t sessionId) {}
};

/// 会话接口
class ISession {
public:
    virtual ~ISession() = default;

    /// 获取会话 ID
    virtual uint64_t getSessionId() const = 0;

    /// 获取玩家 ID
    virtual uint64_t getPlayerId() const = 0;

    /// 设置玩家 ID
    virtual void setPlayerId(uint64_t playerId) = 0;

    /// 获取会话状态
    virtual SessionState getState() const = 0;

    /// 发送数据
    virtual bool send(const void* data, size_t length) = 0;

    /// 发送数据（向量）
    virtual bool send(const std::vector<uint8_t>& data) = 0;

    /// 关闭会话
    virtual void close(int reason = 0) = 0;

    /// 更新会话（检测超时等）
    virtual void update(uint64_t currentTime) = 0;

    /// 获取会话信息
    virtual const SessionInfo& getInfo() const = 0;
};

/// 网络连接接口
class IConnection {
public:
    virtual ~IConnection() = default;

    virtual int getFd() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool send(const void* data, size_t length) = 0;
    virtual int receive(void* buffer, size_t size) = 0;
    virtual void close() = 0;
    virtual std::string getRemoteAddress() const = 0;
    virtual uint16_t getRemotePort() const = 0;
};

/// 会话实现
class Session : public ISession {
public:
    Session(uint64_t sessionId, std::unique_ptr<IConnection> connection,
            ISessionListener* listener);
    ~Session() override;

    // ISession 实现
    uint64_t getSessionId() const override { return info_.sessionId; }
    uint64_t getPlayerId() const override { return info_.playerId; }
    void setPlayerId(uint64_t playerId) override {
        info_.playerId = playerId;
        if (info_.state == SessionState::Connected) {
            info_.state = SessionState::Authenticated;
            if (listener_) {
                listener_->onAuthenticated(info_.sessionId, playerId);
            }
        }
    }
    SessionState getState() const override { return info_.state; }

    bool send(const void* data, size_t length) override;
    bool send(const std::vector<uint8_t>& data) override;
    void close(int reason = 0) override;
    void update(uint64_t currentTime) override;
    const SessionInfo& getInfo() const override { return info_; }

    /// 设置超时时间（毫秒）
    void setTimeout(uint32_t timeoutMs) { timeoutMs_ = timeoutMs; }

    /// 标记为已连接
    void markConnected();

    /// 处理接收的数据
    void handleReceivedData(const void* data, size_t length);

    /// 获取连接对象
    IConnection* getConnection() { return connection_.get(); }

private:
    SessionInfo info_;
    std::unique_ptr<IConnection> connection_;
    ISessionListener* listener_;
    uint32_t timeoutMs_;

    // 发送缓冲区
    static constexpr size_t SEND_BUFFER_SIZE = 64 * 1024;
    std::vector<uint8_t> sendBuffer_;
    size_t sendBufferOffset_;

    // 接收缓冲区
    static constexpr size_t RECV_BUFFER_SIZE = 64 * 1024;
    uint8_t recvBuffer_[RECV_BUFFER_SIZE];
};

/// 会话管理器
class SessionManager : public ISessionListener {
public:
    /// 获取单例
    static SessionManager& instance();

    /// 初始化
    bool initialize(uint32_t maxSessions = 10000);

    /// 关闭
    void shutdown();

    /// 创建会话
    ISession* createSession(std::unique_ptr<IConnection> connection);

    /// 获取会话
    ISession* getSession(uint64_t sessionId);

    /// 通过玩家 ID 获取会话
    ISession* getSessionByPlayerId(uint64_t playerId);

    /// 关闭会话
    void closeSession(uint64_t sessionId, int reason = 0);

    /// 通过玩家 ID 关闭会话
    void closeSessionByPlayerId(uint64_t playerId, int reason = 0);

    /// 更新所有会话
    void update(uint64_t currentTime);

    /// 获取当前会话数
    size_t getSessionCount() const { return sessionCount_; }

    /// 获取最大会话数
    size_t getMaxSessions() const { return maxSessions_; }

    /// 设置监听器
    void setListener(ISessionListener* listener) { listener_ = listener; }

    /// 设置默认超时时间
    void setDefaultTimeout(uint32_t timeoutMs) { defaultTimeoutMs_ = timeoutMs; }

    // ISessionListener 实现
    void onConnected(uint64_t sessionId) override;
    void onDisconnected(uint64_t sessionId, int reason) override;
    void onDataReceived(uint64_t sessionId, const void* data, size_t length) override;
    void onAuthenticated(uint64_t sessionId, uint64_t playerId) override;
    void onTimeout(uint64_t sessionId) override;

    /// 获取所有会话 ID
    std::vector<uint64_t> getAllSessionIds() const;

    /// 踢出玩家
    bool kickPlayer(uint64_t playerId, int reason = 0);

    /// 广播消息给所有会话
    size_t broadcast(const void* data, size_t length, uint64_t exceptSessionId = 0);

private:
    SessionManager();
    ~SessionManager();

    void internalCloseSession(uint64_t sessionId, int reason);

    utils::IdPool sessionIdPool_;
    std::unordered_map<uint64_t, std::unique_ptr<Session>> sessions_;
    std::unordered_map<uint64_t, uint64_t> playerIdToSessionId_;  // playerId -> sessionId
    size_t sessionCount_;
    size_t maxSessions_;
    uint32_t defaultTimeoutMs_;
    ISessionListener* listener_;
    mutable std::mutex mutex_;
};

/// 会话辅助函数
namespace SessionHelper {

/// 发送数据包（带长度头）
bool sendPacket(ISession* session, const void* data, size_t length);

/// 接收并处理完整数据包
void processPacket(ISession* session, const uint8_t* buffer, size_t length,
                   std::function<void(const void*, size_t)> handler);

} // namespace SessionHelper

} // namespace net
} // namespace apollo
