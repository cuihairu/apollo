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

    // 客户端入口多监听（L45 批③）：WS/WSS 附加监听，与 TCP 明文主入口
    // 并存（同一线形，ws binary 载荷 = link Frame）。0 = 关。升级路径
    // wsPath 判据（不命中 = no-such-route 拒绝）。
    std::uint16_t wsPort = 0;
    std::uint16_t wssPort = 0;
    std::string wsPath = "/game";

    // WSS 凭据（PEM 证书链 + 私钥；wssPort 非 0 时必填——缺失即启动失
    // 败，不静默降级成明文。凭据由部署供给，机制不生成，§5.9）。
    std::string tlsCertPath;
    std::string tlsKeyPath;

    // 后端服务地址（ChatApp 项已摘除：fleet 无 chat 进程，P3-4 处方③）
    std::string loginAppUrl = "tcp://127.0.0.1:9001";
    std::string worldUrl = "tcp://127.0.0.1:9100";  // 可以是多个

    // WorldApp M1 出口（2b-iii）：InterServerLink active 端点直连 WorldApp M1 接收端
    std::string worldM1Host = "127.0.0.1";
    std::uint16_t worldM1Port = 9101;
    std::string worldM1RouteJson = APOLLO_GATEWAY_CONTRACT_ROUTE_JSON;

    // Login M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面；0 = 面
    // 关（准入退退化容忍面：仅拒空票）；路由表与 cell 出口共用契约表
    std::string loginM1Host = "127.0.0.1";
    std::uint16_t loginM1Port = 0;

    // Manager M1 控制面（P4-4 批④）：active 端点拨 manager 控制面
    // （ResolveRoute 路由解析）；0 = 面关（路由解析缺席 → 默认路由兜底）
    std::string directoryM1Host = "127.0.0.1";
    std::uint16_t directoryM1Port = 0;

    // Base M1 激活面（L53 登录串联第三环：Base 经 database 激活）：已准入
    // 会话经本面激活玩家锚点（PlayerStore 装载 Anchor）并绑定会话号。
    // 0 = 面关（旧语义：准入过即绑玩家）；非 0 且装配成而激活/绑定不成 =
    // 认证拒绝（面在场 = 错误显形，login 准入面同口径）。路由表与 cell
    // 出口共用契约表。
    std::string baseM1Host = "127.0.0.1";
    std::uint16_t baseM1Port = 0;

    // 进程身份（Base 激活链第二跳写入 SessionBinding.gateway_id；0 = 未配置）
    std::uint32_t gatewayId = 0;

    // 排空控制受理面（P4 排空批）：ops 拨入面（契约 954/955，fleetctl drain）；
    // 0 = 内核分配（drainControlPort 查实占）。路由表与 cell 出口共用契约
    // 表。绑定失败降级（警告 + ops 不可达，不拦启动——端口撞车场景）。
    std::uint16_t controlM1Port = 9106;

    // 会话配置
    int sessionTimeoutMs = 30000;       // 30秒无心跳断开

    // 会话连续保留窗（L53 断线重连）：已认证会话掉线后保留的毫秒数，窗内
    // 重连（可跨 TCP/WS 传输）携旧会话号上行即恢复同一会话；窗到期才销毁
    // 会话并下发延迟的断线通知。0 = 关窗（旧语义：掉线即销毁）。
    std::int64_t sessionRetentionMs = 30000;

    // 性能配置
    int maxConnections = 10000;

    // 心跳配置
    int heartbeatCheckIntervalMs = 1000; // 服务端检查间隔

};

} // namespace gateway
