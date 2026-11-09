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
// Authenticator 实现
//==============================================================================

Authenticator::Authenticator(const LoginConfig& config)
    : config_(config) {
    if (config_.enableTestUsers) {
        initTestUsers();
    }
}

void Authenticator::initTestUsers() {
    // 创建测试用户
    UserInfo user1;
    user1.playerId = 1001;
    user1.username = "player1";
    user1.passwordHash = PasswordHash::format(PasswordHash::hash("pass1"));
    user1.lastLoginMs = 0;
    user1.createdAtMs = 0;

    users_["player1"] = user1;

    UserInfo user2;
    user2.playerId = 1002;
    user2.username = "player2";
    user2.passwordHash = PasswordHash::format(PasswordHash::hash("pass2"));
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
    if (!PasswordHash::verify(password, it->second.passwordHash)) {
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

} // namespace login
