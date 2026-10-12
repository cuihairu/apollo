#pragma once

// Manager M1 控制面（P4-4 批④：nng 迁移——manager RepSocket 控制面
// 换装 link M1）。
//
// 被动端点 + linkrpc RpcServer：login（AssignWorld 落点裁决）与 gateway
// （ResolveRoute 路由解析）两方各以 link active 端点拨入，同一面受理——
// 链路管理器被动端点按拨入者自报名字 fan-out 入表（link_manager.hpp 纪律），
// RpcServer install 于 peer_listener（每对端一挂，批① 准入面同型）。
// 请求体 = MessageCodec 全帧，本面不解释业务——handler（ManagerApp 的
// 分派路径）原样进出，传输换装对执行侧零分叉。
//
// 机制面自带泵线程（10ms 拍；caller 线程不泵）。m1_port = 0 = 面关
// （进程壳不接线，nng REP 路径行为不变）；路由表缺位/端口占绑 = start
// false（装配错误显形，调用方降级）。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace manager {

struct M1ControlServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
};

class M1ControlServer {
public:
    // 请求 handler：入参 = MessageCodec 全帧，返回 = 响应全帧。
    // 在泵线程回调——不得阻塞。
    using Handler = std::function<std::vector<std::uint8_t>(const std::uint8_t* frame,
                                                            std::size_t size)>;

    virtual ~M1ControlServer() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 实际监听端口（port 0 = 内核分配，测试经此读回）。
    virtual std::uint16_t listenPort() const = 0;
    // 已握手对端数（login + gateway 各一，fan-out 面）。
    virtual std::size_t peerCount() const = 0;
};

std::unique_ptr<M1ControlServer> makeM1ControlServer(M1ControlServerConfig config,
                                                     M1ControlServer::Handler handler);

} // namespace manager
