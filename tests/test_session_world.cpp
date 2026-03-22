#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/session_locator.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/game/world/map_instance_manager.hpp"
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
    auto session = manager.create_session(3001, 4001);

    TEST_ASSERT(session != nullptr, "session created");
    session->assign_world(9);
    session->assign_map_instance(12);
    session->assign_space(21);
    session->set_state(apollo::game::world::WorldSessionState::Active);

    TEST_ASSERT(manager.session_count() == 1, "session count");
    TEST_ASSERT(manager.find_session(3001) == session, "find by session");
    TEST_ASSERT(manager.find_by_player(4001) == session, "find by player");
    TEST_ASSERT(session->world_id() == 9, "world assigned");
    TEST_ASSERT(session->map_instance_id() == 12, "instance assigned");
    TEST_ASSERT(session->space_id() == 21, "space assigned");

    manager.suspend_session(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Suspended, "session suspended");

    manager.resume_session(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active, "session resumed");

    manager.transfer_session(3001, 10, 13, 22, false);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::TransferringOut, "session transferring");
    TEST_ASSERT(session->pending_world_id() == 10, "pending world assigned");
    TEST_ASSERT(session->pending_map_instance_id() == 13, "pending instance assigned");
    TEST_ASSERT(session->pending_space_id() == 22, "pending space assigned");

    manager.complete_transfer(3001);
    TEST_ASSERT(session->state() == apollo::game::world::WorldSessionState::Active, "session active after transfer");
    TEST_ASSERT(session->world_id() == 10, "world switched after transfer");
    TEST_ASSERT(session->map_instance_id() == 13, "instance switched after transfer");
    TEST_ASSERT(session->space_id() == 22, "space switched after transfer");

    manager.close_session(3001);
    TEST_ASSERT(manager.session_count() == 0, "session removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_map_instance_manager_create_and_update() {
    std::cout << "Running: test_map_instance_manager_create_and_update..." << std::endl;

    apollo::game::world::MapInstanceManager manager;
    auto instance = manager.create_instance(5001, "test-map");

    TEST_ASSERT(instance != nullptr, "instance created");
    TEST_ASSERT(instance->id() == 5001, "instance id");
    TEST_ASSERT(instance->map_name() == "test-map", "map name");
    TEST_ASSERT(manager.instance_count() == 1, "instance count");
    TEST_ASSERT(manager.find_instance(5001) == instance, "find instance");

    manager.update(0.05f);
    manager.destroy_instance(5001);
    TEST_ASSERT(manager.instance_count() == 0, "instance removed");

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
    run(test_map_instance_manager_create_and_update);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
