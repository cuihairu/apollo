#pragma once

// net M1 接收端缝（M1 上层接线批 · 2b-ii）：world 侧接收面 = InterServerLink
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

#include "world_app/acceptance_queue.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace world_app {

// World 排空状态机值（956/957 契约面；gateway 954 同值口径）。
inline constexpr std::int32_t kWorldDrainServing = 0;
inline constexpr std::int32_t kWorldDrainDraining = 1;
inline constexpr std::int32_t kWorldDrainDrained = 2;

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
    // 进程代次（§5.7）：0 = 进程自铸（见 LinkConfig.authority_epoch）。真进程
    // 各自铸不相等；同进程模拟替换（e2e）在此显式注入不同代次。
    std::uint64_t authority_epoch = 0;
};

// 脚本调试 eval 请求（契约 944，scripting-lua §7.1 attach transport）：admin
// 单入口上行；scene_id 0 = 默认 scene。结果回执 945 按 eval_id 关联（tick
// 边界执行，异步回传——非 rpc 同步面）。
struct DebugEvalRequest {
    std::uint64_t eval_id = 0;
    std::uint64_t scene_id = 0;
    std::string source;
};

// 模块投递编排请求（契约 952，P4 仅代码更新批）：ops 控制面上行，World 本地
// 以单目标 ModuleRollout（target = 本进程 runtime，canary = 自身）执行。
// op：1=prepare 2=commit 3=rollback 4=status。回执 953 按 request_id 关联
// （受理点同步执行——M1 泵在 gameThread_，ModuleRollout 契约线程 = owning
// 线程；观察窗由 gameLoop 拍自驱，控制面按 status 聚合）。
struct ModuleRolloutRequest {
    std::uint64_t request_id = 0;
    std::int32_t op = 0;  // 1=prepare 2=commit 3=rollback 4=status
    std::string name;
    std::uint64_t version = 0;
    std::string deps;  // 逗号分隔依赖名（空 = 无）
    std::string source;
    std::uint64_t observe_ticks = 0;
    std::uint64_t fault_threshold = 0;
};

// World 排空控制请求（契约 956，P4 更新排空批）：停止分配新副本，既有
// 副本服务至结算销毁。op：1=begin 2=status。回执 957 按 request_id 关联
// （受理点同步执行——M1 泵在 gameThread_，与 launch/玩法推进同线程）。
struct WorldDrainRequest {
    std::uint64_t request_id = 0;
    std::int32_t op = 0;  // 1=begin 2=status
    std::int64_t deadline_s = 0;  // begin 有义：期限到未清强停剩余副本
};

class M1LinkServer {
public:
    virtual ~M1LinkServer() = default;

    // 装配 + 监听（passive bind/listen；失败 false——配置错误装配期显形）。
    virtual bool start() { return true; }

    // 停机（先于 worldHost/legacy server 停，G-3 顺序见 WorldApp::stop）。
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

    // eval 回执（945 debug_eval_result）：peer = 请求方自报名（fan-out 命名
    // 查链路），eval_id 关联。kReliable（断连排队重连补投）；链路未连 = 排队
    // 待补投，对端已消失 = 尽力面（admin 超时面重发裁决）。
    virtual void sendEvalResult(const std::string& peer, std::uint64_t eval_id, bool ok,
                                const std::string& result, const std::string& error) {
        (void)peer;
        (void)eval_id;
        (void)ok;
        (void)result;
        (void)error;
    }

    // 模块编排回执（953 module_rollout_reply）：peer/关联键同 eval 面。
    // state = ModuleRollout::RolloutState 原值；detail = 错误行（ok=0）或
    // 状态摘要（status）。kReliable 同 945。
    virtual void sendModuleRolloutReply(const std::string& peer, std::uint64_t request_id,
                                        std::int32_t op, bool ok, std::int32_t state,
                                        std::uint64_t window_faults,
                                        std::uint64_t module_version,
                                        const std::string& detail) {
        (void)peer;
        (void)request_id;
        (void)op;
        (void)ok;
        (void)state;
        (void)window_faults;
        (void)module_version;
        (void)detail;
    }

    // World 排空回执（957 world_drain_reply）：peer/关联键同 953 面。
    // state：0=serving 1=draining 2=drained；active_instances = 活动副本
    // 绑定数；detail = 状态摘要（drained 含强停结果；ok=0 拒绝原因）。
    // kReliable 同 945。
    virtual void sendWorldDrainReply(const std::string& peer, std::uint64_t request_id,
                                     std::int32_t op, bool ok, std::int32_t state,
                                     std::uint64_t active_instances,
                                     const std::string& detail) {
        (void)peer;
        (void)request_id;
        (void)op;
        (void)ok;
        (void)state;
        (void)active_instances;
        (void)detail;
    }

    // 客户端帧下行（939 client_deliver，P2 真实下行）：session_id = 目标
    // 客户端会话（网关按此路由投递）；gateway_id = 本 World 所知网关身份
    //（0 = 不指定——单链路批缺省，多网关批按路由快照回带）；payload =
    // 客户端 wire 全帧（MessageHeader + body），网关原样投递不解释。
    // kReliable（断连排队重连补投）；链路未连 = 排队待补投。
    virtual void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                                   const std::vector<std::uint8_t>& payload) {
        (void)session_id;
        (void)gateway_id;
        (void)payload;
    }

    virtual std::uint16_t listenPort() const { return 0; }
    virtual bool gatewayConnected() const { return false; }

    // 观测面（测试/运维共用一口；计数与 dispatcher/链路口径镜像）。
    virtual std::uint64_t receivedIntents() const { return 0; }
    virtual std::uint64_t rejectedFrames() const { return 0; }
};

// 工厂：config.host:config.port 监听，契约路由表自 config.route_json 装载
// （schema_hash → 握手 route_version）；解出的意图信封经 on_intent 投给宿主
//（pump 线程 = 宿主泵线程内同步调用）。944 debug_eval_request / 952
// module_rollout_request / 956 world_drain_request 裸帧分别经 on_debug_eval /
// on_module_rollout / on_world_drain 投递（入参 = 请求 + 发端 peer 自报名，
// 回执按名回链路）。
std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval = {},
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        on_module_rollout = {},
    std::function<void(const std::string&, const WorldDrainRequest&)>
        on_world_drain = {});

} // namespace world_app
