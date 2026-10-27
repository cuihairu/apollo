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
//   5. 多执行核同进程隔离（L100 方向）：双 CellApp 各自 world/会话/下行独立；
//   6. Space ↔ Scene 显式映射（L99 首切片）：多空间路由、跨空间 AOI 隔离、
//      跨空间转移走 spaceToScene_ 路由表、未知空间回落默认 scene。
//   7. 声明 Space × 小队副本同进程共存（L102 承接切片）：config.spaces 声明
//      面上复用既有副本全流程，实例临时 Scene 独立自建自毁不占 Space 路由，
//      声明 Space 的路由/scene/旁观者全程不受扰动。

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
#include <set>
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

// L99 映射面：带显式 space 的创建帧（默认 make_create_frame 固定 spaceId=1）
std::vector<uint8_t> make_create_frame_in_space(std::uint64_t entity_id,
                                                protocol::SessionID session_id,
                                                protocol::EntityType entity_type,
                                                std::uint64_t space_id) {
    protocol::CellCreateEntity msg;
    msg.entityId = entity_id;
    msg.entityType = entity_type;
    msg.spaceId = static_cast<protocol::SpaceID>(space_id);
    msg.position.x = 1.0f;
    msg.position.y = 2.0f;
    msg.position.z = 3.0f;
    return protocol::MessageCodec::encode(msg, session_id);
}

world_app::IntentEnvelope make_create_player_intent_in_space(
    std::uint64_t entity_id, protocol::SessionID session_id, std::uint64_t space_id,
    std::uint64_t request_id = 0) {
    world_app::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_CREATE_ENTITY;
    intent.frame = make_create_frame_in_space(entity_id, session_id,
                                              protocol::EntityType::PLAYER, space_id);
    intent.session_id = session_id;
    intent.request_id = request_id;
    return intent;
}

