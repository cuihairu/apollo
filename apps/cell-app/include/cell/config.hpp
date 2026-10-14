#pragma once

#include <string>
#include <cstdint>

// 契约路由表 JSON 的构建期默认路径（CMake 注入生成物路径；裸编时为空 =
// M1 端点装配失败显形，不带隐式回退）。
#ifndef APOLLO_CELL_CONTRACT_ROUTE_JSON
#define APOLLO_CELL_CONTRACT_ROUTE_JSON ""
#endif

namespace cell {

struct CellConfig {
    // 监听配置
    std::string host = "0.0.0.0";

    // net M1 接收端（M1 上层接线批 2b-ii；2b-iii 起 cell 唯一受理入口）：
    // InterServerLink passive 端点，网关经它投递 cell 意图（924-928）
    uint16_t m1Port = 9101;
    std::string m1RouteJson = APOLLO_CELL_CONTRACT_ROUTE_JSON;

    // 空间配置
    std::string spaceName = "main_world";
    float spaceWidth = 1000.0f;
    float spaceHeight = 1000.0f;
    float gridSize = 100.0f;

    // AOI 配置
    float viewRadius = 200.0f;

    // Tick 配置
    int tickRateMs = 50;  // 20 ticks per second

    // 脚本宿主（scripting-lua §2/§5.1）：boot 源文件路径（空 = 不加载）。
    // 加载失败不停机（fault 走 runtime 事件面，cell 继续服务）。
    std::string scriptBootFile;

};

} // namespace cell
