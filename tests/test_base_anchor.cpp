#include "base/base_server.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/messages.hpp"

#include <filesystem>
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
// 落点裁决/目录查询（assignWorld/ResolveRoute 族）已拆至 apps/manager，
// 见 test_baseappmgr.cpp。
bool test_base_server_anchor_lifecycle() {
    std::cout << "Running: test_base_server_anchor_lifecycle..." << std::endl;

    base::BaseConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.dataDir = "base_anchor_test_data";  // 隔离落盘目录
    std::filesystem::create_directories(config.dataDir);

    // P1-4 口径：loadPlayer 只读已存在档案，不再凭空 bootstrap——先播种档案
    base::PlayerData seed;
    seed.playerId = 1001;
    seed.username = "anchor-test";
    {
        base::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

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

// G-3 优雅停机（attribute-sync §10.2）：维护闸 + stop 幂等。stub 传输树
// 无真实 REP 面——受理语义经公开的 dispatchRequest 直测。
bool test_shutdown_maintenance_gate() {
    std::cout << "Running: test_shutdown_maintenance_gate..." << std::endl;

    base::BaseConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.host = "127.0.0.1";
    config.port = 39002;  // 高位端口：stub 树 start() 无传输面，nng 复活也避撞
    config.dataDir = "base_anchor_test_gate_data/players";
    config.journalPath = "base_anchor_test_gate_data/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    base::BaseServer server(config);
    server.start();

    apollo::protocol::Ping ping;
    ping.timestamp = 42;
    const auto request = apollo::protocol::MessageCodec::encode(ping, 7);

    auto aliveHeader = apollo::protocol::MessageCodec::parseHeader(server.dispatchRequest(request));
    TEST_ASSERT(aliveHeader.type == static_cast<uint16_t>(apollo::protocol::MessageType::PONG),
                "alive server answers PING with PONG");

    server.stop();
    TEST_ASSERT(server.is_shutting_down(), "shutting-down flag set");
    TEST_ASSERT(!server.isRunning(), "no longer running");

    // §10.2-①：停机后新请求一律维护中应答（错误回包，会话号保留）
    auto gateHeader = apollo::protocol::MessageCodec::parseHeader(server.dispatchRequest(request));
    TEST_ASSERT(gateHeader.type == static_cast<uint16_t>(apollo::protocol::MessageType::ERROR),
                "shut-down server answers ERROR (maintenance gate)");
    TEST_ASSERT(gateHeader.sessionId == 7, "maintenance reply preserves session id");

    server.stop();  // 幂等：二次 stop（信号 + 析构双调用路径）不重复关停
    std::cout << "  PASSED" << std::endl;

    std::filesystem::remove_all("base_anchor_test_gate_data");
    return true;
}

// G-3 优雅停机：脏锚点最后一轮 flush + journal 追平（§10.2-②③）——
// 干净停机后 journal 零回放、档案可重读。
bool test_shutdown_flushes_journal_and_archive() {
    std::cout << "Running: test_shutdown_flushes_journal_and_archive..." << std::endl;

    base::BaseConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.host = "127.0.0.1";
    config.port = 39003;
    config.dataDir = "base_anchor_test_stop_data/players";
    config.journalPath = "base_anchor_test_stop_data/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    // P1-4 口径：先播种档案（load 不再凭空 bootstrap）
    base::PlayerData seed;
    seed.playerId = 1002;
    seed.username = "shutdown-test";
    {
        base::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    base::BaseServer server(config);
    server.start();

    auto anchor = server.activatePlayer(1002);
    TEST_ASSERT(anchor != nullptr, "anchor activated");
    anchor->mark_dirty("shutdown_test");

    server.stop();

    // §10.2-③：journal 文件面追平——重开计 pending 必须为 0（干净停机零回放）
    apollo::data::journal::PersistJournal journal(config.journalPath);
    TEST_ASSERT(journal.open(), "journal reopened after shutdown");
    TEST_ASSERT(journal.pending() == 0, "journal drained to empty on clean shutdown");

    // §10.2-②：脏锚点 flush 的载荷已落档（fresh 实例无缓存，读的是盘）
    {
        base::DatabaseService reloader(config);
        base::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1002, loaded), "archive readable after shutdown");
        TEST_ASSERT(loaded.playerId == 1002, "archive player id preserved");
        TEST_ASSERT(loaded.username == "shutdown-test", "archive content preserved");
    }

    std::cout << "  PASSED" << std::endl;

    std::filesystem::remove_all("base_anchor_test_stop_data");
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
    run(test_shutdown_maintenance_gate);
    run(test_shutdown_flushes_journal_and_archive);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
