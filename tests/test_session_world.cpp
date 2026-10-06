#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"

#include <iostream>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool test_anchor_manager_activate_and_find() {
    std::cout << "Running: test_anchor_manager_activate_and_find..." << std::endl;

    apollo::game::session::AnchorManager manager;
    auto anchor = manager.activate(1001);

    TEST_ASSERT(anchor != nullptr, "anchor created");
    TEST_ASSERT(anchor->player_id() == 1001, "player id assigned");
    TEST_ASSERT(manager.anchor_count() == 1, "anchor count");
    TEST_ASSERT(manager.find(1001) == anchor, "find returns same anchor");

    anchor->mark_dirty("load");
    TEST_ASSERT(anchor->needs_save(), "dirty tracked");

    // snapshot（P0-4）：flush 遍历面
    const auto snapshot = manager.snapshot();
    TEST_ASSERT(snapshot.size() == 1, "snapshot size");
    TEST_ASSERT(snapshot[0] == anchor, "snapshot carries anchor");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_session_locator_bind_and_unbind() {
    std::cout << "Running: test_session_locator_bind_and_unbind..." << std::endl;

    apollo::game::session::SessionLocator locator;
    apollo::game::session::SessionBinding binding;
    binding.session_id = 2001;
    binding.gateway_id = 7;
    binding.gateway_addr = "gateway-01";

    locator.bind(1001, binding);

    const auto by_player = locator.find_by_player(1001);
    TEST_ASSERT(by_player.has_value(), "binding by player exists");
    TEST_ASSERT(by_player->session_id == 2001, "session id preserved");

    const auto by_session = locator.find_player_by_session(2001);
    TEST_ASSERT(by_session.has_value(), "player by session exists");
    TEST_ASSERT(*by_session == 1001, "player id preserved");

    locator.unbind_session(2001);
    TEST_ASSERT(!locator.find_by_player(1001).has_value(), "binding removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_world_session_manager_lifecycle() {
    std::cout << "Running: test_world_session_manager_lifecycle..." << std::endl;

    apollo::game::world::WorldSessionManager manager;
    auto session = manager.create_session(3001, apollo::game::core::PlayerId(4001));

    TEST_ASSERT(session != nullptr, "session created");
    session->assign_world(9);
    session->assign_map_instance(12);
    session->assign_space(21);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    TEST_ASSERT(manager.session_count() == 1, "session count");
    TEST_ASSERT(manager.find_session(3001) == session, "find by session");
    TEST_ASSERT(manager.find_by_player(apollo::game::core::PlayerId(4001)) == session,
                "find by player");
    TEST_ASSERT(session->world_id() == 9, "world assigned");
    TEST_ASSERT(session->map_instance_id() == 12, "instance assigned");
    TEST_ASSERT(session->space_id() == 21, "space assigned");

    // 态校验（P0-4）：非本态迁移拒绝且状态不变
    TEST_ASSERT(manager.resume_session(3001) == nullptr, "resume on Active rejected");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "state unchanged after rejected resume");

    manager.suspend_session(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Suspended, "session suspended");
    TEST_ASSERT(manager.suspend_session(3001) == nullptr, "double suspend rejected");

    manager.resume_session(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active, "session resumed");

    manager.transfer_session(3001, 10, 13, 22, false);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::TransferringOut, "session transferring");
    TEST_ASSERT(session->pending_world_id() == 10, "pending world assigned");
    TEST_ASSERT(session->pending_map_instance_id() == 13, "pending instance assigned");
    TEST_ASSERT(session->pending_space_id() == 22, "pending space assigned");

    // 失败回滚分支（P0-4）：清 pending、回到转移前态
    TEST_ASSERT(manager.abort_transfer(3001) != nullptr, "abort accepted in transferring");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "state rolled back to Active");
    TEST_ASSERT(session->pending_world_id() == 0 && session->pending_map_instance_id() == 0 &&
                    session->pending_space_id() == 0,
                "pending cleared on abort");
    TEST_ASSERT(manager.abort_transfer(3001) == nullptr, "abort outside transferring rejected");

    // 重发转移后确认：字段切换
    TEST_ASSERT(manager.transfer_session(3001, 10, 13, 22, false) != nullptr, "re-transfer accepted");
    manager.complete_transfer(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active, "session active after transfer");
    TEST_ASSERT(session->world_id() == 10, "world switched after transfer");
    TEST_ASSERT(session->map_instance_id() == 13, "instance switched after transfer");
    TEST_ASSERT(session->space_id() == 22, "space switched after transfer");
    TEST_ASSERT(manager.complete_transfer(3001) == nullptr, "complete outside transferring rejected");

    // close 流程修正（P0-4）：Leaving 可观察窗口——会话仍驻留可查
    TEST_ASSERT(manager.close_session(3001) != nullptr, "close accepted");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Leaving,
                "Leaving observable");
    TEST_ASSERT(manager.session_count() == 1, "session retained in Leaving");
    TEST_ASSERT(manager.find_session(3001) == session, "findable in Leaving");
    TEST_ASSERT(manager.close_session(3001) == nullptr, "double close rejected");
    TEST_ASSERT(manager.resume_session(3001) == nullptr, "resume on Leaving rejected");

    // 显式终结：Leaving → Closed 并摘除索引
    TEST_ASSERT(manager.finalize_session(3001) != nullptr, "finalize accepted");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Closed, "Closed terminal");
    TEST_ASSERT(manager.session_count() == 0, "session removed");
    TEST_ASSERT(manager.find_session(3001) == nullptr, "find after finalize misses");
    TEST_ASSERT(manager.finalize_session(3001) == nullptr, "finalize on missing rejected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_world_create_scene_and_instance() {
    std::cout << "Running: test_world_create_scene_and_instance..." << std::endl;

    apollo::game::world::World world;

    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 1;
    descriptor.map_name = "test-map";
    descriptor.width = 100.0f;
    descriptor.height = 100.0f;

    auto* scene = world.create_scene(descriptor);
    TEST_ASSERT(scene != nullptr, "scene created from descriptor");
    TEST_ASSERT(scene->scene_id() != 0, "scene id assigned (non-sentinel)");
    TEST_ASSERT(scene->get_name() == "test-map", "scene name from descriptor");
    TEST_ASSERT(world.scene_count() == 1, "scene count");
    TEST_ASSERT(world.find_scene(scene->scene_id()) == scene, "find scene");

    // 无效 descriptor 拒绝建场
    apollo::game::world::SceneDescriptor invalid;
    TEST_ASSERT(world.create_scene(invalid) == nullptr, "invalid descriptor rejected");

    auto instance = world.create_instance(scene->scene_id(), "test-run");
    TEST_ASSERT(instance != nullptr, "instance created on scene");
    TEST_ASSERT(instance->id() != 0, "instance id assigned");
    TEST_ASSERT(instance->scene_id() == scene->scene_id(), "instance bound to scene");
    TEST_ASSERT(world.instance_count() == 1, "instance count");
    TEST_ASSERT(world.find_instance(instance->id()) == instance.get(), "find instance");

    // 不存在的 scene 拒绝建实例
    TEST_ASSERT(world.create_instance(99999) == nullptr, "instance on missing scene rejected");

    // 全场 tick 推进 scene
    world.tick(0.1);
    TEST_ASSERT(scene->tick_count() == 1, "world tick advances scene");

    world.destroy_scene(scene->scene_id());
    TEST_ASSERT(world.scene_count() == 0, "scene destroyed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

namespace {

// 双场最小布景（P1-1 换幕端到端）：两 scene + 各一 instance（推进至 Running）
struct TwoSceneFixture {
    apollo::game::world::World world;
    apollo::game::world::WorldSessionManager sessions;
    apollo::game::world::Scene* scene_a = nullptr;
    apollo::game::world::Scene* scene_b = nullptr;
    apollo::game::world::InstancePtr instance_a;
    apollo::game::world::InstancePtr instance_b;

    TwoSceneFixture() {
        apollo::game::world::SceneDescriptor descriptor;
        descriptor.map_id = 1;
        descriptor.map_name = "map-a";
        descriptor.width = 100.0f;
        descriptor.height = 100.0f;
        scene_a = world.create_scene(descriptor);

        descriptor.map_id = 2;
        descriptor.map_name = "map-b";
        scene_b = world.create_scene(descriptor);

        instance_a = world.create_instance(scene_a->scene_id(), "run-a");
        instance_a->initialize();
        instance_a->ready();
        instance_a->start();

        instance_b = world.create_instance(scene_b->scene_id(), "run-b");
        instance_b->initialize();
        instance_b->ready();
        instance_b->start();
    }
};

bool test_scene_transfer_end_to_end() {
    std::cout << "Running: test_scene_transfer_end_to_end..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7001);
    auto session = fx.sessions.create_session(6001, player);
    TEST_ASSERT(session != nullptr, "session created");

    // 进 A 场（attach 面与 cell_server 同构：Avatar 重建 + instance.enter）
    const auto entity = apollo::game::core::EntityId(9001);
    auto avatar = std::make_shared<apollo::game::world::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {1.0f, 2.0f, 3.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_world(1);
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->bind_avatar(entity);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // Anchor 投影源（Online 态 → Avatar 不进挂起窗口）
    apollo::game::session::PlayerAnchor anchor(player.value());
    anchor.set_state(apollo::game::session::AnchorState::Online);
    anchor.set_home_zone_id(42);

    // ---- 换幕：A → B ----
    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {10.0f, 20.0f, 30.0f};
    request.anchor = &anchor;

    const auto outcome =
        apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, request);
    TEST_ASSERT(outcome.ok(), "transfer ok");
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::Ok, "result ok");
    TEST_ASSERT(outcome.source_scene_id == fx.scene_a->scene_id(), "source scene recorded");

    // 差距 ②：对象投影——B 场重建 Avatar（新对象、AOI 入列、投影完成）
    TEST_ASSERT(fx.scene_a->avatar_count() == 0, "source scene emptied");
    TEST_ASSERT(fx.scene_b->avatar_count() == 1, "target scene holds avatar");
    const auto rebuilt = fx.scene_b->get_avatar(player);
    TEST_ASSERT(rebuilt != nullptr, "avatar rebuilt in target");
    TEST_ASSERT(rebuilt != avatar, "avatar is a new object (detach-attach)");
    TEST_ASSERT(rebuilt->scene_id() == fx.scene_b->scene_id(), "scene ownership moved");
    TEST_ASSERT(rebuilt->position().x == 10.0f && rebuilt->position().z == 30.0f,
                "landing position applied (易失态坐标不带走)");
    TEST_ASSERT(rebuilt->has_projection(), "anchor projection done");
    TEST_ASSERT(rebuilt->home_zone_id() == 42, "home zone projected");

    // 差距 ⑤：AOI 重建——AOI 出入列随 enter/leave 完成
    TEST_ASSERT(fx.scene_b->aoi().contains(entity), "aoi re-registered in target");

    // instance 成员随迁
    TEST_ASSERT(fx.instance_a->has_player(player) == false, "left source instance");
    TEST_ASSERT(fx.instance_b->has_player(player) == true, "joined target instance");

    // 会话面：Active + 字段切换（state sync）
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "session active after transfer");
    TEST_ASSERT(session->map_instance_id() == fx.instance_b->id(), "session instance switched");
    TEST_ASSERT(session->space_id() == fx.scene_b->scene_id(), "session space switched");
    TEST_ASSERT(session->route_version() == 1, "route version bumped");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_scene_transfer_prepare_rejections() {
    std::cout << "Running: test_scene_transfer_prepare_rejections..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7002);
    auto session = fx.sessions.create_session(6002, player);
    auto avatar = std::make_shared<apollo::game::world::Avatar>(player, apollo::game::core::EntityId(9002));
    TEST_ASSERT(fx.scene_a->enter(avatar, {}), "avatar entered");
    session->assign_space(fx.scene_a->scene_id());
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 差距 ⑥：目标 scene 不存在显式驳回（不再静默同场搬运）
    apollo::game::world::SceneTransferRequest missing;
    missing.player_id = player;
    missing.target_scene_id = 99999;
    auto outcome = apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, missing);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::PrepareFailed,
                "missing target rejected at prepare");
    TEST_ASSERT(fx.scene_a->avatar_count() == 1, "no side effect on rejection");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "state unchanged on rejection");

    // 准入：非 Active 会话驳回（差距 ①）
    TEST_ASSERT(fx.sessions.suspend_session(6002) != nullptr, "suspended");
    apollo::game::world::SceneTransferRequest rejected;
    rejected.player_id = player;
    rejected.target_scene_id = fx.scene_b->scene_id();
    outcome = apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, rejected);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::RejectedByState,
                "non-active session rejected");
    TEST_ASSERT(fx.scene_a->avatar_count() == 1, "still in source scene");

    // 未知会话驳回
    apollo::game::world::SceneTransferRequest ghost;
    ghost.player_id = apollo::game::core::PlayerId(8888);
    ghost.target_scene_id = fx.scene_b->scene_id();
    outcome = apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, ghost);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::UnknownSession,
                "unknown session rejected");

    // 目标 instance 非入场态驳回（Finishing 后不放行）
    TEST_ASSERT(fx.sessions.resume_session(6002) != nullptr, "resumed");
    fx.instance_b->finish();
    apollo::game::world::SceneTransferRequest closed;
    closed.player_id = player;
    closed.target_scene_id = fx.scene_b->scene_id();
    outcome = apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, closed);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::PrepareFailed,
                "instance not admitting rejected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_scene_transfer_rollback_on_attach_failure() {
    std::cout << "Running: test_scene_transfer_rollback_on_attach_failure..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7003);
    auto session = fx.sessions.create_session(6003, player);
    const auto entity = apollo::game::core::EntityId(9003);
    auto avatar = std::make_shared<apollo::game::world::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {5.0f, 6.0f, 7.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 预占目标：B 场已有同名 Avatar → attach 必败（重复进入拒绝）
    auto blocker = std::make_shared<apollo::game::world::Avatar>(player, apollo::game::core::EntityId(9004));
    TEST_ASSERT(fx.scene_b->enter(blocker, {}), "blocker pre-entered target");

    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {1.0f, 1.0f, 1.0f};

    const auto outcome =
        apollo::game::world::execute_scene_transfer(fx.world, fx.sessions, request);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::AttachFailed,
                "attach failed as staged");
    TEST_ASSERT(outcome.failed_at == apollo::game::world::SceneTransferStep::Attach,
                "failed at attach step");

    // 差距 ③：回滚——原 scene 原落点重进 + instance 成员还原 + 会话回 Active
    const auto restored = fx.scene_a->get_avatar(player);
    TEST_ASSERT(restored != nullptr, "avatar restored in source scene");
    TEST_ASSERT(restored != avatar, "restored avatar is a fresh object");
    TEST_ASSERT(restored->position().x == 5.0f && restored->position().z == 7.0f,
                "restored at original position");
    TEST_ASSERT(fx.scene_a->avatar_count() == 1, "source scene membership restored");
    TEST_ASSERT(fx.instance_a->has_player(player), "source instance membership restored");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "session rolled back to Active");
    TEST_ASSERT(session->pending_space_id() == 0, "pending cleared");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_transfer_disconnect_window() {
    std::cout << "Running: test_transfer_disconnect_window..." << std::endl;

    apollo::game::world::WorldSessionManager manager;
    const auto player = apollo::game::core::PlayerId(7004);
    auto session = manager.create_session(6004, player);
    session->assign_world(1);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 差距 ④：断线发生在转移中 → 挂机窗口（非瞬时消亡），pending 保留
    TEST_ASSERT(manager.transfer_session(6004, 2, 13, 22, false) != nullptr, "transfer begun");
    TEST_ASSERT(manager.suspend_transfer_session(6004) != nullptr, "disconnect mid-transfer");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Suspended,
                "in keepalive window");
    TEST_ASSERT(session->pending_world_id() == 2 && session->pending_space_id() == 22,
                "pending preserved across window");

    // 纯挂机（无 pending）不可收口——守卫面不放宽
    TEST_ASSERT(manager.suspend_session(manager.find_session(6004)->session_id()) == nullptr,
                "suspend from window rejected");
    auto plain = manager.create_session(6005, apollo::game::core::PlayerId(7005));
    plain->set_state(apollo::game::world::WorldSessionState::Suspended);
    TEST_ASSERT(manager.complete_transfer(6005) == nullptr, "plain suspended cannot complete");
    TEST_ASSERT(manager.abort_transfer(6005) == nullptr, "plain suspended cannot abort");

    // 重连收口路径一：确认——继续落到目标
    TEST_ASSERT(manager.complete_transfer(6004) != nullptr, "resolve after reconnect");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "active after resolve");
    TEST_ASSERT(session->world_id() == 2 && session->space_id() == 22,
                "fields switched after resolve");

    // 重连收口路径二：回滚——回到转移前
    TEST_ASSERT(manager.transfer_session(6004, 3, 14, 23, false) != nullptr, "second transfer");
    TEST_ASSERT(manager.suspend_transfer_session(6004) != nullptr, "disconnect again");
    TEST_ASSERT(manager.abort_transfer(6004) != nullptr, "rollback after reconnect");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "active after rollback");
    TEST_ASSERT(session->world_id() == 2 && session->space_id() == 22,
                "rolled back to pre-transfer fields");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace


