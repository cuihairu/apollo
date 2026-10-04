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