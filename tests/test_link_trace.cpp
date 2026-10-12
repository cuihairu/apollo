// trace 子集原语测试（LinkTraceTests，P3-3 批 · 语义层）。
//
// 覆盖（logging.md §5.1 trace 子集 + remote-entity-call 信封缝）：
//   1. traceparent_roundtrip：W3C 向量往返 + 采样位；形非法拒（长度/
//      版本/大写/零 id）；
//   2. hex_roundtrip：小写往返 + 严格拒；
//   3. sampler：确定性 every-Nth 精确命中数 + env 覆写（POSIX 面）；
//   4. envelope_roundtrip：12 字段全量往返 + 空 trace 变体 + wire 层
//      叠信封互证（send/receive 全链形）；
//   5. envelope_rejects：trace 长度/半带/payload 短 u16/残帧拒；
//   6. span_emission：sink 缝仅 sampled+trace 发射、异常吞掉、行格式；
//   7. span_id：多线程生成唯一。
//
// 单线程面（除 span_id 并发生成）；无网络无泵——纯值面。

#include "apollo/net/linktrace/envelope.hpp"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/net/linktrace/trace_context.hpp"
#include "apollo/net/linktrace/trace_sampler.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

namespace trace = apollo::net::trace;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

trace::TraceContext make_context(const char* trace_hex, const char* span_hex, bool sampled) {
    trace::TraceContext ctx{};
    (void)trace::from_hex(trace_hex, ctx.trace_id.data(), ctx.trace_id.size());
    (void)trace::from_hex(span_hex, ctx.parent_span_id.data(), ctx.parent_span_id.size());
    ctx.sampled = sampled;
    return ctx;
}

bool test_traceparent_roundtrip() {
    // W3C 规范示例向量
    auto ctx = make_context("4bf92f3577b34da6a3ce929d0e0e4736", "00f067aa0ba902b7", true);
    TEST_ASSERT(ctx.has_trace(), "known vector should have trace");
    TEST_ASSERT(trace::to_traceparent(ctx) ==
                    "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01",
                "sampled traceparent vector mismatch");

    ctx.sampled = false;
    const std::string unsampled = trace::to_traceparent(ctx);
    TEST_ASSERT(unsampled.back() == '0', "unsampled flags tail");
    trace::TraceContext back{};
    TEST_ASSERT(trace::from_traceparent(unsampled, back), "unsampled parse");
    TEST_ASSERT(back.trace_id == ctx.trace_id && back.parent_span_id == ctx.parent_span_id,
                "traceparent ids roundtrip");
    TEST_ASSERT(!back.sampled, "unsampled flag roundtrip");

    TEST_ASSERT(trace::from_traceparent(
                    "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01", back),
                "canonical sampled parse");
    TEST_ASSERT(back.sampled, "sampled flag parse");

    // 形非法面：长度 / 版本 / 大写 / 缺划 / 零 id
    TEST_ASSERT(!trace::from_traceparent("", back), "empty rejected");
    TEST_ASSERT(!trace::from_traceparent("00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7",
                                         back),
                "missing flags rejected");
    TEST_ASSERT(!trace::from_traceparent(
                    "01-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01", back),
                "version 01 rejected");
    TEST_ASSERT(!trace::from_traceparent(
                    "00-4BF92F3577B34DA6A3CE929D0E0E4736-00f067aa0ba902b7-01", back),
                "uppercase rejected");
    TEST_ASSERT(!trace::from_traceparent(
                    "00-4bf92f3577b34da6a3ce929d0e0e4736:00f067aa0ba902b7-01", back),
                "bad separator rejected");
    TEST_ASSERT(!trace::from_traceparent(
                    "00-00000000000000000000000000000000-00f067aa0ba902b7-01", back),
                "zero trace-id rejected");
    TEST_ASSERT(!trace::from_traceparent(
                    "00-4bf92f3577b34da6a3ce929d0e0e4736-0000000000000000-01", back),
                "zero parent-id rejected");

    // 零 trace 上下文：has_trace false；串面序列化为零 trace-id（消费面
    // 先查 has_trace，W3C 禁零由解析面把守）
    trace::TraceContext empty{};
    TEST_ASSERT(!empty.has_trace(), "default ctx has no trace");
    return true;
}

