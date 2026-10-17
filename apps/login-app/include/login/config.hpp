#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifndef APOLLO_LOGIN_CONTRACT_ROUTE_JSON
#define APOLLO_LOGIN_CONTRACT_ROUTE_JSON ""
#endif

namespace login {

struct LoginConfig {
    bool directoryEnabled = false;
    std::uint16_t directoryPort = 0;
    std::string directoryEpochPath;
    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9001;

    // M1 准入受理面（P4-4 批①）：linkrpc 被动端点，gateway 准入 RPC 换装
    // link M1 的受理侧；0 = 面关（非 M1 接线的本地构建面）
    uint16_t m1Port = 0;
    bool m1Enabled = false;
    std::string m1RouteJson = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;

    // 后端服务地址

    // M1 base 客户端面（P4-4 批③）：拨 base-app M1 受理面的链路侧；0 = 面关
    // 0 = 不装配（RPC 面缺席 = 降级缝 throw）。路由表共用 m1RouteJson
    uint16_t worldStoreM1Port = 0;
    std::string worldStoreM1Host = "127.0.0.1";

    // M1 OnlineDirectory 客户端面（P4-4 批④）：拨 baseappmgr 控制面（AssignWorld 落点
    // 裁决）；0 = 不装配（AssignWorld 缺席 = 降级缝 throw）
    uint16_t m1OnlineDirectoryPort = 0;
    std::string m1OnlineDirectoryHost = "127.0.0.1";

    // 默认进入世界
    uint32_t initialWorldId = 1;
    uint64_t initialMapId = 1;
    uint64_t initialInstanceId = 1;
    uint64_t initialSpaceId = 1;

    // 网关配置
    std::vector<std::string> gatewayUrls = {
        "tcp://127.0.0.1:8888"
    };

    // 认证配置
    int maxLoginAttempts = 5;
    int lockoutDurationMs = 300000;  // 5分钟

    // 会话配置
    int sessionTimeoutMs = 300000;   // 5分钟

};

} // namespace login
