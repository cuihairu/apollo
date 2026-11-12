/**
 * @file session_manager.cpp
 * @brief 会话管理器实现
 */

#include "apollo/net/session_manager.h"
#include "apollo/utils/time.h"
#include <algorithm>
#include <cstring>

namespace apollo {
namespace net {

//==============================================================================
// Session 实现
//==============================================================================

Session::Session(uint64_t sessionId, std::unique_ptr<IConnection> connection,
                 ISessionListener* listener)
    : connection_(std::move(connection))
    , listener_(listener)
    , timeoutMs_(300000)  // 默认 5 分钟超时
    , sendBufferOffset_(0) {
    info_.sessionId = sessionId;
    info_.state = SessionState::Connecting;
    state_.store(SessionState::Connecting, std::memory_order_release);
    info_.connectTime = static_cast<uint64_t>(utils::Time::now());
    info_.lastActiveTime = info_.connectTime;

    if (connection_) {
        info_.remoteAddress = connection_->getRemoteAddress();
        info_.remotePort = connection_->getRemotePort();
    }

    sendBuffer_.reserve(SEND_BUFFER_SIZE);
}

Session::~Session() {
    if (connection_) {
        connection_->close();
    }
}

void Session::markConnected() {
    state_.store(SessionState::Connected, std::memory_order_release);
    if (listener_) {
        listener_->onConnected(info_.sessionId);
    }
}

bool Session::send(const void* data, size_t length) {
    if (!connection_ || !connection_->isConnected()) {
        return false;
    }

    const auto st = state_.load(std::memory_order_acquire);
    if (st == SessionState::Closing ||
        st == SessionState::Disconnected) {
        return false;
    }

    // 如果有待发送数据，先加入缓冲区
    if (sendBufferOffset_ > 0) {
        // 简化处理：直接尝试发送
        // 实际应该缓冲后继续发送
    }

    const bool sent = connection_->send(data, length);
    if (sent) {
        info_.totalSentBytes += length;
        info_.sentPackets++;
        info_.lastActiveTime = static_cast<uint64_t>(utils::Time::now());

        if (listener_) {
            listener_->onDataSent(info_.sessionId);
        }
        return true;
    }

    return false;
}

bool Session::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

void Session::close(int reason) {
    if (state_.load(std::memory_order_acquire) == SessionState::Disconnected) {
        return;
    }

    state_.store(SessionState::Disconnected, std::memory_order_release);

    if (connection_) {
        connection_->close();
    }

    if (listener_) {
        listener_->onDisconnected(info_.sessionId, reason);
    }
}

void Session::update(uint64_t currentTime) {
    // 检查超时
    uint64_t elapsed = currentTime - info_.lastActiveTime;
    if (elapsed > timeoutMs_) {
        if (listener_) {
            listener_->onTimeout(info_.sessionId);
        }
        close(1);  // 超时断开
    }
}

void Session::handleReceivedData(const void* data, size_t length) {
    info_.totalRecvBytes += length;
    info_.recvPackets++;
    info_.lastActiveTime = static_cast<uint64_t>(utils::Time::now());

    if (listener_) {
        listener_->onDataReceived(info_.sessionId, data, length);
    }
}

//==============================================================================
// SessionManager 实现
//==============================================================================

SessionManager::SessionManager()
    : sessionCount_(0)
    , maxSessions_(10000)
    , defaultTimeoutMs_(300000)
    , listener_(nullptr) {
}

SessionManager::~SessionManager() {
    shutdown();
}

SessionManager& SessionManager::instance() {
    static SessionManager instance;
    return instance;
}

bool SessionManager::initialize(uint32_t maxSessions) {
    std::lock_guard<std::mutex> lock(mutex_);

    maxSessions_ = maxSessions;
    sessionIdPool_ = utils::IdPool(1, maxSessions);
    return true;
}

void SessionManager::shutdown() {
    std::vector<std::shared_ptr<Session>> toClose;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        toClose.reserve(sessions_.size());
        for (auto& pair : sessions_) {
            toClose.push_back(std::move(pair.second));
        }
        sessions_.clear();
        playerIdToSessionId_.clear();
        sessionCount_ = 0;
    }

