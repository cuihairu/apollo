// m1_link_server 拆分单元（LAY-008）：裸帧受理面——12 个 handle_*_frame 契约
// 解码 + 宿主回调投递（944/952/956/960/963/967 上行命令与 945/953/957/929
// cell 回程、939 client_deliver 回程、932 信封帧受理含原始 envelope 转发缝）。
#include "m1_link_server_impl.hpp"

#if !defined(_WIN32)

namespace world_app {

void M1LinkServerImpl::handle_cell_client_deliver_frame(const std::uint8_t* data,
                                      std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::client_deliver), data, size,
                              fields)
        || fields.size() != 3) {
        ++rejected_;
        return;
    }
    sendClientDeliver(
        static_cast<std::uint64_t>(fields[0].integer),
        static_cast<std::uint32_t>(fields[1].integer),
        std::move(fields[2].bytes));
}

void M1LinkServerImpl::handle_debug_eval_frame(const std::string& peer, const std::uint8_t* data,
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

void M1LinkServerImpl::handle_module_rollout_frame(const std::string& peer, const std::uint8_t* data,
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

void M1LinkServerImpl::handle_world_drain_frame(const std::string& peer, const std::uint8_t* data,
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

void M1LinkServerImpl::handle_cell_register_frame(const std::string& peer, const std::uint8_t* data,
                                std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (on_cell_register_ == nullptr) {
        ++rejected_;
        return;
    }
    // 960 线格式向后兼容（L109 认证凭据面收口）：新版帧 11 字段（第 11 =
    // credential）。旧版 cell 以 10 字段旧格式（无 credential 字段）注册
    // 时，decode 层按旧字段数剥离——credential 视为未自证（空），由
    // 受理面按 registerToken 配置裁决（配置非空即拒 code 6）。因此旧帧
    // 受理仅在认证关（registerToken 空）时成立；认证开时旧帧在凭据裁决
    // 处被 code 6 拒，到不了兼容白名单路径（world_app.cpp 凭据先判）。
    // 混编期间认证关下显式声明白名单投影的旧 cell 才能通过兼容裁决
    // （见 config.hpp compatWhitelist），不因旧格式静默丢弃。
    const bool full = wire::decode_payload(
        id_of(MsgId::cell_register_request), data, size, fields);
    const bool ok = full ? true
                         : wire::decode_payload_fields(
                               id_of(MsgId::cell_register_request), data, size,
                               fields, 10);
    if (!ok) {
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
    req.compat = fields[6].integer;
    req.load = static_cast<std::int32_t>(fields[7].integer);
    req.drain_state = static_cast<std::int32_t>(fields[8].integer);
    req.ready = fields[9].integer != 0;
    req.credential = full ? fields[10].text : std::string{};
    on_cell_register_(peer, req);
}

void M1LinkServerImpl::handle_cell_retire_frame(const std::string& peer, const std::uint8_t* data,
                              std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::cell_retire_request), data, size, fields)
        || fields.size() != 3 || on_cell_retire_ == nullptr) {
        ++rejected_;
        return;
    }
    CellRetireRequest req;
    req.request_id = static_cast<std::uint64_t>(fields[0].integer);
    req.op = static_cast<std::int32_t>(fields[1].integer);
    req.cell_id = static_cast<std::uint64_t>(fields[2].integer);
    on_cell_retire_(peer, req);
}

void M1LinkServerImpl::handle_cell_space_report_frame(const std::string& peer, const std::uint8_t* data,
                                    std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::cell_space_report), data, size, fields)
        || fields.size() != 6 || on_cell_space_report_ == nullptr) {
        ++rejected_;
        return;
    }
    CellSpaceReport req;
    req.request_id = static_cast<std::uint64_t>(fields[0].integer);
    req.cell_id = static_cast<std::uint64_t>(fields[1].integer);
    req.start_epoch = static_cast<std::uint64_t>(fields[2].integer);
    req.space_id = static_cast<std::uint64_t>(fields[3].integer);
    req.applied_version = static_cast<std::uint64_t>(fields[4].integer);
    req.applied_mgmt_epoch = static_cast<std::uint64_t>(fields[5].integer);
    on_cell_space_report_(peer, req);
}

void M1LinkServerImpl::handle_cell_eval_result_frame(const std::string& peer, const std::uint8_t* data,
                                   std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::debug_eval_result), data, size, fields)
        || fields.size() != 4 || on_cell_eval_result_ == nullptr) {
        ++rejected_;
        return;
    }
    CellEvalResult res;
    res.eval_id = static_cast<std::uint64_t>(fields[0].integer);
    res.ok = fields[1].integer != 0;
    res.result = fields[2].text;
    res.error = fields[3].text;
    on_cell_eval_result_(peer, res);
}

void M1LinkServerImpl::handle_cell_rollout_reply_frame(const std::string& peer, const std::uint8_t* data,
                                     std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::module_rollout_reply), data, size, fields)
        || fields.size() != 7 || on_cell_rollout_reply_ == nullptr) {
        ++rejected_;
        return;
    }
    CellRolloutReply rep;
    rep.request_id = static_cast<std::uint64_t>(fields[0].integer);
    rep.op = static_cast<std::int32_t>(fields[1].integer);
    rep.ok = fields[2].integer != 0;
    rep.state = static_cast<std::int32_t>(fields[3].integer);
    rep.window_faults = static_cast<std::uint64_t>(fields[4].integer);
    rep.module_version = static_cast<std::uint64_t>(fields[5].integer);
    rep.detail = fields[6].text;
    on_cell_rollout_reply_(peer, rep);
}

void M1LinkServerImpl::handle_cell_drain_reply_frame(const std::string& peer, const std::uint8_t* data,
                                   std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::world_drain_reply), data, size, fields)
        || fields.size() != 6 || on_cell_drain_reply_ == nullptr) {
        ++rejected_;
        return;
    }
    CellDrainReply rep;
    rep.request_id = static_cast<std::uint64_t>(fields[0].integer);
    rep.op = static_cast<std::int32_t>(fields[1].integer);
    rep.ok = fields[2].integer != 0;
    rep.state = static_cast<std::int32_t>(fields[3].integer);
    rep.active_instances = static_cast<std::uint64_t>(fields[4].integer);
    rep.detail = fields[5].text;
    on_cell_drain_reply_(peer, rep);
}

void M1LinkServerImpl::handle_cell_intent_ack_frame(const std::string& peer, const std::uint8_t* data,
                                  std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(id_of(MsgId::cell_intent_ack), data, size, fields)
        || fields.size() != 3 || on_cell_intent_ack_ == nullptr) {
        ++rejected_;
        return;
    }
    CellIntentAck ack;
    ack.request_id = static_cast<std::uint64_t>(fields[0].integer);
    ack.code = static_cast<std::int32_t>(fields[1].integer);
    ack.message = fields[2].text;
    on_cell_intent_ack_(peer, ack);
}

void M1LinkServerImpl::handle_envelope_frame(std::uint16_t /*msg_id*/, const std::uint8_t* data,
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
    if (on_intent_raw_) {
        // 切片2 意图族转发缝：原始 932 packed envelope 原样交给宿主
        //（World 无内嵌内核时转发到注册 cell；内嵌形态不走此面）。
        on_intent_raw_(std::vector<std::uint8_t>(data, data + size));
    }
    if (on_intent_) {
        on_intent_(std::move(intent));
    }
}


} // namespace world_app

#endif // !_WIN32

