#include "world_app/m1_link_server.hpp"

#if !defined(_WIN32)

// net M1 接收端真实实现（M1 上层接线批 · 2b-ii）：InterServerLink passive
// 端点 + 契约 wire 解包 + legacy 帧镜像。机制零内部线程——泵由宿主
//（WorldApp::gameLoop）驱动，on_intent 投递与 sendIntentAck 都发生在
// 宿主泵线程内（单写者纪律，ADR-015(a)）。
//
// 镜像纪律：契约 924-928 字段与 legacy Cell*/Combat* 平铺一一对应（M1 契约
// 批定），此处把契约载荷翻译回 legacy MessageHeader 帧——执行侧
//（handleCellCreateEntity 等）的解析路径零改动（受理面骨架零废弃，
// ADR-017）；翻译面消失的条件 = 执行侧改读契约信封（nng 底座替换收口批）。
//
// 受理面纪律：上行恒 932 internal_envelope 信封（P3-3 移交余件②：bare
// 924-928 双受理退役——消费面清零，gateway m1_egress 恒包信封，无其余发端）。
// 裸五族帧不再登记 handler = dispatcher no_handler 静默丢弃（不受理/不
// 回执/不断连/不计 handler 拒收）。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>
#include "m1_link_server_impl.hpp"

#endif // !_WIN32

namespace world_app {

#if !defined(_WIN32)

M1LinkServerImpl::M1LinkServerImpl(M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
                 std::function<void(const std::vector<std::uint8_t>&)> on_intent_raw,
                 std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval,
                 std::function<void(const std::string&, const ModuleRolloutRequest&)>
                     on_module_rollout,
                 std::function<void(const std::string&, const WorldDrainRequest&)>
                     on_world_drain,
                 std::function<void(const std::string&, const CellRegisterRequest&)>
                     on_cell_register,
                 std::function<void(const std::string&, const CellRetireRequest&)>
                     on_cell_retire,
                 std::function<void(const std::string&, const CellSpaceReport&)>
                     on_cell_space_report,
                 std::function<void(const std::string&, const CellEvalResult&)>
                     on_cell_eval_result,
                 std::function<void(const std::string&, const CellRolloutReply&)>
                     on_cell_rollout_reply,
                 std::function<void(const std::string&, const CellDrainReply&)>
                     on_cell_drain_reply,
                 std::function<void(const std::string&, const CellIntentAck&)>
                     on_cell_intent_ack)
    : config_(std::move(config))
    , on_intent_(std::move(on_intent))
    , on_intent_raw_(std::move(on_intent_raw))
    , on_debug_eval_(std::move(on_debug_eval))
    , on_module_rollout_(std::move(on_module_rollout))
    , on_world_drain_(std::move(on_world_drain))
    , on_cell_register_(std::move(on_cell_register))
    , on_cell_retire_(std::move(on_cell_retire))
    , on_cell_space_report_(std::move(on_cell_space_report))
    , on_cell_eval_result_(std::move(on_cell_eval_result))
    , on_cell_rollout_reply_(std::move(on_cell_rollout_reply))
    , on_cell_drain_reply_(std::move(on_cell_drain_reply))
    , on_cell_intent_ack_(std::move(on_cell_intent_ack)) {}

bool M1LinkServerImpl::start() {
    if (started_.load(std::memory_order_acquire)) {
        return true;
    }
    route_table_ = std::make_unique<link::RouteTable>();
    if (!load_route_table(*route_table_)) {
        route_table_.reset();
        return false;  // 装配错误显形（空路径/坏 JSON = 拒起，不带隐式回退）
    }

    link::LinkManagerConfig mc;
    mc.self_name = "world";
    mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
    mc.compat_route_versions = link::compat_route_versions_from_env();
    mc.authority_epoch = config_.authority_epoch;  // 0 = 进程自铸
    auto mgr = std::make_shared<link::LinkManager>(mc);

    link::EndpointConfig ep;
    ep.name = "gateway";  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
    ep.host = config_.host;
    ep.port = config_.port;
    ep.role = link::EndpointRole::kPassive;
    if (!mgr->add_endpoint(ep)) {
        route_table_.reset();
        return false;
    }

    // handler/表/白名单随新 peer 上线装配（fan-out 面：链路对象在
    // peer_listener 回调后地址稳定，重连 = 同名新链重新过此装配）。
    mgr->set_peer_listener([this](link::InterServerLink& peer) {
        install_dispatching(peer);
    });

    // 对端断连转报（L101 断连存疑切片）：LinkManager::on_down 在 pump
    // 上下文（宿主内核线程）触发；握手期坏链无名字，框架不出账——这里
    // 收到的 peer 名都可对上登记。断连原因 M1 层不透传（上层只裁决
    // 存疑与否；诊断走链路计数/日志面）。
    mgr->on_down([this](const std::string& name, link::LinkDownReason reason) {
        (void)reason;
        if (on_peer_down_fn_) {
            on_peer_down_fn_(name);
        }
    });

    // 先发布再置 started（读侧 load 后即见完整装配）。
    manager_.store(mgr, std::memory_order_release);
    started_.store(true, std::memory_order_release);
    return true;
}

void M1LinkServerImpl::stop() {
    started_.store(false, std::memory_order_release);
    manager_.store({}, std::memory_order_release);  // 读侧副本保活至析构
}

void M1LinkServerImpl::pump(std::uint64_t now_ms) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (mgr) {
        mgr->pump(now_ms);
    }
}

