#pragma once
//
// 932 internal_envelope 编解码缝（remote-entity-call 四件套 · 语义层批：
// InternalMessageEnvelope 落契约首件，字段序 = 生成头表序即唯一事实源）。
//
// 本缝收拢 12 字段装配/解包与 trace 长度校验，调用面（gateway 发送面 /
// cell 接收面）不重复字段下标知识。与 wire 层分工：
//   wire::append_envelope/decode_envelope = u16 id + 载荷的线形信封（通用）；
//   trace::pack_envelope/unpack_envelope = 932 字段级编解码（信封内型）。
// 链路发送：pack(932) → link->send(932, …)（InterServerLink::send 自封
// u16 信封，喂 pack 产物即整帧载荷）；接收：wire 解 u16 → unpack。

#include <cstdint>
#include <string>
#include <vector>

#include "apollo/net/linktrace/trace_context.hpp"

namespace apollo::net::trace {

// 信封旗标位（契约 desc 口径：bit0 = 采样命中 trace）
inline constexpr std::int32_t kFlagSampled = 0x1;

// 信封内型四分类（remote-entity-call §六；0 = 未分类）
enum class EnvelopeKind : std::int32_t {
    Unclassified = 0,
    EntityMethodCall = 1,
    RouteControl = 2,
    LifecycleEvent = 3,
    ReplicationCommand = 4,
};

struct EnvelopeFields {
    EnvelopeKind message_type = EnvelopeKind::Unclassified;
    std::int32_t flags = 0;                  // bit0 = sampled（pack/unpack 与 trace 互证）
    std::uint64_t request_id = 0;
    std::string source_app;
    std::string target_app;
    std::uint64_t entity_id = 0;
    std::string method_alias;
    std::uint64_t route_version = 0;
    std::uint64_t authority_epoch = 0;
    TraceContext trace;                      // trace_id 空 = 未带 trace
    std::vector<std::uint8_t> payload;       // u16 LE id + 契约载荷（内层自识）
};

// false = 契约面失配（正常装配不发生；out 清空）。旗标 bit0 以
// has_trace() && sampled 落位——无 trace 载体不携采样位。
bool pack_envelope(const EnvelopeFields& in, std::vector<std::uint8_t>& out);

// false = 字段级非法（trace 长度不符 16/8B、payload 带位却不足 u16、
// 尾部余字节）；out 不承诺内容。
bool unpack_envelope(const std::uint8_t* data, std::size_t size, EnvelopeFields& out);

// 载荷线形拆装（u16 LE id + 载荷体）——与 wire::append_envelope 对偶，
// 收端剥内层 id 后走 decode_payload。
bool split_payload(const std::vector<std::uint8_t>& payload, std::uint16_t& inner_id,
                   const std::uint8_t*& body, std::size_t& body_size);

}  // namespace apollo::net::trace
