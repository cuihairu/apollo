// Scene 运行时容器单测（P0-3 产物验证）。
//
// 覆盖（object-model §2.5 / 任务书 §8 口径）：
//   1. Scene 拥有实体集合（承接 EntityManager 职责，行为等价迁移）；
//   2. 玩家进出 API（enter/leave）：Avatar 进 scene 生、出 scene 死；
//      重复进入拒绝；AOI 随进出联动；
//   3. Scene 拥有 AOI（SceneAoi，scene_id 隔离）；
//   4. tick 六阶段骨架（固定顺序 + 逐阶段计数可观察）。

#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/world/scene.hpp"

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

using apollo::game::core::Entity;
using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::world::Avatar;
using apollo::game::world::Scene;
using apollo::game::world::SceneAoi;
using apollo::game::world::SceneTickPhase;

class CountingEntity final : public Entity {
public:
    explicit CountingEntity(EntityId id) : Entity(id, "counting") {}

    void on_update(float delta_time) override {
        ++update_count;
        last_delta = delta_time;
    }

    int update_count = 0;
    float last_delta = 0.0f;
};

bool test_scene_owns_entities() {
    Scene scene(1, "field");

    auto entity = std::make_shared<CountingEntity>(EntityId(10));
    scene.spawn_entity(entity);
    TEST_ASSERT(scene.get_entity_count() == 1, "entity registered in scene");
    TEST_ASSERT(entity->update_count == 0, "spawn 不触发 update");
    TEST_ASSERT(scene.get_entity(EntityId(10)) == entity, "按 id 取回实体");

    scene.update(0.1f);
    TEST_ASSERT(entity->update_count == 1, "scene tick 驱动实体 update");
    TEST_ASSERT(entity->last_delta == 0.1f, "delta 透传");

    scene.despawn_entity(EntityId(10));
    TEST_ASSERT(scene.get_entity_count() == 0, "despawn 移除实体");
    TEST_ASSERT(scene.get_entity(EntityId(10)) == nullptr, "移除后取不到");
    return true;
}

bool test_scene_enter_leave_avatar() {
    Scene scene(2, "town");

    auto avatar = std::make_shared<Avatar>(PlayerId(7), EntityId(70), "Alice");
    TEST_ASSERT(scene.enter(avatar, {1.0f, 2.0f, 3.0f}), "enter 成功");
    TEST_ASSERT(scene.has_avatar(PlayerId(7)), "在场玩家可查");
    TEST_ASSERT(scene.avatar_count() == 1, "在场人数");
    TEST_ASSERT(scene.get_avatar(PlayerId(7)) == avatar, "按玩家取回 Avatar");
    TEST_ASSERT(avatar->scene_id() == 2, "Avatar 挂 scene 归属");
    TEST_ASSERT(avatar->state() == apollo::game::world::AvatarState::Active, "进 scene 生");
    TEST_ASSERT(avatar->position().x == 1.0f && avatar->position().z == 3.0f,
                "权威落点生效");
    TEST_ASSERT(scene.aoi().contains(EntityId(70)), "AOI 入列");

    TEST_ASSERT(!scene.enter(avatar, {0.0f, 0.0f, 0.0f}), "重复进入拒绝");
    TEST_ASSERT(scene.avatar_count() == 1, "重复进入无副作用");

    TEST_ASSERT(scene.leave(PlayerId(7)), "leave 成功");
    TEST_ASSERT(!scene.has_avatar(PlayerId(7)), "离场可查");
    TEST_ASSERT(avatar->scene_id() == 0, "出 scene 归属解除");
    TEST_ASSERT(avatar->state() == apollo::game::world::AvatarState::Leaving, "出 scene 死前态");
    TEST_ASSERT(!scene.aoi().contains(EntityId(70)), "AOI 出列");
    TEST_ASSERT(!scene.leave(PlayerId(7)), "重复 leave 拒绝");

    // 进场顺序可观察（多玩家）
    auto a1 = std::make_shared<Avatar>(PlayerId(1), EntityId(11), "A");
    auto a2 = std::make_shared<Avatar>(PlayerId(2), EntityId(12), "B");
    scene.enter(a1, {});
    scene.enter(a2, {});
    const auto& order = scene.avatars();
    TEST_ASSERT(order.size() == 2 && order[0] == PlayerId(1) && order[1] == PlayerId(2),
                "进场顺序保序");
    return true;
}

