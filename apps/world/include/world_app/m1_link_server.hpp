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

// 排空状态机值与 M1 错误码迁移至 acceptance_queue.hpp（受理面契约词汇；
// L98 后执行内核 CellApp 只依赖该头）。

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

// cell 执行进程注册请求（契约 960，L101 注册首切片）：cell→World M1 控制
// 受理面。声明逻辑 ID、启动代次、控制/模拟地址、能力与就绪/排空状态——
// 认证凭据面按 world-cell-management 差距表另行挂账（裸帧受理同 944/952/
// 956 惯例）。回执 961 按 request_id 关联（受理点同步执行——M1 泵在
// gameThread_，与 drain 同线程）。
struct CellRegisterRequest {
    std::uint64_t request_id = 0;
    std::uint64_t cell_id = 0;        // 逻辑 cell ID（0 = 非法——PID/IP 不当身份）
    std::uint64_t start_epoch = 0;    // 进程启动代次（0 = 非法；重启自增）
    std::string host;                 // 控制/模拟监听地址（World 回连用；本切片仅记录）
    std::int32_t port = 0;            // cell 监听端口（0 = 未监听，仅登记）
    std::int32_t capabilities = 0;    // 能力位掩码（0 = 未声明，记录不裁决）
    std::int32_t load = 0;            // 负载 0-100（0 = 未上报）
    std::int32_t drain_state = 0;     // 排空状态：0=serving 1=draining 2=drained
    bool ready = false;               // 就绪（进程存活不等于可分配）
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

    // cell 注册回执（961 cell_register_reply）：peer/关联键同 957 面。
    // code = kCellRegister*（acceptance_queue.hpp 契约词汇）；accepted =
    // 登记受理（幂等刷新/新代次替换同 true；旧代次/非法拒）；management_
    // epoch = World 管理代次回带；assigned_cell_id = 确认的 cell ID（本
    // 切片原样回带）。kReliable 同 945。
    virtual void sendCellRegisterReply(const std::string& peer, std::uint64_t request_id,
                                       bool accepted, std::int32_t code,
                                       std::uint64_t management_epoch,
                                       std::uint64_t assigned_cell_id,
                                       const std::string& detail) {
        (void)peer;
        (void)request_id;
        (void)accepted;
        (void)code;
        (void)management_epoch;
        (void)assigned_cell_id;
        (void)detail;
    }

    // Space 分配推送（962 cell_space_assign，L101 分配面首切片）：peer = cell
    // 自报名；分配版本 = 该 Space 当前版本（首分配 = 1，重平衡随 L108）。
    // kReliable 同 945。
    virtual void sendCellSpaceAssign(const std::string& peer, std::uint64_t assignment_version,
                                     std::uint64_t management_epoch, std::uint64_t space_id,
                                     std::uint64_t assigned_cell_id, bool ready) {
        (void)peer;
        (void)assignment_version;
        (void)management_epoch;
        (void)space_id;
        (void)assigned_cell_id;
        (void)ready;
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
// module_rollout_request / 956 world_drain_request / 960 cell_register_request
// 裸帧分别经 on_debug_eval / on_module_rollout / on_world_drain /
// on_cell_register 投递（入参 = 请求 + 发端 peer 自报名，回执按名回链路）。
std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval = {},
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        on_module_rollout = {},
    std::function<void(const std::string&, const WorldDrainRequest&)>
        on_world_drain = {},
    std::function<void(const std::string&, const CellRegisterRequest&)>
        on_cell_register = {});

} // namespace world_app
