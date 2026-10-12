#pragma once

#include <string>
#include <cstdint>

#ifndef APOLLO_GATEWAY_CONTRACT_ROUTE_JSON
#define APOLLO_GATEWAY_CONTRACT_ROUTE_JSON ""
#endif

namespace gateway {

struct GatewayConfig {
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 8888;

    // 后端服务地址（ChatApp 项已摘除：fleet 无 chat 进程，P3-4 处方③）
    std::string loginAppUrl = "tcp://127.0.0.1:9001";
    std::string baseAppUrl = "tcp://127.0.0.1:9002";
    std::string cellAppUrl = "tcp://127.0.0.1:9100";  // 可以是多个

    // Cell M1 出口（2b-iii）：InterServerLink active 端点直连 cell M1 接收端
    std::string cellM1Host = "127.0.0.1";
    std::uint16_t cellM1Port = 9101;
    std::string cellM1RouteJson = APOLLO_GATEWAY_CONTRACT_ROUTE_JSON;

    // Login M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面；0 = 面
    // 关（准入退退化容忍面：仅拒空票）；路由表与 cell 出口共用契约表
    std::string loginM1Host = "127.0.0.1";
    std::uint16_t loginM1Port = 0;

    // Manager M1 控制面（P4-4 批④）：active 端点拨 manager 控制面
    // （ResolveRoute 路由解析）；0 = 面关（路由解析缺席 → 默认路由兜底）
    std::string managerM1Host = "127.0.0.1";
    std::uint16_t managerM1Port = 0;

    // 会话配置
    int sessionTimeoutMs = 30000;       // 30秒无心跳断开

    // 性能配置
    int maxConnections = 10000;

    // 心跳配置
    int heartbeatCheckIntervalMs = 1000; // 服务端检查间隔

};

} // namespace gateway
