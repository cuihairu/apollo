#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/base/avatar_manager.hpp"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/game/base/avatar.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/base/world_assignment.hpp"
#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/game/world/world.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/game/social/party.hpp"
#include "apollo/base/rng.hpp"
#include "squad_instance/squad_instance.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool test_avatar_manager_activate_and_find() {
    std::cout << "Running: test_avatar_manager_activate_and_find..." << std::endl;

    apollo::game::base::AvatarManager manager;
    auto avatar = manager.activate(1001);

    TEST_ASSERT(avatar != nullptr, "avatar created");
    TEST_ASSERT(avatar->player_id() == 1001, "player id assigned");
    TEST_ASSERT(manager.avatar_count() == 1, "avatar count");
    TEST_ASSERT(manager.find(1001) == avatar, "find returns same avatar");

    avatar->mark_dirty("load");
    TEST_ASSERT(avatar->needs_save(), "dirty tracked");

    // snapshot（P0-4）：flush 遍历面
    const auto snapshot = manager.snapshot();
    TEST_ASSERT(snapshot.size() == 1, "snapshot size");
    TEST_ASSERT(snapshot[0] == avatar, "snapshot carries avatar");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_session_locator_bind_and_unbind() {
    std::cout << "Running: test_session_locator_bind_and_unbind..." << std::endl;

    apollo::game::session::SessionLocator locator;
    apollo::game::base::SessionBinding binding;
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

bool test_world_session_manager_relogin_supersedes() {
    std::cout << "Running: test_world_session_manager_relogin_supersedes..." << std::endl;

    apollo::game::world::WorldSessionManager manager;
    const auto player = apollo::game::core::PlayerId(4001);

    // 首登：会话 A 绑定玩家
    auto session_a = manager.create_session(3001, player);
    session_a->set_state(apollo::game::world::WorldSessionState::Active);
    TEST_ASSERT(manager.find_by_player(player) == session_a, "first session bound");
    TEST_ASSERT(manager.session_count() == 1, "one session after first login");

    // 重登顶号（D10 case 4 唯一资料写入者）：同玩家新会话 B 落表——旧会话
    // A 撤销，旧会话号不再解析（其迟到模拟输入/结算不得再写）
    auto session_b = manager.create_session(3002, player);
    TEST_ASSERT(manager.find_by_player(player) == session_b, "new session supersedes");
    TEST_ASSERT(manager.find_session(3001) == nullptr, "superseded session no longer resolves");
    TEST_ASSERT(session_a->state() == apollo::game::world::WorldSessionState::Closed,
                "superseded session terminal Closed");
    TEST_ASSERT(manager.find_session(3002) == session_b, "new session resolves");
    TEST_ASSERT(manager.session_count() == 1, "old session evicted");

    // 顶号后再顶：会话 C 顶 B——单活跃会话不变量持续
    auto session_c = manager.create_session(3003, player);
    TEST_ASSERT(manager.find_session(3002) == nullptr, "second supersession evicts prior");
    TEST_ASSERT(manager.find_by_player(player) == session_c, "latest session bound");
    TEST_ASSERT(manager.session_count() == 1, "single live session invariant");

    // 跨玩家同号不受影响：另一玩家新会话独立落表
    auto other = manager.create_session(3001, apollo::game::core::PlayerId(4002));
    TEST_ASSERT(other != nullptr, "different player new session independent");
    TEST_ASSERT(manager.find_session(3001) == other, "cross-player session resolves");
    TEST_ASSERT(manager.find_session(3003) == session_c, "player's session unaffected");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_world_session_manager_session_id_reuse() {
    std::cout << "Running: test_world_session_manager_session_id_reuse..." << std::endl;

    apollo::game::world::WorldSessionManager manager;
    const auto player_a = apollo::game::core::PlayerId(4001);
    const auto player_b = apollo::game::core::PlayerId(4002);

    // 玩家 A 持号 3001（活跃）
    auto session_a = manager.create_session(3001, player_a);
    session_a->set_state(apollo::game::world::WorldSessionState::Active);
    TEST_ASSERT(manager.find_session(3001) == session_a, "A owns 3001");
    TEST_ASSERT(manager.find_by_player(player_a) == session_a, "A indexed");

    // 会话号复用（网关重启归基址重发旧号）：玩家 B 拿同一号 3001——撤销
    // A 的旧行，不静默覆写（跨玩家串号禁）
    auto session_b = manager.create_session(3001, player_b);
    TEST_ASSERT(session_b != nullptr, "B takes the reused id");
    TEST_ASSERT(manager.find_session(3001) == session_b, "id now resolves to B");
    TEST_ASSERT(manager.find_by_player(player_b) == session_b, "B indexed");
    TEST_ASSERT(manager.find_by_player(player_a) == nullptr, "A no longer resolves");
    TEST_ASSERT(session_a->state() == apollo::game::world::WorldSessionState::Closed,
                "reclaimed session terminal Closed");
    TEST_ASSERT(manager.session_count() == 1, "one row per id");

    // 同号同玩家重入：幂等返回既有行，不新建不覆写
    auto same = manager.create_session(3001, player_b);
    TEST_ASSERT(same == session_b, "same-player same-id is idempotent");
    TEST_ASSERT(manager.find_session(3001) == session_b, "row unchanged");
    TEST_ASSERT(manager.find_by_player(player_b) == session_b, "index kept");
    TEST_ASSERT(manager.session_count() == 1, "no duplicate row");

    // 换号顶替仍成立：B 新号 3002 顶 3001——旧号撤销
    auto session_b2 = manager.create_session(3002, player_b);
    TEST_ASSERT(manager.find_session(3002) == session_b2, "new id bound");
    TEST_ASSERT(manager.find_session(3001) == nullptr, "old id evicted");
    TEST_ASSERT(manager.session_count() == 1, "single live session invariant");

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

    const auto destroyed_id = scene->scene_id();
    TEST_ASSERT(world.create_instance(destroyed_id) == nullptr, "one Instance per Scene");
    TEST_ASSERT(!world.destroy_scene(destroyed_id), "active Instance protects Scene");
    TEST_ASSERT(!world.destroy_instance(instance->id()), "active Instance cannot be reclaimed");
    TEST_ASSERT(instance->cancel(), "unstarted Instance can be cancelled");
    TEST_ASSERT(instance->settle() && instance->drain() && instance->destroy(), "cancelled Instance reaches terminal state");
    const auto instance_id = instance->id();
    TEST_ASSERT(world.destroy_instance(instance_id), "terminal Instance and Scene reclaimed together");
    TEST_ASSERT(world.find_instance(instance_id) == nullptr && world.instance_count() == 0, "Instance registry cleared");
    TEST_ASSERT(!world.destroy_instance(instance_id), "repeated reclamation rejected");
    TEST_ASSERT(world.scene_count() == 0, "scene destroyed");
    // 摘除后顺序表同步干净（destroy 次序错即悬垂指针 use-after-free，
    // sanitizer 树实抓）——重建场后 tick 必须在干净顺序表上推进
    TEST_ASSERT(world.scenes().empty(), "order list cleared with map");
    auto* again = world.create_scene(descriptor);
    TEST_ASSERT(again != nullptr, "re-create after destroy");
    TEST_ASSERT(again->scene_id() != destroyed_id, "scene id not reused");
    world.tick(0.1);
    TEST_ASSERT(again->tick_count() == 1, "tick on fresh order list");

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

    // 进 A 场（attach 面与 world_app 同构：Avatar 重建 + instance.enter）
    const auto entity = apollo::game::core::EntityId(9001);
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {1.0f, 2.0f, 3.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_world(1);
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->bind_avatar(entity);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 投影源 = 只读玩法快照（值语义；跨界不传 Avatar 指针，todo L84）
    apollo::game::base::GameplaySnapshot snapshot;
    snapshot.home_zone_id = 42;
    snapshot.max_hp = 120;
    snapshot.hp = 90;

    // ---- 换幕：A → B ----
    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {10.0f, 20.0f, 30.0f};
    request.gameplay_snapshot = snapshot;

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
    TEST_ASSERT(rebuilt->has_projection(), "avatar projection done");
    TEST_ASSERT(rebuilt->home_zone_id() == 42, "home zone projected");
    TEST_ASSERT(rebuilt->max_hp() == 120 && rebuilt->hp() == 90,
                "gameplay attributes seeded from read-only snapshot");

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
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, apollo::game::core::EntityId(9002));
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
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {5.0f, 6.0f, 7.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 预占目标：B 场已有同名 Avatar → attach 必败（重复进入拒绝）
    auto blocker = std::make_shared<apollo::game::cell::Avatar>(player, apollo::game::core::EntityId(9004));
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

bool test_scene_migration_runtime_preserved() {
    std::cout << "Running: test_scene_migration_runtime_preserved..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7004);
    auto session = fx.sessions.create_session(6004, player);
    TEST_ASSERT(session != nullptr, "session created");

    const auto entity = apollo::game::core::EntityId(9005);
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {1.0f, 2.0f, 3.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_world(1);
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->bind_avatar(entity);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // cell 权威运行态（血量/序号水位）——迁移应随迁的载荷
    avatar->set_hp(37, 80);
    TEST_ASSERT(avatar->reconcile(12, {4.0f, 5.0f, 6.0f}), "runtime seq advanced");

    // 快照只管长期属性；运行态必须覆盖快照播种值（hp 90/120 不得胜出）
    apollo::game::base::GameplaySnapshot snapshot;
    snapshot.home_zone_id = 42;
    snapshot.max_hp = 120;
    snapshot.hp = 90;

    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {10.0f, 20.0f, 30.0f};
    request.gameplay_snapshot = snapshot;

    const auto outcome =
        apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, request);
    TEST_ASSERT(outcome.ok(), "migration ok");
    TEST_ASSERT(outcome.source_scene_id == fx.scene_a->scene_id(), "source recorded");

    // 五步逐步确认（D16：源冻结→目标准备→条件提交→路由投影→激活/清理）
    using SM = apollo::game::world::SceneMigrationStep;
    TEST_ASSERT(outcome.step_confirmed(SM::SourceFreeze), "step 1 confirmed");
    TEST_ASSERT(outcome.step_confirmed(SM::TargetPrepare), "step 2 confirmed");
    TEST_ASSERT(outcome.step_confirmed(SM::Commit), "step 3 confirmed");
    TEST_ASSERT(outcome.step_confirmed(SM::RouteProject), "step 4 confirmed");
    TEST_ASSERT(outcome.step_confirmed(SM::ActivateAndCleanup), "step 5 confirmed");

    // 运行态随迁：实体 ID 稳定、血量为 cell 模拟态（非快照播种值）
    const auto rebuilt = fx.scene_b->get_avatar(player);
    TEST_ASSERT(rebuilt != nullptr, "avatar rebuilt in target");
    TEST_ASSERT(rebuilt->entity_id() == entity, "entity id stable across migration");
    TEST_ASSERT(rebuilt->hp() == 37 && rebuilt->max_hp() == 80,
                "cell-authoritative hp survives (not snapshot seed)");
    TEST_ASSERT(rebuilt->last_reconciled_seq() == 12, "seq watermark preserved");
    TEST_ASSERT(rebuilt->position().x == 10.0f && rebuilt->position().z == 30.0f,
                "landing applied (坐标仍是易失态)");
    TEST_ASSERT(rebuilt->home_zone_id() == 42, "long-term projection kept");

    // 水位门随迁：旧包重放拒绝、新序号受理（迁移清零即反作弊门失效）
    TEST_ASSERT(!rebuilt->reconcile(5, {7.0f, 7.0f, 7.0f}), "stale replay rejected");
    TEST_ASSERT(rebuilt->reconcile(13, {7.0f, 7.0f, 7.0f}), "new seq accepted");
    TEST_ASSERT(rebuilt->apply_damage(7) == false && rebuilt->hp() == 30,
                "combat settles on migrated avatar");

    TEST_ASSERT(fx.scene_a->avatar_count() == 0, "source cleaned after activation");
    TEST_ASSERT(fx.scene_b->aoi().contains(entity), "aoi re-registered in target");
    TEST_ASSERT(fx.instance_a->has_player(player) == false, "left source instance");
    TEST_ASSERT(fx.instance_b->has_player(player) == true, "joined target instance");

    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "session active after migration");
    TEST_ASSERT(session->map_instance_id() == fx.instance_b->id(), "instance switched");
    TEST_ASSERT(session->space_id() == fx.scene_b->scene_id(), "space switched");
    TEST_ASSERT(session->route_version() == 1, "route version bumped");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_scene_migration_commit_cancel_unfreezes_source() {
    std::cout << "Running: test_scene_migration_commit_cancel_unfreezes_source..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7005);
    auto session = fx.sessions.create_session(6005, player);
    const auto entity = apollo::game::core::EntityId(9006);
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {5.0f, 6.0f, 7.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->set_state(apollo::game::world::WorldSessionState::Active);
    avatar->set_hp(55, 90);
    TEST_ASSERT(avatar->reconcile(7, {8.0f, 8.0f, 8.0f}), "runtime seq advanced");

    // 预占目标：B 场已有同玩家 Avatar → 条件提交必败（重复进入拒绝）
    auto blocker = std::make_shared<apollo::game::cell::Avatar>(player,
                                                                apollo::game::core::EntityId(9007));
    TEST_ASSERT(fx.scene_b->enter(blocker, {}), "blocker pre-entered target");

    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {1.0f, 1.0f, 1.0f};

    const auto outcome =
        apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, request);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::AttachFailed,
                "commit failed as staged");
    TEST_ASSERT(outcome.failed_at == apollo::game::world::SceneMigrationStep::Commit,
                "failed at commit step");

    // 取消确认轨迹：只确认前两步，提交位不得置位
    using SM = apollo::game::world::SceneMigrationStep;
    TEST_ASSERT(outcome.step_confirmed(SM::SourceFreeze), "freeze confirmed");
    TEST_ASSERT(outcome.step_confirmed(SM::TargetPrepare), "prepare confirmed");
    TEST_ASSERT(!outcome.step_confirmed(SM::Commit), "commit not confirmed on cancel");
    TEST_ASSERT(!outcome.step_confirmed(SM::RouteProject), "route not confirmed on cancel");
    TEST_ASSERT(!outcome.step_confirmed(SM::ActivateAndCleanup), "cleanup not confirmed on cancel");

    // 源解冻（非重建）：原对象未离场，运行态无损（D16 序反转——目标激活
    // 才源清理，提交失败即源解冻，区别于 D15 的快照重建回滚）
    const auto restored = fx.scene_a->get_avatar(player);
    TEST_ASSERT(restored == avatar, "source avatar never left scene (same object)");
    TEST_ASSERT(restored->state() == apollo::game::cell::AvatarState::Active,
                "source unfrozen to Active");
    TEST_ASSERT(restored->hp() == 55 && restored->max_hp() == 90, "source hp intact");
    TEST_ASSERT(restored->last_reconciled_seq() == 7, "source seq intact");
    TEST_ASSERT(restored->reconcile(8, {9.0f, 9.0f, 9.0f}), "source accepts input again");

    TEST_ASSERT(fx.scene_b->avatar_count() == 1, "target keeps only blocker (staged discarded)");
    TEST_ASSERT(fx.scene_b->get_avatar(player) == blocker, "staged not left in target");
    TEST_ASSERT(fx.instance_a->has_player(player), "source instance membership intact");
    TEST_ASSERT(fx.instance_b->has_player(player) == false, "target instance untouched");
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active,
                "session stays Active (transfer window never opened)");
    TEST_ASSERT(session->pending_space_id() == 0, "no pending route");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// D21 §3 项 6「随机」等价表示验收：RNG 无可变游标——任意 (world_seed, tick,
// stream) 由纯函数派生子流。在真实迁移路径前后按同一三元组取样，断言逐值
// 相等：迁移不需要搬运随机状态，也不产生调用次序/进程替换漂移。
bool test_scene_migration_rng_equivalence() {
    std::cout << "Running: test_scene_migration_rng_equivalence..." << std::endl;

    using apollo::base::StreamId;
    using apollo::base::derive_substream;
    constexpr std::uint64_t kWorldSeed = 0x5EED'1234'ABCD'5678ULL;

    const auto sample = [](std::uint64_t tick, StreamId stream) {
        auto rng = derive_substream(kWorldSeed, tick, stream);
        std::vector<std::uint32_t> values;
        for (int i = 0; i < 8; ++i) {
            values.push_back(rng.next_uint32());
        }
        return values;
    };
    const auto combat_before = sample(37, StreamId::CombatRoll);
    const auto drop_before = sample(37, StreamId::Drop);

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7007);
    auto session = fx.sessions.create_session(6007, player);
    TEST_ASSERT(session != nullptr, "session created");
    const auto entity = apollo::game::core::EntityId(9008);
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player, entity);
    TEST_ASSERT(fx.scene_a->enter(avatar, {1.0f, 2.0f, 3.0f}), "avatar entered scene a");
    TEST_ASSERT(fx.instance_a->enter(player), "player entered instance a");
    session->assign_world(1);
    session->assign_map_instance(fx.instance_a->id());
    session->assign_space(fx.scene_a->scene_id());
    session->bind_avatar(entity);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 先消费一次其它流（次序无关，证明不靠全局游标续流）
    auto interleaved = derive_substream(kWorldSeed, 37, StreamId::AiDecision);
    (void)interleaved.next_uint32();

    apollo::game::world::SceneTransferRequest request;
    request.player_id = player;
    request.target_scene_id = fx.scene_b->scene_id();
    request.landing = {10.0f, 20.0f, 30.0f};
    const auto outcome =
        apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, request);
    TEST_ASSERT(outcome.ok(), "migration ok");

    const auto combat_after = sample(37, StreamId::CombatRoll);
    const auto drop_after = sample(37, StreamId::Drop);
    TEST_ASSERT(combat_after == combat_before, "combat substream identical across migration");
    TEST_ASSERT(drop_after == drop_before, "drop substream identical across migration");
    TEST_ASSERT(combat_before != drop_before, "stream domains stay isolated");
    // 后续 tick 仍可独立重放（迁移不推进任何随机游标）
    const auto next_tick = sample(38, StreamId::CombatRoll);
    TEST_ASSERT(next_tick != combat_before, "next tick derives a distinct sequence");
    TEST_ASSERT(sample(37, StreamId::CombatRoll) == combat_before,
                "prior tick remains replayable after migration");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// D21 立即执行动作：样本玩法在真实迁移路径前后记录状态与事件序列，迁移中
// 触发计时/击杀/结算；未满足的状态列缺口（见 06-update-continuity §3 记录）。
// 结构事实：可用迁移面只有玩家级 execute_migration_transfer——活动（副本
// Instance、怪、目标、玩法时钟）不随迁、留源场继续走。本样本证明的是「成员
// 迁移不扰活动、结算按名册不按在场」，不是活动级连续：D21 项 1/3/4/5/7 的
// 跨活动快照协议未设计（主行缺口），此处只钉住现有面上的行为事实。
bool test_scene_migration_activity_gameplay_sample() {
    std::cout << "Running: test_scene_migration_activity_gameplay_sample..." << std::endl;

    apollo::game::social::PartyManager parties;
    apollo::game::world::World world;
    apollo::game::world::WorldSessionManager sessions;

    constexpr std::uint64_t kLeader = 7100;
    constexpr std::uint64_t kMember = 7101;
    auto* party = parties.create(1, kLeader);
    party->add_member(kMember);

    squad_instance::SquadInstanceService service(parties, &world, /*local_world_id=*/3);
    service.register_world(3, 4);

    // 结算缝（launch 前装配）：记录 958 落账调用（事件序列 S4 面）
    struct SinkCall {
        std::uint64_t player = 0;
        std::int64_t score = 0;
        std::uint64_t settlement_id = 0;
    };
    std::vector<SinkCall> sink_calls;
    service.set_business_sink(
        [&](std::uint64_t player_id, std::int64_t score, std::uint64_t settlement_id) {
            sink_calls.push_back({player_id, score, settlement_id});
            return squad_instance::SettlementLedger::ApplyError::Ok;
        });

    squad_instance::LaunchRequest request;
    request.party_id = 1;
    request.requester_id = kLeader;
    request.map_id = 42;
    request.map_name = "activity-map";
    const auto launch = service.launch(request);
    TEST_ASSERT(launch.error == squad_instance::LaunchError::Ok, "activity launched");
    const auto* binding = service.find_by_party(1);
    TEST_ASSERT(binding != nullptr && binding->members.size() == 2,
                "roster snapshot taken");
    auto* activity = world.find_instance(launch.instance_id);
    TEST_ASSERT(activity != nullptr, "activity instance exists");

    // 成员进入 + 队长启动（Waiting→Running）
    TEST_ASSERT(service.enter(1, kLeader) ==
                    squad_instance::SquadInstanceService::EnterError::Ok,
                "leader entered");
    TEST_ASSERT(service.enter(1, kMember) ==
                    squad_instance::SquadInstanceService::EnterError::Ok,
                "member entered");
    TEST_ASSERT(service.start_gameplay(1, kLeader) ==
                    squad_instance::SquadInstanceService::EnterError::Ok,
                "gameplay running");
    TEST_ASSERT(activity->state() == apollo::game::world::Instance::State::Running,
                "activity Running");

    // 目标 + 刷怪（配额 2；超时 60 tick 本样本不触线）
    squad_instance::GameplayGoal goal;
    goal.monsters_to_defeat = 2;
    goal.timeout_ticks = 60;
    TEST_ASSERT(service.set_goal(1, goal) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "goal set");
    TEST_ASSERT(service.spawn_wave(1, 2) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "wave spawned");
    auto progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->monsters_alive.size() == 2,
                "two monsters alive");
    const auto monster_a = progress->monsters_alive[0];
    const auto monster_b = progress->monsters_alive[1];

    // 迁移目标场（普通场 + 普通实例，同 TwoSceneFixture 形）
    apollo::game::world::SceneDescriptor plain;
    plain.map_id = 9;
    plain.map_name = "plain-b";
    plain.width = 100.0f;
    plain.height = 100.0f;
    auto* plain_scene = world.create_scene(plain);
    auto plain_instance = world.create_instance(plain_scene->scene_id(), "plain-run");
    plain_instance->initialize();
    plain_instance->ready();
    plain_instance->start();

    // 队长会话入副本场（attach 面与 world_app 同构）
    const auto leader_id = apollo::game::core::PlayerId(kLeader);
    auto session = sessions.create_session(6100, leader_id);
    TEST_ASSERT(session != nullptr, "leader session created");
    const auto entity = apollo::game::core::EntityId(9100);
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(leader_id, entity);
    TEST_ASSERT(world.find_scene(binding->scene_id)->enter(avatar, {1.0f, 2.0f, 3.0f}),
                "leader avatar in activity scene");
    session->assign_world(3);
    session->assign_map_instance(launch.instance_id);
    session->assign_space(binding->scene_id);
    session->bind_avatar(entity);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // ---- S1 迁移前（活动 Running 中）：计时走 4 拍 + 击杀 1（队长归属）----
    TEST_ASSERT(service.advance_tick(1, 3) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "timer advanced");
    TEST_ASSERT(service.report_monster_defeated(1, monster_a, kLeader) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "kill by leader admitted");
    TEST_ASSERT(service.advance_tick(1, 1) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "kill score entered battle seq");
    progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->ticks_elapsed == 4 &&
                    progress->monsters_defeated == 1 && progress->monsters_spawned == 2 &&
                    progress->monsters_alive.size() == 1,
                "pre-migration state recorded");
    std::cout << "  S1 pre-migration: ticks=" << progress->ticks_elapsed
              << " defeated=" << progress->monsters_defeated
              << " alive=" << progress->monsters_alive.size() << std::endl;

    // ---- S2 迁移中：Running 期队长真迁移（五步 D16 路径）----
    apollo::game::world::SceneTransferRequest migration;
    migration.player_id = leader_id;
    migration.target_scene_id = plain_scene->scene_id();
    migration.landing = {10.0f, 20.0f, 30.0f};
    const auto outcome =
        apollo::game::world::execute_migration_transfer(world, sessions, migration);
    TEST_ASSERT(outcome.ok(), "mid-gameplay migration ok");
    using SM = apollo::game::world::SceneMigrationStep;
    TEST_ASSERT(outcome.step_confirmed(SM::SourceFreeze) &&
                    outcome.step_confirmed(SM::TargetPrepare) &&
                    outcome.step_confirmed(SM::Commit) &&
                    outcome.step_confirmed(SM::RouteProject) &&
                    outcome.step_confirmed(SM::ActivateAndCleanup),
                "all five steps confirmed");
    // 玩家面：世界级成员随迁移换场
    TEST_ASSERT(activity->has_player(leader_id) == false, "left activity instance");
    TEST_ASSERT(plain_instance->has_player(leader_id) == true, "joined plain instance");
    TEST_ASSERT(world.find_scene(binding->scene_id)->avatar_count() == 0,
                "activity scene emptied of leader avatar");
    TEST_ASSERT(plain_scene->avatar_count() == 1, "leader avatar rebuilt in target");
    // 活动面：副本/怪/目标/时钟原地不动（迁移不触活动）
    const auto* binding_after = service.find_by_party(1);
    TEST_ASSERT(binding_after->instance_id == launch.instance_id &&
                    binding_after->scene_id == binding->scene_id,
                "activity binding untouched");
    // 名册不感知迁移（D21 项 1 缺口的行为证据）：队长仍在名册快照中
    TEST_ASSERT(std::find(binding_after->members.begin(), binding_after->members.end(),
                          kLeader) != binding_after->members.end(),
                "roster unaware of migration (gap, recorded)");
    progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->goal_set &&
                    progress->monsters_spawned == 2 && progress->monsters_defeated == 1 &&
                    progress->monsters_alive.size() == 1,
                "activity state intact through member migration");
    std::cout << "  S2 migration: 5 steps confirmed, member left scene, "
                 "activity untouched" << std::endl;

    // ---- S3 迁移后：计时续走（不重置）+ 击杀 2（留场成员归属）→ 目标达标 ----
    TEST_ASSERT(service.advance_tick(1, 2) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "timer continues after migration");
    progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->ticks_elapsed == 6,
                "clock monotonic across migration (4 pre + 2, no reset)");
    TEST_ASSERT(service.report_monster_defeated(1, monster_b, kMember) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "post-migration kill attributed to remaining member");
    TEST_ASSERT(service.advance_tick(1, 1) ==
                    squad_instance::SquadInstanceService::GameplayError::Ok,
                "quota met, settlement entered next tick");
    progress = service.gameplay(1);
    TEST_ASSERT(progress.has_value() && progress->ticks_elapsed == 7,
                "clock kept counting into settlement tick");
    TEST_ASSERT(activity->state() == apollo::game::world::Instance::State::Finishing,
                "goal completion drives settlement");
    TEST_ASSERT(activity->settle(), "settlement advances to Rewarding");

    // ---- S4 结算落账：958 按名册发放（迁移离场成员不缺席）----
    TEST_ASSERT(sink_calls.size() == 2, "one credit per roster member");
    bool leader_paid = false;
    bool member_paid = false;
    for (const auto& call : sink_calls) {
        TEST_ASSERT(call.score > 0, "kill score attributed");
        TEST_ASSERT(call.settlement_id == sink_calls[0].settlement_id &&
                        call.settlement_id != 0,
                    "one settlement id for the run");
        leader_paid |= call.player == kLeader;
        member_paid |= call.player == kMember;
    }
    TEST_ASSERT(leader_paid, "migrated member still settled (by roster)");
    TEST_ASSERT(member_paid, "remaining member settled");
    const auto settle_snap = service.settlement(1);
    TEST_ASSERT(settle_snap.has_value() && settle_snap->applied == 2 && settle_snap->complete,
                "settlement complete for full roster");
    TEST_ASSERT(service.dismiss(1), "activity released after settlement");
    TEST_ASSERT(service.active_count() == 0, "binding cleared");
    std::cout << "  S4 settlement: 2 credits (full roster), settlement_id="
              << sink_calls[0].settlement_id << std::endl;

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_scene_migration_rejections() {
    std::cout << "Running: test_scene_migration_rejections..." << std::endl;

    TwoSceneFixture fx;
    const auto player = apollo::game::core::PlayerId(7006);
    auto session = fx.sessions.create_session(6006, player);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    // 源无 Avatar：运行态无从捕获——显式驳回，不静默降级为重建
    apollo::game::world::SceneTransferRequest no_avatar;
    no_avatar.player_id = player;
    no_avatar.target_scene_id = fx.scene_b->scene_id();
    auto outcome =
        apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, no_avatar);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::PrepareFailed,
                "no source avatar rejected");
    TEST_ASSERT(outcome.failed_at == apollo::game::world::SceneMigrationStep::SourceFreeze,
                "rejected at freeze step");
    TEST_ASSERT(outcome.confirmed_steps == 0, "no steps confirmed");
    TEST_ASSERT(fx.scene_b->avatar_count() == 0, "no side effect on target");

    // 非 Active 会话驳回（冻结/转移中不可发起）
    TEST_ASSERT(fx.sessions.suspend_session(6006) != nullptr, "suspended");
    outcome = apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, no_avatar);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::RejectedByState,
                "non-active session rejected");

    // 未知会话驳回
    apollo::game::world::SceneTransferRequest ghost;
    ghost.player_id = apollo::game::core::PlayerId(8889);
    ghost.target_scene_id = fx.scene_b->scene_id();
    outcome = apollo::game::world::execute_migration_transfer(fx.world, fx.sessions, ghost);
    TEST_ASSERT(outcome.result == apollo::game::world::SceneTransferResult::UnknownSession,
                "unknown session rejected");

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


