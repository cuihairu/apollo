#pragma once

#include <cstdint>
#include <map>
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

    // ---- 目录/恢复/死亡编队面（P3-1 re-home：baseappmgr 退场后目录 owner
    //      = Login 进程；三面全 0 = 关，行为与旧进程壳一致）----
    // 目录镜像 owner 面（增量②）：目录事件 → Delta + 周期全量快照
    std::string mirrorHost = "127.0.0.1";  // 镜像消费方地址
    std::uint16_t mirrorPort = 0;          // 0 = 镜像面关
    std::uint32_t mirrorSnapshotMs = 10000;
    // 编队恢复 intake 面（增量③）：收各 World/gateway 全量重报，恢复相位
    // 排他（拒新）直至收敛（全报或超时）
    std::uint16_t recoveryPort = 0;        // 0 = 恢复面关（0 端口绑定 = 内核分配）
    std::uint32_t recoveryExpected = 0;
    std::uint32_t recoveryTimeoutMs = 5000;
    // 死亡订阅面（machined → Login）：World/gateway 进程死亡 → 目录死亡行
    // 窗口处置。machinedPort = 0 = 两面全关
    std::string machinedHost = "127.0.0.1";
    std::uint16_t machinedPort = 0;
    std::uint64_t machinedComponentId = 0;  // 订阅者身份（本进程组件号）
    std::uint32_t suspendWindowTicks = 30;  // 保活窗口（主循环秒拍 tick）
    // 死亡 wire 不带 gateway 身份：component → gateway 显式映射（骨架期桥接）
    std::map<std::uint64_t, std::uint32_t> gatewayByComponent;

    // 监听配置
    std::string host = "0.0.0.0";
    uint16_t port = 9001;

    // M1 准入受理面（P4-4 批①）：linkrpc 被动端点，gateway 准入 RPC 换装
    // link M1 的受理侧；0 = 面关（非 M1 接线的本地构建面）
    uint16_t m1Port = 0;
    bool m1Enabled = false;
    std::string m1RouteJson = APOLLO_LOGIN_CONTRACT_ROUTE_JSON;

    // M1 服务注册受理面（L123 服务目录首切片）：Base/World 裸 964 拨入登记
    // 逻辑身份/进程代次/管理地址/兼容/就绪。显式 opt-in（CLI 传
    // --service-register-port 即装配；不传 = 不装配）。port 0 = 内核分配
    //（listenPort 读回实际端口），路由表共用 m1RouteJson。
    uint16_t serviceRegisterPort = 0;
    bool serviceRegisterEnabled = false;

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