// ---- P2-4：Anchor 长期态字段模型 + 奖励单向落 Anchor 真实接线 ----

bool test_anchor_longterm_state_fields() {
    std::cout << "Running: test_anchor_longterm_state_fields..." << std::endl;
    apollo::game::session::PlayerAnchor anchor(1001);

    // Inventory：同 id 合并 / 不足整体拒绝
    TEST_ASSERT(anchor.add_item(7001, 3), "入包");
    TEST_ASSERT(anchor.add_item(7001, 2), "同 id 合并");
    TEST_ASSERT(anchor.inventory().size() == 1 && anchor.inventory()[0].count == 5,
                "合并计数");
    TEST_ASSERT(!anchor.remove_item(7001, 6), "超扣整体拒绝");
    TEST_ASSERT(anchor.remove_item(7001, 5), "足额扣减");
    TEST_ASSERT(anchor.inventory().empty(), "清零即收格");
    TEST_ASSERT(!anchor.add_item(0, 1) && !anchor.add_item(7001, 0), "非法入参拒绝");

    // Equipment：固定槽位
    TEST_ASSERT(anchor.equip(0, 8001), "装 0 号位");
    TEST_ASSERT(anchor.equipment_at(0) == 8001, "槽位可查");
    TEST_ASSERT(!anchor.equip(apollo::game::session::PlayerAnchor::kEquipSlotCount, 8002), "越界槽位拒绝");
    TEST_ASSERT(!anchor.equip(1, 0), "空物品 id 拒绝");
    TEST_ASSERT(anchor.unequip(0) && anchor.equipment_at(0) == 0, "卸下");
    TEST_ASSERT(!anchor.unequip(0), "空槽卸下拒绝");

    // Quest：进度写读
    TEST_ASSERT(anchor.quest_progress(9001) == 0, "未接任务进度为 0");
    anchor.set_quest_progress(9001, 7);
    TEST_ASSERT(anchor.quest_progress(9001) == 7, "进度可查");
    anchor.set_quest_progress(9001, 9);
    TEST_ASSERT(anchor.quest_progress(9001) == 9, "进度覆盖写");

    // Progress：通用成长键值
    anchor.add_progress("exp", 100);
    anchor.add_progress("exp", 50);
    TEST_ASSERT(anchor.progress("exp") == 150, "累计");
    anchor.add_progress("exp", 0);
    TEST_ASSERT(anchor.progress("exp") == 150, "零增量无副作用");

    // Social：公会归属
    TEST_ASSERT(anchor.guild_id() == 0, "未入会");
    anchor.set_guild_id(1);
    TEST_ASSERT(anchor.guild_id() == 1, "归属可写");

    // 长期态变更即脏：每域操作都应留痕
    TEST_ASSERT(anchor.needs_save(), "长期态变更后需要保存");
    bool saw_inventory = false, saw_progress = false, saw_guild = false;
    for (const auto& r : anchor.dirty_reasons()) {
        saw_inventory = saw_inventory || r == "inventory_add";
        saw_progress = saw_progress || r == "progress_add";
        saw_guild = saw_guild || r == "guild_change";
    }
    TEST_ASSERT(saw_inventory && saw_progress && saw_guild, "三域脏因留痕");
    anchor.clear_dirty();
    TEST_ASSERT(!anchor.needs_save(), "清理后复位");
    return true;
}

