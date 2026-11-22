#include "apollo/net/linktrace/envelope.hpp"

#include <cstring>

#include "apollo/net/wire/contract_wire.hpp"

namespace apollo::net::trace {

namespace {

std::vector<apollo::net::wire::FieldValue> to_wire_fields(const EnvelopeFields& in) {
    using apollo::net::wire::FieldValue;
    std::vector<FieldValue> fields(14);
    fields[0].integer = static_cast<std::int64_t>(in.message_type);
    fields[1].integer = in.flags;
    fields[2].integer = static_cast<std::int64_t>(in.request_id);
    fields[3].text = in.source_app;
    fields[4].text = in.target_app;
    fields[5].integer = static_cast<std::int64_t>(in.entity_id);
    fields[6].text = in.method_alias;
    fields[7].integer = static_cast<std::int64_t>(in.route_version);
    fields[8].integer = static_cast<std::int64_t>(in.authority_epoch);
    // trace 长度校验在 pack/unpack 主体；此处按 has_trace 落空/全载体
    if (in.trace.has_trace()) {
        fields[9].bytes.assign(in.trace.trace_id.begin(), in.trace.trace_id.end());
        fields[10].bytes.assign(in.trace.parent_span_id.begin(), in.trace.parent_span_id.end());
    }
    fields[11].bytes = in.payload;
    // 路由提示（多 cell 批；0 = 未带）——尾随两字段，既有 12 字段序不动
    fields[12].integer = static_cast<std::int64_t>(in.cell_id);
    fields[13].integer = static_cast<std::int64_t>(in.space_id);
    return fields;
}

}  // namespace

bool pack_envelope(const EnvelopeFields& in, std::vector<std::uint8_t>& out) {
    using apollo::net::wire::encode_payload;

    out.clear();

    EnvelopeFields sane = in;
    sane.flags = (sane.trace.has_trace() && sane.trace.sampled) ? kFlagSampled : 0;

    // 空 trace 不带 bytes（契约口径：空 = 未带）；非空恒 16/8B 全载体
    auto fields = to_wire_fields(sane);
    if (!encode_payload(
            static_cast<std::uint16_t>(apollo::contract::gen::MsgId::internal_envelope), fields,
            out)) {
        out.clear();
        return false;
    }
    return true;
}

bool unpack_envelope(const std::uint8_t* data, std::size_t size, EnvelopeFields& out) {
    using apollo::net::wire::decode_payload_fields;

    out = EnvelopeFields{};
    std::vector<apollo::net::wire::FieldValue> fields;
    // 多 cell 批前旧端信封（12 字段）与 13/14 字段新端混编滚动升级（旧
    // gateway → 新 World）：字段平铺无 tag、帧不自描述字段数——按 14→13→12
    // 顺序探测，恰好消费全部字节者命中（新端 14 字段帧不会误判为 13/12；
    // 真坏帧在各级均因余字节/字段级非法拒收）。缺尾随路由提示字段缺省 0
    // （未带提示，World 按首就绪回退）。
    constexpr std::uint16_t kEnvelopeId =
        static_cast<std::uint16_t>(apollo::contract::gen::MsgId::internal_envelope);
    if (!decode_payload_fields(kEnvelopeId, data, size, fields, 14)
        && !decode_payload_fields(kEnvelopeId, data, size, fields, 13)
        && !decode_payload_fields(kEnvelopeId, data, size, fields, 12)) {
        return false;
    }
    const std::size_t field_count = fields.size();

    out.message_type = static_cast<EnvelopeKind>(fields[0].integer);
    out.flags = static_cast<std::int32_t>(fields[1].integer);
    out.request_id = static_cast<std::uint64_t>(fields[2].integer);
    out.source_app = fields[3].text;
    out.target_app = fields[4].text;
    out.entity_id = static_cast<std::uint64_t>(fields[5].integer);
    out.method_alias = fields[6].text;
    out.route_version = static_cast<std::uint64_t>(fields[7].integer);
    out.authority_epoch = static_cast<std::uint64_t>(fields[8].integer);

    const auto& trace_bytes = fields[9].bytes;
    const auto& parent_bytes = fields[10].bytes;
    if (trace_bytes.empty()) {
        // 空载体 = 未带 trace；parent 必须同样为空（半带 = 形非法）
        if (!parent_bytes.empty()) return false;
    } else {
        if (trace_bytes.size() != kTraceIdSize) return false;
        if (parent_bytes.size() != kSpanIdSize) return false;
        std::copy(trace_bytes.begin(), trace_bytes.end(), out.trace.trace_id.begin());
        std::copy(parent_bytes.begin(), parent_bytes.end(), out.trace.parent_span_id.begin());
        out.trace.sampled = (out.flags & kFlagSampled) != 0;
    }

    out.payload = fields[11].bytes;
    if (!out.payload.empty() && out.payload.size() < 2) return false;  // u16 id 不全
    // 路由提示（多 cell 批；0 = 未带）——尾随两字段，缺省 0（legacy 12 字段）
    if (field_count >= 13) {
        out.cell_id = static_cast<std::uint64_t>(fields[12].integer);
    }
    if (field_count >= 14) {
        out.space_id = static_cast<std::uint64_t>(fields[13].integer);
    }
    return true;
}

bool split_payload(const std::vector<std::uint8_t>& payload, std::uint16_t& inner_id,
                   const std::uint8_t*& body, std::size_t& body_size) {
    if (payload.size() < 2) return false;
    inner_id = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(payload[0]) |
        (static_cast<std::uint16_t>(payload[1]) << 8));
    body = payload.data() + 2;
    body_size = payload.size() - 2;
    return true;
}

}  // namespace apollo::net::trace
