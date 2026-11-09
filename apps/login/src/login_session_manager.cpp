#include "login/login_server.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <cstdint>
#include <iostream>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <filesystem>
#include "apollo/utils/crypto.h"

using apollo::utils::PasswordHash;

namespace login {
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

bool SessionManager::validateSessionByTicket(
    const std::string& loginTicket,
    SessionID& outSessionId,
    PlayerID& outPlayerId,
    std::string& outGatewayUrl
) {
    std::lock_guard<std::mutex> lock(mutex_);

    // L174：ticket 是跨端凭证主键——gateway 会话号与 login 会话号分属两个
    // 编号空间，准入校验不要求一致，以 ticket 反查 login 会话（会话号仅
    // 观测/目录追踪用）。
    if (loginTicket.empty()) {
        return false;
    }
    const auto ticketIt = tickets_.find(loginTicket);
    if (ticketIt == tickets_.end()) {
        return false;
    }
    const auto it = sessions_.find(ticketIt->second);
    if (it == sessions_.end()) {
        tickets_.erase(ticketIt);
        return false;
    }
    if (it->second.loginTicket != loginTicket) {
        return false;
    }

    const auto& info = it->second;
    int64_t now = getCurrentTimeMs();
    if (now - info.lastAccessMs > config_.sessionTimeoutMs) {
        tickets_.erase(ticketIt);
        sessions_.erase(it);
        return false;
    }
    const_cast<SessionInfo&>(info).lastAccessMs = now;

    outSessionId = info.sessionId;
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

} // namespace login
