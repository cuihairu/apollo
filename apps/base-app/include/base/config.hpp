#pragma once

#include <string>
#include <cstdint>

namespace base {

struct BaseConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9002;

    // 数据库配置
    std::string dbHost = "localhost";
    uint16_t dbPort = 3306;
    std::string dbName = "apollo";
    std::string dbUser = "root";
    std::string dbPassword = "";

    // Redis 配置
    std::string redisHost = "localhost";
    uint16_t redisPort = 6379;
    std::string redisPassword = "";

    // 缓存配置
    bool enableCache = true;
    int cacheTimeoutMs = 60000;  // 1分钟

    // 保存配置
    std::string dataDir = "base-app-data/players";       // 玩家档案落盘目录（P1-4）
    std::string journalPath = "base-app-data/journal/persist.log";  // write-ahead（P1-5）
    int autoSaveIntervalMs = 60000;  // 1分钟自动保存
    int saveTimeoutMs = 5000;
    int journalDrainQuota = 32;      // 每轮 journal 定额出队（attribute-sync §8.2）

    // 性能配置
    int connectionPoolSize = 10;
    int workerThreads = 4;

    // 日志配置
    std::string logLevel = "info";
    std::string logFile = "logs/base.log";
};

} // namespace base