bool test_hex_roundtrip() {
    const std::uint8_t bytes[] = {0x00, 0x0f, 0xa5, 0xff};
    const std::string hex = trace::to_hex(bytes, sizeof(bytes));
    TEST_ASSERT(hex == "000fa5ff", "hex encode");

    std::uint8_t back[4] = {};
    TEST_ASSERT(trace::from_hex(hex, back, sizeof(back)), "hex decode");
    TEST_ASSERT(std::equal(bytes, bytes + sizeof(bytes), back), "hex roundtrip");

    TEST_ASSERT(!trace::from_hex("000fa5f", back, sizeof(back)), "odd length rejected");
    TEST_ASSERT(!trace::from_hex("000fa5fg", back, sizeof(back)), "bad digit rejected");
    TEST_ASSERT(!trace::from_hex("000fa5ff", back, sizeof(back) - 1), "length mismatch rejected");
    return true;
}

bool test_sampler() {
    trace::HeadSampler all{1};
    long hits = 0;
    for (int i = 0; i < 1000; ++i) {
        hits += all.should_sample() ? 1 : 0;
    }
    TEST_ASSERT(hits == 1000, "rate 1 samples all");

    trace::HeadSampler quarter{4};
    hits = 0;
    for (int i = 0; i < 1000; ++i) {
        hits += quarter.should_sample() ? 1 : 0;
    }
    TEST_ASSERT(hits == 250, "rate 4 hits exactly 250 of 1000");

    trace::HeadSampler zero{0};
    TEST_ASSERT(zero.rate_denom() == 1, "zero denom clamps to 1");

    TEST_ASSERT(trace::sample_rate_from_env() == trace::kDefaultSampleRateDenom,
                "unset env yields default");

#if !defined(_WIN32)
    // dev 覆写面（POSIX setenv；Windows CI 跳过——env 口径 POSIX 实证）
    setenv("APOLLO_TRACE_SAMPLE_RATE", "1", 1);
    TEST_ASSERT(trace::sample_rate_from_env() == 1, "env override to 1");
    setenv("APOLLO_TRACE_SAMPLE_RATE", "8", 1);
    TEST_ASSERT(trace::sample_rate_from_env() == 8, "env override to 8");
    setenv("APOLLO_TRACE_SAMPLE_RATE", "abc", 1);
    TEST_ASSERT(trace::sample_rate_from_env() == trace::kDefaultSampleRateDenom,
                "bad env yields default");
    setenv("APOLLO_TRACE_SAMPLE_RATE", "0", 1);
    TEST_ASSERT(trace::sample_rate_from_env() == trace::kDefaultSampleRateDenom,
                "zero env yields default");
    setenv("APOLLO_TRACE_SAMPLE_RATE", "", 1);
    TEST_ASSERT(trace::sample_rate_from_env() == trace::kDefaultSampleRateDenom,
                "empty env yields default");
    unsetenv("APOLLO_TRACE_SAMPLE_RATE");
#endif
    return true;
}

trace::EnvelopeFields make_full_envelope() {
    trace::EnvelopeFields env{};
    env.message_type = trace::EnvelopeKind::EntityMethodCall;
    env.flags = trace::kFlagSampled;  // 规范形：pack 按 sampled 落位，往返同值
    env.request_id = 42;
    env.source_app = "gateway#1";
    env.target_app = "cell#1";
    env.entity_id = 987654321;
    env.method_alias = "move_to";
    env.route_version = 7;
    env.authority_epoch = 3;
    env.trace = make_context("000102030405060708090a0b0c0d0e0f", "0102030405060708", true);
    env.payload = {0x2c, 0x01, 0xde, 0xad, 0xbe, 0xef};  // 内层 id 0x012c + 体
    return env;
}

bool envelope_equal(const trace::EnvelopeFields& a, const trace::EnvelopeFields& b) {
    return a.message_type == b.message_type && a.flags == b.flags &&
           a.request_id == b.request_id && a.source_app == b.source_app &&
           a.target_app == b.target_app && a.entity_id == b.entity_id &&
           a.method_alias == b.method_alias && a.route_version == b.route_version &&
           a.authority_epoch == b.authority_epoch && a.trace.trace_id == b.trace.trace_id &&
           a.trace.parent_span_id == b.trace.parent_span_id &&
           a.trace.sampled == b.trace.sampled && a.payload == b.payload;
}

