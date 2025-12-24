/**
 * @file session_manager.cpp
 * @brief 会话管理器实现
 */

#include "apollo/net/session_manager.h"
#include "apollo/utils/time.h"
#include <algorithm>

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
    info_.connectTime = Time::now();
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
    info_.state = SessionState::Connected;
    if (listener_) {
        listener_->onConnected(info_.sessionId);
    }
}

bool Session::send(const void* data, size_t length) {
    if (!connection_ || !connection_->isConnected()) {
        return false;
    }

    if (info_.state == SessionState::Closing ||
        info_.state == SessionState::Disconnected) {
        return false;
    }

    // 如果有待发送数据，先加入缓冲区
    if (sendBufferOffset_ > 0) {
        // 简化处理：直接尝试发送
        // 实际应该缓冲后继续发送
    }

    int sent = connection_->send(data, static_cast<int>(length));
    if (sent > 0) {
        info_.totalSentBytes += sent;
        info_.sentPackets++;
        info_.lastActiveTime = Time::now();

        if (listener_) {
            listener_->onDataSent(info_.sessionId);
        }
        return static_cast<size_t>(sent) == length;
    }

    return false;
}

bool Session::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

void Session::close(int reason) {
    if (info_.state == SessionState::Disconnected) {
        return;
    }

    info_.state = SessionState::Disconnected;

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
    info_.lastActiveTime = Time::now();

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
    std::lock_guard<std::mutex> lock(mutex_);

    // 关闭所有会话
    for (auto& pair : sessions_) {
        pair.second->close(0);
    }
    sessions_.clear();
    playerIdToSessionId_.clear();
    sessionCount_ = 0;
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

    auto session = std::make_unique<Session>(sessionId, std::move(connection), listener_);
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

    return getSession(it->second);
}

void SessionManager::closeSession(uint64_t sessionId, int reason) {
    std::lock_guard<std::mutex> lock(mutex_);
    internalCloseSession(sessionId, reason);
}

void SessionManager::closeSessionByPlayerId(uint64_t playerId, int reason) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = playerIdToSessionId_.find(playerId);
    if (it == playerIdToSessionId_.end()) {
        return;
    }

    internalCloseSession(it->second, reason);
}

void SessionManager::internalCloseSession(uint64_t sessionId, int reason) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return;
    }

    // 移除玩家映射
    uint64_t playerId = it->second->getPlayerId();
    if (playerId != 0) {
        playerIdToSessionId_.erase(playerId);
    }

    // 关闭会话
    it->second->close(reason);
    sessions_.erase(it);
    sessionIdPool_.release(sessionId);
    sessionCount_--;
}

void SessionManager::update(uint64_t currentTime) {
    std::vector<uint64_t> toRemove;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& pair : sessions_) {
            pair.second->update(currentTime);

            // 清理已断开的会话
            if (pair.second->getState() == SessionState::Disconnected) {
                toRemove.push_back(pair.first);
            }
        }
    }

    // 移除断开的会话
    for (uint64_t sessionId : toRemove) {
        closeSession(sessionId, 0);
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
        auto* session = getSession(sessionId);
        if (session) {
            uint64_t playerId = session->getPlayerId();
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
    return closeSessionByPlayerId(playerId, reason);
}

size_t SessionManager::broadcast(const void* data, size_t length, uint64_t exceptSessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t sentCount = 0;

    for (auto& pair : sessions_) {
        if (pair.first != exceptSessionId) {
            if (pair.second->send(data, length)) {
                sentCount++;
            }
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

    // 发送长度头（4字节，大端序）
    uint32_t packetLength = static_cast<uint32_t>(length);
    uint8_t lengthHeader[4];
    lengthHeader[0] = (packetLength >> 24) & 0xFF;
    lengthHeader[1] = (packetLength >> 16) & 0xFF;
    lengthHeader[2] = (packetLength >> 8) & 0xFF;
    lengthHeader[3] = packetLength & 0xFF;

    // 先发送长度头
    if (!session->send(lengthHeader, 4)) {
        return false;
    }

    // 再发送数据
    return session->send(data, length);
}

void processPacket(ISession* session, const uint8_t* buffer, size_t length,
                   std::function<void(const void*, size_t)> handler) {
    if (!session || !buffer || length < 4) {
        return;
    }

    static thread_local ByteLoopBuffer packetBuffer(64 * 1024);

    // 写入接收到的数据
    packetBuffer.write(buffer, length);

    // 处理完整包
    while (packetBuffer.availableRead() >= 4) {
        // 读取包长度
        uint8_t lengthHeader[4];
        size_t peeked = packetBuffer.peek(lengthHeader, 4);

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
