#include "apollo/net/remote/rpc_trace.hpp"

namespace apollo::net::remote {

using apollo::net::trace::TraceContext;
using apollo::net::trace::SpanRecord;
using apollo::net::trace::generate_trace_id;
using apollo::net::trace::generate_span_id;
using apollo::net::trace::emit_span;

TraceContext RpcTrace::create_context() {
    TraceContext ctx;
    ctx.trace_id  = generate_trace_id();
    ctx.sampled   = true;
    // parent_span_id stays zero (root span)
    return ctx;
}

void RpcTrace::emit_rpc_span(const std::string& method_alias,
                              const RemoteEntityRef& ref,
                              const TraceContext& ctx,
                              std::uint64_t start_ms,
                              std::uint64_t end_ms) {
    if (!ctx.has_trace() || !ctx.sampled) {
        return;
    }

    SpanRecord span;
    span.name          = method_alias;
    span.ctx           = ctx;
    span.span_id       = generate_span_id();
    span.start_ms      = start_ms;
    span.end_ms        = end_ms;

    emit_span(span);
}

TraceContext RpcTrace::extract_context(const TraceContext& envelope_ctx) {
    return envelope_ctx;
}

}  // namespace apollo::net::remote
