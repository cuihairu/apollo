#pragma once

// Login M1 准入受理面（P4-4 批①：nng 迁移——gateway 准入 RPC 换装 link M1）。
//
// 被动端点 + linkrpc RpcServer：gateway 以 link active 端点拨入，请求体 =
// MessageCodec 全帧（header+body），本面不解释业务——handler（LoginServer
// 的会话校验路径）原样进出，传输换装对执行侧零改动。
//
// 机制面自带泵线程（10ms 拍；caller 线程不泵——准入 RPC 是同步等待语义，
// 泵必须独立于调用方存活）。m1_port = 0 = 面关（进程壳不接线，v1 行为
// 不变）；路由表缺位/端口占绑 = start false（装配错误显形，调用方降级）。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace login {

struct M1AdmissionConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;              // 0 = 面关
    std::string route_json;              // 契约路由表（缺位 = start false）
};

class M1AdmissionServer {
public:
    // 请求 handler：入参 = MessageCodec 全帧，返回 = 响应全帧。
    // 在泵线程回调——不得阻塞。
    using Handler = std::function<std::vector<std::uint8_t>(const std::uint8_t* frame,
                                                            std::size_t size)>;

    virtual ~M1AdmissionServer() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 实际监听端口（port 0 = 内核分配，测试经此读回）。
    virtual std::uint16_t listenPort() const = 0;
    // 有无已握手对端（gateway 链路 kUp）。
    virtual bool peerConnected() const = 0;
};

std::unique_ptr<M1AdmissionServer> makeM1AdmissionServer(M1AdmissionConfig config,
                                                         M1AdmissionServer::Handler handler);

} // namespace login
