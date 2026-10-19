#pragma once

// Gateway M1 Base 客户端面（L53 登录串联第三环：Base 经 database 激活）。
//
// active 端点拨 Base PlayerStore M1 受理面 + linkrpc RpcClient：请求/响应
// 体 = MessageCodec 全帧，业务语义（编解码/成败裁决）归调用方
// （GatewayServer::activateAndBindAtBase），本面只管传输与关联（批④
// route client 同型，批③ login base 客户端面同族）。
//
// 机制面自带泵线程（10ms 拍）——activateAndBindAtBase 是同步等待语义且
// 其调用点在 gateway ingress 宿主泵线程上，自泵自等 = 死锁（准入面同
// 纪律）。port = 0 = 面关（不装配，认证退容忍面）；路由表缺位 =
// start false（装配错误显形，调用方降级）。

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gateway {

struct M1BaseClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
};

class M1BaseClient {
public:
    virtual ~M1BaseClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 同步 RPC：入参/返回 = MessageCodec 全帧。返回空 = 传输不可达/超时
    // （调用方按面在场裁决拒绝，准入面同口径）。禁止在本类泵线程上调用。
    virtual std::optional<std::vector<std::uint8_t>> call(
        const std::vector<std::uint8_t>& frame, std::chrono::milliseconds timeout) = 0;

    // 对端链路 kUp（就绪观测面，测试与编排面用）。
    virtual bool peerUp() const = 0;
};

std::unique_ptr<M1BaseClient> makeM1BaseClient(M1BaseClientConfig config);

} // namespace gateway
