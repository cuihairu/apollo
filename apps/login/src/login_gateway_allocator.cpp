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
// GatewayAllocator 实现
//==============================================================================

GatewayAllocator::GatewayAllocator(const LoginConfig& config)
    : config_(config) {
    // 初始化网关列表
    int index = 0;
    for (const auto& url : config_.gatewayUrls) {
        GatewayInfo info;
        info.url = url;
        info.connections = 0;
        info.lastUpdateMs = 0;
        gateways_.push_back(info);
        index++;
    }
}

std::string GatewayAllocator::selectGateway() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (gateways_.empty()) {
        return "";
    }

    // 选择连接数最少的网关
    auto it = std::min_element(gateways_.begin(), gateways_.end(),
        [](const GatewayInfo& a, const GatewayInfo& b) {
            return a.connections < b.connections;
        });

    return it->url;
}

void GatewayAllocator::updateGatewayLoad(const std::string& gatewayUrl, int connections) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& gateway : gateways_) {
        if (gateway.url == gatewayUrl) {
            gateway.connections = connections;
            gateway.lastUpdateMs = getCurrentTimeMs();
            break;
        }
    }
}

int64_t GatewayAllocator::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace login
