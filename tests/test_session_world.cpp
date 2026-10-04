#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
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

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
