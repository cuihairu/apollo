// cell 执行内核独立宿主测试（L98 首切片：执行/管理分离——执行内核
// CellApp 可独立构造、独立 tick，无传输无管理面）。
//
// 直编源惯例（同 world_app_acceptance_tests）：只编 cell_app.cpp + 协议源，
// 不依赖 WorldApp / M1LinkServer——接缝（IM1Egress + pre/post tick 钩子）
// 即断言面。覆盖：
//   1. 维护闸：start 前 / stop 后 submitIntent 拒收，onIntent 拒收即时回
//      2001；受理成功不回包（延时应答随 tick 边界消费后按 request_id 回带）；
//   2. tick 边界执行（单写者纪律）：submit 即刻探测会话无绑定、无回执，
//      tick 后绑定显形 + 延时应答 code 0 + 移动广播经下行出口逐会话重编帧头；
//   3. 顶格拒收（ADR-016）：突发 4×cap 必现拒收、队列不越界、stop 尾扫清空；
//   4. 停机 G-3 尾扫：stop() join 前残余意图执行完毕（确定性结果）；
//   5. 多执行核同进程隔离（L100 方向）：双 CellApp 各自 world/会话/下行独立。

#include "world_app/cell_app.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <utility>
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

namespace protocol = apollo::protocol;

// 测试侧 M1 出口缝：真收下行帧与延时应答（断言事实源，不只看日志）。
struct FakeEgress final : public world_app::IM1Egress {
    mutable std::mutex mutex;
    std::vector<std::pair<std::uint64_t, std::int32_t>> acks;   // (request_id, code)
    std::vector<std::pair<std::uint64_t, std::vector<std::uint8_t>>> frames;  // (session, frame)

    void sendClientDeliver(std::uint64_t session_id, std::uint32_t gateway_id,
                           const std::vector<std::uint8_t>& payload) override {
        (void)gateway_id;
        std::lock_guard<std::mutex> lock(mutex);
        frames.emplace_back(session_id, payload);
    }
    void sendIntentAck(std::uint64_t request_id, std::int32_t code,
                       const std::string& message) override {
        (void)message;
        std::lock_guard<std::mutex> lock(mutex);
        acks.emplace_back(request_id, code);
    }
};

// 有界轮询（Linux 绿不担保 macOS 绿：每拍 sleep >= 2ms——macos-ci-yield-polls）。
bool wait_until(const std::function<bool()>& cond, std::chrono::milliseconds timeout,
                std::chrono::milliseconds tick = std::chrono::milliseconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!cond()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(tick);
    }
    return true;
}

std::vector<uint8_t> make_create_frame(std::uint64_t entity_id, protocol::SessionID session_id,
                                       protocol::EntityType entity_type) {
    protocol::CellCreateEntity msg;
    msg.entityId = entity_id;
    msg.entityType = entity_type;
    msg.spaceId = 1;
    msg.position.x = 1.0f;
    msg.position.y = 2.0f;
    msg.position.z = 3.0f;
    return protocol::MessageCodec::encode(msg, session_id);
}

world_app::IntentEnvelope make_create_player_intent(std::uint64_t entity_id,
                                                    protocol::SessionID session_id,
                                                    std::uint64_t request_id = 0) {
    world_app::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_CREATE_ENTITY;
    intent.frame = make_create_frame(entity_id, session_id, protocol::EntityType::PLAYER);
    intent.session_id = session_id;
    intent.request_id = request_id;
    return intent;
}

world_app::IntentEnvelope make_move_intent(std::uint64_t entity_id,
                                           protocol::SessionID session_id) {
    protocol::CellEntityMove msg;
    msg.entityId = entity_id;
    msg.newPos.x = 4.0f;
    msg.newPos.y = 5.0f;
    msg.newPos.z = 6.0f;
    world_app::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_ENTITY_MOVE;
    intent.frame = protocol::MessageCodec::encode(msg, session_id);
    intent.session_id = session_id;
    return intent;
}

bool ack_seen(const FakeEgress& e, std::uint64_t request_id, std::int32_t code) {
    std::lock_guard<std::mutex> lock(e.mutex);
    for (const auto& a : e.acks) {
        if (a.first == request_id && a.second == code) {
            return true;
        }
    }
    return false;
}

std::size_t ack_count(const FakeEgress& e) {
    std::lock_guard<std::mutex> lock(e.mutex);
    return e.acks.size();
}

