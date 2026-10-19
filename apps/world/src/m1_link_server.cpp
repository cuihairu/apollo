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
#include <sstream>
#include <utility>

#endif // !_WIN32

namespace world_app {

#if !defined(_WIN32)



namespace {

namespace link = apollo::net::link;
namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

// 契约 msg id → legacy 类型 + 帧字节（字段序 = 契约表序，M1 契约批镜像定案）。
// false = id 不在本进程装配面（dispatcher 白名单已挡，此处兜底计数）。
bool build_legacy_intent(std::uint16_t msg_id, const std::vector<wire::FieldValue>& f,
                         IntentEnvelope& out) {
    out = IntentEnvelope{};
    // 五族意图共用头部两字段：request_id（应答关联键）+ session_id（源会话）。
    if (f.size() < 2) {
        return false;
    }
    out.request_id = static_cast<std::uint64_t>(f[0].integer);
    out.session_id = static_cast<protocol::SessionID>(f[1].integer);

    switch (msg_id) {
        case id_of(MsgId::cell_create_entity): {
            if (f.size() != 8) {
                return false;
            }
            protocol::CellCreateEntity msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.entityType = static_cast<protocol::EntityType>(
                static_cast<std::uint8_t>(f[3].integer));
            msg.spaceId = static_cast<protocol::SpaceID>(f[4].integer);
            msg.position = protocol::Position(static_cast<float>(f[5].real),
                                              static_cast<float>(f[6].real),
                                              static_cast<float>(f[7].real));
            out.type = protocol::MessageType::CELL_CREATE_ENTITY;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_destroy_entity): {
            if (f.size() != 3) {
                return false;
            }
            protocol::CellDestroyEntity msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::CELL_DESTROY_ENTITY;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_entity_move): {
            if (f.size() != 9) {
                return false;
            }
            protocol::CellEntityMove msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.oldPos = protocol::Position(static_cast<float>(f[3].real),
                                            static_cast<float>(f[4].real),
                                            static_cast<float>(f[5].real));
            msg.newPos = protocol::Position(static_cast<float>(f[6].real),
                                            static_cast<float>(f[7].real),
                                            static_cast<float>(f[8].real));
            out.type = protocol::MessageType::CELL_ENTITY_MOVE;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_cross_border): {
            if (f.size() != 8) {
                return false;
            }
            protocol::CellCrossBorder msg;
            msg.entityId = static_cast<protocol::EntityID>(f[2].integer);
            msg.fromSpace = static_cast<protocol::SpaceID>(f[3].integer);
            msg.toSpace = static_cast<protocol::SpaceID>(f[4].integer);
            msg.position = protocol::Position(static_cast<float>(f[5].real),
                                              static_cast<float>(f[6].real),
                                              static_cast<float>(f[7].real));
            out.type = protocol::MessageType::CELL_CROSS_BORDER;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::cell_skill_cast): {
            if (f.size() != 11) {
                return false;
            }
            protocol::CombatSkillCast msg;
            msg.casterId = static_cast<protocol::EntityID>(f[2].integer);
            msg.targetId = static_cast<protocol::EntityID>(f[3].integer);
            msg.skillId = static_cast<std::uint32_t>(f[4].integer);
            msg.casterPos = protocol::Position(static_cast<float>(f[5].real),
                                               static_cast<float>(f[6].real),
                                               static_cast<float>(f[7].real));
            msg.targetPos = protocol::Position(static_cast<float>(f[8].real),
                                               static_cast<float>(f[9].real),
                                               static_cast<float>(f[10].real));
            out.type = protocol::MessageType::COMBAT_SKILL_CAST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        // 队伍与小队副本意图族（P2 真实链路；契约 946-951）：头部两字段同
        // cell 族（request_id + session_id），业务字段 → 协议 native 帧。
        case id_of(MsgId::party_create_intent): {
            if (f.size() != 2) {
                return false;
            }
            protocol::PartyCreateRequest msg;
            out.type = protocol::MessageType::PARTY_CREATE_REQUEST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::party_join_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::PartyJoinRequest msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::PARTY_JOIN_REQUEST;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_launch_intent): {
            if (f.size() != 4) {
                return false;
            }
            protocol::SquadLaunch msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            msg.mapId = static_cast<protocol::EntityID>(f[3].integer);
            out.type = protocol::MessageType::SQUAD_LAUNCH;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_enter_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::SquadEnter msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::SQUAD_ENTER;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_start_intent): {
            if (f.size() != 3) {
                return false;
            }
            protocol::SquadStart msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            out.type = protocol::MessageType::SQUAD_START;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        case id_of(MsgId::squad_kill_intent): {
            if (f.size() != 4) {
                return false;
            }
            protocol::SquadKill msg;
            msg.partyId = static_cast<protocol::EntityID>(f[2].integer);
            msg.monsterEntityId = static_cast<protocol::EntityID>(f[3].integer);
            out.type = protocol::MessageType::SQUAD_KILL;
            out.frame = protocol::MessageCodec::encode(msg, out.session_id);
            return true;
        }
        default:
            return false;
    }
}

class M1LinkServerImpl final : public M1LinkServer {
public:
    M1LinkServerImpl(M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
                     std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval,
                     std::function<void(const std::string&, const ModuleRolloutRequest&)>
                         on_module_rollout,
                     std::function<void(const std::string&, const WorldDrainRequest&)>
                         on_world_drain,
                     std::function<void(const std::string&, const CellRegisterRequest&)>
                         on_cell_register)
        : config_(std::move(config))
        , on_intent_(std::move(on_intent))
        , on_debug_eval_(std::move(on_debug_eval))
        , on_module_rollout_(std::move(on_module_rollout))
        , on_world_drain_(std::move(on_world_drain))
        , on_cell_register_(std::move(on_cell_register)) {}

    bool start() override {
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
        manager_ = std::make_unique<link::LinkManager>(mc);

        link::EndpointConfig ep;
        ep.name = "gateway";  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
        ep.host = config_.host;
        ep.port = config_.port;
        ep.role = link::EndpointRole::kPassive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // handler/表/白名单随新 peer 上线装配（fan-out 面：链路对象在
        // peer_listener 回调后地址稳定，重连 = 同名新链重新过此装配）。
        manager_->set_peer_listener([this](link::InterServerLink& peer) {
            install_dispatching(peer);
        });

        started_.store(true, std::memory_order_release);
        return true;
    }

    void stop() override {
        started_.store(false, std::memory_order_release);
        manager_.reset();  // 监听器与链路随管理器析构关闭
    }

    void pump(std::uint64_t now_ms) override {
        if (manager_) {
            manager_->pump(now_ms);
        }
    }

    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override {
        auto* link = gateway_link();
        if (link == nullptr) {
            return;  // 端点未装配 = 静默（start 前回调不应发生）
        }
        std::vector<wire::FieldValue> fields(3);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = code;
        fields[2].text = message;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::cell_intent_ack), fields, payload)) {
            return;
        }
        // Reliable：回执不可丢；链路断连期间入队，重连恰一次补投。
        //（InterServerLink::send 自封 u16 id 信封——传裸载荷，勿再包一层）
        (void)link->send(id_of(MsgId::cell_intent_ack), payload.data(), payload.size(),
                         link::SendClass::kReliable);
    }