void M1LinkServerImpl::on_peer_down(std::function<void(const std::string& peer)> fn) {
    on_peer_down_fn_ = std::move(fn);
}

std::vector<link::LinkSnapshot> M1LinkServerImpl::snapshot_links() const {
    auto mgr = manager_.load(std::memory_order_acquire);
    return mgr ? mgr->snapshot_links() : std::vector<link::LinkSnapshot>{};
}

std::uint16_t M1LinkServerImpl::listenPort() const {
    auto mgr = manager_.load(std::memory_order_acquire);
    return mgr ? mgr->endpoint_port("gateway") : 0;
}

bool M1LinkServerImpl::gatewayConnected() const {
    auto* link = const_cast<M1LinkServerImpl*>(this)->gateway_link();
    return link != nullptr && link->state() == link::LinkState::kUp;
}

std::uint64_t M1LinkServerImpl::receivedIntents() const {
    return received_.load(std::memory_order_relaxed);
}

std::uint64_t M1LinkServerImpl::rejectedFrames() const {
    return rejected_.load(std::memory_order_relaxed);
}

link::InterServerLink* M1LinkServerImpl::gateway_link() {
    auto mgr = manager_.load(std::memory_order_acquire);
    return mgr ? mgr->link("gateway") : nullptr;
}

bool M1LinkServerImpl::load_route_table(link::RouteTable& table) {
    if (config_.route_json.empty()) {
        return false;
    }
    std::ifstream in(config_.route_json, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
    return link::load_route_table_json(buffer.str(), table);
#else
    (void)table;
    return false;
#endif
}

void M1LinkServerImpl::install_dispatching(link::InterServerLink& link) {
    auto& dispatcher = link.dispatcher();
    dispatcher.set_route_table(route_table_.get());
    dispatcher.allow_domain("internal");  // §11.3 上行白名单：意图族入闸
    // 932 internal_envelope = 唯一上行受理面（P3-3 移交余件②：bare
    // 924-928 双受理已退役——消费面清零，gateway m1_egress 恒包信封
    // 5bdca27f，无其余发端）。裸五族帧不再登记 handler = dispatcher
    // no_handler 静默丢弃：不受理、不回执、不断连、不计 handler 拒收。
    dispatcher.set_handler(
        id_of(MsgId::internal_envelope),
        [this](std::uint16_t msg_id, const std::uint8_t* data, std::size_t size) {
            handle_envelope_frame(msg_id, data, size);
        });
    // 944 debug_eval_request 裸帧受理（§7.1 attach transport 契约面：
    // admin 单入口上行——eval 非意图信封族，不走 932；回执 945 按
    // eval_id 异步回传，发端 peer 自报名在此捕获——重连 = 同名新链
    // 重新过本装配，回执按名查链路而非存链路指针）。
    dispatcher.set_handler(
        id_of(MsgId::debug_eval_request),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_debug_eval_frame(link.peer_name(), data, size);
        });
    // 952 module_rollout_request 裸帧受理（P4 仅代码更新批）：受理点同步
    // 执行于 gameThread_（M1 泵 = 宿主泵同线程）——ModuleRollout 契约线
    // 程 = owning 线程，无需 tick 窗口排队；观察窗由 gameLoop 拍自驱。
    dispatcher.set_handler(
        id_of(MsgId::module_rollout_request),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_module_rollout_frame(link.peer_name(), data, size);
        });
    // 956 world_drain_request 裸帧受理（P4 更新排空批）：受理点同步执行
    // 于 gameThread_（与 launch/玩法推进同线程）——排空状态机单线程独占
    // 无锁，同步回 957。
    dispatcher.set_handler(
        id_of(MsgId::world_drain_request),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_world_drain_frame(link.peer_name(), data, size);
        });
    // 960 cell_register_request 裸帧受理（L101 注册首切片）：cell 执行
    // 进程拨入登记——受理点同步执行于 gameThread_（注册表单线程独占
    // 无锁），同步回 961；peer 自报名 = 链路名（回执/后续命令按名查链路）。
    dispatcher.set_handler(
        id_of(MsgId::cell_register_request),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_register_frame(link.peer_name(), data, size);
        });
    // 963 cell_retire_request 裸帧受理（L104 缩容管理命令面）：受理点
    // 同步执行于 gameThread_（登记表/分配池单线程独占无锁），同步回 966。
    dispatcher.set_handler(
        id_of(MsgId::cell_retire_request),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_retire_frame(link.peer_name(), data, size);
        });
    // 967 cell_space_report 裸帧受理（断连对账批B）：受理点同步裁决于
    // gameThread_（登记表/分配池单写者上下文）；无回执——962 即裁决载体。
    dispatcher.set_handler(
        id_of(MsgId::cell_space_report),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_space_report_frame(link.peer_name(), data, size);
        });
    // 945/953/957 cell 执行端回执（切片2 World→cell 命令转发）：cell 宿主
    // 经注册连接反向发送，World 在此受理并按 eval_id/request_id 关联原
    // fleetctl/admin peer 后回传。发端 peer = cell 自报名。
    dispatcher.set_handler(
        id_of(MsgId::debug_eval_result),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_eval_result_frame(link.peer_name(), data, size);
        });
    dispatcher.set_handler(
        id_of(MsgId::module_rollout_reply),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_rollout_reply_frame(link.peer_name(), data, size);
        });
    dispatcher.set_handler(
        id_of(MsgId::world_drain_reply),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_drain_reply_frame(link.peer_name(), data, size);
        });
    // 929 cell_intent_ack（切片2 意图转发回程）：cell 宿主经注册连接反向
    // 发送，World 在此受理并经 gateway_link() 转发 gateway。
    dispatcher.set_handler(
        id_of(MsgId::cell_intent_ack),
        [&link, this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_intent_ack_frame(link.peer_name(), data, size);
        });
    // 939 client_deliver（切片3 独立 cell 回程）：cell 宿主经注册连接
    // 反向发送客户端帧，World 在此受理并经 gateway_link() 转发 gateway
    //（与 sendClientDeliver 的 gateway 直连面同载荷格式）。
    dispatcher.set_handler(
        id_of(MsgId::client_deliver),
        [this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            handle_cell_client_deliver_frame(data, size);
        });
}

