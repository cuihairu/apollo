#pragma once

// net M1 接收端缝（M1 上层接线批 · 2b-ii）：cell-app 侧接收面 = InterServerLink
// passive 端点（网关拨入，net-abstraction §5.6/§5.7）+ 契约 wire 解包 + 意图
// 信封投递。
//
// 缝纪律同 gateway ingress（宿主泵驱动，机制零内部线程）：宿主只持本接口，
// pump() 由 gameLoop tick 驱动；POSIX 真实实现 / Windows 退 null 桩由工厂
// 收口，消费面不被 #if 污染。
//
// 应答关联（ADR-017 延时应答升级）：受理点不再即时回执——受理成功不回包，
// 回执在 gameThread_ 消费后按 request_id 回 929 cell_intent_ack（code 0 =
// 已消费）；顶格拒收（2101）与不受理（2001）在受理点即时回——这两类永不
// 进队列，无需延后。

#include "cell/acceptance_queue.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace cell {

// M1 错误码（errors.xml 契约错误表；0 成功）。
inline constexpr std::int32_t kIntentAckOk = 0;
inline constexpr std::int32_t kIntentAckInternal = 2001;        // internal
inline constexpr std::int32_t kIntentAckBackpressure = 2101;    // cell_backpressure

struct M1LinkConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9101;
    // 契约路由表 JSON（生成物）：表是 Dispatcher 白名单事实源，schema_hash
    // 是握手 route_version 事实源。空路径 = 装配失败（start false）。
    std::string route_json;
};

class M1LinkServer {
public:
    virtual ~M1LinkServer() = default;

    // 装配 + 监听（passive bind/listen；失败 false——配置错误装配期显形）。
    virtual bool start() { return true; }

    // 停机（先于 worldHost/legacy server 停，G-3 顺序见 CellServer::stop）。
    virtual void stop() {}

    // 宿主泵：LinkManager 全部端点一拍（accept / 链路收发 / 重连）。now_ms =
    // 单调毫秒（注入，与 gameLoop 同钟）。on_intent 投递发生在本调用内。
    virtual void pump(std::uint64_t now_ms) { (void)now_ms; }

    // 延时应答（929 cell_intent_ack）：request_id 关联。链路未连 = 丢弃
    // （尽力面，与 legacy 空 ack 同型；重发裁决在网关 pending-reply 超时面）。
    virtual void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                               const std::string& message) {
        (void)request_id;
        (void)code;
        (void)message;
    }

    virtual std::uint16_t listenPort() const { return 0; }
    virtual bool gatewayConnected() const { return false; }

    // 观测面（测试/运维共用一口；计数与 dispatcher/链路口径镜像）。
    virtual std::uint64_t receivedIntents() const { return 0; }
    virtual std::uint64_t rejectedFrames() const { return 0; }
};

// 工厂：config.host:config.port 监听，契约路由表自 config.route_json 装载
// （schema_hash → 握手 route_version）；解出的意图信封经 on_intent 投给宿主
//（pump 线程 = 宿主泵线程内同步调用）。
std::unique_ptr<M1LinkServer> makeM1LinkServer(M1LinkConfig config,
                                               std::function<void(IntentEnvelope)> on_intent);

} // namespace cell
