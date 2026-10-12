#include "apollo/net/linktrace/span_emitter.hpp"

#include <chrono>
#include <mutex>
#include <random>
#include <utility>
#include <vector>

#include "apollo/core/log/logger.h"
#include "apollo/core/log/log_manager.h"

namespace apollo::net::trace {

namespace {

std::uint64_t steady_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

// span id 种子（进程生命期一次）+ 原子计数经 splitmix64 混淆——进程内
// 唯一性由计数单调保证，混淆位防跨进程同刻碰撞成习惯性同值。
std::uint64_t span_seed() {
    static const std::uint64_t seed = [] {
        std::random_device rd;
        return (static_cast<std::uint64_t>(rd()) << 32) | rd();
    }();
    return seed;
}

std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

std::mutex& sink_mutex() {
    static std::mutex m;
    return m;
}

SpanSink& sink_slot() {
    static SpanSink sink;  // 空 = 默认行出口
    return sink;
}

apollo::core::log::Logger& trace_log() {
    return *apollo::core::log::global_log_manager().createLogger("trace");
}

}  // namespace

SpanId generate_span_id() {
    static std::atomic<std::uint64_t> counter{0};
    const std::uint64_t mixed = splitmix64(span_seed() ^ splitmix64(
        counter.fetch_add(1, std::memory_order_relaxed)));
    SpanId id{};
    for (std::size_t i = 0; i < id.size(); ++i) {
        id[i] = static_cast<std::uint8_t>((mixed >> (i * 8)) & 0xFF);
    }
    return id;
}

std::uint64_t now_ms() {
    return steady_ms();
}

void set_span_sink(SpanSink sink) {
    std::lock_guard<std::mutex> guard(sink_mutex());
    sink_slot() = std::move(sink);
}

std::string span_log_line(const SpanRecord& record) {
    // parent 全零 = 根，行面记 '-'（与信封 bytes 空载体同义）
    bool has_parent = false;
    for (std::uint8_t b : record.ctx.parent_span_id) {
        if (b != 0) {
            has_parent = true;
            break;
        }
    }
    std::string parent_field = has_parent
        ? to_hex(record.ctx.parent_span_id.data(), record.ctx.parent_span_id.size())
        : std::string("-");
    const std::uint64_t dur = record.end_ms >= record.start_ms
        ? record.end_ms - record.start_ms : 0;
    std::string out;
    out.reserve(96 + record.name.size());
    out += "span name=" + record.name;
    out += " trace_id=" + to_hex(record.ctx.trace_id.data(), record.ctx.trace_id.size());
    out += " span_id=" + to_hex(record.span_id.data(), record.span_id.size());
    out += " parent_span_id=" + parent_field;
    out += " start_ms=" + std::to_string(record.start_ms);
    out += " end_ms=" + std::to_string(record.end_ms);
    out += " dur_ms=" + std::to_string(dur);
    return out;
}

void emit_span(const SpanRecord& record) {
    // 观测面三禁同源：未采样零开销直返；默认出口抛异常吞掉不外溢
    if (!record.ctx.sampled || !record.ctx.has_trace()) return;

    SpanSink snapshot;
    {
        std::lock_guard<std::mutex> guard(sink_mutex());
        snapshot = sink_slot();
    }
    if (snapshot) {
        try {
            snapshot(record);
        } catch (...) {
        }
        return;
    }
    try {
        trace_log().info(span_log_line(record));
    } catch (...) {
    }
}

}  // namespace apollo::net::trace
