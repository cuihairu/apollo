#pragma once

#include <string>
#include <cstdint>

namespace gateway {

struct GatewayConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 8888;

    // 后端服务地址
    std::string loginAppUrl = "tcp://127.0.0.1:9001";
    std::string baseAppUrl = "tcp://127.0.0.1:9002";
    std::string cellAppUrl = "tcp://127.0.0.1:9100";  // 可以是多个
    std::string chatAppUrl = "tcp://127.0.0.1:9003";

    // 会话配置
    int sessionTimeoutMs = 30000;       // 30秒无心跳断开
    int reconnectWindowMs = 30000;     // 掉线重连窗口

    // 性能配置
    int maxConnections = 10000;
    int recvBufferSize = 65536;        // 64KB
    int sendBufferSize = 65536;        // 64KB
    int workerThreads = 4;

    // 心跳配置
    int heartbeatIntervalMs = 5000;    // 客户端心跳间隔
    int heartbeatCheckIntervalMs = 1000; // 服务端检查间隔

    // 日志配置
    std::string logLevel = "info";
    std::string logFile = "logs/gateway.log";
};

} // namespace gateway