world_app::IntentEnvelope make_xborder_intent(std::uint64_t entity_id,
                                              protocol::SessionID session_id,
                                              std::uint64_t to_space,
                                              std::uint64_t from_space = 1) {
    protocol::CellCrossBorder msg;
    msg.entityId = entity_id;
    msg.fromSpace = static_cast<protocol::SpaceID>(from_space);
    msg.toSpace = static_cast<protocol::SpaceID>(to_space);
    msg.position.x = 5.0f;
    msg.position.y = 5.0f;
    msg.position.z = 5.0f;
    world_app::IntentEnvelope intent;
    intent.type = protocol::MessageType::CELL_CROSS_BORDER;
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

// 某会话收到的移动广播帧数（跨 space 可见集隔离的观测面：AOI 不跨 scene）
std::size_t move_frame_count(const FakeEgress& e, std::uint64_t session_id) {
    std::lock_guard<std::mutex> lock(e.mutex);
    std::size_t count = 0;
    for (const auto& f : e.frames) {
        if (f.first != session_id || f.second.size() < sizeof(protocol::MessageHeader)) {
            continue;
        }
        const auto h = protocol::MessageCodec::parseHeader(f.second);
        if (h.sessionId == session_id &&
            h.type == static_cast<std::uint16_t>(protocol::MessageType::CELL_ENTITY_MOVE)) {
            ++count;
        }
    }
    return count;
}

// 第二空间显式登记用 descriptor（与默认 space 尺寸口径一致；map_id 独立）
apollo::game::world::SceneDescriptor second_space_descriptor() {
    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 2;
    descriptor.map_name = "space-7";
    descriptor.width = 1000.0f;
    descriptor.height = 1000.0f;
    descriptor.grid_size = 100.0f;
    descriptor.view_radius = 200.0f;
    return descriptor;
}

// ---- 小队流程驱动面（L102 共存切片）：意图直投 + 下行帧解析 ----

// 下行帧解包（会话 + 类型 + 去 header body）：副本族回执的事实源解析面，
// 同 test_world_drain 的 ClientFace 口径搬到 FakeEgress 上。
struct DeliveredFrame {
    std::uint64_t session_id;
    std::uint16_t type;
    std::vector<std::uint8_t> body;
};

std::vector<DeliveredFrame> take_frames(const FakeEgress& e, std::uint16_t type) {
    std::lock_guard<std::mutex> lock(e.mutex);
    std::vector<DeliveredFrame> out;
    for (const auto& f : e.frames) {
        if (f.second.size() < sizeof(protocol::MessageHeader)) continue;
        const auto h = protocol::MessageCodec::parseHeader(f.second);
        if (h.type != type) continue;
        out.push_back({f.first, h.type,
                       std::vector<std::uint8_t>(f.second.begin() +
                                                     sizeof(protocol::MessageHeader),
                                                 f.second.end())});
    }
    return out;
}

// 泛型帧等待：类型 Msg 的任一已收帧满足 pred（帧只增不消费，重复扫描幂等）
template <typename Msg, typename Pred>
bool wait_frame(const FakeEgress& e, Pred&& pred, std::chrono::milliseconds timeout) {
    return wait_until(
        [&] {
            for (const auto& f :
                 take_frames(e, static_cast<std::uint16_t>(Msg::TYPE))) {
                if (pred(protocol::MessageCodec::decodeBody<Msg>(f.body),
                         f.session_id)) {
                    return true;
                }
            }
            return false;
        },
        timeout);
}

world_app::IntentEnvelope make_squad_intent(protocol::MessageType type,
                                            const std::vector<std::uint8_t>& frame,
                                            std::uint64_t session_id) {
    world_app::IntentEnvelope intent;
    intent.type = type;
    intent.frame = frame;
    intent.session_id = session_id;
    return intent;
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
    // 会话表先于回执显形（create_session 早于 sendAck）——回执断言用有界
    // 轮询（同 wait_until 纪律），不做单点采样（高载下单点必现假红）
    TEST_ASSERT(wait_until([&] { return ack_seen(egress, 3001, 0); },
                           std::chrono::seconds(3)),
                "deferred ack code 0 after consumption");

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

// ---- L99 首切片：Space ↔ Scene 显式映射 + 三 ID 域分开 ----
//
// 口径（docs bigworld-apollo-mapping §2/§6）：Space 对应现有 Scene 的空间语义
//（不叠同义容器）；space_id 是内核路由键（spaceToScene_），scene_id 由 world
// 自增分配——两域数值独立，映射是真实的表而不是恒等别名。分区 ID 今日退化
// 为 scene_id（每 Space 单全幅 Cell），执行进程身份 = 本进程；跨进程拆分随
// L101/L102 注册批落地，本批不动。

// 多空间路由与跨空间可见集隔离：space 1 与 space 7 各落各的 scene，
// AOI 广播不跨空间。
bool test_cell_kernel_multi_space_routing_and_isolation() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    // start 前显式登记第二空间（单线程注册窗，同 ctor 时序合法）
    const auto space7_scene = cell.add_space(7, second_space_descriptor());
    TEST_ASSERT(space7_scene != 0, "second space scene created");
    TEST_ASSERT(space7_scene != 7, "scene id independent of space id (not identity alias)");

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    TEST_ASSERT(cell.space_count() == 2, "two space routes registered");
    TEST_ASSERT(cell.scene_id_of_space(7) == space7_scene, "route table holds created scene");
    TEST_ASSERT(cell.scene_id_of_space(1) != cell.scene_id_of_space(7),
                "default and second space map to distinct scenes");
    // 路由存在查询（撤销面「真持有」判据）：本表命中为 true，未知空间为
    // false——不回落默认（scene_id_of_space 的未知回落语义不适合作持有判据）。
    TEST_ASSERT(cell.hosts_space(7), "hosts the declared second space");
    TEST_ASSERT(cell.hosts_space(1), "hosts the default space");
    TEST_ASSERT(!cell.hosts_space(999), "never-hosted space not present");

    // 各空间一玩家：A 落默认 space 1，B 落 space 7
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(42, 77)), "A create accepted");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(900, 88, 7)),
                "B create accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(77) == 42; },
                           std::chrono::seconds(3)),
                "A bound in default space");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(88) == 900; },
                           std::chrono::seconds(3)),
                "B bound in second space");

    // B 移动：只有 B 自己收到（A 在另一空间的 scene，AOI 不跨空间）
    TEST_ASSERT(cell.submitIntent(make_move_intent(900, 88)), "B move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 88) >= 1; },
                           std::chrono::seconds(3)),
                "B receives own move");
    TEST_ASSERT(move_frame_count(egress, 77) == 0, "A sees nothing across spaces");

    // 对称：A 移动也到不了 B
    TEST_ASSERT(cell.submitIntent(make_move_intent(42, 77)), "A move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 77) >= 1; },
                           std::chrono::seconds(3)),
                "A receives own move");
    TEST_ASSERT(move_frame_count(egress, 88) == 1, "B still sees nothing across spaces");

    cell.stop();
    return true;
}

// 跨空间转移走 spaceToScene_ 路由表：跨境 toSpace=7 落第二空间 scene，
// 转移后与 B 同空间互相可见。
bool test_cell_kernel_cross_space_transfer() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);
    const auto space7_scene = cell.add_space(7, second_space_descriptor());
    TEST_ASSERT(space7_scene != 0, "second space scene created");

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    // A 落默认空间，B 落 space 7
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(42, 77)), "A create accepted");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(900, 88, 7)),
                "B create accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(77) == 42; },
                           std::chrono::seconds(3)),
                "A bound");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(88) == 900; },
                           std::chrono::seconds(3)),
                "B bound");

    // 转移前：B 移动到不了 A（跨空间）
    TEST_ASSERT(move_frame_count(egress, 77) == 0, "A blind to B before transfer");

    // A 跨境到 space 7：路由表解析目标 scene，scene_transfer 走完
    TEST_ASSERT(cell.submitIntent(make_xborder_intent(42, 77, 7)), "cross-border accepted");
    // 转移完成的观测面：A 与 B 同空间后 B 的移动抵达 A
    TEST_ASSERT(cell.submitIntent(make_move_intent(900, 88)), "B move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 77) >= 1; },
                           std::chrono::seconds(3)),
                "A sees B after cross-space transfer");

    cell.stop();
    return true;
}

