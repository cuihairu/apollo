// Instance 八态生命周期单测（P0-3 产物验证）。
//
// 覆盖（任务书 §9 / improvement-plan P0-3 口径）：
//   1. 八态全序推进：Create→Initialize→Waiting→Running→Finishing→Rewarding→
//      Draining→Destroyed 逐态可达；
//   2. 转换校验表：回退/跳段/终态后转换全部拒绝且状态不变；
//   3. 玩家进出：仅 Waiting/Running 放行；重复进入拒绝；离场可查；
//   4. 玩法节奏：Running 态才累计 tick/时长。

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/instance.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::game::core::PlayerId;
using apollo::game::world::Instance;
using State = Instance::State;

bool test_instance_full_lifecycle_progression() {
    Instance instance(1, 100, "run-1");
    TEST_ASSERT(instance.state() == State::Create, "初始 Create");
    TEST_ASSERT(instance.id() == 1, "实例 id");
    TEST_ASSERT(instance.scene_id() == 100, "绑定 scene");
    TEST_ASSERT(instance.name() == "run-1", "实例名");

    TEST_ASSERT(instance.initialize(), "Create→Initialize");
    TEST_ASSERT(instance.state() == State::Initialize, "状态推进");

    TEST_ASSERT(instance.ready(), "Initialize→Waiting");
    TEST_ASSERT(instance.start(), "Waiting→Running");
    TEST_ASSERT(instance.finish(), "Running→Finishing");
    TEST_ASSERT(instance.settle(), "Finishing→Rewarding");
    TEST_ASSERT(instance.drain(), "Rewarding→Draining");
    TEST_ASSERT(instance.destroy(), "Draining→Destroyed");

    TEST_ASSERT(instance.is_destroyed(), "终态可查");
    TEST_ASSERT(Instance::to_string(instance.state()) == "Destroyed", "状态名可观察");
    return true;
}

bool test_instance_transition_validation_table() {
    // 转换校验表：合法相邻推进
    TEST_ASSERT(Instance::is_transition_valid(State::Create, State::Initialize),
                "Create→Initialize 合法");
    TEST_ASSERT(Instance::is_transition_valid(State::Waiting, State::Running),
                "Waiting→Running 合法");
    TEST_ASSERT(Instance::is_transition_valid(State::Draining, State::Destroyed),
                "Draining→Destroyed 合法");

    // 非法：跳段
    TEST_ASSERT(!Instance::is_transition_valid(State::Create, State::Running),
                "Create→Running 跳段拒绝");
    TEST_ASSERT(!Instance::is_transition_valid(State::Create, State::Destroyed),
                "Create→Destroyed 跳段拒绝");
    TEST_ASSERT(!Instance::is_transition_valid(State::Running, State::Rewarding),
                "Running→Rewarding 跳段拒绝");

    // 非法：回退（八态单向）
    TEST_ASSERT(!Instance::is_transition_valid(State::Running, State::Waiting),
                "Running→Waiting 回退拒绝");
    TEST_ASSERT(!Instance::is_transition_valid(State::Destroyed, State::Create),
                "Destroyed→Create 回退拒绝");

    // 非法：终态封死
    TEST_ASSERT(!Instance::is_transition_valid(State::Destroyed, State::Destroyed),
                "Destroyed 自环拒绝");

    // 实例行为面：非法调用不改变状态
    Instance instance(2, 200, "run-2");
    TEST_ASSERT(!instance.start(), "Create 直接 start 拒绝");
    TEST_ASSERT(instance.state() == State::Create, "状态保持 Create");
    TEST_ASSERT(!instance.destroy(), "Create 直接 destroy 拒绝");
    TEST_ASSERT(!instance.is_destroyed(), "未到终态");
    return true;
}