// ---- P2-4：Avatar 长期态字段模型 + 奖励单向落 Avatar 真实接线 ----

bool test_avatar_longterm_state_fields() {
    std::cout << "Running: test_avatar_longterm_state_fields..." << std::endl;
    apollo::game::base::Avatar avatar(1001);

    // Inventory：同 id 合并 / 不足整体拒绝
    TEST_ASSERT(avatar.add_item(7001, 3), "入包");
    TEST_ASSERT(avatar.add_item(7001, 2), "同 id 合并");
    TEST_ASSERT(avatar.inventory().size() == 1 && avatar.inventory()[0].count == 5,
                "合并计数");
    TEST_ASSERT(!avatar.remove_item(7001, 6), "超扣整体拒绝");
    TEST_ASSERT(avatar.remove_item(7001, 5), "足额扣减");
    TEST_ASSERT(avatar.inventory().empty(), "清零即收格");
    TEST_ASSERT(!avatar.add_item(0, 1) && !avatar.add_item(7001, 0), "非法入参拒绝");

    // Equipment：固定槽位
    TEST_ASSERT(avatar.equip(0, 8001), "装 0 号位");
    TEST_ASSERT(avatar.equipment_at(0) == 8001, "槽位可查");
    TEST_ASSERT(!avatar.equip(apollo::game::base::Avatar::kEquipSlotCount, 8002), "越界槽位拒绝");
    TEST_ASSERT(!avatar.equip(1, 0), "空物品 id 拒绝");
    TEST_ASSERT(avatar.unequip(0) && avatar.equipment_at(0) == 0, "卸下");
    TEST_ASSERT(!avatar.unequip(0), "空槽卸下拒绝");

    // Quest：进度写读
    TEST_ASSERT(avatar.quest_progress(9001) == 0, "未接任务进度为 0");
    avatar.set_quest_progress(9001, 7);
    TEST_ASSERT(avatar.quest_progress(9001) == 7, "进度可查");
    avatar.set_quest_progress(9001, 9);
    TEST_ASSERT(avatar.quest_progress(9001) == 9, "进度覆盖写");

    // Progress：通用成长键值
    avatar.add_progress("exp", 100);
    avatar.add_progress("exp", 50);
    TEST_ASSERT(avatar.progress("exp") == 150, "累计");
    avatar.add_progress("exp", 0);
    TEST_ASSERT(avatar.progress("exp") == 150, "零增量无副作用");

    // Social：公会归属
    TEST_ASSERT(avatar.guild_id() == 0, "未入会");
    avatar.set_guild_id(1);
    TEST_ASSERT(avatar.guild_id() == 1, "归属可写");

    // 长期态变更即脏：每域操作都应留痕
    TEST_ASSERT(avatar.needs_save(), "长期态变更后需要保存");
    bool saw_inventory = false, saw_progress = false, saw_guild = false;
    for (const auto& r : avatar.dirty_reasons()) {
        saw_inventory = saw_inventory || r == "inventory_add";
        saw_progress = saw_progress || r == "progress_add";
        saw_guild = saw_guild || r == "guild_change";
    }
    TEST_ASSERT(saw_inventory && saw_progress && saw_guild, "三域脏因留痕");
    avatar.clear_dirty();
    TEST_ASSERT(!avatar.needs_save(), "清理后复位");
    return true;
}