bool test_anchor_reward_sink_wiring() {
    std::cout << "Running: test_anchor_reward_sink_wiring..." << std::endl;
    apollo::game::session::PlayerAnchor anchor(2001);
    apollo::game::session::AnchorRewardSink sink(anchor);

    // 奖励单向落 Anchor（P2-4 接线）：score 累计入 Progress「exp」
    sink.on_reward(2001, 30);
    sink.on_reward(2001, 12);
    TEST_ASSERT(anchor.progress(apollo::game::session::AnchorRewardSink::kExpKey) == 42, "奖励累计入 exp");
    TEST_ASSERT(anchor.needs_save(), "落账即脏（write-behind 面可见）");

    // 单 Anchor 绑定：他人分账忽略；零分忽略
    sink.on_reward(2999, 100);
    sink.on_reward(2001, 0);
    TEST_ASSERT(anchor.progress("exp") == 42, "越主/零分不落账");

    // battle 只出不进：sink 侧无回写通道（编译面即证——IRewardSink 纯单向），
    // 运行时证：经 BattleRuntime finish 结算，Anchor 收到账
    apollo::game::session::PlayerAnchor winner(2001);
    apollo::game::session::AnchorRewardSink winner_sink(winner);
    apollo::game::battle::BattleRuntime battle(1, 777, &winner_sink);
    TEST_ASSERT(battle.enter_player(2001), "参战");
    TEST_ASSERT(battle.begin(), "开局");
    // 确定性命中 tick：判定只依赖 (world_seed, tick, CombatRoll) 子流（约束④
    // 无游标），测试用同源纯函数复算选出必命中的 tick 再驱动
    std::uint32_t hit_tick = 0;
    for (std::uint32_t t = 1; t <= 50; ++t) {
        if (battle.substream(t, apollo::base::StreamId::CombatRoll).next_double() <
            0.5) {
            hit_tick = t;
            break;
        }
    }
    TEST_ASSERT(hit_tick > 0, "50 tick 内必存在命中窗口");
    std::vector<apollo::game::battle::BattleInput> inputs = {
        {hit_tick, 2001, 1}};
    TEST_ASSERT(battle.tick(hit_tick, inputs), "命中推进");
    TEST_ASSERT(battle.finish(), "散场");
    TEST_ASSERT(battle.phase() == apollo::game::battle::BattlePhase::Finished, "五段走完");
    TEST_ASSERT(winner.progress(apollo::game::session::AnchorRewardSink::kExpKey) > 0, "结算奖励落 Anchor");
    TEST_ASSERT(winner.needs_save(), "战斗结算留脏");
    return true;
}

int main() {
    std::cout << "=== Apollo Session/World Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_anchor_manager_activate_and_find);
    run(test_session_locator_bind_and_unbind);
    run(test_world_session_manager_lifecycle);
    run(test_world_create_scene_and_instance);
    run(test_scene_transfer_end_to_end);
    run(test_scene_transfer_prepare_rejections);
    run(test_scene_transfer_rollback_on_attach_failure);
    run(test_transfer_disconnect_window);
    run(test_anchor_longterm_state_fields);
    run(test_anchor_reward_sink_wiring);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
