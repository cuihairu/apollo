#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace login {

struct LoginConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9001;

    // 后端服务地址
    std::string baseAppUrl = "tcp://127.0.0.1:9002";

    // 默认进入世界
    uint32_t initialWorldId = 1;
    uint64_t initialMapId = 1;
    uint64_t initialInstanceId = 1;
    uint64_t initialSpaceId = 1;

    // 网关配置
    std::vector<std::string> gatewayUrls = {
        "tcp://127.0.0.1:8888"
    };

    // 认证配置
    int maxLoginAttempts = 5;
    int lockoutDurationMs = 300000;  // 5分钟

    // 会话配置
    int sessionTimeoutMs = 300000;   // 5分钟
    std::string sessionSecret = "change-this-secret";

    // 日志配置
    std::string logLevel = "info";
    std::string logFile = "logs/login.log";
};

} // namespace login