bool test_instance_enter_windows() {
    Instance instance(3, 300, "run-3");
    const PlayerId p1(1);
    const PlayerId p2(2);

    // Create/Initialize 态不可进
    TEST_ASSERT(!instance.enter(p1), "Create 态拒绝进入");

    instance.initialize();
    TEST_ASSERT(!instance.enter(p1), "Initialize 态拒绝进入");

    // Waiting（预进窗口）可进
    instance.ready();
    TEST_ASSERT(instance.enter(p1), "Waiting 态可预进");
    TEST_ASSERT(instance.has_player(p1), "在场可查");
    TEST_ASSERT(instance.player_count() == 1, "人数");

    // Running 进行中可进
    instance.start();
    TEST_ASSERT(instance.enter(p2), "Running 态可进");

    // 重复进入拒绝
    TEST_ASSERT(!instance.enter(p1), "重复进入拒绝");
    TEST_ASSERT(instance.player_count() == 2, "重复进入无副作用");

    // Finishing 起闭门
    instance.finish();
    TEST_ASSERT(!instance.enter(PlayerId(3)), "Finishing 态闭门");

    // 已在场者可离场（结算期退场）
    TEST_ASSERT(instance.leave(p1), "Finishing 态可离场");
    TEST_ASSERT(!instance.has_player(p1), "离场可查");

    // 保序玩家清单
    TEST_ASSERT(instance.players().size() == 1 && instance.players()[0] == p2,
                "玩家清单保序");
    return true;
}

bool test_instance_tick_only_when_running() {
    Instance instance(4, 400, "run-4");
    instance.initialize();
    instance.ready();
    instance.start();

    instance.tick(0.1);
    instance.tick(0.1);
    instance.tick(0.1);
    TEST_ASSERT(instance.tick_count() == 3, "Running 态累计 tick");
    TEST_ASSERT(instance.elapsed_seconds() > 0.29 && instance.elapsed_seconds() < 0.31,
                "时长累计（浮点容差）");

    // 结算期不再累计玩法时长
    const auto ticks_at_finish = instance.tick_count();
    instance.finish();
    instance.tick(0.1);
    instance.settle();
    instance.tick(0.1);
    TEST_ASSERT(instance.tick_count() == ticks_at_finish, "非 Running 态 tick 不累计");
    return true;
}

bool test_instance_invalid_player_rejected() {
    Instance instance(5, 500, "run-5");
    instance.initialize();
    instance.ready();
    instance.start();

    TEST_ASSERT(!instance.enter(PlayerId{}), "无效玩家 id 拒绝进入");
    TEST_ASSERT(instance.player_count() == 0, "无副作用");
    return true;
}

// ---- P2-2：instance×battle 挂接闭环（create→attach→enter→battle→reward）----

// 内存奖励账本（IRewardSink 骨架实现；真 Anchor 落账随 P2-4）
struct MemoryRewardSink : apollo::game::battle::IRewardSink {
    std::vector<std::pair<std::uint64_t, std::int64_t>> payouts;

    void on_reward(std::uint64_t player_id, std::int64_t score) override {
        payouts.emplace_back(player_id, score);
    }
};

bool test_instance_battle_hangup_window() {
    Instance instance(6, 600, "run-6");
    auto sink = std::make_unique<MemoryRewardSink>();
    auto* sink_ptr = sink.get();
    auto battle = std::make_unique<apollo::game::battle::BattleRuntime>(
        601, 777, sink_ptr);

    instance.initialize();
    instance.ready();
    TEST_ASSERT(instance.attach_battle(std::move(battle)), "Waiting 前挂接成功");
    TEST_ASSERT(instance.battle() != nullptr, "挂接可观察");
    TEST_ASSERT(!instance.attach_battle(
        std::make_unique<apollo::game::battle::BattleRuntime>(602, 777)),
        "重复挂接拒绝");

    TEST_ASSERT(instance.enter(PlayerId{2001}), "准入（参战同步）");
    TEST_ASSERT(instance.start(), "start 推进（battle begin 随行）");
    TEST_ASSERT(instance.battle()->phase() ==
                    apollo::game::battle::BattlePhase::Battling,
                "battle 进入 Battling");

    // Running 起挂接拒绝（开局装载窗口封死）
    Instance running(7, 700, "run-7");
    running.initialize();
    running.ready();
    TEST_ASSERT(running.start(), "无负载 instance 直接 start");
    TEST_ASSERT(!running.attach_battle(
        std::make_unique<apollo::game::battle::BattleRuntime>(701, 777)),
        "Running 态挂接拒绝");

    // start 前置校验：空参战者 battle.begin 失败 → instance start 失败不推进
    Instance empty(8, 800, "run-8");
    empty.initialize();
    empty.ready();
    TEST_ASSERT(empty.attach_battle(
        std::make_unique<apollo::game::battle::BattleRuntime>(801, 777)),
        "空场挂接成功");
    TEST_ASSERT(!empty.start(), "无参战者 start 失败");
    TEST_ASSERT(empty.state() == State::Waiting, "状态不推进（可观察可校验）");
    return true;
}