// 未知空间回落默认 scene（旧「空间无关全落全局表」行为等价保持；不登记路由）
bool test_cell_kernel_unknown_space_falls_back_to_default() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(42, 77, 999)),
                "unknown-space create accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(77) == 42; },
                           std::chrono::seconds(3)),
                "session bound via fallback");
    TEST_ASSERT(cell.space_count() == 1, "unknown space not registered as route");

    // 回落 = 落默认 scene：移动广播正常（avatar 确在默认空间 scene 内）
    TEST_ASSERT(cell.submitIntent(make_move_intent(42, 77)), "move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 77) >= 1; },
                           std::chrono::seconds(3)),
                "avatar live in default scene");

    cell.stop();
    return true;
}

// L100 多 Space 默认接线：config.spaces 声明面——start 装配窗逐条登记
//（add_space 同幂等语义），默认 Space 不受影响；声明 Space 为 Instance-less
//（批㉑口径，实例随玩法副本流程自建）。CLI 入口另在装配拒绝重复/非法 id，
// config API 层只测内核幂等。
bool test_config_declared_spaces() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    config.spaces = {{7, "aux"}, {8, "lobby"}};
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    // 声明面装配：默认 + 7 + 8 三路由；scene id 域独立且互异
    TEST_ASSERT(cell.space_count() == 3, "default plus two declared spaces");
    const auto s1 = cell.scene_id_of_space(1);
    const auto s7 = cell.scene_id_of_space(7);
    const auto s8 = cell.scene_id_of_space(8);
    TEST_ASSERT(s1 != 0 && s7 != 0 && s8 != 0, "all declared routes resolved");
    TEST_ASSERT(s1 != s7 && s7 != s8 && s1 != s8, "scene ids distinct across spaces");

    // 会话落点证明（仅线程安全观测面——egress 可见性 + 转移链；scene 直读
    // 非跨线程观测面）：A 落 space 7、B 落 space 8，跨声明空间互盲；A 7→8
    // 转移成功 = A 确在 space 7 scene（config 声明 Space 对路由/落点面与
    // add_space 手工登记完全同权）
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(51, 81, 7, 4001)),
                "create in declared space 7 accepted");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(52, 82, 8, 4002)),
                "create in declared space 8 accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(81) == 51; },
                           std::chrono::seconds(3)),
                "A bound in declared space 7");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(82) == 52; },
                           std::chrono::seconds(3)),
                "B bound in declared space 8");
    TEST_ASSERT(cell.submitIntent(make_move_intent(51, 81)), "A move accepted");
    std::this_thread::sleep_for(std::chrono::milliseconds(60));  // 让移动先落地
    TEST_ASSERT(move_frame_count(egress, 82) == 0, "B blind to A across declared spaces");
    TEST_ASSERT(cell.submitIntent(make_xborder_intent(51, 81, 8)),
                "A transfer 7 to 8 accepted");
    TEST_ASSERT(cell.submitIntent(make_move_intent(52, 82)), "B move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 81) >= 1; },
                           std::chrono::seconds(3)),
                "A sees B after declared-space transfer");

    cell.stop();

    // 重复声明幂等（config API 同 add_space 口径：重复登记不重建 scene）
    world_app::WorldAppConfig dup;
    dup.tickRateMs = 20;
    dup.spaces = {{7, "first"}, {7, "second"}};
    world_app::CellApp cell2(dup);
    cell2.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    TEST_ASSERT(cell2.space_count() == 2, "duplicate declaration idempotent");
    TEST_ASSERT(cell2.scene_id_of_space(7) != 0, "duplicate route exists");
    cell2.stop();
    return true;
}

