#pragma once
//
// trace 子集上下文原语（P3-3 批 · logging.md §5.1：借 W3C 模型不引 SDK——
// trace_id/span_id + traceparent 语义（含采样旗标））。
//
// 语义（logging.md §5.1 定案）：
//   - 生成在进程入口（客户端会话首触点 / 跨进程消息首生成处）；
//   - span 界定 = 消息边界；头部采样极低率（默认 1/10⁴），裁决在根进程
//     （gateway）做出，旗标随信封传播，下游不再重采；
//   - 采样命中的 span 以结构化行进日志流（span_emitter，不引 OTLP）。
//
// TraceContext 是「发给下一跳的父链信息」：trace_id + 发端 span_id
// （对端以此为 parent）+ 采样旗标。本进程自己的 span 记录由 span_emitter
// 另带 span_id。纯值面，零平台分支（POSIX/Windows 同形）。

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace apollo::net::trace {

inline constexpr std::size_t kTraceIdSize = 16;  // W3C trace-id
inline constexpr std::size_t kSpanIdSize = 8;    // W3C span-id

using TraceId = std::array<std::uint8_t, kTraceIdSize>;
using SpanId = std::array<std::uint8_t, kSpanIdSize>;

struct TraceContext {
    TraceId trace_id{};                    // 全零 = 未带 trace
    SpanId parent_span_id{};               // 发端 span（对端的 parent）；全零 = 根
    bool sampled = false;                  // W3C flags bit0（头采样裁决随链传播）

    bool has_trace() const;                // trace_id 非全零
};

// hex 往返（小写；traceparent 与 span 行共用一口）。from_* 严格：长度/
// 字符任一不符即 false（out 内容不承诺）。
std::string to_hex(const std::uint8_t* data, std::size_t size);
bool from_hex(std::string_view hex, std::uint8_t* out, std::size_t size);

// W3C traceparent version-00："00-<32hex trace-id>-<16hex parent-id>-<2hex flags>"
// （flags 只取 bit0 = sampled）。空 parent（根）以 16 个 '0' 序列化——
// 与信封 bytes 空载体对偶由信封编解码层负责，本串面恒全 id。
std::string to_traceparent(const TraceContext& ctx);

// false = 形不合法（长度/字符/版本非 00/零 trace-id/零 parent-id）。
bool from_traceparent(std::string_view tp, TraceContext& out);

}  // namespace apollo::net::trace
