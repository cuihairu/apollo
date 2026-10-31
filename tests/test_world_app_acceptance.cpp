// Cell 受理面批测试（P3-2 批 B / ADR-015(a)/016/017；2b-iii 迁 M1 受理缝）：
//   受理队列单元面（FIFO/顶格拒收/两级水位滞回）。内核级受理判定（维护闸
//   /tick 边界 drain/顶格拒收）与畸形意图已由 cell_app_tests 直接测执行内
//   核宿主（切片3 删除内嵌内核后 WorldApp 不再承载模拟域）；wire 回执面
//   （2101/2001/延时 929）归 world_m1_link_tests（真 M1 链路断言）。
// 直编惯例同 scene_tests（无 GTest，plain-assert + main 汇总）；
#include "world_app/acceptance_queue.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_failures = 0;

#define TEST_ASSERT(cond, msg)                                                    \
    do {                                                                          \
        if (!(cond)) {                                                            \
            std::cout << "FAIL: " << (msg) << " (" << #cond << ") at "            \
                      << __LINE__ << std::endl;                                   \
            ++g_failures;                                                         \
            return false;                                                         \
        }                                                                         \
    } while (0)

using world_app::AcceptanceQueue;
using world_app::IntentEnvelope;
namespace protocol = apollo::protocol;

// ---------- 受理队列单元面 ----------

bool test_queue_fifo_order_and_drain() {
    AcceptanceQueue queue;
    for (std::uint64_t id = 1; id <= 3; ++id) {
        IntentEnvelope intent;
        intent.type = protocol::MessageType::CELL_CREATE_ENTITY;
        intent.frame = {static_cast<std::uint8_t>(id)};
        intent.session_id = id;
        TEST_ASSERT(queue.push(std::move(intent)).accepted, "push accepted");
    }
    TEST_ASSERT(queue.size() == 3, "size after three pushes");
    for (std::uint64_t id = 1; id <= 3; ++id) {
        auto intent = queue.try_pop();
        TEST_ASSERT(intent.has_value(), "pop has value");
        TEST_ASSERT(intent->session_id == id, "fifo order");
    }
    TEST_ASSERT(!queue.try_pop().has_value(), "empty after drain");
    return true;
}

bool test_queue_cap_rejects_new_keeps_old() {
    // ADR-016：顶格拒收不丢旧——先入条目保留，新到者被拒并计数
    AcceptanceQueue queue(/*cap=*/4, /*high_watermark=*/4);
    for (int i = 0; i < 6; ++i) {
        IntentEnvelope intent;
        intent.session_id = static_cast<std::uint64_t>(i);
        TEST_ASSERT(queue.push(std::move(intent)).accepted == (i < 4), "cap boundary");
    }
    TEST_ASSERT(queue.size() == 4, "size capped");
    TEST_ASSERT(queue.rejected_count() == 2, "two rejections counted");
    auto oldest = queue.try_pop();
    TEST_ASSERT(oldest.has_value() && oldest->session_id == 0, "oldest kept");
    return true;
}

bool test_queue_high_watermark_hysteresis() {
    // ADR-016 两级水位：越线只报一次，回落后重新武装
    AcceptanceQueue queue(/*cap=*/8, /*high_watermark=*/5);
    for (int i = 0; i < 4; ++i) {
        IntentEnvelope intent;
        TEST_ASSERT(queue.push(std::move(intent)).accepted, "below watermark accepted");
    }
    {
        IntentEnvelope intent;
        auto result = queue.push(std::move(intent));
        TEST_ASSERT(result.accepted && result.high_watermark, "crossing push reports watermark");
    }
    for (int i = 0; i < 3; ++i) {
        IntentEnvelope intent;
        auto result = queue.push(std::move(intent));
        TEST_ASSERT(result.accepted && !result.high_watermark, "no repeat report while high");
    }
    // 回落：降到水位线下后重新武装
    while (queue.size() > 4) {
        (void)queue.try_pop();
    }
    {
        IntentEnvelope intent;
        auto result = queue.push(std::move(intent));
        TEST_ASSERT(result.accepted && result.high_watermark, "re-armed after fall below");
    }
    return true;
}

} // namespace

int main() {
    struct Case {
        const char* name;
        bool (*fn)();
    };
    const Case cases[] = {
        {"queue_fifo_order_and_drain", test_queue_fifo_order_and_drain},
        {"queue_cap_rejects_new_keeps_old", test_queue_cap_rejects_new_keeps_old},
        {"queue_high_watermark_hysteresis", test_queue_high_watermark_hysteresis},
    };
    for (const auto& c : cases) {
        const int before = g_failures;
        const bool ok = c.fn();
        const bool passed = ok && g_failures == before;
        std::cout << (passed ? "[ PASS ] " : "[ FAIL ] ") << c.name << std::endl;
    }
    if (g_failures > 0) {
        std::cout << g_failures << " assertion(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all cell acceptance tests passed" << std::endl;
    return 0;
}