bool test_instance_battle_tick_loop() {
    Instance instance(9, 900, "run-9");
    auto sink = std::make_unique<MemoryRewardSink>();
    auto* sink_ptr = sink.get();

    instance.initialize();
    instance.ready();
    TEST_ASSERT(instance.attach_battle(
        std::make_unique<apollo::game::battle::BattleRuntime>(
            901, 4242, sink_ptr)),
        "挂接");
    TEST_ASSERT(instance.enter(PlayerId{1001}), "玩家准入（参战同步）");
    TEST_ASSERT(instance.battle()->players().size() == 1, "参战者收集");
    TEST_ASSERT(instance.start(), "开局");

    const auto seed_hash = instance.battle()->hash_chain();
    for (int i = 0; i < 5; ++i) {
        instance.tick(0.1);
    }
    TEST_ASSERT(instance.tick_count() == 5, "instance tick 计数");
    TEST_ASSERT(instance.battle()->last_tick() == 5, "battle tick 随行");
    // 骨架期 instance.tick 无输入通道（玩家意图协议面随 P2-3）——无事件 tick
    // hash 链稳定不滚（事件级 hash 滚动由 battle_runtime_tests 覆盖）
    TEST_ASSERT(instance.battle()->hash_chain() == seed_hash,
                "无事件 tick hash 链稳定");

    // 确定性复算（四元组口径：同 seed 同 tick 数同 hash）
    Instance replay(10, 910, "run-10");
    auto replay_sink = std::make_unique<MemoryRewardSink>();
    replay.initialize();
    replay.ready();
    TEST_ASSERT(replay.attach_battle(
        std::make_unique<apollo::game::battle::BattleRuntime>(
            911, 4242, replay_sink.get())),
        "复算挂接");
    TEST_ASSERT(replay.enter(PlayerId{1001}), "复算准入");
    TEST_ASSERT(replay.start(), "复算开局");
    for (int i = 0; i < 5; ++i) {
        replay.tick(0.1);
    }
    TEST_ASSERT(replay.battle()->hash_chain() == instance.battle()->hash_chain(),
                "同四元组同 hash 链（复算回放判定）");

    TEST_ASSERT(instance.finish(), "散场");
    TEST_ASSERT(instance.battle()->phase() ==
                    apollo::game::battle::BattlePhase::Finished,
                "battle 五段走完");
    TEST_ASSERT(sink_ptr->payouts.size() == 1, "奖励单向落账（sink 收到）");
    TEST_ASSERT(sink_ptr->payouts[0].first == 1001, "落账 player_id 正确");
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"instance_full_lifecycle_progression", test_instance_full_lifecycle_progression},
        {"instance_transition_validation_table", test_instance_transition_validation_table},
        {"instance_enter_windows", test_instance_enter_windows},
        {"instance_tick_only_when_running", test_instance_tick_only_when_running},
        {"instance_invalid_player_rejected", test_instance_invalid_player_rejected},
        {"instance_battle_hangup_window", test_instance_battle_hangup_window},
        {"instance_battle_tick_loop", test_instance_battle_tick_loop},
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

    std::cout << "InstanceTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}