    void sendEvalResult(const std::string& peer, std::uint64_t eval_id, bool ok,
                        const std::string& result, const std::string& error) override {
        if (!manager_ || eval_id == 0) {
            return;  // 0 保留（回执关联键缺位即静默）
        }
        auto* link = manager_->link(peer);
        if (link == nullptr) {
            return;  // 未知 peer = 尽力面（admin 超时面重发裁决）
        }
        std::vector<wire::FieldValue> fields(4);
        fields[0].integer = static_cast<std::int64_t>(eval_id);
        fields[1].integer = ok ? 1 : 0;
        fields[2].text = result;
        fields[3].text = error;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::debug_eval_result), fields, payload)) {
            return;
        }
        // Reliable：回执不可丢（sendIntentAck 同型——断连排队，重连恰一次补投）。
        (void)link->send(id_of(MsgId::debug_eval_result), payload.data(), payload.size(),
                         link::SendClass::kReliable);
    }

    void sendModuleRolloutReply(const std::string& peer, std::uint64_t request_id,
                                std::int32_t op, bool ok, std::int32_t state,
                                std::uint64_t window_faults,
                                std::uint64_t module_version,
                                const std::string& detail) override {
        if (!manager_ || request_id == 0) {
            return;  // 0 保留（回执关联键缺位即静默）
        }
        auto* link = manager_->link(peer);
        if (link == nullptr) {
            return;  // 未知 peer = 尽力面（控制面超时面重发裁决）
        }
        std::vector<wire::FieldValue> fields(7);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = op;
        fields[2].integer = ok ? 1 : 0;
        fields[3].integer = state;
        fields[4].integer = static_cast<std::int64_t>(window_faults);
        fields[5].integer = static_cast<std::int64_t>(module_version);
        fields[6].text = detail;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::module_rollout_reply), fields, payload)) {
            return;
        }
        (void)link->send(id_of(MsgId::module_rollout_reply), payload.data(), payload.size(),
                         link::SendClass::kReliable);
    }

    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override {
        auto* link = gateway_link();
        if (link == nullptr || payload.empty()) {
            return;  // 端点未装配 = 静默（start 前回调不应发生）
        }
        std::vector<wire::FieldValue> fields(3);
        fields[0].integer = static_cast<std::int64_t>(session_id);
        fields[1].integer = gateway_id;
        fields[2].bytes = payload;
        std::vector<std::uint8_t> packed;
        if (!wire::encode_payload(id_of(MsgId::client_deliver), fields, packed)) {
            return;
        }
        // Reliable：下行不可丢（断连排队，重连恰一次补投；sendIntentAck 同型）。
        //（InterServerLink::send 自封 u16 id 信封——传裸载荷，勿再包一层）
        (void)link->send(id_of(MsgId::client_deliver), packed.data(), packed.size(),
                         link::SendClass::kReliable);
    }

    void sendWorldDrainReply(const std::string& peer, std::uint64_t request_id,                             std::int32_t op, bool ok, std::int32_t state,
                             std::uint64_t active_instances,
                             const std::string& detail) override {
        if (!manager_ || request_id == 0) {
            return;  // 0 保留（回执关联键缺位即静默）
        }
        auto* link = manager_->link(peer);
        if (link == nullptr) {
            return;  // 未知 peer = 尽力面（控制面超时面重发裁决）
        }
        std::vector<wire::FieldValue> fields(6);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = op;
        fields[2].integer = ok ? 1 : 0;
        fields[3].integer = state;
        fields[4].integer = static_cast<std::int64_t>(active_instances);
        fields[5].text = detail;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::world_drain_reply), fields, payload)) {
            return;
        }
        (void)link->send(id_of(MsgId::world_drain_reply), payload.data(), payload.size(),
                         link::SendClass::kReliable);
    }

    void sendCellRegisterReply(const std::string& peer, std::uint64_t request_id,
                               bool accepted, std::int32_t code,
                               std::uint64_t management_epoch,
                               std::uint64_t assigned_cell_id,
                               const std::string& detail) override {
        if (!manager_ || request_id == 0) {
            return;  // 0 保留（回执关联键缺位即静默）
        }
        auto* link = manager_->link(peer);
        if (link == nullptr) {
            return;  // 未知 peer = 尽力面（cell 超时面重发裁决）
        }
        std::vector<wire::FieldValue> fields(6);
        fields[0].integer = static_cast<std::int64_t>(request_id);
        fields[1].integer = accepted ? 1 : 0;
        fields[2].integer = code;
        fields[3].integer = static_cast<std::int64_t>(management_epoch);
        fields[4].integer = static_cast<std::int64_t>(assigned_cell_id);
        fields[5].text = detail;
        std::vector<std::uint8_t> payload;
        if (!wire::encode_payload(id_of(MsgId::cell_register_reply), fields, payload)) {
            return;
        }
        (void)link->send(id_of(MsgId::cell_register_reply), payload.data(), payload.size(),
                         link::SendClass::kReliable);
    }

    std::uint16_t listenPort() const override {
        return manager_ ? manager_->endpoint_port("gateway") : 0;
    }

    bool gatewayConnected() const override {
        auto* link = const_cast<M1LinkServerImpl*>(this)->gateway_link();
        return link != nullptr && link->state() == link::LinkState::kUp;
    }

    std::uint64_t receivedIntents() const override {
        return received_.load(std::memory_order_relaxed);
    }

    std::uint64_t rejectedFrames() const override {
        return rejected_.load(std::memory_order_relaxed);
    }

