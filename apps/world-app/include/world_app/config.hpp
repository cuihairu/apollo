#pragma once

#include <string>
#include "player_store/config.hpp"
#include <cstddef>
#include <cstdint>

// 契约路由表 JSON 的构建期默认路径（CMake 注入生成物路径；裸编时为空 =
// M1 端点装配失败显形，不带隐式回退）。
#ifndef APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON
#define APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON ""
#endif

namespace world_app {

struct WorldAppConfig {
    std::uint32_t worldId = 1;
    // 小队副本容量（P2）：本 World 可同时承载的独立 Instance 数上限。
    std::size_t maxInstances = 16;
    bool playerStoreEnabled = false;
    player_store::PlayerStoreConfig playerStore;
    // 监听配置
    std::string host = "0.0.0.0";

    // net M1 接收端（M1 上层接线批 2b-ii；2b-iii 起 WorldApp 唯一受理入口）：
    // InterServerLink passive 端点，网关经它投递 cell 意图（924-928）
    uint16_t m1Port = 9101;
    std::string m1RouteJson = APOLLO_WORLD_APP_CONTRACT_ROUTE_JSON;
    // 进程代次透传（§5.7；0 = 进程自铸）——真进程不配（自铸即不同）；同进程
    // 模拟替换（e2e）在此注入不同代次。
    std::uint64_t m1AuthorityEpoch = 0;

    // 空间配置
    std::string spaceName = "main_world";
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;

    // AOI 配置
    float viewRadius = 200.0f;

    // Tick 配置
    int tickRateMs = 50;  // 20 ticks per second

    // 脚本宿主（scripting-lua §2/§5.1）：启动装载序 = handler 面 → 契约 →
    // boot 源（契约装载闸要求 handler 先行在位，boot 源可读 contract 全局）。
    // 各文件路径空 = 不加载；任一步加载/装载失败不停机（fault 走 runtime
    // 事件面，cell 继续服务）。
    std::string scriptHandersFile;
    std::string scriptContractFile;
    std::string scriptBootFile;

    // ---- 恢复相位全量重报面（P3-1 re-home：base-app 退场后 reporter = World
    //      进程；reportPort = 0 = 关）----
    // 锚点快照（AnchorManager）→ FullReport 分片发目录 owner（Login 的
    // intake 口）——启动即报 + 周期刷新（幂等 intake，restore-not-kick）。
    std::string reportHost = "127.0.0.1";
    std::uint16_t reportPort = 0;
    std::uint64_t reportComponentId = 0;  // 0 = 派生 worldId（fleet 同号口径）
    std::uint32_t reportZoneId = 0;       // 0 = 派生 worldId
    std::uint32_t reportIntervalMs = 30000;  // 0 = 仅启动期一报

};

} // namespace world_app
