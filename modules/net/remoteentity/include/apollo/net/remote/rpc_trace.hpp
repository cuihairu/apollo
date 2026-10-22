#pragma once
//
// RpcTrace — RPC 链 trace 集成（remote-entity-call 四件套之「RPC 链 trace」）。
//
// 为 RPC 调用创建/传播 trace context 并发射 span。复用 linktrace 既有
// 原语（TraceContext/SpanRecord/emit_span），不引 OTLP exporter。
// 设计见 docs/zh/architecture/remote-entity-call-design.md §7 + logging.md §5.1。
//

#include <cstdint>
#include <string>

#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/linktrace/trace_context.hpp"
#include "apollo/net/remote/entity_ref.hpp"

namespace apollo::net::remote {

class RpcTrace {
public:
    // 为新的 RPC 调用创建 trace context（生成新 trace_id，root span）
    static apollo::net::trace::TraceContext create_context();

    // 为 RPC 调用发射 span（仅 sampled 且 trace 非零时实际发射）
    static void emit_rpc_span(const std::string& method_alias,
                              const RemoteEntityRef& ref,
                              const apollo::net::trace::TraceContext& ctx,
                              std::uint64_t start_ms,
                              std::uint64_t end_ms);

    // 从 envelope trace context 提取/延续（对端以此为 parent）
    static apollo::net::trace::TraceContext extract_context(
        const apollo::net::trace::TraceContext& envelope_ctx);
};

}  // namespace apollo::net::remote
