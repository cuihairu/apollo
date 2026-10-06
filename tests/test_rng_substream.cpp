/**
 * @file test_rng_substream.cpp
 * @brief RNG 子流派生测试（battle-determinism §4 约束④：随机数）
 *
 * 断言口径（docs/design/battle-determinism.md §4）：
 *  - 同 (world_seed, tick, stream_id) 三元组派生同一序列——任意机器、
 *    任意调用次序下可重放（无全局状态、无续流游标）；
 *  - stream_id 分域隔离——一个系统抽几次不影响别系统序列；
 *  - 任意 tick 独立可重放；
 *  - next_double 为 [0,1) 上 24 位精度均匀分布（跨平台位精确）。
 */

#include "apollo/base/rng.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

using apollo::base::Pcg32;
using apollo::base::StreamId;
using apollo::base::derive_substream;
using apollo::base::splitmix64;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// 抽 K 个 [0,1) 值
static std::vector<double> draw_doubles(std::uint64_t seed, std::uint64_t tick,
                                        StreamId stream, int k) {
    Pcg32 rng = derive_substream(seed, tick, stream);
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i) out.push_back(rng.next_double());
    return out;
}

/**
 * @brief 约束④-1：同三元组派生同一序列（重放可重派生）
 */
bool test_same_tuple_same_sequence() {
    std::cout << "Running: test_same_tuple_same_sequence..." << std::endl;

    const auto a = draw_doubles(0xC0FFEE, 42, StreamId::CombatRoll, 16);
    const auto b = draw_doubles(0xC0FFEE, 42, StreamId::CombatRoll, 16);
    TEST_ASSERT(a == b, "same (seed,tick,stream) must yield identical sequence");

    // 逐值相等（operator== 覆盖不到的空序列边界另测）
    TEST_ASSERT(!a.empty(), "sequence not empty");
    bool all_equal = true;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) all_equal = false;
    }
    TEST_ASSERT(all_equal, "element-wise equality");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 约束④-2：stream_id 分域隔离（combat/drop/ai/proc 互不影响）
 */