    // 锁外 close：onDisconnected 回调可重入 manager（audit P1#10）
    for (auto& session : toClose) {
        session->close(0);
    }
}

ISession* SessionManager::createSession(std::unique_ptr<IConnection> connection) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessionCount_ >= maxSessions_) {
        return nullptr;  // 会话已满
    }

    uint64_t sessionId = sessionIdPool_.allocate();
    if (sessionId == UINT32_MAX) {
        return nullptr;  // ID 耗尽
    }

    auto session = std::make_shared<Session>(sessionId, std::move(connection), listener_);
    session->setTimeout(defaultTimeoutMs_);
    session->markConnected();

    ISession* ptr = session.get();
    sessions_[sessionId] = std::move(session);
    sessionCount_++;

    return ptr;
}

ISession* SessionManager::getSession(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    return (it != sessions_.end()) ? it->second.get() : nullptr;
}

ISession* SessionManager::getSessionByPlayerId(uint64_t playerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = playerIdToSessionId_.find(playerId);
    if (it == playerIdToSessionId_.end()) {
        return nullptr;
    }
    auto sessionIt = sessions_.find(it->second);
    if (sessionIt == sessions_.end()) {
        return nullptr;
    }

    return sessionIt->second.get();
}

void SessionManager::closeSession(uint64_t sessionId, int reason) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session = internalCloseSession(sessionId, reason);
    }
    // 锁外 close：onDisconnected 回调可重入 manager（audit P1#10）
    if (session) {
        session->close(reason);
    }
}

void SessionManager::closeSessionByPlayerId(uint64_t playerId, int reason) {
    uint64_t sessionId = 0;
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = playerIdToSessionId_.find(playerId);
        if (it == playerIdToSessionId_.end()) {
            return;
        }
        sessionId = it->second;
        session = internalCloseSession(sessionId, reason);
    }
    // 锁外 close（audit P1#10）
    if (session) {
        session->close(reason);
    }
}

// 仅做映射簿记并摘出会话，不在锁内触发任何监听回调：close() 会调
// listener_->onDisconnected，监听器典型实现会重入 manager（查会话/踢人/
// 广播），持 mutex_ 调 close 即自死锁（std::mutex 非递归，audit P1#10）。
// 调用方出锁后自行 close。
std::shared_ptr<Session> SessionManager::internalCloseSession(uint64_t sessionId, int reason) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return nullptr;
    }

    // 移除玩家映射
    std::shared_ptr<Session> session = std::move(it->second);
    uint64_t playerId = session->getPlayerId();
    if (playerId != 0) {
        playerIdToSessionId_.erase(playerId);
    }

    sessions_.erase(it);
    sessionIdPool_.release(sessionId);
    sessionCount_--;

    return session;
}

void SessionManager::update(uint64_t currentTime) {
    // 超时判定只读快照在锁内完成；onTimeout 与关会话（会触发 onDisconnected）
    // 都出锁触发——监听器可重入 manager，原实现锁内调 session->update 即
    // 自死锁（audit P1#10）
    std::vector<std::shared_ptr<Session>> timedOut;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        timedOut.reserve(sessions_.size());
        for (auto& pair : sessions_) {
            const SessionInfo info = pair.second->getInfo();
            if (info.state != SessionState::Disconnected &&
                currentTime - info.lastActiveTime > pair.second->getTimeoutMs()) {
                timedOut.push_back(pair.second);
            }
        }
    }

    for (auto& session : timedOut) {
        if (listener_) {
            listener_->onTimeout(session->getSessionId());
        }
        // Session::close 对已断开会话幂等早退，不会双发 onDisconnected
        session->close(1);
    }

    // 清理已断开的会话（internalCloseSession 仅簿记不触发回调，可锁内调用；
    // close 已在其他路径出锁完成过，此处不重复）
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint64_t> deadIds;
        deadIds.reserve(sessions_.size());
        for (auto& pair : sessions_) {
            if (pair.second->getState() == SessionState::Disconnected) {
                deadIds.push_back(pair.first);
            }
        }
        for (uint64_t id : deadIds) {
            internalCloseSession(id, 0);
        }
    }
}

void SessionManager::onConnected(uint64_t sessionId) {
    if (listener_) {
        listener_->onConnected(sessionId);
    }
}

void SessionManager::onDisconnected(uint64_t sessionId, int reason) {
    // 通知上层
    if (listener_) {
        listener_->onDisconnected(sessionId, reason);
    }

    // 移除映射
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(sessionId);
        if (it != sessions_.end()) {
            uint64_t playerId = it->second->getPlayerId();
            if (playerId != 0) {
                playerIdToSessionId_.erase(playerId);
            }
        }
    }
}

