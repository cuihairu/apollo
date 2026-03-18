#include "gateway/session_manager.hpp"
#include <chrono>

namespace gateway {

SessionManager::SessionManager() {
    // 初始化
}

SessionID SessionManager::createSession(const std::string& clientIP, uint16_t clientPort) {
    std::lock_guard<std::mutex> lock(mutex_);

    SessionID sessionId = nextSessionId_++;

    auto session = std::make_shared<ClientConnection>();
    session->sessionId = sessionId;
    session->playerId = 0;
    session->clientIP = clientIP;
    session->clientPort = clientPort;
    session->state = SessionState::CONNECTING;
    session->lastHeartbeatMs = getCurrentTimeMs();

    sessions_[sessionId] = session;

    return sessionId;
}

std::shared_ptr<ClientConnection> SessionManager::getSession(SessionID sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        return it->second;
    }
    return nullptr;
}

void SessionManager::removeSession(SessionID sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(sessionId);
}

void SessionManager::bindPlayer(SessionID sessionId, PlayerID playerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->playerId = playerId;
        it->second->state = SessionState::IN_GAME;
    }
}

void SessionManager::setState(SessionID sessionId, SessionState state) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->state = state;
    }
}

void SessionManager::updateHeartbeat(SessionID sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->lastHeartbeatMs = getCurrentTimeMs();
    }
}

void SessionManager::assignCellApp(SessionID sessionId, const std::string& cellAppUrl) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        it->second->assignedCellApp = cellAppUrl;
    }
}

std::vector<std::shared_ptr<ClientConnection>> SessionManager::getAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ClientConnection>> result;
    result.reserve(sessions_.size());

    for (auto& pair : sessions_) {
        result.push_back(pair.second);
    }

    return result;
}

size_t SessionManager::getSessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

std::vector<SessionID> SessionManager::checkTimeouts(int timeoutMs) {
    std::vector<SessionID> timeoutSessions;

    std::lock_guard<std::mutex> lock(mutex_);

    int64_t now = getCurrentTimeMs();

    for (auto& pair : sessions_) {
        int64_t elapsed = now - pair.second->lastHeartbeatMs;
        if (elapsed > timeoutMs) {
            timeoutSessions.push_back(pair.first);
        }
    }

    return timeoutSessions;
}

bool SessionManager::hasSession(SessionID sessionId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.find(sessionId) != sessions_.end();
}

int64_t SessionManager::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace gateway
