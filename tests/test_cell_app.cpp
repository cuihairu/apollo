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
                                              std::uint64_t to_space) {
    protocol::CellCrossBorder msg;
    msg.entityId = entity_id;
    msg.fromSpace = 1;
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
