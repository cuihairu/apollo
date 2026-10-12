#pragma once

// Gateway M1 路由解析客户端面（P4-4 批④：nng 迁移——gateway→manager
// ResolveRoute 换装 link M1）。
//
// active 端点拨 manager M1 控制面 + linkrpc RpcClient：请求/响应体 =
// MessageCodec 全帧，业务语义（编解码/成败裁决）归调用方
// （GatewayServer::fetchRouteSnapshot），本面只管传输与关联。
//
// 机制面自带泵线程（10ms 拍）——fetchRouteSnapshot 是同步等待语义且其
// 调用点在 gateway ingress 宿主泵线程上，自泵自等 = 死锁（批①③ 同纪律）。
// port = 0 = 面关（进程壳不接线，nng 桩路径行为不变）；路由表缺位 =
// start false（装配错误显形，调用方降级）。

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gateway {

struct M1RouteClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
};

class M1RouteClient {
public:
    virtual ~M1RouteClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 同步 RPC：入参/返回 = MessageCodec 全帧。返回空 = 传输不可达/超时
    // （调用方降级，桩时代同缝）。禁止在本类泵线程上调用。
    virtual std::optional<std::vector<std::uint8_t>> call(
        const std::vector<std::uint8_t>& frame, std::chrono::milliseconds timeout) = 0;

    // 对端链路 kUp（就绪观测面，测试与编排面用）。
    virtual bool peerUp() const = 0;
};

std::unique_ptr<M1RouteClient> makeM1RouteClient(M1RouteClientConfig config);

} // namespace gateway
