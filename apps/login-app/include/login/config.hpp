#pragma once

#include <string>
#include <cstdint>

namespace login {

struct LoginConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9001;

    // 后端服务地址
    std::string baseAppUrl = "tcp://127.0.0.1:9002";

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
