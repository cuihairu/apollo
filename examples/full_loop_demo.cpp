// 全流程最小链路驱动（P4-2，任务书 §29：login→lobby→create instance→enter
// →spawn→AOI→battle→reward→leave）。
//
// 形态：进程内单线程驱动六模块真件（benchmark 同款直编惯例，不依赖
// GAME_MODULE target）——登录锚（session/AnchorManager）→ 副本八态生命周期
// （world/Instance）→ 进场 + AOI 事件流（world/Scene+SceneAoi）→ 五段战斗
// （battle/BattleRuntime）→ 奖励落账（session/AnchorRewardSink→Anchor 长期态
// mark_dirty）。跨进程接线（网关/login 协议栈、G-1 编队）不在本例——归
// P3-1 G-1 收尾批与 net M1。
//
// 验证器口径：每步带断言门（失败即退出非零），可作 P0-P2 交付面的组合回归。

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_aoi.hpp"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

using apollo::game::battle::BattleInput;
using apollo::game::battle::BattlePhase;
using apollo::game::battle::BattleRuntime;
using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::session::AnchorManager;
using apollo::game::session::AnchorRewardSink;
using apollo::game::world::Avatar;
using apollo::game::world::AvatarPtr;
using apollo::game::world::Instance;
using apollo::game::world::Scene;
using apollo::game::world::SceneAoi;

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                                     \
    do {                                                                                     \
        if (cond) {                                                                          \
            std::cout << "  [OK] " << (msg) << std::endl;                                    \
        } else {                                                                             \
            std::cout << "  [FAIL] " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"  \
                      << std::endl;                                                          \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

void step(const char* name) {
    std::cout << "\n== " << name << " ==" << std::endl;
}

} // namespace

int main() {
    constexpr std::uint64_t kPlayer = 1001;     // 主角（登录→战斗→奖励全程）
    constexpr std::uint64_t kObserver = 1002;   // 场景观察者（AOI 事件视角）
    constexpr float kWorld = 100.0f;            // 100² 世界（模型 B 同款）
    constexpr std::uint64_t kSeed = 42;

    AnchorManager anchors;
    std::uint64_t enter_events = 0;
    std::uint64_t sync_events = 0;
    std::uint64_t leave_events = 0;

    // ---- 1. login：登录锚激活（session 长期态在场） ----
    step("1. login（AnchorManager::activate）");
    auto anchor = anchors.activate(kPlayer);
    CHECK(anchor != nullptr, "锚激活");
    CHECK(anchors.find(kPlayer) == anchor, "重登幂等（同锚）");

    // ---- 2. lobby：建副本（八态前两段 Create→Initialize→Waiting） ----
    step("2. lobby + create instance（八态 Create→Waiting）");
    Instance instance(1, 1, "demo-instance");
    CHECK(!instance.enter(PlayerId{kPlayer}), "Create 态进场拒绝（放行面仅 Waiting/Running）");
    CHECK(instance.initialize() && instance.ready(), "initialize→ready");
    CHECK(instance.state() == Instance::State::Waiting, "等待态");

    // ---- 3. 装载玩法负载（挂接窗口 = Waiting；奖励单向落锚） ----
    step("3. attach battle（IRewardSink→Anchor 单向落账口）");
    AnchorRewardSink sink(*anchor);
    auto battle = std::make_unique<BattleRuntime>(1, kSeed, &sink);
    CHECK(instance.attach_battle(std::move(battle)), "Waiting 窗口挂接");
    CHECK(instance.battle() != nullptr, "副本持有玩法负载");

    // ---- 4. enter：副本进场 + 场景落点（观察者先行，主视角后进） ----
    step("4. enter instance + spawn to scene");
    CHECK(instance.enter(PlayerId{kObserver}), "观察者预进（Waiting 可进）");
    CHECK(instance.enter(PlayerId{kPlayer}), "主角预进");

    Scene scene(1, kWorld > 0 ? "demo-scene" : "");
    scene.aoi() = SceneAoi(kWorld, kWorld, 25.0f, 20.0f);
    scene.aoi().set_event_sink([&](const SceneAoi::Event& e) {
        switch (e.kind) {
        case SceneAoi::Event::Kind::Enter: ++enter_events; break;
        case SceneAoi::Event::Kind::Sync: ++sync_events; break;
        case SceneAoi::Event::Kind::Leave: ++leave_events; break;
        }
    });

    auto observer = std::make_shared<Avatar>(PlayerId(kObserver), EntityId(kObserver),
                                             "observer");
    CHECK(scene.enter(observer, SceneAoi::Vec3{50.0f, 0.0f, 50.0f}), "观察者落点");

    auto avatar = std::make_shared<Avatar>(PlayerId(kPlayer), EntityId(kPlayer), "hero");
    CHECK(scene.enter(avatar, SceneAoi::Vec3{40.0f, 0.0f, 50.0f}), "主角落点（观察者视野内）");
    CHECK(enter_events == 1, "观察者视角 Enter×1（主角进视野）");

    // ---- 5. move + AOI：匀速漂移差集（Sync 事件=广播消息量口径） ----
    step("5. move + AOI diff（Enter/Sync/Leave 计数）");
    for (int f = 0; f < 3; ++f) {
        scene.aoi().move(EntityId(kPlayer),
                         SceneAoi::Vec3{40.0f + static_cast<float>(f + 1), 0.0f, 50.0f});
    }
    CHECK(sync_events >= 1, "主角移动产生 Sync 差集");
    CHECK(leave_events == 0, "视野内未出界无 Leave");

    // ---- 6. battle：五段中段（start 内部 begin；判定域显式 tick 推进） ----
    // 骨架期口径：参战者收集由 Instance::enter 转发（收集期 Created/Entering）；
    // Instance::start() 内部校验参战非空并调 battle.begin()；tick_index 空间
    // 归 Instance::tick（内部驱动 battle.tick），显式 battle->tick 带输入是
    // 骨架期唯一注入面——两者不可混用（抢拍）。本例走显式输入面，
    // instance.tick 驱动随输入通道批接线。
    step("6. battle（Waiting→start→Running；tick×5 带 opcode=1 意图）");
    CHECK(instance.start(), "start→Running（参战者收集完成校验在 start 内）");
    CHECK(instance.battle()->phase() == BattlePhase::Battling, "begin 已随 start 完成");
    CHECK(!instance.battle()->enter_player(kPlayer), "Battling 起进人只观战不参战");
    CHECK(!instance.attach_battle(nullptr), "Running 起挂接拒绝");
    for (std::uint32_t t = 0; t < 5; ++t) {
        std::vector<BattleInput> inputs;
        inputs.push_back(BattleInput{t, kPlayer, 1}); // opcode 1=攻击
        CHECK(instance.battle()->tick(t, inputs), "tick 推进");
    }
    CHECK(instance.battle()->hash_chain() != 0, "hash 链滚动（确定性指纹）");
    CHECK(instance.tick_count() == 0, "副本 tick 计数（显式判定域驱动不经 instance.tick）");

    // ---- 7. reward：结算落账（battle→Anchor 长期态 mark_dirty） ----
    step("7. reward + settle（五段尾 Rewarding→Finished；exp 落账）");
    CHECK(instance.battle()->finish(), "finish 结算");
    CHECK(instance.battle()->phase() == BattlePhase::Finished, "散场终态");
    CHECK(anchor->progress(AnchorRewardSink::kExpKey) > 0, "exp 落账（长期态）");
    CHECK(!anchor->dirty_reasons().empty(), "长期态变更即脏（随 SaveQueue 落档）");

    // ---- 8. leave：出视野 + 副本八态走完 + 登出 ----
    step("8. leave（AOI Leave→finish→settle→drain→destroy→deactivate）");
    const auto leave_before = leave_events;
    CHECK(scene.leave(PlayerId{kObserver}), "观察者离场");
    CHECK(leave_events > leave_before, "Leave 差集（离场双向事件：离场者视角+他人视角）");
    CHECK(instance.leave(PlayerId{kPlayer}) && instance.leave(PlayerId{kObserver}), "副本出场");
    CHECK(instance.finish() && instance.settle() && instance.drain() && instance.destroy(),
          "八态尾四段单向推进");
    CHECK(instance.is_destroyed(), "Destroyed 终态封死");
    anchors.deactivate(kPlayer);
    CHECK(anchors.find(kPlayer) == nullptr, "登出锚回收");

    // ---- 总结 ----
    std::cout << "\n全链路（login→lobby→instance→enter→spawn→AOI→battle→reward→leave）："
              << (g_failures == 0 ? "全部通过" : "存在失败") << std::endl;
    std::cout << "AOI 事件：Enter=" << enter_events << " Sync=" << sync_events
              << " Leave=" << leave_events << std::endl;
    return g_failures == 0 ? 0 : 1;
}