bool test_stream_domain_isolation() {
    std::cout << "Running: test_stream_domain_isolation..." << std::endl;

    const auto combat = draw_doubles(7, 1, StreamId::CombatRoll, 12);
    const auto drop = draw_doubles(7, 1, StreamId::Drop, 12);
    const auto ai = draw_doubles(7, 1, StreamId::AiDecision, 12);
    const auto proc = draw_doubles(7, 1, StreamId::Proc, 12);

    TEST_ASSERT(combat != drop, "combat_roll stream differs from drop stream");
    TEST_ASSERT(combat != ai, "combat_roll stream differs from ai_decision stream");
    TEST_ASSERT(combat != proc, "combat_roll stream differs from proc stream");
    TEST_ASSERT(drop != ai, "drop stream differs from ai_decision stream");

    // 抽 combat 不改变 drop 序列（分域不耦合）
    (void)draw_doubles(7, 1, StreamId::CombatRoll, 5);
    const auto drop_after = draw_doubles(7, 1, StreamId::Drop, 12);
    TEST_ASSERT(drop == drop_after, "drawing other streams never perturbs a domain");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 约束④-3：无游标——调用次序变化不产生漂移
 *
 * 交错派生 (tick1, tick2, tick1, tick2) 与顺序派生 (tick1×2, tick2×2)
 * 得到同一批序列——派生是纯函数，不按调用次数续流。
 */
bool test_call_order_independence() {
    std::cout << "Running: test_call_order_independence..." << std::endl;

    // 顺序：tick 1 派生两次、tick 2 派生两次
    const auto seq1_first = draw_doubles(99, 1, StreamId::CombatRoll, 8);
    const auto seq1_second = draw_doubles(99, 1, StreamId::CombatRoll, 8);
    (void)draw_doubles(99, 2, StreamId::CombatRoll, 8);
    (void)draw_doubles(99, 2, StreamId::CombatRoll, 8);

    // 交错：tick 2 插入中间再回到 tick 1
    (void)draw_doubles(99, 2, StreamId::CombatRoll, 8);
    const auto seq1_interleaved = draw_doubles(99, 1, StreamId::CombatRoll, 8);
    (void)draw_doubles(99, 2, StreamId::CombatRoll, 8);
    const auto seq1_interleaved2 = draw_doubles(99, 1, StreamId::CombatRoll, 8);

    TEST_ASSERT(seq1_first == seq1_second, "repeated derive at same tick stable");
    TEST_ASSERT(seq1_first == seq1_interleaved, "interleaving other ticks does not drift tick 1");
    TEST_ASSERT(seq1_first == seq1_interleaved2, "interleaving (2) does not drift tick 1");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 约束④-4：任意 tick 独立重放（相邻 tick 序列互不串扰）
 */
bool test_tick_independence() {
    std::cout << "Running: test_tick_independence..." << std::endl;

    const auto t5_before = draw_doubles(1234, 5, StreamId::Drop, 10);
    // 大量使用 tick 6/7 的子流
    for (int i = 0; i < 64; ++i) {
        (void)draw_doubles(1234, 6, StreamId::Drop, 10);
        (void)draw_doubles(1234, 7, StreamId::AiDecision, 10);
    }
    const auto t5_after = draw_doubles(1234, 5, StreamId::Drop, 10);
    TEST_ASSERT(t5_before == t5_after, "tick 5 replay unaffected by later-tick usage");

    // 相邻 tick 序列本身不同（非退化）
    const auto t6 = draw_doubles(1234, 6, StreamId::Drop, 10);
    TEST_ASSERT(t5_before != t6, "adjacent ticks yield distinct sequences");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 分布口径：[0,1) 均匀、24 位精度（跨平台位精确的算术口径）
 */
bool test_distribution_contract() {
    std::cout << "Running: test_distribution_contract..." << std::endl;

    Pcg32 rng = derive_substream(0xABCD, 3, StreamId::Proc);
    bool in_range = true;
    bool quarter_bits = true;
    for (int i = 0; i < 4096; ++i) {
        const double v = rng.next_double();
        if (!(v >= 0.0 && v < 1.0)) in_range = false;
        // 24 位精度：v * 2^24 必为整数（低位 8 位已右移丢弃）
        const double scaled = v * 16777216.0;
        if (std::floor(scaled) != scaled) quarter_bits = false;
    }
    TEST_ASSERT(in_range, "next_double within [0,1)");
    TEST_ASSERT(quarter_bits, "next_double is exactly 24-bit precision");

    // next_uint32 非退化：两不同 initstate 的前 8 值不同
    Pcg32 a, b;
    a.seed(1, 1);
    b.seed(2, 1);
    bool distinct = false;
    for (int i = 0; i < 8; ++i) {
        if (a.next_uint32() != b.next_uint32()) distinct = true;
    }
    TEST_ASSERT(distinct, "distinct initstate yields distinct streams");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief splitmix64 扩散性 smoke：相邻键输出互异且高位翻转
 */
bool test_splitmix64_diffusion() {
    std::cout << "Running: test_splitmix64_diffusion..." << std::endl;

    std::uint64_t prev = splitmix64(0);
    bool all_distinct = true;
    bool high_bits_flip = false;
    for (std::uint64_t i = 1; i < 256; ++i) {
        const std::uint64_t cur = splitmix64(i);
        if (cur == prev) all_distinct = false;
        // 高 32 位应有显著翻转（雪崩 smoke）
        if ((cur >> 32) != (prev >> 32)) high_bits_flip = true;
        prev = cur;
    }
    TEST_ASSERT(all_distinct, "splitmix64 injective on small domain");
    TEST_ASSERT(high_bits_flip, "splitmix64 diffuses into high bits");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// main
//==============================================================================

int main() {
    std::cout << "=== RNG Substream Tests (battle-determinism constraint-4) ===" << std::endl;

    int failed = 0;
    failed += test_same_tuple_same_sequence() ? 0 : 1;
    failed += test_stream_domain_isolation() ? 0 : 1;
    failed += test_call_order_independence() ? 0 : 1;
    failed += test_tick_independence() ? 0 : 1;
    failed += test_distribution_contract() ? 0 : 1;
    failed += test_splitmix64_diffusion() ? 0 : 1;

    std::cout << "=== RNG Substream Tests: "
              << (failed == 0 ? "ALL PASSED" : "FAILURES") << " ===" << std::endl;
    return failed == 0 ? 0 : 1;
}
