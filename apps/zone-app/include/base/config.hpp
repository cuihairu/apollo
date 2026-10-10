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

    // 保存配置
    std::string dataDir = "base-app-data/players";       // 玩家档案落盘目录（P1-4）
    std::string journalPath = "base-app-data/journal/persist.log";  // write-ahead（P1-5）
    int autoSaveIntervalMs = 60000;  // 1分钟自动保存
    int journalDrainQuota = 32;      // 每轮 journal 定额出队（attribute-sync §8.2）
    int shutdownFlushTimeoutMs = 5000;  // 停机 journal 追平超时上限（§10.2-③：日志完整性优先于停机速度）

    // 性能配置
    int workerThreads = 4;

};

} // namespace base
