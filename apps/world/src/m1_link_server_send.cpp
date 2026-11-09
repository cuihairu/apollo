// m1_link_server 拆分单元（LAY-008）：回执与命令发送面——12 个 send* 出站封装
//（945/953/957 回执回传、929 intent ack 转发、961/966 注册/缩容回执、
// 962 Space 指派、944/952/956 下行命令、939 client_deliver 下行、932 裸转发），
// Reliable 投递纪律原样。
#include "m1_link_server_impl.hpp"

#if !defined(_WIN32)

namespace world_app {

void M1LinkServerImpl::sendIntentAck(std::uint64_t request_id, std::int32_t code,
                   const std::string& message) {
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

void M1LinkServerImpl::sendEvalResult(const std::string& peer, std::uint64_t eval_id, bool ok,
                    const std::string& result, const std::string& error) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || eval_id == 0) {
        return;  // 0 保留（回执关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
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

void M1LinkServerImpl::sendModuleRolloutReply(const std::string& peer, std::uint64_t request_id,
                            std::int32_t op, bool ok, std::int32_t state,
                            std::uint64_t window_faults,
                            std::uint64_t module_version,
                            const std::string& detail) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request_id == 0) {
        return;  // 0 保留（回执关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
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

void M1LinkServerImpl::sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                       const std::vector<std::uint8_t>& payload) {
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

void M1LinkServerImpl::sendWorldDrainReply(const std::string& peer, std::uint64_t request_id,                             std::int32_t op, bool ok, std::int32_t state,
                         std::uint64_t active_instances,
                         const std::string& detail) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request_id == 0) {
        return;  // 0 保留（回执关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
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

void M1LinkServerImpl::sendCellRegisterReply(const std::string& peer, std::uint64_t request_id,
                           bool accepted, std::int32_t code,
                           std::uint64_t management_epoch,
                           std::uint64_t assigned_cell_id,
                           const std::string& detail) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request_id == 0) {
        return;  // 0 保留（回执关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
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

void M1LinkServerImpl::sendCellRetireReply(const std::string& peer, std::uint64_t request_id,
                         bool accepted, std::int32_t code,
                         std::int64_t management_epoch, std::uint64_t cell_id,
                         const std::string& detail) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request_id == 0) {
        return;  // 0 保留（回执关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return;  // 未知 peer = 尽力面
    }
    std::vector<wire::FieldValue> fields(6);
    fields[0].integer = static_cast<std::int64_t>(request_id);
    fields[1].integer = accepted ? 1 : 0;
    fields[2].integer = code;
    fields[3].integer = management_epoch;
    fields[4].integer = static_cast<std::int64_t>(cell_id);
    fields[5].text = detail;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::cell_retire_reply), fields, payload)) {
        return;
    }
    (void)link->send(id_of(MsgId::cell_retire_reply), payload.data(), payload.size(),
                     link::SendClass::kReliable);
}

void M1LinkServerImpl::sendCellSpaceAssign(const std::string& peer, std::uint64_t assignment_version,
                         std::uint64_t management_epoch, std::uint64_t space_id,
                         std::uint64_t assigned_cell_id, bool ready) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr) {
        return;
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return;  // 未知 peer = 尽力面（重推随下个受理拍）
    }
    std::vector<wire::FieldValue> fields(5);
    fields[0].integer = static_cast<std::int64_t>(assignment_version);
    fields[1].integer = static_cast<std::int64_t>(management_epoch);
    fields[2].integer = static_cast<std::int64_t>(space_id);
    fields[3].integer = static_cast<std::int64_t>(assigned_cell_id);
    fields[4].integer = ready ? 1 : 0;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::cell_space_assign), fields, payload)) {
        return;
    }
    (void)link->send(id_of(MsgId::cell_space_assign), payload.data(), payload.size(),
                     link::SendClass::kReliable);
}

bool M1LinkServerImpl::sendCellDebugEvalRequest(const std::string& peer,
                              const DebugEvalRequest& request) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request.eval_id == 0) {
        return false;  // 0 保留（关联键缺位即静默）
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return false;  // 未知 peer = 尽力面（管理面超时重发裁决）
    }
    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(request.eval_id);
    fields[1].integer = static_cast<std::int64_t>(request.scene_id);
    fields[2].text = request.source;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::debug_eval_request), fields, payload)) {
        return false;
    }
    // Reliable：管理命令不可丢（断连排队，重连恰一次补投；同 962）。
    return link->send(id_of(MsgId::debug_eval_request), payload.data(),
                      payload.size(), link::SendClass::kReliable)
           == link::SendCode::kAccepted;
}

bool M1LinkServerImpl::sendCellModuleRolloutRequest(const std::string& peer,
                                  const ModuleRolloutRequest& request) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request.request_id == 0) {
        return false;  // 0 保留
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return false;
    }
    std::vector<wire::FieldValue> fields(8);
    fields[0].integer = static_cast<std::int64_t>(request.request_id);
    fields[1].integer = request.op;
    fields[2].text = request.name;
    fields[3].integer = static_cast<std::int64_t>(request.version);
    fields[4].text = request.deps;
    fields[5].text = request.source;
    fields[6].integer = static_cast<std::int64_t>(request.observe_ticks);
    fields[7].integer = static_cast<std::int64_t>(request.fault_threshold);
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::module_rollout_request), fields,
                              payload)) {
        return false;
    }
    return link->send(id_of(MsgId::module_rollout_request), payload.data(),
                      payload.size(), link::SendClass::kReliable)
           == link::SendCode::kAccepted;
}

bool M1LinkServerImpl::sendCellWorldDrainRequest(const std::string& peer,
                               const WorldDrainRequest& request) {
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || request.request_id == 0) {
        return false;  // 0 保留
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return false;
    }
    std::vector<wire::FieldValue> fields(3);
    fields[0].integer = static_cast<std::int64_t>(request.request_id);
    fields[1].integer = request.op;
    fields[2].integer = request.deadline_s;
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(id_of(MsgId::world_drain_request), fields,
                              payload)) {
        return false;
    }
    return link->send(id_of(MsgId::world_drain_request), payload.data(),
                      payload.size(), link::SendClass::kReliable)
           == link::SendCode::kAccepted;
}

bool M1LinkServerImpl::sendCellIntentRaw(const std::string& peer,
                       const std::vector<std::uint8_t>& payload) {
    // 意图族原样转发（切片2）：payload = 932 internal_envelope 的 packed
    // 帧（InterServerLink 自封 u16 id，此处只传 payload）。空帧/未知
    // peer = false（调用方可即时回 2001/2002）。
    auto mgr = manager_.load(std::memory_order_acquire);
    if (!mgr || payload.empty()) {
        return false;
    }
    auto* link = mgr->link(peer);
    if (link == nullptr) {
        return false;
    }
    return link->send(id_of(MsgId::internal_envelope), payload.data(),
                      payload.size(), link::SendClass::kReliable)
           == link::SendCode::kAccepted;
}


} // namespace world_app

#endif // !_WIN32