// L101 失联策略切片：内核冻结面——冻结 = tick 边界停模拟推进（意图消费
// 暂停、已受理意图保序滞留）+ 关受理闸（新输入拒收）；解冻恢复闸并让滞留
// 意图在 tick 边界执行 + 延时回执。泵/钩子照跑（听 World 复核解冻）由
// cell_register_tests 的生产客户端例覆盖，本例钉内核语义。
bool test_cell_kernel_freeze_face() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));  // 首拍 settle

    // 冻结即关闸：新输入拒收（IO 层关闭新受理——§4 失联策略）
    cell.set_frozen(true);
    TEST_ASSERT(cell.frozen(), "frozen flag visible");
    TEST_ASSERT(!cell.submitIntent(make_create_player_intent(9, 70, 5001)),
                "gate closed while frozen");

    // 已受理意图滞留不执行：冻结拍不消费队列（模拟推进暂停）、无延时回执
    cell.set_frozen(false);
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(55, 90, 5002)),
                "intent accepted before freeze");
    cell.set_frozen(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));  // > 5 拍（20ms tick）
    TEST_ASSERT(cell.resolveIntentPlayer(90) == 0, "held intent not executed while frozen");
    TEST_ASSERT(cell.pendingIntentCount() == 1, "intent held in queue, in order");
    TEST_ASSERT(ack_count(egress) == 0, "no deferred ack while frozen");

    // 解冻：闸随 running 恢复，滞留意图在 tick 边界执行 + 延时回执补上
    cell.set_frozen(false);
    TEST_ASSERT(!cell.frozen(), "unfrozen");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(8, 71, 5003)),
                "gate reopened on unfreeze");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(90) == 55; },
                           std::chrono::seconds(3)),
                "held intent executed after unfreeze");
    TEST_ASSERT(wait_until([&] { return ack_seen(egress, 5002, 0); },
                           std::chrono::seconds(3)),
                "held intent acked after unfreeze");
    TEST_ASSERT(wait_until([&] { return ack_seen(egress, 5003, 0); },
                           std::chrono::seconds(3)),
                "post-unfreeze intent acked");

    cell.stop();
    return true;
}

// L108 回收首切片：内核撤销面 remove_space——默认 Space 拒撤、未知空间幂
// 等 0、占用中（scene 有 avatar）拒撤保路由、清场后撤销 = 摘路由 + 销毁空
// scene。撤销面在生产里由 962 监听在 pre-tick 泵上下文调用（内核线程），
// 本例经 post-tick 钩子在同上下文驱动（同纪律——测试线程不碰内核状态）。
bool test_cell_kernel_remove_space() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    // start 前注册窗：登记 space 7 + 默认/未知空间的撤销面语义
    const auto space7_scene = cell.add_space(7, second_space_descriptor());
    TEST_ASSERT(space7_scene != 0, "space 7 scene created");
    TEST_ASSERT(cell.remove_space(1) == 0, "default space refuses revoke");
    TEST_ASSERT(cell.remove_space(999) == 0, "unknown space idempotent no-op");
    TEST_ASSERT(
        cell.space_count() == 1 && cell.scene_id_of_space(7) == space7_scene,
        "routes intact (default route registers lazily at start)");

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // 占用中拒撤：玩家在 space 7（post-tick 每拍尝试撤销面，记录首个结果）
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(51, 91, 7, 6001)),
                "create in space 7 accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(91) == 51; },
                           std::chrono::seconds(3)),
                "player bound in space 7");
    std::atomic<int> revoke_seen{0};  // 0 = 拒撤（占用中），非 0 = 撤销的 scene id
    cell.set_post_tick([&](double) {
        const auto r = cell.remove_space(7);
        int expected = 0;
        revoke_seen.compare_exchange_strong(expected, static_cast<int>(r));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(120));  // > 5 拍
    TEST_ASSERT(revoke_seen.load() == 0, "occupied space refuses revoke");
    TEST_ASSERT(cell.space_count() == 2, "route kept while occupied");

    // 清场（跨境回默认 space）→ 撤销成功：摘路由 + 销毁空 scene（幂等 0）
    TEST_ASSERT(cell.submitIntent(make_xborder_intent(51, 91, 1, 7)),
                "player vacates space 7 via border");
    TEST_ASSERT(wait_until([&] { return revoke_seen.load() != 0; },
                           std::chrono::seconds(3)),
                "revoke succeeds after vacate");
    TEST_ASSERT(revoke_seen.load() == static_cast<int>(space7_scene),
                "revoked scene id matches");
    TEST_ASSERT(cell.space_count() == 1, "route removed");
    TEST_ASSERT(cell.find_scene(space7_scene) == nullptr, "scene destroyed");

    cell.stop();
    return true;
}

