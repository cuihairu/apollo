#pragma once

// Login M1 客户端面（P4-4 批③④：nng 迁移——login 出站 RPC 链换装
// link M1）。
//
// active 端点拨对端 M1 受理面 + linkrpc RpcClient：请求/响应体 =
// MessageCodec 全帧，业务语义（编解码/成败裁决）归调用方
// （LoginServer::preparePlayerOnline），本面只管传输与关联。
// peer_name 择对端：批③ = "world"（base 链）；批④ = "baseappmgr"（AssignWorld
// 落点裁决——AssignWorld 已拆出 zone，随 manager 控制面受理）。
//
// 机制面自带泵线程（10ms 拍）——preparePlayerOnline 是同步等待语义且其
// 调用点在 login ingress 宿主泵线程上，自泵自等 = 死锁（批① 准入面同
// 纪律）。port = 0 = 面关（进程壳不接线，RPC 面缺席 = 调用方降级缝）；路由表
// 缺位 = start false（装配错误显形，调用方降级）。

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace login {

struct M1BaseClientConfig {
    std::string self_name = "login";
    std::string host = "127.0.0.1";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
    std::string peer_name = "world";      // 拨入对端 G-1 组件名
};

class M1BaseClient {
public:
    virtual ~M1BaseClient() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 同步 RPC：入参/返回 = MessageCodec 全帧。返回空 = 传输不可达/超时
    // （调用方降级，桩时代同缝）。禁止在本类泵线程上调用。
    virtual std::optional<std::vector<std::uint8_t>> call(
        const std::vector<std::uint8_t>& frame, std::chrono::milliseconds timeout) = 0;

    // 对端链路 kUp（就绪观测面，测试与编排面用）。
    virtual bool peerUp() const = 0;
};

std::unique_ptr<M1BaseClient> makeM1BaseClient(M1BaseClientConfig config);

} // namespace login
