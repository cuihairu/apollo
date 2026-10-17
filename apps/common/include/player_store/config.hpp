#pragma once

#include <string>
#include <cstdint>

#ifndef APOLLO_BASEAPP_CONTRACT_ROUTE_JSON
#define APOLLO_BASEAPP_CONTRACT_ROUTE_JSON ""
#endif

namespace player_store {

struct PlayerStoreConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9002;

    // M1 base 受理面（P4-4 批③）：linkrpc 被动端点，login base RPC 换装
    // link M1 的受理侧；0 = 面关（RPC 面缺席）
    uint16_t m1Port = 0;
    bool m1Enabled = false;
    std::string m1RouteJson = APOLLO_BASEAPP_CONTRACT_ROUTE_JSON;

    // 数据库配置
    std::string dbHost = "localhost";
    uint16_t dbPort = 3306;
    std::string dbName = "apollo";

    // 保存配置
    std::string dataDir = "data/world-1/players";       // 玩家档案落盘目录（P1-4）
    std::string journalPath = "data/world-1/journal/persist.log";  // write-ahead（P1-5）
    int autoSaveIntervalMs = 60000;  // 1分钟自动保存
    int journalDrainQuota = 32;      // 每轮 journal 定额出队（attribute-sync §8.2）
    int shutdownFlushTimeoutMs = 5000;  // 停机 journal 追平超时上限（§10.2-③：日志完整性优先于停机速度）

    // 性能配置
    int workerThreads = 4;

};

} // namespace player_store
