#include "base/base_server.hpp"
#include "apollo/game/session/player_anchor.hpp"

#include <iostream>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// baseapp（数据面）锚点生命周期测试。
// 落点裁决/目录查询（assignWorld/ResolveRoute 族）已拆至 apps/baseappmgr，
// 见 test_baseappmgr.cpp。
bool test_base_server_anchor_lifecycle() {
    std::cout << "Running: test_base_server_anchor_lifecycle..." << std::endl;

    base::BaseConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;

    base::BaseServer server(config);

    auto anchor = server.activatePlayer(1001);
    TEST_ASSERT(anchor != nullptr, "anchor activated");
    TEST_ASSERT(anchor->player_id() == 1001, "player id preserved");
    TEST_ASSERT(anchor->state() == apollo::game::session::AnchorState::Online, "anchor online after activate");

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9001;
    binding.gateway_id = 7;
    binding.gateway_addr = "gateway://127.0.0.1:8888";

    TEST_ASSERT(server.bindSession(1001, binding), "session bound");

    anchor = server.findAnchor(1001);
    TEST_ASSERT(anchor != nullptr, "anchor found after bind");
    TEST_ASSERT(anchor->session_binding().session_id == 9001, "session stored on anchor");

    const auto playerBySession = server.findPlayerBySession(9001);
    TEST_ASSERT(playerBySession.has_value(), "player resolved by session");
    TEST_ASSERT(*playerBySession == 1001, "resolved player id matches");

    TEST_ASSERT(server.unbindSession(9001), "session unbound");
    TEST_ASSERT(anchor->state() == apollo::game::session::AnchorState::Disconnected, "anchor disconnected after unbind");
    TEST_ASSERT(!anchor->session_binding().is_bound(), "binding removed from anchor");
    TEST_ASSERT(!server.findPlayerBySession(9001).has_value(), "session locator cleaned");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo Base Anchor Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_base_server_anchor_lifecycle);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