// 维护闸 + tick 边界执行 + 延时应答 + 下行重编帧头（内核级全链）。
bool test_cell_kernel_gate_and_tick_execution() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    // start 前：维护闸关 → 拒收；onIntent 拒收即时回 2001
    TEST_ASSERT(!cell.submitIntent(make_create_player_intent(1, 77)), "pre-start gated");
    world_app::IntentEnvelope gated = make_create_player_intent(1, 77, 2001);
    cell.onIntent(std::move(gated));
    TEST_ASSERT(ack_seen(egress, 2001, 2001), "maintenance gate immediate ack");

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));  // 首拍 settle

    // 受理成功不入队应答（ADR-017 延时应答随执行）；意图入队不待 tick
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(42, 77, 3001)),
                "create accepted");
    TEST_ASSERT(cell.pendingIntentCount() == 1, "intent queued, not applied");
    TEST_ASSERT(cell.resolveIntentPlayer(77) == 0, "no session before tick (single writer)");
    TEST_ASSERT(ack_count(egress) == 1, "no deferred ack before tick");

    // tick 边界执行：会话绑定显形 + 延时应答 code 0
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(77) == 42; },
                           std::chrono::seconds(3)),
                "session bound at tick boundary");
    TEST_ASSERT(cell.pendingIntentCount() == 0, "queue drained");
    TEST_ASSERT(ack_seen(egress, 3001, 0), "deferred ack code 0 after consumption");

    // 移动广播经下行出口逐会话重编帧头（mover 收己方回传）
    TEST_ASSERT(cell.submitIntent(make_move_intent(42, 77)), "move accepted");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(egress.mutex);
                    for (const auto& f : egress.frames) {
                        if (f.first != 77 || f.second.size() < sizeof(protocol::MessageHeader)) {
                            continue;
                        }
                        const auto h = protocol::MessageCodec::parseHeader(f.second);
                        if (h.sessionId == 77 &&
                            h.type == static_cast<std::uint16_t>(
                                          protocol::MessageType::CELL_ENTITY_MOVE)) {
                            return true;
                        }
                    }
                    return false;
                },
                std::chrono::seconds(3)),
            "move broadcast re-addressed to own session");

    cell.stop();
    // stop 后：维护闸关
    TEST_ASSERT(!cell.submitIntent(make_create_player_intent(43, 77)), "post-stop gated");
    return true;
}

// 顶格拒收（ADR-016）：突发 4×cap 必现拒收、队列不越界、stop 尾扫清空。
bool test_cell_kernel_overload_backpressure() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 1000;  // 突发窗口内内核至多 drain 一次
    world_app::CellApp cell(config);
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    constexpr std::size_t kCap = world_app::AcceptanceQueue::kDefaultCap;
    constexpr std::size_t kBurst = 4 * kCap;
    std::size_t accepted = 0;
    std::size_t rejected = 0;
    for (std::size_t i = 0; i < kBurst; ++i) {
        if (cell.submitIntent(make_create_player_intent(1000 + i, 77))) {
            ++accepted;
        } else {
            ++rejected;
        }
    }
    TEST_ASSERT(accepted + rejected == kBurst, "every intent classified");
    TEST_ASSERT(accepted >= kCap, "at least a full queue accepted");
    TEST_ASSERT(rejected > 0, "overflow rejected");
    TEST_ASSERT(cell.pendingIntentCount() <= kCap, "queue bounded at cap");

    // G-3：stop 关闸 → 尾扫 drain → 队列清空
    cell.stop();
    TEST_ASSERT(cell.pendingIntentCount() == 0, "queue drained on stop tail scan");
    return true;
}

// 停机 G-3 尾扫：stop() join 前残余意图执行完毕（确定性——tick 或尾扫，
// 终态一致）。
bool test_cell_kernel_shutdown_tail_scan() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 1000;  // 收窄 tick 窗口，尾扫为执行主体
    world_app::CellApp cell(config);
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(cell.submitIntent(make_create_player_intent(55, 90)), "queued");
    cell.stop();  // 立即停：tick 大概率未到，尾扫承担执行
    TEST_ASSERT(cell.pendingIntentCount() == 0, "tail scan drained");
    TEST_ASSERT(cell.resolveIntentPlayer(90) == 55, "residual intent executed on stop");
    return true;
}

// 多执行核同进程隔离（L100 方向）：双 CellApp 各自 world/会话/下行独立。
bool test_cell_kernel_two_instances_isolated() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp a(config);
    world_app::CellApp b(config);
    FakeEgress ea;
    FakeEgress eb;
    a.set_egress(&ea);
    b.set_egress(&eb);
    a.start();
    b.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    TEST_ASSERT(a.submitIntent(make_create_player_intent(42, 77)), "A create accepted");
    TEST_ASSERT(wait_until([&] { return a.resolveIntentPlayer(77) == 42; },
                           std::chrono::seconds(3)),
                "A session bound");
    TEST_ASSERT(a.submitIntent(make_move_intent(42, 77)), "A move accepted");
    TEST_ASSERT(wait_until([&] {
                    std::lock_guard<std::mutex> lock(ea.mutex);
                    return !ea.frames.empty();
                },
                std::chrono::seconds(3)),
                "A downlink delivered");
    TEST_ASSERT(b.resolveIntentPlayer(77) == 0, "B session untouched");
    {
        std::lock_guard<std::mutex> lock(eb.mutex);
        TEST_ASSERT(eb.frames.empty(), "B downlink empty");
    }
    a.stop();
    b.stop();
    return true;
}

} // namespace

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);  // abort 时块缓冲吞行误判定位面（orders_tests 坑账）
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"cell_kernel_gate_and_tick_execution", test_cell_kernel_gate_and_tick_execution},
        {"cell_kernel_overload_backpressure", test_cell_kernel_overload_backpressure},
        {"cell_kernel_shutdown_tail_scan", test_cell_kernel_shutdown_tail_scan},
        {"cell_kernel_two_instances_isolated", test_cell_kernel_two_instances_isolated},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "CellAppTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
