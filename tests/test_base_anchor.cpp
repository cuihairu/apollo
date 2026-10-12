#include "base/base_server.hpp"
#include "base/migration_wire.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"
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
// 落点裁决/目录查询（assignWorld/ResolveRoute 族）已拆至 apps/baseappmgr，
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

// ---- 玩家迁移受理面（P0-2 路 2 第一步，backup-revive §9.2 五步 zone 侧；
// wire = 契约 933-938，migration_wire 全帧经 dispatchRequest 分流）----

// 隔离数据目录护栏：预清残留
struct MigrationDir {
    std::string path;
    explicit MigrationDir(std::string p) : path(std::move(p)) {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
        std::filesystem::create_directories(path);
    }
    ~MigrationDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

bool test_player_migration_faces() {
    std::cout << "Running: test_player_migration_faces..." << std::endl;
    namespace migration = base::migration;
    namespace protocol = apollo::protocol;

    MigrationDir oldDir("base_migration_old_data");
    MigrationDir newDir("base_migration_new_data");

    base::BaseConfig oldCfg;
    oldCfg.workerThreads = 1;
    oldCfg.dataDir = oldDir.path + "/players";
    oldCfg.journalPath = oldDir.path + "/journal/persist.log";
    std::filesystem::create_directories(oldCfg.dataDir);
    std::filesystem::create_directories(oldDir.path + "/journal");

    base::BaseConfig newCfg = oldCfg;
    newCfg.dataDir = newDir.path + "/players";
    newCfg.journalPath = newDir.path + "/journal/persist.log";
    std::filesystem::create_directories(newCfg.dataDir);
    std::filesystem::create_directories(newDir.path + "/journal");

    // 老 zone：播种档案 + 激活锚点 + 会话（脏锚——freeze 要排其 journal 段）
    {
        base::PlayerData seed;
        seed.playerId = 42;
        seed.username = "migrate-me";
        seed.level = 5;
        seed.exp = 500;
        seed.hp = 88;
        base::DatabaseService seeder(oldCfg);
        TEST_ASSERT(seeder.savePlayer(seed), "old side seed archive written");
    }
    base::BaseServer oldServer(oldCfg);
    {
        auto anchor = oldServer.activatePlayer(42);
        TEST_ASSERT(anchor != nullptr, "old side anchor activated");
        apollo::game::session::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(oldServer.bindSession(42, binding), "old side session bound");
        anchor = oldServer.findAnchor(42);
        anchor->set_home_zone_id(2);
        TEST_ASSERT(anchor->needs_save(), "old side anchor dirty before freeze");
    }

    // ① 停写（933 → 934 accepted；Transferring + journal 段排平）
    {
        auto frame = migration::encodeFrame(migration::FreezeRequest{42, 1});
        auto reply = oldServer.dispatchRequest(frame);
        protocol::SessionID session = 0;
        migration::FreezeReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session), "934 reply decodes");
        TEST_ASSERT(out.accepted && out.player_id == 42 && out.request_id == 1,
                    "freeze accepted with echo keys");
        auto anchor = oldServer.findAnchor(42);
        TEST_ASSERT(anchor->state()
                        == apollo::game::session::AnchorState::Transferring,
                    "anchor read-only after freeze");
        TEST_ASSERT(!anchor->needs_save(), "journal segment drained (not dirty)");
    }
    // 未知玩家拒
    {
        auto frame = migration::encodeFrame(migration::FreezeRequest{999, 2});
        auto reply = oldServer.dispatchRequest(frame);
        protocol::SessionID session = 0;
        migration::FreezeReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session) && !out.accepted,
                    "freeze unknown player rejected");
    }

    // ② 快照（935 → 936；未冻结拒 + 冻结后字段齐）
    {
        auto frame = migration::encodeFrame(migration::SnapshotRequest{42, 3});
        auto reply = oldServer.dispatchRequest(frame);
        protocol::SessionID session = 0;
        migration::SnapshotReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session), "936 reply decodes");
        TEST_ASSERT(out.success, "snapshot ready after freeze");
        TEST_ASSERT(out.profile_json.find("\"playerId\":42") != std::string::npos
                        && out.profile_json.find("\"level\":5") != std::string::npos,
                    "profile json carried as-is (ADR-007)");
        TEST_ASSERT(out.session_id == 777 && out.gateway_id == 3
                        && out.home_zone_id == 2,
                    "anchor metadata carried");
    }
    {
        // 未冻结玩家（43 无档案无锚）快照拒
        auto frame = migration::encodeFrame(migration::SnapshotRequest{43, 4});
        auto reply = oldServer.dispatchRequest(frame);
        protocol::SessionID session = 0;
        migration::SnapshotReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session) && !out.success,
                    "snapshot of unfrozen player rejected");
    }

    // 新 zone：②' 快照落位 → ③ 接权（937 → 938 accepted）
    base::BaseServer newServer(newCfg);
    {
        // 未落快照先要接权 = 拒（五步序 ②→③ 不可跳）
        auto early = migration::encodeFrame(
            migration::HandoffRequest{42, 5, 8, 777, 3});
        auto reply = newServer.dispatchRequest(early);
        protocol::SessionID session = 0;
        migration::HandoffReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session) && !out.accepted,
                    "handoff without stored snapshot rejected");

        base::BaseServer::PlayerSnapshotData snap;
        snap.success = true;
        snap.profile_json = "{\"playerId\":42,\"username\":\"migrate-me\",\"level\":5,"
                            "\"exp\":500,\"hp\":88,\"maxHp\":100,\"mp\":50,\"maxMp\":50,"
                            "\"x\":1.5,\"y\":2.5,\"z\":3.5}";
        snap.home_zone_id = 2;
        newServer.storeMigrationSnapshot(42, snap);

        auto frame = migration::encodeFrame(
            migration::HandoffRequest{42, 6, 8, 777, 3});
        reply = newServer.dispatchRequest(frame);
        TEST_ASSERT(migration::decodeFrame(reply, out, session),
                    "938 reply decodes");
        TEST_ASSERT(out.accepted && out.player_id == 42 && out.request_id == 6,
                    "handoff accepted (write authority moved)");

        auto anchor = newServer.findAnchor(42);
        TEST_ASSERT(anchor != nullptr
                        && anchor->state() == apollo::game::session::AnchorState::Online,
                    "new side anchor online");
        TEST_ASSERT(anchor->session_binding().session_id == 777
                        && anchor->session_binding().gateway_id == 3,
                    "binding restored on new side");
        TEST_ASSERT(anchor->home_zone_id() == 2, "home zone from snapshot");

        // 档案真落新目录（loadPlayer 校验内容一致）
        base::PlayerData migrated;
        base::DatabaseService reader(newCfg);
        TEST_ASSERT(reader.loadPlayer(42, migrated) && migrated.level == 5
                        && migrated.username == "migrate-me",
                    "profile landed on new side");
    }

    // 坏帧：非迁移 type / 截断 body → ErrorMessage（Unknown 同族惯例）
    {
        auto frame = migration::encodeFrame(migration::FreezeRequest{42, 7});
        frame.resize(frame.size() - 4);  // 截 body
        auto reply = oldServer.dispatchRequest(frame);
        auto header = protocol::MessageCodec::parseHeader(reply);
        TEST_ASSERT(header.type
                        == static_cast<uint16_t>(protocol::MessageType::ERROR),
                    "truncated freeze request answered with error frame");
    }
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
    run(test_player_migration_faces);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