private:
    link::InterServerLink* gateway_link() {
        return manager_ ? manager_->link("gateway") : nullptr;
    }

    bool load_route_table(link::RouteTable& table) {
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

    void install_dispatching(link::InterServerLink& link) {
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
    }

    void handle_debug_eval_frame(const std::string& peer, const std::uint8_t* data,
                                 std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::debug_eval_request), data, size, fields)
            || fields.size() != 3 || on_debug_eval_ == nullptr) {
            ++rejected_;
            return;
        }
        DebugEvalRequest req;
        req.eval_id = static_cast<std::uint64_t>(fields[0].integer);
        req.scene_id = static_cast<std::uint64_t>(fields[1].integer);
        req.source = fields[2].text;
        on_debug_eval_(peer, req);
    }

    void handle_module_rollout_frame(const std::string& peer, const std::uint8_t* data,
                                     std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::module_rollout_request), data, size,
                                  fields)
            || fields.size() != 8 || on_module_rollout_ == nullptr) {
            ++rejected_;
            return;
        }
        ModuleRolloutRequest req;
        req.request_id = static_cast<std::uint64_t>(fields[0].integer);
        req.op = static_cast<std::int32_t>(fields[1].integer);
        req.name = fields[2].text;
        req.version = static_cast<std::uint64_t>(fields[3].integer);
        req.deps = fields[4].text;
        req.source = fields[5].text;
        req.observe_ticks = static_cast<std::uint64_t>(fields[6].integer);
        req.fault_threshold = static_cast<std::uint64_t>(fields[7].integer);
        on_module_rollout_(peer, req);
    }

    void handle_world_drain_frame(const std::string& peer, const std::uint8_t* data,
                                  std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::world_drain_request), data, size, fields)
            || fields.size() != 3 || on_world_drain_ == nullptr) {
            ++rejected_;
            return;
        }
        WorldDrainRequest req;
        req.request_id = static_cast<std::uint64_t>(fields[0].integer);
        req.op = static_cast<std::int32_t>(fields[1].integer);
        req.deadline_s = fields[2].integer;
        on_world_drain_(peer, req);
    }

    void handle_cell_register_frame(const std::string& peer, const std::uint8_t* data,
                                    std::size_t size) {
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(id_of(MsgId::cell_register_request), data, size, fields)
            || fields.size() != 9 || on_cell_register_ == nullptr) {
            ++rejected_;
            return;
        }
        CellRegisterRequest req;
        req.request_id = static_cast<std::uint64_t>(fields[0].integer);
        req.cell_id = static_cast<std::uint64_t>(fields[1].integer);
        req.start_epoch = static_cast<std::uint64_t>(fields[2].integer);
        req.host = fields[3].text;
        req.port = static_cast<std::int32_t>(fields[4].integer);
        req.capabilities = static_cast<std::int32_t>(fields[5].integer);
        req.load = static_cast<std::int32_t>(fields[6].integer);
        req.drain_state = static_cast<std::int32_t>(fields[7].integer);
        req.ready = fields[8].integer != 0;
        on_cell_register_(peer, req);
    }

    void handle_envelope_frame(std::uint16_t /*msg_id*/, const std::uint8_t* data,
                               std::size_t size) {
        trace::EnvelopeFields env;
        if (!trace::unpack_envelope(data, size, env)) {
            ++rejected_;
            return;
        }
        // 内层载荷自识（u16 LE id + 契约载荷）；非五族意图内层 = 拒收
        // 计数——本接收面只装意图族（RouteControl 等内型归各自接线批）。
        std::uint16_t inner_id = 0;
        const std::uint8_t* body = nullptr;
        std::size_t body_size = 0;
        if (!trace::split_payload(env.payload, inner_id, body, body_size)) {
            ++rejected_;
            return;
        }
        std::vector<wire::FieldValue> fields;
        if (!wire::decode_payload(inner_id, body, body_size, fields)) {
            ++rejected_;
            return;
        }
        IntentEnvelope intent;
        if (!build_legacy_intent(inner_id, fields, intent)) {
            ++rejected_;
            return;
        }
        if (env.trace.has_trace()) {
            intent.trace = env.trace;  // 旗标随信封传播，下游不再重采
        }
        intent.received_ms = trace::now_ms();
        ++received_;
        if (on_intent_) {
            on_intent_(std::move(intent));
        }
    }

    M1LinkConfig config_;
    std::function<void(IntentEnvelope)> on_intent_;
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval_;
    std::function<void(const std::string&, const ModuleRolloutRequest&)> on_module_rollout_;
    std::function<void(const std::string&, const WorldDrainRequest&)> on_world_drain_;
    std::function<void(const std::string&, const CellRegisterRequest&)> on_cell_register_;
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::atomic<bool> started_{false};
    std::atomic<std::uint64_t> received_{0};
    std::atomic<std::uint64_t> rejected_{0};
};

} // namespace

