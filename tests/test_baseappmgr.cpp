#include "baseappmgr/baseappmgr.hpp"
#include "apollo/game/session/world_assignment.hpp"

#include <iostream>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// baseappmgr（调度面）目录裁决与路由解析测试。
// 纯内存目录操作，不启动网络。
bool test_baseappmgr_directory_and_route() {
    std::cout << "Running: test_baseappmgr_directory_and_route..." << std::endl;

    baseappmgr::BaseAppMgr mgr(0); // 不 start()，只测目录方法

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9001;
    binding.gateway_id = 7;
    binding.gateway_addr = "gateway://127.0.0.1:8888";

    TEST_ASSERT(mgr.bindSession(1001, binding), "session bound in directory");

    const auto playerBySession = mgr.findPlayerBySession(9001);
    TEST_ASSERT(playerBySession.has_value(), "player resolved by session");
    TEST_ASSERT(*playerBySession == 1001, "resolved player id matches");

    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = 3;
    assignment.map_id = 100;
    assignment.instance_id = 200;
    assignment.space_id = 300;
    assignment.route_version = 1;

    TEST_ASSERT(mgr.assignWorld(1001, assignment), "world assignment applied");
    TEST_ASSERT(!mgr.assignWorld(0, assignment), "zero player id rejected");

    const auto resolvedAssignment = mgr.resolveWorldAssignment(1001);
    TEST_ASSERT(resolvedAssignment.has_value(), "world assignment resolved");
    TEST_ASSERT(resolvedAssignment->route_version == 1, "route version resolved");
    TEST_ASSERT(resolvedAssignment->instance_id == 200, "instance id resolved");

    // 按 session 反查（playerId=0 时走目录解析）
    const auto resolvedBySession = mgr.resolveWorldAssignment(0, 9001);
    TEST_ASSERT(resolvedBySession.has_value(), "assignment resolved via session");
    TEST_ASSERT(resolvedBySession->world_id == 3, "world id resolved via session");

    const auto resolvedBinding = mgr.resolveSessionBinding(1001, 9001);
    TEST_ASSERT(resolvedBinding.has_value(), "session binding resolved");
    TEST_ASSERT(resolvedBinding->gateway_addr == "gateway://127.0.0.1:8888", "gateway addr resolved");

    TEST_ASSERT(!mgr.resolveWorldAssignment(9999).has_value(), "unknown player has no assignment");

    TEST_ASSERT(mgr.clearWorldAssignment(1001), "world assignment cleared");
    TEST_ASSERT(!mgr.clearWorldAssignment(1001), "double clear returns false");
    TEST_ASSERT(!mgr.resolveWorldAssignment(1001).has_value(), "assignment removed");

    TEST_ASSERT(mgr.unbindSession(9001), "session unbound");
    TEST_ASSERT(!mgr.findPlayerBySession(9001).has_value(), "session locator cleaned");
    TEST_ASSERT(!mgr.resolveSessionBinding(1001).has_value(), "binding removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo BaseAppMgr Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_baseappmgr_directory_and_route);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
