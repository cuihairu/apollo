#pragma once

#include <string>
#include <cstdint>

namespace cell {

struct CellConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9100;

    // 空间配置
    std::string spaceName = "main_world";
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;

    // AOI 配置
    float viewRadius = 200.0f;

    // Tick 配置
    int tickRateMs = 50;  // 20 ticks per second

    // 性能配置
    int maxEntities = 10000;
    int workerThreads = 2;

    // 日志配置
    std::string logLevel = "info";
    std::string logFile = "logs/cell.log";
};

} // namespace cell
