#pragma once

// Zone M1 base 受理面（P4-4 批③：nng 迁移——login→base RPC 链换装 link M1）。
//
// 被动端点 + linkrpc RpcServer：login 以 link active 端点拨入，请求体 =
// MessageCodec 全帧（header+body），本面不解释业务——handler（BaseServer
// 的 dispatchRequest 直通）原样进出，传输换装对执行侧零分叉。
//
// 机制面自带泵线程（10ms 拍；caller 线程不泵——受理/应答全在泵拍链内，
// LogPushClient/批① 准入面同型）。m1_port = 0 = 面关（进程壳不接线，nng
// REP 路径行为不变）；路由表缺位/端口占绑 = start false（装配错误显形，
// 调用方降级）。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace base {

struct M1BaseServerConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
};

class M1BaseServer {
public:
    // 请求 handler：入参 = MessageCodec 全帧，返回 = 响应全帧。
    // 在泵线程回调——不得阻塞。
    using Handler = std::function<std::vector<std::uint8_t>(const std::uint8_t* frame,
                                                            std::size_t size)>;

    virtual ~M1BaseServer() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 实际监听端口（port 0 = 内核分配，测试经此读回）。
    virtual std::uint16_t listenPort() const = 0;
    // 有无已握手对端（login 链路 kUp）。
    virtual bool peerConnected() const = 0;
};

std::unique_ptr<M1BaseServer> makeM1BaseServer(M1BaseServerConfig config,
                                               M1BaseServer::Handler handler);

} // namespace base