// 撤销待重试面（L108 占用联动切片）：request_space_revoke 受理跨线程只入
// 队，gameLoop tick 边界重试——占用中被拒的撤销在玩家跨境腾空后自动成功，
// 无需第二次显式调用。观测面经 post_tick 钩子（内核线程）读 space_count，
// 测试线程不碰内核状态（同 remove_space 例纪律）。
bool test_cell_kernel_pending_revoke_retry() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    const auto space7_scene = cell.add_space(7, second_space_descriptor());
    TEST_ASSERT(space7_scene != 0, "space 7 scene created");
    TEST_ASSERT(!cell.request_space_revoke(1), "default space revoke refused");
    // 未知空间受理入队（跨线程面不读内核态），消费 tick 边界按「无路由」
    // 出表丢弃——start 后首拍内归零
    TEST_ASSERT(cell.request_space_revoke(999), "unknown space id accepted");
    TEST_ASSERT(cell.pending_revoke_count() == 1, "pending holds unknown id");

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(cell.pending_revoke_count() == 0, "unknown id discarded by drain");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(61, 71, 7, 7001)),
                "create in space 7 accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(71) == 61; },
                           std::chrono::seconds(3)),
                "player bound in space 7");

    TEST_ASSERT(cell.request_space_revoke(7), "occupied space revoke accepted");
    TEST_ASSERT(!cell.request_space_revoke(7), "duplicate revoke idempotent");
    TEST_ASSERT(cell.pending_revoke_count() == 1, "one pending revoke");

    std::atomic<int> route_count{-1};
    cell.set_post_tick([&](double) {
        route_count.store(static_cast<int>(cell.space_count()));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // > 5 拍
    TEST_ASSERT(route_count.load() == 2, "occupied keeps route under retry");
    TEST_ASSERT(cell.pending_revoke_count() == 1, "still pending while occupied");

    TEST_ASSERT(cell.submitIntent(make_xborder_intent(61, 71, 1, 7)),
                "player vacates space 7 via border");
    TEST_ASSERT(wait_until([&] { return route_count.load() == 1; },
                           std::chrono::seconds(3)),
                "pending retry removes route after vacate");
    TEST_ASSERT(cell.pending_revoke_count() == 0, "pending cleared on success");

    cell.stop();
    return true;
}

// 取消待撤面：World 重认领（962 ready=1 回推）优先于陈旧撤销——占用中
// 的待撤被取消后，腾空不再触发撤销，路由保持。
bool test_cell_kernel_cancel_space_revoke() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    TEST_ASSERT(cell.add_space(7, second_space_descriptor()) != 0, "space 7 created");
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(62, 72, 7, 7002)),
                "create in space 7 accepted");
    TEST_ASSERT(wait_until([&] { return cell.resolveIntentPlayer(72) == 62; },
                           std::chrono::seconds(3)),
                "player bound in space 7");

    TEST_ASSERT(cell.request_space_revoke(7), "revoke pending");
    TEST_ASSERT(cell.pending_revoke_count() == 1, "one pending before cancel");
    cell.cancel_space_revoke(7);
    TEST_ASSERT(cell.pending_revoke_count() == 0, "cancel clears pending");
    cell.cancel_space_revoke(7);  // 重复取消 = no-op 不崩不回退

    std::atomic<int> route_count{-1};
    cell.set_post_tick([&](double) {
        route_count.store(static_cast<int>(cell.space_count()));
    });
    TEST_ASSERT(cell.submitIntent(make_xborder_intent(62, 72, 1, 7)),
                "player vacates space 7 via border");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));  // > 5 拍
    TEST_ASSERT(route_count.load() == 2, "cancelled revoke keeps route after vacate");

    cell.stop();
    return true;
}