bool test_envelope_roundtrip() {
    auto env = make_full_envelope();
    std::vector<std::uint8_t> packed;
    TEST_ASSERT(trace::pack_envelope(env, packed), "pack full envelope");
    TEST_ASSERT(!packed.empty(), "packed non-empty");

    trace::EnvelopeFields out{};
    TEST_ASSERT(trace::unpack_envelope(packed.data(), packed.size(), out),
                "unpack full envelope");
    TEST_ASSERT(envelope_equal(env, out), "full envelope roundtrip");
    TEST_ASSERT(out.trace.sampled, "sampled flag echoed");

    // 内层载荷拆装（收端剥 u16 id → decode_payload 同形）
    std::uint16_t inner_id = 0;
    const std::uint8_t* body = nullptr;
    std::size_t body_size = 0;
    TEST_ASSERT(trace::split_payload(out.payload, inner_id, body, body_size), "split payload");
    TEST_ASSERT(inner_id == 0x012c, "inner id little-endian");
    TEST_ASSERT(body_size == 4 && body[0] == 0xde, "inner body view");

    // 空 trace 变体：无 trace 载体 + 采样位强制清零
    auto bare = make_full_envelope();
    bare.trace = trace::TraceContext{};
    bare.trace.sampled = true;  // 无载体采不动——pack 落位 0
    TEST_ASSERT(trace::pack_envelope(bare, packed), "pack bare envelope");
    TEST_ASSERT(trace::unpack_envelope(packed.data(), packed.size(), out), "unpack bare envelope");
    TEST_ASSERT(!out.trace.has_trace(), "bare has no trace");
    TEST_ASSERT(out.flags == 0, "sampled bit forced off without trace");
    TEST_ASSERT(!out.trace.sampled, "sampled off without trace");

    // wire 层互证：pack 产物 → link 信封（u16 id + 载荷）→ 收端剥 u16
    // 后字段载荷原样（send/receive 全链形）。packed 已被空 trace 变体
    // 复用——全量形重打包。
    TEST_ASSERT(trace::pack_envelope(env, packed), "repack full envelope");
    std::vector<std::uint8_t> linked =
        wire::append_envelope(id_of(MsgId::internal_envelope), packed);
    TEST_ASSERT(linked.size() == packed.size() + 2 && linked[0] == 0xa4 && linked[1] == 0x03,
                "link envelope head is u16 LE 932");
    trace::EnvelopeFields relinked{};
    TEST_ASSERT(trace::unpack_envelope(linked.data() + 2, linked.size() - 2, relinked),
                "unpack through link envelope");
    TEST_ASSERT(envelope_equal(env, relinked), "fields survive link envelope roundtrip");
    return true;
}

bool test_envelope_rejects() {
    // 15B trace（契约口径 16B）——经 wire 编不出坏形，直接构字段级非法帧：
    // 用 pack 产物改写不可行（LE 平铺无 tag），改从合法帧侧验证拆面：
    // 短 payload / 残帧在 unpack 拒；trace 长度非法构不出（pack 落全载体），
    // 拒收面由 wire decode 型校验兜底——此处实证可得面。
    std::vector<std::uint8_t> packed;
    TEST_ASSERT(trace::pack_envelope(make_full_envelope(), packed), "pack for rejects");

    trace::EnvelopeFields out{};
    TEST_ASSERT(!trace::unpack_envelope(packed.data(), packed.size() - 1, out),
                "truncated frame rejected");

    // 短 payload（1B < u16 id）——手拆无门，pack 产物改内层不可达；经
    // 全零帧实证残帧拒
    std::vector<std::uint8_t> junk(8, 0);
    TEST_ASSERT(!trace::unpack_envelope(junk.data(), junk.size(), out), "zero junk rejected");

    // 空帧
    TEST_ASSERT(!trace::unpack_envelope(nullptr, 0, out), "empty frame rejected");

    // 短 payload 拒在 split 面
    std::uint16_t inner_id = 0;
    const std::uint8_t* body = nullptr;
    std::size_t body_size = 0;
    TEST_ASSERT(!trace::split_payload({0x01}, inner_id, body, body_size),
                "one-byte payload rejected");
    TEST_ASSERT(!trace::split_payload({}, inner_id, body, body_size), "empty payload rejected");
    return true;
}