bool test_scene_owns_aoi() {
    Scene scene(3, "aoi-scene");
    scene.aoi() = SceneAoi(100.0f, 100.0f, 10.0f, 20.0f);

    auto a1 = std::make_shared<Avatar>(PlayerId(1), EntityId(11), "A");
    auto a2 = std::make_shared<Avatar>(PlayerId(2), EntityId(12), "B");
    scene.enter(a1, {10.0f, 0.0f, 10.0f});
    scene.enter(a2, {15.0f, 0.0f, 12.0f});

    const auto viewers = scene.aoi().viewers_of(EntityId(11));
    TEST_ASSERT(viewers.size() == 2, "半径内互相可见（含自身）");
    TEST_ASSERT(scene.aoi().entity_count() == 2, "AOI 集合计数");

    scene.leave(PlayerId(2));
    TEST_ASSERT(scene.aoi().entity_count() == 1, "离场自动出 AOI");

    // 远处实体不可见
    auto a3 = std::make_shared<Avatar>(PlayerId(3), EntityId(13), "C");
    scene.enter(a3, {90.0f, 0.0f, 90.0f});
    const auto viewers2 = scene.aoi().viewers_of(EntityId(11));
    TEST_ASSERT(viewers2.size() == 1, "半径外不可见");
    return true;
}

bool test_scene_tick_six_phases() {
    Scene scene(4, "phases");
    TEST_ASSERT(scene.tick_count() == 0, "初始未 tick");

    scene.tick(0.1);
    TEST_ASSERT(scene.tick_count() == 1, "tick 计数");

    // 六阶段固定顺序（concurrency.md：simulate→recalc→aoidecay→collect→
    // budget+flush→persist-batch）；每 tick 各阶段恰好一次
    TEST_ASSERT(scene.phase_count(SceneTickPhase::Simulate) == 1, "simulate 阶段计数");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::Recalc) == 1, "recalc 阶段计数");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::AoiDecay) == 1, "aoidecay 阶段计数");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::Collect) == 1, "collect 阶段计数");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::BudgetFlush) == 1, "budget+flush 阶段计数");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::PersistBatch) == 1, "persist-batch 阶段计数");

    scene.tick(0.1);
    scene.tick(0.1);
    TEST_ASSERT(scene.tick_count() == 3, "tick 累计");
    TEST_ASSERT(scene.phase_count(SceneTickPhase::Simulate) == 3, "阶段随 tick 累计");

    // to_string 可观察（阶段名）
    TEST_ASSERT(apollo::game::world::to_string(SceneTickPhase::Simulate) == "simulate",
                "阶段名字面");
    TEST_ASSERT(apollo::game::world::to_string(SceneTickPhase::BudgetFlush) == "budget+flush",
                "阶段名字面（budget+flush）");
    return true;
}

bool test_scene_id_sentinel() {
    Scene unnamed;  // 旧构造兼容（name-only，scene_id=0 哨兵）
    TEST_ASSERT(unnamed.scene_id() == 0, "未赋 id 场景哨兵 0");
    TEST_ASSERT(unnamed.get_name() == "DefaultScene", "默认名保留");

    Scene named(9, "nine");
    TEST_ASSERT(named.scene_id() == 9, "带 id 构造");
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
        {"scene_owns_entities", test_scene_owns_entities},
        {"scene_enter_leave_avatar", test_scene_enter_leave_avatar},
        {"scene_owns_aoi", test_scene_owns_aoi},
        {"scene_tick_six_phases", test_scene_tick_six_phases},
        {"scene_id_sentinel", test_scene_id_sentinel},
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

    std::cout << "SceneTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}