// L102 承接切片：声明 Space × 小队副本同进程共存。config.spaces 声明面上
// 走完整副本流程（建队→入队→launch→enter×3→start→击杀满额→结算销毁）：
// 实例临时 Scene 与两个 Space scene 三域互异、生命周期内不占 Space 路由表，
// 结算销毁只回收临时 Scene——声明 Space 的路由/scene/在场旁观者全程不动。
// 内核态（路由数/实例绑定/scene 在场性）经 post_tick 钩子在内核线程快照
//（同 remove_space 例纪律）；协议面断言取 FakeEgress 下行帧（事实源）。
bool test_declared_space_squad_flow_isolation() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    config.spaces = {{7, "aux"}};
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    // 内核线程观测面：路由数 / 实例绑定 scene（sticky）/ scene 在场性
    std::atomic<std::uint64_t> party_hint{0};  // 测试线程从 PARTY_RESULT 回填
    std::atomic<std::size_t> route_count{0};
    std::atomic<std::uint64_t> instance_scene{0};  // 最近一次可见的实例 scene id
    std::atomic<bool> binding_alive{false};
    std::atomic<bool> instance_scene_present{false};
    cell.set_post_tick([&](double) {
        route_count.store(cell.space_count());
        const auto pid = party_hint.load(std::memory_order_acquire);
        const auto* binding = pid != 0
                                  ? cell.squadInstanceService()->find_by_party(pid)
                                  : nullptr;
        binding_alive.store(binding != nullptr, std::memory_order_release);
        if (binding != nullptr) {
            instance_scene.store(binding->scene_id, std::memory_order_release);
        }
        const auto scene = instance_scene.load(std::memory_order_acquire);
        instance_scene_present.store(scene != 0 && cell.find_scene(scene) != nullptr,
                                     std::memory_order_release);
    });

    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    TEST_ASSERT(route_count.load() == 2, "default plus declared space 7");
    const auto default_scene = cell.scene_id_of_space(1);
    const auto aux_scene = cell.scene_id_of_space(7);
    TEST_ASSERT(default_scene != 0 && aux_scene != 0 && default_scene != aux_scene,
                "declared route resolved");

    // 名册：默认空间三人小队（77/78/79）+ space 7 旁观者（88）
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(101, 77)), "leader spawn");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(102, 78)), "member spawn");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent(103, 79)), "member spawn");
    TEST_ASSERT(cell.submitIntent(make_create_player_intent_in_space(900, 88, 7)),
                "observer spawn in declared space");
    TEST_ASSERT(wait_until([&] {
                    return cell.resolveIntentPlayer(77) == 101 &&
                           cell.resolveIntentPlayer(78) == 102 &&
                           cell.resolveIntentPlayer(79) == 103 &&
                           cell.resolveIntentPlayer(88) == 900;
                },
                           std::chrono::seconds(3)),
                "roster and observer bound");

    // 建队（队长 77）：服务端分配 party_id（契约 23 语义）
    TEST_ASSERT(cell.submitIntent(make_squad_intent(
                    protocol::MessageType::PARTY_CREATE_REQUEST,
                    protocol::MessageCodec::encode(protocol::PartyCreateRequest{}, 77),
                    77)),
                "party create accepted");
    TEST_ASSERT(wait_frame<protocol::PartyResult>(
                    egress,
                    [](const protocol::PartyResult& r, std::uint64_t s) {
                        return s == 77 &&
                               r.op == static_cast<std::int32_t>(
                                           protocol::SquadOp::PARTY_CREATE) &&
                               r.code == 0 && r.partyId != 0;
                    },
                    std::chrono::seconds(3)),
                "party created with server id");
    std::uint64_t party_id = 0;
    for (const auto& f : take_frames(egress, static_cast<std::uint16_t>(
                                                protocol::MessageType::PARTY_RESULT))) {
        const auto r = protocol::MessageCodec::decodeBody<protocol::PartyResult>(f.body);
        if (f.session_id == 77 &&
            r.op == static_cast<std::int32_t>(protocol::SquadOp::PARTY_CREATE) &&
            r.code == 0) {
            party_id = r.partyId;
        }
    }
    TEST_ASSERT(party_id != 0, "party id parsed from downlink");
    party_hint.store(party_id, std::memory_order_release);

    // 入队 ×2：成员 78/79
    for (const auto session : {std::uint64_t{78}, std::uint64_t{79}}) {
        protocol::PartyJoinRequest join;
        join.partyId = party_id;
        TEST_ASSERT(cell.submitIntent(make_squad_intent(
                        protocol::MessageType::PARTY_JOIN_REQUEST,
                        protocol::MessageCodec::encode(join, session), session)),
                    "party join accepted");
        TEST_ASSERT(wait_frame<protocol::PartyResult>(
                        egress,
                        [&](const protocol::PartyResult& r, std::uint64_t s) {
                            return s == session &&
                                   r.op == static_cast<std::int32_t>(
                                               protocol::SquadOp::PARTY_JOIN) &&
                                   r.code == 0;
                        },
                        std::chrono::seconds(3)),
                    "party join acked");
    }

    // launch（mapId 0 = 服务端默认资产）：实例临时 Scene 自建
    protocol::SquadLaunch launch;
    launch.partyId = party_id;
    TEST_ASSERT(cell.submitIntent(make_squad_intent(
                    protocol::MessageType::SQUAD_LAUNCH,
                    protocol::MessageCodec::encode(launch, 77), 77)),
                "squad launch accepted");
    TEST_ASSERT(wait_frame<protocol::SquadResult>(
                    egress,
                    [](const protocol::SquadResult& r, std::uint64_t s) {
                        return s == 77 &&
                               r.op == static_cast<std::int32_t>(
                                           protocol::SquadOp::SQUAD_LAUNCH) &&
                               r.code == 0;
                    },
                    std::chrono::seconds(3)),
                "squad launched");
    std::uint64_t launch_scene = 0;
    for (const auto& f : take_frames(egress, static_cast<std::uint16_t>(
                                                protocol::MessageType::SQUAD_RESULT))) {
        const auto r = protocol::MessageCodec::decodeBody<protocol::SquadResult>(f.body);
        if (f.session_id == 77 &&
            r.op == static_cast<std::int32_t>(protocol::SquadOp::SQUAD_LAUNCH) &&
            r.code == 0) {
            launch_scene = r.sceneId;
        }
    }
    TEST_ASSERT(wait_until([&] { return binding_alive.load(); },
                           std::chrono::seconds(3)),
                "instance binding visible to kernel-thread hook");
    const auto temp_scene = instance_scene.load();
    TEST_ASSERT(temp_scene != 0, "instance temp scene allocated");
    TEST_ASSERT(temp_scene != default_scene && temp_scene != aux_scene,
                "temp scene distinct from both space scenes (three id domains)");
    TEST_ASSERT(launch_scene == temp_scene, "wire scene id matches binding");
    TEST_ASSERT(route_count.load() == 2, "instance adds no space route");

    // enter ×3 → start → Running（phase 3）带首波怪
    for (const auto session :
         {std::uint64_t{77}, std::uint64_t{78}, std::uint64_t{79}}) {
        protocol::SquadEnter enter;
        enter.partyId = party_id;
        TEST_ASSERT(cell.submitIntent(make_squad_intent(
                        protocol::MessageType::SQUAD_ENTER,
                        protocol::MessageCodec::encode(enter, session), session)),
                    "squad enter accepted");
        TEST_ASSERT(wait_frame<protocol::SquadResult>(
                        egress,
                        [&](const protocol::SquadResult& r, std::uint64_t s) {
                            return s == session &&
                                   r.op == static_cast<std::int32_t>(
                                               protocol::SquadOp::SQUAD_ENTER) &&
                                   r.code == 0;
                        },
                        std::chrono::seconds(3)),
                    "squad enter acked");
    }
    protocol::SquadStart start;
    start.partyId = party_id;
    TEST_ASSERT(cell.submitIntent(make_squad_intent(
                    protocol::MessageType::SQUAD_START,
                    protocol::MessageCodec::encode(start, 77), 77)),
                "squad start accepted");
    TEST_ASSERT(wait_frame<protocol::SquadResult>(
                    egress,
                    [](const protocol::SquadResult& r, std::uint64_t s) {
                        return s == 77 &&
                               r.op == static_cast<std::int32_t>(
                                           protocol::SquadOp::SQUAD_START) &&
                               r.code == 0;
                    },
                    std::chrono::seconds(3)),
                "squad start acked");
    std::set<std::uint64_t> monster_ids;  // 帧只增不消费：按 id 去重收全波
    TEST_ASSERT(wait_until(
                    [&] {
                        for (const auto& f :
                             take_frames(egress, static_cast<std::uint16_t>(
                                                      protocol::MessageType::SQUAD_STATE))) {
                            const auto st =
                                protocol::MessageCodec::decodeBody<protocol::SquadState>(
                                    f.body);
                            if (st.partyId == party_id && st.phase == 3) {
                                monster_ids.insert(st.monsters.begin(),
                                                   st.monsters.end());
                            }
                        }
                        return monster_ids.size() >= 3;
                    },
                    std::chrono::seconds(3)),
                "instance running with host wave");
    TEST_ASSERT(monster_ids.size() == 3, "host goal wave spawned");

    // 共存期隔离面：路由不变、临时 Scene 存活、旁观者照常且收不到副本流量
    TEST_ASSERT(route_count.load() == 2, "routes intact while instance live");
    TEST_ASSERT(instance_scene_present.load(), "temp scene alive while running");
    TEST_ASSERT(cell.submitIntent(make_move_intent(900, 88)), "observer move accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 88) >= 1; },
                           std::chrono::seconds(3)),
                "observer in declared space still served");
    {
        std::size_t squad_frames_on_observer = 0;
        std::lock_guard<std::mutex> lock(egress.mutex);
        for (const auto& f : egress.frames) {
            if (f.first != 88 || f.second.size() < sizeof(protocol::MessageHeader)) {
                continue;
            }
            const auto h = protocol::MessageCodec::parseHeader(f.second);
            const auto t = static_cast<protocol::MessageType>(h.type);
            if (t == protocol::MessageType::PARTY_RESULT ||
                t == protocol::MessageType::SQUAD_RESULT ||
                t == protocol::MessageType::SQUAD_STATE) {
                ++squad_frames_on_observer;
            }
        }
        TEST_ASSERT(squad_frames_on_observer == 0,
                    "instance traffic never reaches space 7 observer");
    }

    // 击杀满额（三怪）→ 目标达成 → 结算 → Destroyed 终态回执 + 绑定释放
    for (const auto monster : monster_ids) {
        protocol::SquadKill kill;
        kill.partyId = party_id;
        kill.monsterEntityId = monster;
        TEST_ASSERT(cell.submitIntent(make_squad_intent(
                        protocol::MessageType::SQUAD_KILL,
                        protocol::MessageCodec::encode(kill, 77), 77)),
                    "squad kill accepted");
        TEST_ASSERT(wait_frame<protocol::SquadResult>(
                        egress,
                        [&](const protocol::SquadResult& r, std::uint64_t s) {
                            return s == 77 &&
                                   r.op == static_cast<std::int32_t>(
                                               protocol::SquadOp::SQUAD_KILL) &&
                                   r.code == 0 && r.partyId == party_id;
                        },
                        std::chrono::seconds(3)),
                    "kill acked");
    }
    TEST_ASSERT(wait_frame<protocol::SquadState>(
                    egress,
                    [&](const protocol::SquadState& st, std::uint64_t) {
                        return st.partyId == party_id && st.phase == 7;
                    },
                    std::chrono::seconds(5)),
                "destroyed final state broadcast to party");
    TEST_ASSERT(wait_until([&] { return !binding_alive.load(); },
                           std::chrono::seconds(3)),
                "binding released after goal settle");
    TEST_ASSERT(wait_until([&] { return !instance_scene_present.load(); },
                           std::chrono::seconds(3)),
                "temp scene destroyed after settle (reclaimed)");

    // 回收边界：只收临时 Scene——声明 Space 路由/scene/旁观者原样
    TEST_ASSERT(route_count.load() == 2, "declared space route untouched by teardown");
    TEST_ASSERT(cell.find_scene(aux_scene) != nullptr, "space 7 scene survives");
    TEST_ASSERT(cell.resolveIntentPlayer(88) == 900, "observer session intact");
    const auto observer_moves = move_frame_count(egress, 88);
    TEST_ASSERT(cell.submitIntent(make_move_intent(900, 88)),
                "observer move after settle accepted");
    TEST_ASSERT(wait_until([&] { return move_frame_count(egress, 88) > observer_moves; },
                           std::chrono::seconds(3)),
                "observer still live after instance teardown");

    cell.stop();
    return true;
}