std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
    std::function<void(const std::vector<std::uint8_t>&)> on_intent_raw,
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval,
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        on_module_rollout,
    std::function<void(const std::string&, const WorldDrainRequest&)> on_world_drain,
    std::function<void(const std::string&, const CellRegisterRequest&)> on_cell_register,
    std::function<void(const std::string&, const CellRetireRequest&)> on_cell_retire,
    std::function<void(const std::string&, const CellSpaceReport&)>
        on_cell_space_report,
    std::function<void(const std::string&, const CellEvalResult&)>
        on_cell_eval_result,
    std::function<void(const std::string&, const CellRolloutReply&)>
        on_cell_rollout_reply,
    std::function<void(const std::string&, const CellDrainReply&)>
        on_cell_drain_reply,
    std::function<void(const std::string&, const CellIntentAck&)>
        on_cell_intent_ack) {
    return std::make_unique<M1LinkServerImpl>(std::move(config), std::move(on_intent),
                                              std::move(on_intent_raw),
                                              std::move(on_debug_eval),
                                              std::move(on_module_rollout),
                                              std::move(on_world_drain),
                                              std::move(on_cell_register),
                                              std::move(on_cell_retire),
                                              std::move(on_cell_space_report),
                                              std::move(on_cell_eval_result),
                                              std::move(on_cell_rollout_reply),
                                              std::move(on_cell_drain_reply),
                                              std::move(on_cell_intent_ack));
}

#else // _WIN32

std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig /*config*/, std::function<void(IntentEnvelope)> /*on_intent*/,
    std::function<void(const std::vector<std::uint8_t>&)> /*on_intent_raw*/,
    std::function<void(const std::string&, const DebugEvalRequest&)> /*on_debug_eval*/,
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        /*on_module_rollout*/,
    std::function<void(const std::string&, const WorldDrainRequest&)>
        /*on_world_drain*/,
    std::function<void(const std::string&, const CellRegisterRequest&)>
        /*on_cell_register*/,
    std::function<void(const std::string&, const CellRetireRequest&)>
        /*on_cell_retire*/,
    std::function<void(const std::string&, const CellSpaceReport&)>
        /*on_cell_space_report*/,
    std::function<void(const std::string&, const CellEvalResult&)>
        /*on_cell_eval_result*/,
    std::function<void(const std::string&, const CellRolloutReply&)>
        /*on_cell_rollout_reply*/,
    std::function<void(const std::string&, const CellDrainReply&)>
        /*on_cell_drain_reply*/,
    std::function<void(const std::string&, const CellIntentAck&)>
        /*on_cell_intent_ack*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退 null 桩（缝缺省
    // 实现：start true / 泵空转 / 应答丢弃），WorldApp 装配面无 #if。
    return std::make_unique<M1LinkServer>();
}

#endif // !_WIN32

} // namespace world_app