void SessionManager::onDataReceived(uint64_t sessionId, const void* data, size_t length) {
    if (listener_) {
        listener_->onDataReceived(sessionId, data, length);
    }
}

void SessionManager::onDataSent(uint64_t sessionId) {
    if (listener_) {
        listener_->onDataSent(sessionId);
    }
}

void SessionManager::onAuthenticated(uint64_t sessionId, uint64_t playerId) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        playerIdToSessionId_[playerId] = sessionId;
    }

    if (listener_) {
        listener_->onAuthenticated(sessionId, playerId);
    }
}

void SessionManager::onTimeout(uint64_t sessionId) {
    if (listener_) {
        listener_->onTimeout(sessionId);
    }
}

std::vector<uint64_t> SessionManager::getAllSessionIds() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint64_t> result;
    result.reserve(sessions_.size());

    for (const auto& pair : sessions_) {
        result.push_back(pair.first);
    }

    return result;
}

bool SessionManager::kickPlayer(uint64_t playerId, int reason) {
    std::shared_ptr<Session> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = playerIdToSessionId_.find(playerId);
        if (it == playerIdToSessionId_.end()) {
            return false;
        }

        session = internalCloseSession(it->second, reason);
    }
    // 锁外 close（audit P1#10）
    if (session) {
        session->close(reason);
    }
    return true;
}

size_t SessionManager::broadcast(const void* data, size_t length, uint64_t exceptSessionId) {
    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions.reserve(sessions_.size());
        for (auto& pair : sessions_) {
            if (pair.first != exceptSessionId) {
                sessions.push_back(pair.second);
            }
        }
    }

    size_t sentCount = 0;
    for (const auto& session : sessions) {
        if (session->send(data, length)) {
            ++sentCount;
        }
    }

    return sentCount;
}

//==============================================================================
// SessionHelper 实现
//==============================================================================

namespace SessionHelper {

bool sendPacket(ISession* session, const void* data, size_t length) {
    if (!session) {
        return false;
    }

    // 头体合并单次发送（audit P1#10）：原实现长度头与体分两次 send，第二
    // 次失败时长度头已上流而体缺席——对端解析器把后续包的字节当体吞掉，
    // 整条流从此错位（发送失败必须断连，流不可续）。
    uint32_t packetLength = static_cast<uint32_t>(length);
    std::vector<uint8_t> packet(4 + length);
    packet[0] = static_cast<uint8_t>((packetLength >> 24) & 0xFF);
    packet[1] = static_cast<uint8_t>((packetLength >> 16) & 0xFF);
    packet[2] = static_cast<uint8_t>((packetLength >> 8) & 0xFF);
    packet[3] = static_cast<uint8_t>(packetLength & 0xFF);
    if (data && length > 0) {
        std::memcpy(packet.data() + 4, data, length);
    }

    if (!session->send(packet.data(), packet.size())) {
        // 发送失败：断连防止错位流上继续解析（audit P1#10 修复）
        session->close(2);
        return false;
    }

    return true;
}

void processPacket(ISession* session, const uint8_t* buffer, size_t length,
                   std::function<void(const void*, size_t)> handler) {
    if (!session || !buffer || length < 4) {
        return;
    }

    static thread_local utils::ByteLoopBuffer packetBuffer(64 * 1024);

    // 写入接收到的数据
    packetBuffer.write(buffer, length);

    // 处理完整包
    while (packetBuffer.availableRead() >= 4) {
        // 读取包长度
        uint8_t lengthHeader[4];
        (void)packetBuffer.peek(lengthHeader, 4);

        uint32_t packetLength = (static_cast<uint32_t>(lengthHeader[0]) << 24) |
                                (static_cast<uint32_t>(lengthHeader[1]) << 16) |
                                (static_cast<uint32_t>(lengthHeader[2]) << 8) |
                                static_cast<uint32_t>(lengthHeader[3]);

        if (packetLength > 64 * 1024) {
            // 包太大，异常处理
            packetBuffer.clear();
            return;
        }

        if (packetBuffer.availableRead() < 4 + packetLength) {
            // 数据不完整，等待更多数据
            break;
        }

        // 跳过长度头
        packetBuffer.skip(4);

        // 读取包数据
        std::vector<uint8_t> packetData(packetLength);
        packetBuffer.read(packetData.data(), packetLength);

        // 调用处理器
        if (handler) {
            handler(packetData.data(), packetLength);
        }
    }
}

} // namespace SessionHelper

} // namespace net
} // namespace apollo
