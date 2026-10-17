#pragma once
//
// span 发射缝（logging.md §5.1：采样命中的 span 以结构化行进日志流——
// 默认出口 = cat=trace 日志器的结构化行（六键 + span kv 段），与既有
// push 管线自然汇流；不引 OTLP exporter）。
//
// 发射点 = 消息处理跨度终点的一次性整段记录（start/end 毫秒都带，dur_ms
// 由行面现算）。未采样上下文零开销直返（emit 前不取锁不构串）。
//
// 测试缝：set_span_sink 注入收集器（进程级）；注入 nullptr = 恢复默认
// 行出口。sink 抛异常吞掉——观测面不构成运行依赖（logging.md §5 三禁
// 同源）。

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

#include "apollo/net/linktrace/trace_context.hpp"

namespace apollo::net::trace {

// span id 生成（进程内唯一即可——链关联键是 trace_id；进程种子 + 原子
// 计数经 splitmix64 混淆，线程安全）。
SpanId generate_span_id();

// trace id 生成（根进程入口逐消息新链——span 界定 = 消息边界，M1 前递面
// 每意图独立链；与 span id 同混淆族，线程安全）。
TraceId generate_trace_id();

// 稳态毫秒（跨度计时的统一时钟口径——steady_clock since epoch，
// 与 logpush/metricspush 同式）。
std::uint64_t now_ms();

struct SpanRecord {
    std::string name;       // 跨度名（gateway_intent_forward / world_intent_execute …）
    TraceContext ctx;       // trace_id + 发端 parent + sampled（恒 true——未采样不发射）
    SpanId span_id{};       // 本 span（对端下一跳的 parent_span_id 来源）
    std::uint64_t start_ms = 0;
    std::uint64_t end_ms = 0;
};

using SpanSink = std::function<void(const SpanRecord&)>;

// 进程级注入缝（测试收集器/特化出口；线程安全——mutex 护换入，读取经
// 拷贝快照）。
void set_span_sink(SpanSink sink);

// 默认行出口的 kv 段格式（msg=span 后接；公开供格式断言）：
//   span name=<name> trace_id=<32hex> span_id=<16hex>
//        parent_span_id=<16hex|-> start_ms=<n> end_ms=<n> dur_ms=<n>
std::string span_log_line(const SpanRecord& record);

// 发射（仅 sampled 且 trace 非零；否则零开销直返）。
void emit_span(const SpanRecord& record);

}  // namespace apollo::net::trace