std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig config, std::function<void(IntentEnvelope)> on_intent,
    std::function<void(const std::string&, const DebugEvalRequest&)> on_debug_eval,
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        on_module_rollout,
    std::function<void(const std::string&, const WorldDrainRequest&)> on_world_drain,
    std::function<void(const std::string&, const CellRegisterRequest&)> on_cell_register) {
    return std::make_unique<M1LinkServerImpl>(std::move(config), std::move(on_intent),
                                              std::move(on_debug_eval),
                                              std::move(on_module_rollout),
                                              std::move(on_world_drain),
                                              std::move(on_cell_register));
}

#else // _WIN32

std::unique_ptr<M1LinkServer> makeM1LinkServer(
    M1LinkConfig /*config*/, std::function<void(IntentEnvelope)> /*on_intent*/,
    std::function<void(const std::string&, const DebugEvalRequest&)> /*on_debug_eval*/,
    std::function<void(const std::string&, const ModuleRolloutRequest&)>
        /*on_module_rollout*/,
    std::function<void(const std::string&, const WorldDrainRequest&)>
        /*on_world_drain*/,
    std::function<void(const std::string&, const CellRegisterRequest&)>
        /*on_cell_register*/) {
    // link TCP 底座不进 M1 批（POSIX only）——Windows 退 null 桩（缝缺省
    // 实现：start true / 泵空转 / 应答丢弃），WorldApp 装配面无 #if。
    return std::make_unique<M1LinkServer>();
}

#endif // !_WIN32

} // namespace world_app