bool test_avatar_reward_sink_wiring() {
    std::cout << "Running: test_avatar_reward_sink_wiring..." << std::endl;
    apollo::game::base::Avatar avatar(2001);
    apollo::game::base::AvatarRewardSink sink(avatar);

    // 奖励单向落 Avatar（P2-4 接线）：score 累计入 Progress「exp」
    sink.on_reward(2001, 30);
    sink.on_reward(2001, 12);
    TEST_ASSERT(avatar.progress(apollo::game::base::AvatarRewardSink::kExpKey) == 42, "奖励累计入 exp");
    TEST_ASSERT(avatar.needs_save(), "落账即脏（write-behind 面可见）");

    // 单 Avatar 绑定：他人分账忽略；零分忽略
    sink.on_reward(2999, 100);
    sink.on_reward(2001, 0);
    TEST_ASSERT(avatar.progress("exp") == 42, "越主/零分不落账");

    // battle 只出不进：sink 侧无回写通道（编译面即证——IRewardSink 纯单向），
    // 运行时证：经 BattleRuntime finish 结算，Avatar 收到账
    apollo::game::base::Avatar winner(2001);
    apollo::game::base::AvatarRewardSink winner_sink(winner);
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
    TEST_ASSERT(winner.progress(apollo::game::base::AvatarRewardSink::kExpKey) > 0, "结算奖励落 Avatar");
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

    run(test_avatar_manager_activate_and_find);
    run(test_session_locator_bind_and_unbind);
    run(test_world_session_manager_lifecycle);
    run(test_world_session_manager_relogin_supersedes);
    run(test_world_session_manager_session_id_reuse);
    run(test_world_create_scene_and_instance);
    run(test_scene_transfer_end_to_end);
    run(test_scene_transfer_prepare_rejections);
    run(test_scene_transfer_rollback_on_attach_failure);
    run(test_scene_migration_runtime_preserved);
    run(test_scene_migration_commit_cancel_unfreezes_source);
    run(test_scene_migration_rng_equivalence);
    run(test_scene_migration_activity_gameplay_sample);
    run(test_scene_migration_rejections);
    run(test_transfer_disconnect_window);
    run(test_avatar_longterm_state_fields);
    run(test_avatar_reward_sink_wiring);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
