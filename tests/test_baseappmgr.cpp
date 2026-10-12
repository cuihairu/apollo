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

// 准入闸门（P3-1 增量③：恢复相位排他——gate 关闭期 bindSession/assignWorld
// 拒新，开放后恢复；未设闸门恒放行）。
bool test_baseappmgr_admission_gate() {
    std::cout << "Running: test_baseappmgr_admission_gate..." << std::endl;

    baseappmgr::BaseAppMgr mgr(0);

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9101;
    binding.gateway_id = 7;
    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = 3;

    // 无闸门：恒放行
    TEST_ASSERT(mgr.bindSession(2001, binding), "no gate: bind admitted");
    TEST_ASSERT(mgr.assignWorld(2001, assignment), "no gate: assign admitted");
    TEST_ASSERT(mgr.unbindSession(9101), "cleanup bind");

    bool open = false;
    mgr.set_admission_gate([&open]() { return open; });

    // 恢复相位排他：拒新
    TEST_ASSERT(!mgr.bindSession(2002, binding), "closed gate: bind rejected");
    TEST_ASSERT(!mgr.assignWorld(2002, assignment), "closed gate: assign rejected");
    TEST_ASSERT(!mgr.findPlayerBySession(9101).has_value(),
                "rejected bind left no locator entry");

    // 收敛开放：恢复放行
    open = true;
    TEST_ASSERT(mgr.bindSession(2002, binding), "open gate: bind admitted");
    TEST_ASSERT(mgr.assignWorld(2002, assignment), "open gate: assign admitted");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// §6 死亡行窗口处置透传（G-1 收尾批遗留项）：进程壳 → 目录批量反查 +
// 窗口满扫描。供数走 intake（restore-not-kick 面，增量③）。
bool test_baseappmgr_window_disposition_passthrough() {
    std::cout << "Running: test_baseappmgr_window_disposition_passthrough..."
              << std::endl;

    baseappmgr::BaseAppMgr mgr(0);

    namespace session = apollo::game::session;
    std::vector<session::MirrorEntry> report;
    for (const std::uint64_t pid : {3001ULL, 3002ULL, 3003ULL}) {
        session::MirrorEntry e;
        e.player_id = pid;
        e.session_id = 9000 + pid;
        e.gateway_id = 7;
        e.zone_id = 2;
        e.state = session::PlayerDirectory::EntryState::Online;
        report.push_back(e);
    }
    {
        session::MirrorEntry e;
        e.player_id = 3004;
        e.session_id = 9004;
        e.gateway_id = 9;
        e.zone_id = 3;
        e.state = session::PlayerDirectory::EntryState::Online;
        report.push_back(e);
    }
    TEST_ASSERT(mgr.intake_directory_full_report(report) == 4, "intake 4 条");
    TEST_ASSERT(mgr.directory().size() == 4, "目录 4 条目");

    // Zone 行反查：zone2 → 3 条进窗口；反查键 0 拒绝
    TEST_ASSERT(mgr.suspend_zone_sessions(2, 100, 50) == 3, "zone2 挂起 3 条");
    TEST_ASSERT(mgr.suspend_zone_sessions(0, 100, 50) == 0, "zone=0 不处置");
    TEST_ASSERT(mgr.directory().find(3001)->state ==
                    session::PlayerDirectory::EntryState::Suspended,
                "3001 挂起");
    TEST_ASSERT(mgr.directory().find(3004)->state ==
                    session::PlayerDirectory::EntryState::Online,
                "zone3 条目不受牵连");

    // gateway 行反查：gw9 → 3004 进窗口
    TEST_ASSERT(mgr.suspend_gateway_sessions(9, 100, 50) == 1, "gw9 挂起 1 条");
    TEST_ASSERT(mgr.suspend_gateway_sessions(0, 100, 50) == 0, "gateway=0 不处置");

    // 窗口满扫描：未到期无终结，到期 4 条全终结（resume 面 1001 例已在
    // player_directory 契约单测覆盖）
    TEST_ASSERT(mgr.sweep_suspended(149) == 0, "窗口内 sweep 无终结");
    TEST_ASSERT(mgr.sweep_suspended(150) == 4, "到期 sweep 终结 4 条");
    TEST_ASSERT(mgr.directory().size() == 0, "目录清空");
    TEST_ASSERT(mgr.sweep_suspended(151) == 0, "空目录 sweep 幂等");

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
    run(test_baseappmgr_admission_gate);
    run(test_baseappmgr_window_disposition_passthrough);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