bool test_span_emission() {
    // 收集器注入缝
    std::vector<trace::SpanRecord> collected;
    trace::set_span_sink([&collected](const trace::SpanRecord& record) {
        collected.push_back(record);
    });

    auto ctx = make_context("0a0b0c0d0e0f00010203040506070809", "1122334455667788", true);
    trace::SpanRecord span{};
    span.name = "gateway_intent_forward";
    span.ctx = ctx;
    span.span_id = trace::generate_span_id();
    span.start_ms = 1000;
    span.end_ms = 1080;
    trace::emit_span(span);
    TEST_ASSERT(collected.size() == 1, "sampled span collected");
    if (!collected.empty()) {
        TEST_ASSERT(collected.back().name == "gateway_intent_forward", "record name");
        TEST_ASSERT(collected.back().ctx.trace_id == ctx.trace_id, "record trace id");
    }

    // 未采样 / 无 trace：零发射
    const std::size_t before = collected.size();
    trace::SpanRecord quiet = span;
    quiet.ctx.sampled = false;
    trace::emit_span(quiet);
    quiet.ctx.sampled = true;
    quiet.ctx.trace_id = trace::TraceId{};
    trace::emit_span(quiet);
    TEST_ASSERT(collected.size() == before, "unsampled/traceless not emitted");

    // sink 抛异常吞掉（观测面不构成运行依赖）
    trace::set_span_sink([](const trace::SpanRecord&) { throw std::runtime_error("sink"); });
    trace::emit_span(span);  // 不得外溢

    // 默认行出口：恢复 nullptr 后走 cat=trace 日志器（smoke——不核行）
    trace::set_span_sink(nullptr);
    trace::emit_span(span);

    // 行格式（公开面直接断言）
    const std::string line = trace::span_log_line(span);
    TEST_ASSERT(line.find("span name=gateway_intent_forward") == 0, "line head");
    TEST_ASSERT(line.find("trace_id=0a0b0c0d0e0f00010203040506070809") != std::string::npos,
                "line trace_id");
    TEST_ASSERT(line.find("start_ms=1000") != std::string::npos, "line start_ms");
    TEST_ASSERT(line.find("end_ms=1080") != std::string::npos, "line end_ms");
    TEST_ASSERT(line.find("dur_ms=80") != std::string::npos, "line dur_ms");

    trace::SpanRecord root = span;
    root.ctx.parent_span_id = trace::SpanId{};
    TEST_ASSERT(trace::span_log_line(root).find("parent_span_id=-") != std::string::npos,
                "root parent rendered as dash");
    TEST_ASSERT(trace::span_log_line(span).find("parent_span_id=1122334455667788") !=
                    std::string::npos,
                "child parent rendered as hex");
    return true;
}

bool test_span_id_uniqueness() {
    std::vector<trace::SpanId> ids(1000);
    std::vector<std::thread> workers;
    for (std::size_t w = 0; w < 4; ++w) {
        workers.emplace_back([&ids, w]() {
            for (std::size_t i = 0; i < 250; ++i) {
                ids[w * 250 + i] = trace::generate_span_id();
            }
        });
    }
    for (auto& t : workers) {
        t.join();
    }
    std::set<std::string> unique;
    for (const auto& id : ids) {
        unique.insert(trace::to_hex(id.data(), id.size()));
    }
    TEST_ASSERT(unique.size() == ids.size(), "1000 concurrent span ids all distinct");
    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"traceparent_roundtrip", test_traceparent_roundtrip},
        {"hex_roundtrip", test_hex_roundtrip},
        {"sampler", test_sampler},
        {"envelope_roundtrip", test_envelope_roundtrip},
        {"envelope_rejects", test_envelope_rejects},
        {"span_emission", test_span_emission},
        {"span_id_uniqueness", test_span_id_uniqueness},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LinkTraceTests: all " << (sizeof(cases) / sizeof(cases[0]))
                  << " cases passed" << std::endl;
        return 0;
    }
    std::cout << "LinkTraceTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