// L100 承接切片后续接线（批㉕ 收尾半片）：声明名回查 + 分配（962）应用面
// 装配——声明 Space 经撤路由→重认领重建时保留声明名，不静默回退成
// space-<id> 缺省名。声明名回查/装配是纯 config 域（无内核态）；撤路由与
// 重建在 post_tick（内核线程上下文）内做并读 scene 名，测试线程不碰内核态。
bool test_declared_space_reassignment_keeps_name() {
    world_app::WorldAppConfig config;
    config.tickRateMs = 20;
    config.spaces = {{7, "aux"}};
    world_app::CellApp cell(config);
    FakeEgress egress;
    cell.set_egress(&egress);

    // 声明名回查面：已声明命中；默认/未知返回空（调用方走缺省名）
    TEST_ASSERT(world_app::declared_space_name(config, 7) == "aux",
                "declared name resolved");
    TEST_ASSERT(world_app::declared_space_name(config, 1).empty(),
                "default space undeclared");
    TEST_ASSERT(world_app::declared_space_name(config, 99).empty(),
                "unknown space undeclared");

    // 分配（962）应用面装配：声明名保留；未声明走 space-<id> 缺省
    const auto desc7 =
        world_app::space_descriptor_for(
            config, 7, world_app::declared_space_name(config, 7));
    TEST_ASSERT(desc7.map_name == "aux", "assignment descriptor carries declared name");
    const auto desc99 =
        world_app::space_descriptor_for(
            config, 99, world_app::declared_space_name(config, 99));
    TEST_ASSERT(desc99.map_name == "space-99", "assignment descriptor default fallback");

    // 撤路由→重认领（962 重建路径同式）：重建 scene 名仍为声明名（非
    // space-7 缺省）。engine：先撤一次，下一拍按 desc7 重建。
    std::mutex rebuilt_mutex;
    std::string rebuilt_name;
    std::atomic<int> step{0};  // 0=待撤 1=已撤待重建 2=已重建
    cell.set_post_tick([&](double) {
        const int s = step.load();
        if (s == 0) {
            if (cell.scene_id_of_space(7) != cell.default_scene_id()) {
                const auto removed = cell.remove_space(7);
                if (removed != 0) {
                    step.store(1);
                }
                return;
            }
            return;  // 未知/兜底必读默认 scene——声明路由未即显形，等拍
        }
        if (s == 1) {
            // 路由已撤（remove 后 scene_id_of_space 回落默认 scene）；直接按
            // 声明名装配的 descriptor 重建（962 应用面同式）。
            const auto sid = cell.add_space(7, desc7);
            auto* scene = cell.find_scene(sid);
            std::lock_guard<std::mutex> lock(rebuilt_mutex);
            rebuilt_name = scene ? scene->get_name() : "";
            step.store(2);
            return;
        }
        // s == 2：已重建，后续拍不再动
    });
    cell.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    TEST_ASSERT(step.load() >= 1, "declared space revoked (empty scene)");
    TEST_ASSERT(wait_until([&] { return step.load() == 2; },
                           std::chrono::seconds(3)),
                "declared space re-added after revoke");
    {
        std::lock_guard<std::mutex> lock(rebuilt_mutex);
        TEST_ASSERT(rebuilt_name == "aux", "rebuilt scene keeps declared name");
    }

    cell.stop();
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
        {"cell_kernel_multi_space_routing_and_isolation",
         test_cell_kernel_multi_space_routing_and_isolation},
        {"cell_kernel_cross_space_transfer", test_cell_kernel_cross_space_transfer},
        {"cell_kernel_unknown_space_falls_back_to_default",
         test_cell_kernel_unknown_space_falls_back_to_default},
        {"config_declared_spaces", test_config_declared_spaces},
        {"cell_kernel_freeze_face", test_cell_kernel_freeze_face},
        {"cell_kernel_remove_space", test_cell_kernel_remove_space},
        {"cell_kernel_pending_revoke_retry", test_cell_kernel_pending_revoke_retry},
        {"cell_kernel_cancel_space_revoke", test_cell_kernel_cancel_space_revoke},
        {"declared_space_squad_flow_isolation", test_declared_space_squad_flow_isolation},
        {"declared_space_reassignment_keeps_name",
         test_declared_space_reassignment_keeps_name},
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
