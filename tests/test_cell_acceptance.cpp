// Cell 受理面批测试（P3-2 批 B / ADR-015(a)/016/017）：
//   受理队列单元面（FIFO/顶格拒收/两级水位滞回）+ CellServer 受理点接线
//   （维护闸/受理回执/Pong wire echo/tick 边界 drain/过载回错误包）。
// 直编惯例同 scene_tests（无 GTest，plain-assert + main 汇总）；
// 协议源直编同 apps/cell-app（stub 传输，APOLLO_USE_NNG 未定义）。
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

bool body_of(const std::vector<uint8_t>& frame, std::vector<uint8_t>* out) {
    if (frame.size() < sizeof(protocol::MessageHeader)) {
        return false;
    }
    out->assign(frame.begin() + static_cast<std::ptrdiff_t>(sizeof(protocol::MessageHeader)),
                frame.end());
    return true;
}

bool test_acceptance_ack_ping_and_maintenance_gate() {
    // tickRateMs=1000 + 首段 settle：gameThread 的首次 drain（空队列）已过，
    // 后续 drain 在 1s 外——入队计数断言确定性（不依赖调度时序）
    CellConfig config;
    config.tickRateMs = 1000;
    CellServer server(config);

    // start 前：维护闸关 → 维护中错误包
    auto gateReply = server.acceptRequest(make_create_frame(1, 77));
    TEST_ASSERT(!gateReply.empty(), "pre-start request answered at gate");
    {
        std::vector<uint8_t> body;
        TEST_ASSERT(body_of(gateReply, &body), "gate reply has body");
        auto err = protocol::MessageCodec::decodeBody<protocol::ErrorMessage>(body);
        TEST_ASSERT(err.message.find("maintenance") != std::string::npos, "maintenance reply");
    }

    server.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 变更类：受理点空 ack（逐字节同现状），意图入队不待 tick
    TEST_ASSERT(server.acceptRequest(make_create_frame(42, 77)).empty(), "create acked empty");
    TEST_ASSERT(server.pendingIntentCount() == 1, "intent queued");

    // 变更类第二型：move 同样空 ack（CellDestroyEntity 无 encode overload，
    // codec 只解不编——wire 生成方在网关，测试面用可编码的 move 型）
    protocol::CellEntityMove moveMsg;
    moveMsg.entityId = 42;
    moveMsg.newPos.x = 4.0f;
    moveMsg.newPos.y = 5.0f;
    moveMsg.newPos.z = 6.0f;
    TEST_ASSERT(
        server.acceptRequest(protocol::MessageCodec::encode(moveMsg, 77)).empty(),
        "move acked empty");

    // PING：受理点回 Pong（wire echo），不入队
    protocol::Ping ping;
    ping.timestamp = 12345;
    auto pongReply = server.acceptRequest(protocol::MessageCodec::encode(ping, 77));
    TEST_ASSERT(!pongReply.empty(), "ping answered at acceptance");
    {
        std::vector<uint8_t> body;
        TEST_ASSERT(body_of(pongReply, &body), "pong reply has body");
        auto pong = protocol::MessageCodec::decodeBody<protocol::Pong>(body);
        TEST_ASSERT(pong.timestamp == 12345, "pong echoes timestamp");
    }
    TEST_ASSERT(server.pendingIntentCount() == 2, "ping not queued");

    server.stop();

    // stop 后：维护闸关
    auto stoppedReply = server.acceptRequest(make_create_frame(43, 77));
    TEST_ASSERT(!stoppedReply.empty(), "post-stop request answered at gate");
    return true;
}

bool test_overload_reply_at_cap() {
    // ADR-016：顶格拒收不静默——受理点回错误包，队列永不超过 cap。
    // tickRateMs=1000：突发窗口内 gameThread 至多 drain 一次，故用 4×cap 突发
    // 保证拒收必现（单次 drain 让出的容量至多 cap，接得下上限 2×cap）；
    // 断言取调度容错形态——不依赖「突发恰好抢在首个 tick 前跑完」。
    CellConfig config;
    config.tickRateMs = 1000;
    CellServer server(config);
    server.start();

    constexpr std::size_t kCap = AcceptanceQueue::kDefaultCap;
    constexpr std::size_t kBurst = 4 * kCap;
    std::size_t acked = 0;
    std::size_t overloaded = 0;
    for (std::size_t i = 0; i < kBurst; ++i) {
        auto reply = server.acceptRequest(make_create_frame(1000 + i, 77));
        if (reply.empty()) {
            ++acked;
        } else {
            ++overloaded;
        }
    }
    TEST_ASSERT(acked + overloaded == kBurst, "every request classified");
    TEST_ASSERT(acked >= kCap, "at least a full queue acked");
    TEST_ASSERT(overloaded > 0, "overflow rejected");
    TEST_ASSERT(server.pendingIntentCount() <= kCap, "queue bounded at cap");
    // 过载回执面：突发后队列未必仍满格（drain 可让出容量），补推到回过载包
    std::string overloadMessage;
    for (std::size_t i = 0; i < 2 * kCap; ++i) {
        auto reply = server.acceptRequest(make_create_frame(9000 + i, 77));
        if (!reply.empty()) {
            std::vector<uint8_t> body;
            TEST_ASSERT(body_of(reply, &body), "overload reply has body");
            overloadMessage =
                protocol::MessageCodec::decodeBody<protocol::ErrorMessage>(body).message;
            break;
        }
    }
    TEST_ASSERT(overloadMessage.find("overloaded") != std::string::npos, "overload reply");

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
        TEST_ASSERT(server.acceptRequest(make_create_frame(200 + i, 77)).empty(), "acked");
    }
    TEST_ASSERT(server.pendingIntentCount() == 3, "intents queued before tick");

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));  // ≥ 1 tick
    TEST_ASSERT(server.pendingIntentCount() == 0, "drained at tick boundary");

    TEST_ASSERT(server.acceptRequest(make_create_frame(300, 77)).empty(),
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
        {"acceptance_ack_ping_and_maintenance_gate", test_acceptance_ack_ping_and_maintenance_gate},
        {"overload_reply_at_cap", test_overload_reply_at_cap},
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
