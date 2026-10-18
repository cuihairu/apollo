#pragma once

// Gateway 排空控制受理面（P4 排空批；契约 954/955）：ops 控制面上行——
// 更新 Gateway 时旧实例停止接受新连接，既有连接服务至自然退出或明确期限
// （session-and-online-directory §2、gateway-topology-survey §2）。
//
// 缝纪律同 world M1 接收端（宿主泵驱动，机制零内部线程）：泵 = Gateway
// 客户端入口泵同线程（acceptNewConnections 循环），on_request 回调发生在
// 本调用内——排空状态机因此单线程独占，无锁。POSIX 真实实现 / Windows 退
// null 桩由工厂收口（link TCP 底座不进 M1 批，同 ingress 口径）。
//
// 受理面纪律：954 裸帧受理（952 同型——控制编排族不走 932 信封）；应答
// 955 kReliable 现场回带（受理点同步执行，无延时面）。

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace gateway {

// 排空状态机原值（955 state 字段口径）
inline constexpr std::int32_t kDrainServing = 0;   // 正常服务（未排空）
inline constexpr std::int32_t kDrainDraining = 1;  // 已关监听，既有连接服务中
inline constexpr std::int32_t kDrainDrained = 2;   // 终态：连接清零或期限强停

// 954 gateway_drain_request 解包面
struct DrainControlRequest {
    std::uint64_t request_id = 0;
    std::int32_t op = 0;  // 1=begin 2=status
    std::uint64_t deadline_s = 0;  // 排空期限（秒；0 = 无期限自然排空）
};

// 955 gateway_drain_reply 渲染面（宿主状态快照；ok=0 时 detail 为拒绝原因）
struct DrainStatus {
    bool ok = true;
    std::int32_t state = kDrainServing;
    std::size_t active_connections = 0;
    std::string detail;
};

class GatewayControlServer {
public:
    virtual ~GatewayControlServer() = default;

    // 装配 + 监听（passive bind/listen）；失败 false——装配错误显形，宿主
    // 降级收口（警告 + ops 不可达，客户端服务不受影响）。
    // 缺省实现 = Windows null 桩（start true / 泵空转 / 端口 0——WorldApp
    // M1 缝缺省实现同口径），宿主装配面无 #if。
    virtual bool start() { return true; }

    // 停机（监听器与链路随管理器析构关闭）。
    virtual void stop() {}

    // 宿主泵一拍（accept / 链路收发）。on_request 回调发生在本调用内。
    virtual void pump(std::uint64_t now_ms) { (void)now_ms; }

    // 实际监听端口（port 0 = 内核分配时经此读回；null 桩恒 0）。
    virtual std::uint16_t listenPort() const { return 0; }
};

struct GatewayControlConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9106;  // 0 = 内核分配（listenPort 查实占）
    // 契约路由表 JSON（生成物）：dispatcher 白名单 + 握手 route_version
    // 事实源。空路径 = 装配失败（start false）。
    std::string route_json;
};

// 工厂：on_request 在泵线程内同步调用（宿主 = GatewayServer 泵线程），
// 返回值现场渲染回 955。
std::unique_ptr<GatewayControlServer> makeGatewayControlServer(
    GatewayControlConfig config,
    std::function<DrainStatus(const DrainControlRequest&)> on_request);

} // namespace gateway
