#include "apollo/net/linktrace/trace_context.hpp"

namespace apollo::net::trace {

namespace {

constexpr char kHexDigits[] = "0123456789abcdef";

bool is_lower_hex(std::string_view s, std::size_t expect) {
    if (s.size() != expect) return false;
    for (char c : s) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

bool all_zero(const std::uint8_t* data, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        if (data[i] != 0) return false;
    }
    return true;
}

}  // namespace

bool TraceContext::has_trace() const {
    return !all_zero(trace_id.data(), trace_id.size());
}

std::string to_hex(const std::uint8_t* data, std::size_t size) {
    std::string out;
    out.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(kHexDigits[data[i] >> 4]);
        out.push_back(kHexDigits[data[i] & 0x0F]);
    }
    return out;
}

bool from_hex(std::string_view hex, std::uint8_t* out, std::size_t size) {
    if (hex.size() != size * 2) return false;
    for (std::size_t i = 0; i < size; ++i) {
        auto nib = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        int hi = nib(hex[i * 2]);
        int lo = nib(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

std::string to_traceparent(const TraceContext& ctx) {
    // 空 trace 序列化为零 trace-id（W3C 禁零——消费面先查 has_trace；
    // 本函数只对有效上下文有意义，此处不构造拒绝路径）
    std::string trace_hex = to_hex(ctx.trace_id.data(), ctx.trace_id.size());
    std::string span_hex = to_hex(ctx.parent_span_id.data(), ctx.parent_span_id.size());
    std::string out;
    out.reserve(3 + 1 + 32 + 1 + 16 + 1 + 2);
    out += "00-";
    out += trace_hex;
    out += '-';
    out += span_hex;
    out += '-';
    out += ctx.sampled ? "01" : "00";
    return out;
}

bool from_traceparent(std::string_view tp, TraceContext& out) {
    // version-00 严格面：非 00 版本（含未来扩展尾段）即拒——本仓信封
    // 传播不走 traceparent 串，串面只服务测试/日志互操作
    if (tp.size() != 55) return false;  // 3+1+32+1+16+1+2
    if (tp[0] != '0' || tp[1] != '0' || tp[2] != '-' || tp[35] != '-' || tp[52] != '-') {
        return false;
    }
    if (!is_lower_hex(tp.substr(3, 32), 32)) return false;
    if (!is_lower_hex(tp.substr(36, 16), 16)) return false;
    if (!is_lower_hex(tp.substr(53, 2), 2)) return false;

    TraceId trace{};
    SpanId span{};
    if (!from_hex(tp.substr(3, 32), trace.data(), trace.size())) return false;
    if (!from_hex(tp.substr(36, 16), span.data(), span.size())) return false;
    if (all_zero(trace.data(), trace.size())) return false;   // W3C 禁零 trace-id
    if (all_zero(span.data(), span.size())) return false;     // W3C 禁零 parent-id

    int flag_hi = tp[53] - '0';
    int flag_lo = tp[54] - '0';
    int flags = flag_hi * 16 + flag_lo;

    out.trace_id = trace;
    out.parent_span_id = span;
    out.sampled = (flags & 0x01) != 0;
    return true;
}

}  // namespace apollo::net::trace
