// Cell 受理面批测试（P3-2 批 B / ADR-015(a)/016/017；2b-iii 迁 M1 受理缝）：
//   受理队列单元面（FIFO/顶格拒收/两级水位滞回）+ CellServer submitIntent
//   受理判定（维护闸/tick 边界 drain/顶格拒收）。wire 回执面（2101/2001/延
//   时 929）归 cell_m1_link_tests（真 M1 链路断言），此处只断受理判定。
// 直编惯例同 scene_tests（无 GTest，plain-assert + main 汇总）；
#include "cell/acceptance_queue.hpp"
#include "cell/cell_server.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
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

using cell::AcceptanceQueue;
using cell::CellConfig;
using cell::CellServer;
using cell::IntentEnvelope;
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

// ---------- CellServer 受理点接线 ----------

std::vector<uint8_t> make_create_frame(std::uint64_t entity_id, protocol::SessionID session_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = entity_id;
    msg.entityType = protocol::EntityType::NPC;
    msg.spaceId = 1;
    msg.position.x = 1.0f;
    msg.position.y = 2.0f;
    msg.position.z = 3.0f;
    return protocol::MessageCodec::encode(msg, session_id);
}

cell::IntentEnvelope make_create_intent(std::uint64_t entity_id,
                                        protocol::SessionID session_id) {
    cell::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_CREATE_ENTITY;
    intent.frame = make_create_frame(entity_id, session_id);
    intent.session_id = session_id;
    return intent;
}

cell::IntentEnvelope make_move_intent(std::uint64_t entity_id,
                                      protocol::SessionID session_id) {
    protocol::CellEntityMove msg;
    msg.entityId = entity_id;
    msg.newPos.x = 4.0f;
    msg.newPos.y = 5.0f;
    msg.newPos.z = 6.0f;
    cell::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_ENTITY_MOVE;
    intent.frame = protocol::MessageCodec::encode(msg, session_id);
    intent.session_id = session_id;
    return intent;
}

bool test_acceptance_gate_and_queue() {
    // tickRateMs=1000 + 首段 settle：gameThread 的首次 drain（空队列）已过，
    // 后续 drain 在 1s 外——入队计数断言确定性（不依赖调度时序）
    CellConfig config;
    config.tickRateMs = 1000;
    CellServer server(config);

    // start 前：维护闸关 → 拒收
    TEST_ASSERT(!server.submitIntent(make_create_intent(1, 77)), "pre-start gated");

    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 受理成功不入队应答（延时应答随执行，ADR-017），意图入队不待 tick
    TEST_ASSERT(server.submitIntent(make_create_intent(42, 77)), "create accepted");
    TEST_ASSERT(server.pendingIntentCount() == 1, "intent queued");

    // 变更类第二型：move 同受理
    TEST_ASSERT(server.submitIntent(make_move_intent(42, 77)), "move accepted");
    TEST_ASSERT(server.pendingIntentCount() == 2, "second intent queued");

    server.stop();

    // stop 后：维护闸关
    TEST_ASSERT(!server.submitIntent(make_create_intent(43, 77)), "post-stop gated");
    return true;
}

bool test_overload_rejects_at_cap() {
    // ADR-016：顶格拒收不丢旧——队列永不超过 cap，拒收计数显形。
    // tickRateMs=1000：突发窗口内 gameThread 至多 drain 一次，故用 4×cap 突发
    // 保证拒收必现（单次 drain 让出的容量至多 cap，接得下上限 2×cap）；
    // 断言取调度容错形态——不依赖「突发恰好抢在首个 tick 前跑完」。
    CellConfig config;
    config.tickRateMs = 1000;
    CellServer server(config);
    server.start();

    constexpr std::size_t kCap = AcceptanceQueue::kDefaultCap;
    constexpr std::size_t kBurst = 4 * kCap;
    std::size_t accepted = 0;
    std::size_t rejected = 0;
    for (std::size_t i = 0; i < kBurst; ++i) {
        if (server.submitIntent(make_create_intent(1000 + i, 77))) {
            ++accepted;
        } else {
            ++rejected;
        }
    }
    TEST_ASSERT(accepted + rejected == kBurst, "every intent classified");
    TEST_ASSERT(accepted >= kCap, "at least a full queue accepted");
    TEST_ASSERT(rejected > 0, "overflow rejected");
    TEST_ASSERT(server.pendingIntentCount() <= kCap, "queue bounded at cap");

    // G-3：stop 关闸 → gameLoop 尾扫 drain → 队列清空
    server.stop();
    TEST_ASSERT(server.pendingIntentCount() == 0, "queue drained on stop");
    return true;
}

bool test_drain_at_tick_boundary() {
    // ADR-015(a)：意图在 tick 边界执行——受理先入队，tick 后清空，随后恢复受理。
    // tickRateMs=1000 + 首段 settle：入队计数确定性，随后等 ≥1 tick 断言清空
    CellConfig config;
    config.tickRateMs = 1000;
    CellServer server(config);
    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (std::uint64_t i = 0; i < 3; ++i) {
        TEST_ASSERT(server.submitIntent(make_create_intent(200 + i, 77)), "accepted");
    }
    TEST_ASSERT(server.pendingIntentCount() == 3, "intents queued before tick");

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));  // ≥ 1 tick
    TEST_ASSERT(server.pendingIntentCount() == 0, "drained at tick boundary");

    TEST_ASSERT(server.submitIntent(make_create_intent(300, 77)),
                "accepts again after drain");
    server.stop();
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
        {"acceptance_gate_and_queue", test_acceptance_gate_and_queue},
        {"overload_rejects_at_cap", test_overload_rejects_at_cap},
        {"drain_at_tick_boundary", test_drain_at_tick_boundary},
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
