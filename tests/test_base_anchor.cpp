#include "player_store/player_store.hpp"
#include "player_store/migration_wire.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/net/link/tcp_transport.hpp"

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

    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.dataDir = "base_anchor_test_data";  // 隔离落盘目录
    std::filesystem::create_directories(config.dataDir);

    // P1-4 口径：loadPlayer 只读已存在档案，不再凭空 bootstrap——先播种档案
    player_store::PlayerData seed;
    seed.playerId = 1001;
    seed.username = "anchor-test";
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    player_store::PlayerStore server(config);

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

    player_store::PlayerStoreConfig config;
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

    player_store::PlayerStore server(config);
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

    player_store::PlayerStoreConfig config;
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
    player_store::PlayerData seed;
    seed.playerId = 1002;
    seed.username = "shutdown-test";
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    player_store::PlayerStore server(config);
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
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1002, loaded), "archive readable after shutdown");
        TEST_ASSERT(loaded.playerId == 1002, "archive player id preserved");
        TEST_ASSERT(loaded.username == "shutdown-test", "archive content preserved");
    }

    std::cout << "  PASSED" << std::endl;

    std::filesystem::remove_all("base_anchor_test_stop_data");
    return true;
}

// ---- 真实奖励持久化（P2-4）：奖励落 Anchor 长期态，flush 载荷收集自
// Anchor（exp 覆盖写回）而非仅重写旧档案；重启播种不双记；冻结迁移
// 路径同样收集。----

bool test_reward_collect_persists_profile() {
    std::cout << "Running: test_reward_collect_persists_profile..." << std::endl;

    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.host = "127.0.0.1";
    config.port = 39004;
    config.dataDir = "base_anchor_test_reward_data/players";
    config.journalPath = "base_anchor_test_reward_data/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    player_store::PlayerData seed;
    seed.playerId = 1003;
    seed.username = "reward-test";
    seed.exp = 100;
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    {
        player_store::PlayerStore server(config);
        server.start();

        auto anchor = server.activatePlayer(1003);
        TEST_ASSERT(anchor != nullptr, "anchor activated");
        // 结算奖励正路径：AnchorRewardSink 语义（exp 键 + mark_dirty）
        apollo::game::session::AnchorRewardSink sink(*anchor);
        sink.on_reward(1003, 50);
        TEST_ASSERT(anchor->needs_save(), "reward marks anchor dirty");
        TEST_ASSERT(
            anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey) == 150,
            "anchor carries profile exp plus reward");

        server.stop();
    }

    // flush 载荷收集自 Anchor：档案 exp = 播种 100 + 奖励 50（旧路径只
    // 重写旧档案——奖励随进程消亡）
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1003, loaded), "archive readable");
        TEST_ASSERT(loaded.exp == 150, "reward collected into archive on flush");
    }

    std::cout << "  PASSED" << std::endl;
    std::filesystem::remove_all("base_anchor_test_reward_data");
    return true;
}

bool test_reward_survives_restart_no_double_count() {
    std::cout << "Running: test_reward_survives_restart_no_double_count..." << std::endl;

    const std::string root = "base_anchor_test_restart_data";
    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.host = "127.0.0.1";
    config.port = 39005;
    config.dataDir = root + "/players";
    config.journalPath = root + "/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    player_store::PlayerData seed;
    seed.playerId = 1004;
    seed.username = "restart-test";
    seed.exp = 100;
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    // 代次一：+50 落档 150
    {
        player_store::PlayerStore server(config);
        server.start();
        auto anchor = server.activatePlayer(1004);
        TEST_ASSERT(anchor != nullptr, "gen1 anchor activated");
        apollo::game::session::AnchorRewardSink(*anchor).on_reward(1004, 50);
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1004, loaded) && loaded.exp == 150,
                    "gen1 flush collected reward");
    }

    // 代次二：重启播种（anchor exp = 档案 150）再 +30 → 落档 180
    {
        player_store::PlayerStore server(config);
        server.start();
        auto anchor = server.activatePlayer(1004);
        TEST_ASSERT(anchor != nullptr, "gen2 anchor activated");
        TEST_ASSERT(
            anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey) == 150,
            "restart seeds anchor from archive (no double count)");
        apollo::game::session::AnchorRewardSink(*anchor).on_reward(1004, 30);
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1004, loaded) && loaded.exp == 180,
                    "gen2 adds on seeded baseline");
    }

    // 代次三：再激活再停——覆写幂等（不因重复 flush/播种加出 210）
    {
        player_store::PlayerStore server(config);
        server.start();
        auto anchor = server.activatePlayer(1004);
        TEST_ASSERT(anchor != nullptr, "gen3 anchor activated");
        TEST_ASSERT(
            anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey) == 180,
            "gen3 seeds from archive");
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1004, loaded) && loaded.exp == 180,
                    "idempotent across re-flush and restart");
    }

    std::cout << "  PASSED" << std::endl;
    std::filesystem::remove_all(root);
    return true;
}

bool test_freeze_collects_pending_reward() {
    std::cout << "Running: test_freeze_collects_pending_reward..." << std::endl;

    const std::string root = "base_anchor_test_freeze_data";
    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 60000;  // 冻结前不触发自动保存——增量只经冻结收口
    config.host = "127.0.0.1";
    config.port = 39006;
    config.dataDir = root + "/players";
    config.journalPath = root + "/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    player_store::PlayerData seed;
    seed.playerId = 1005;
    seed.username = "freeze-test";
    seed.exp = 100;
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    player_store::PlayerStore server(config);
    server.start();
    auto anchor = server.activatePlayer(1005);
    TEST_ASSERT(anchor != nullptr, "anchor activated");
    apollo::game::session::AnchorRewardSink(*anchor).on_reward(1005, 50);

    // 冻结段同步排平：未 flush 的奖励增量随冻结落档（否则迁移快照丢奖励）
    TEST_ASSERT(server.freezePlayer(1005), "player frozen");
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1005, loaded) && loaded.exp == 150,
                    "freeze collects pending reward");
    }
    const auto snapshot = server.snapshotPlayer(1005);
    TEST_ASSERT(snapshot.success, "snapshot taken after freeze");
    TEST_ASSERT(snapshot.profile_json.find("150") != std::string::npos,
                "snapshot carries collected exp");

    // 目标侧接权播种：adopt 后锚点 exp = 快照档案值（真源随档走）
    TEST_ASSERT(server.adoptPlayer(1006, snapshot.profile_json, 9002, 3, 5),
                "adopted with write authority");
    auto adopted = server.findAnchor(1006);
    TEST_ASSERT(adopted != nullptr, "adopted anchor online");
    TEST_ASSERT(adopted->progress(apollo::game::session::AnchorRewardSink::kExpKey) == 150,
                "adopt seeds anchor exp from adopted profile");

    server.stop();

    std::cout << "  PASSED" << std::endl;
    std::filesystem::remove_all(root);
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
    namespace migration = player_store::migration;
    namespace protocol = apollo::protocol;

    MigrationDir oldDir("base_migration_old_data");
    MigrationDir newDir("base_migration_new_data");

    player_store::PlayerStoreConfig oldCfg;
    oldCfg.workerThreads = 1;
    oldCfg.dataDir = oldDir.path + "/players";
    oldCfg.journalPath = oldDir.path + "/journal/persist.log";
    std::filesystem::create_directories(oldCfg.dataDir);
    std::filesystem::create_directories(oldDir.path + "/journal");

    player_store::PlayerStoreConfig newCfg = oldCfg;
    newCfg.dataDir = newDir.path + "/players";
    newCfg.journalPath = newDir.path + "/journal/persist.log";
    std::filesystem::create_directories(newCfg.dataDir);
    std::filesystem::create_directories(newDir.path + "/journal");

    // 老 zone：播种档案 + 激活锚点 + 会话（脏锚——freeze 要排其 journal 段）
    {
        player_store::PlayerData seed;
        seed.playerId = 42;
        seed.username = "migrate-me";
        seed.level = 5;
        seed.exp = 500;
        seed.hp = 88;
        player_store::DatabaseService seeder(oldCfg);
        TEST_ASSERT(seeder.savePlayer(seed), "old side seed archive written");
    }
    player_store::PlayerStore oldServer(oldCfg);
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
    player_store::PlayerStore newServer(newCfg);
    {
        // 未落快照先要接权 = 拒（五步序 ②→③ 不可跳）
        auto early = migration::encodeFrame(
            migration::HandoffRequest{42, 5, 8, 777, 3});
        auto reply = newServer.dispatchRequest(early);
        protocol::SessionID session = 0;
        migration::HandoffReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session) && !out.accepted,
                    "handoff without stored snapshot rejected");

        player_store::PlayerStore::PlayerSnapshotData snap;
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
        player_store::PlayerData migrated;
        player_store::DatabaseService reader(newCfg);
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

    // 937 全字段 roundtrip（source 双员——新 zone 自主拉取的拨入地址）
    {
        migration::HandoffRequest full{42, 8, 9, 777, 3, "127.0.0.1", 19610};
        auto frame = migration::encodeFrame(full);
        protocol::SessionID session = 0;
        migration::HandoffRequest back;
        TEST_ASSERT(migration::decodeFrame(frame, back, session)
                        && back.player_id == 42 && back.request_id == 8
                        && back.new_epoch == 9 && back.session_id == 777
                        && back.gateway_id == 3 && back.source_host == "127.0.0.1"
                        && back.source_port == 19610,
                    "937 roundtrip carries source address");
    }

    // 942/943 全字段 roundtrip（编排命令族：ops/fleetctl ↔ manager；耗时
    // 三员 i64 按位经 u64 通道——负值不会出现在正常耗时，位型保真即可）
    {
        migration::MigrateRequest full{
            7001, 42, "10.0.0.1", 19610, "10.0.0.2", 19611, 5};
        auto frame = migration::encodeFrame(full);
        protocol::SessionID session = 0;
        migration::MigrateRequest back;
        TEST_ASSERT(migration::decodeFrame(frame, back, session)
                        && back.request_id == 7001 && back.player_id == 42
                        && back.source_host == "10.0.0.1"
                        && back.source_port == 19610
                        && back.target_host == "10.0.0.2"
                        && back.target_port == 19611
                        && back.target_zone_id == 5,
                    "942 roundtrip carries source/target addresses");
    }
    {
        migration::MigrateReply full{
            7001, 42, true, 9, "", 3, 21, 24};
        auto frame = migration::encodeFrame(full);
        protocol::SessionID session = 0;
        migration::MigrateReply back;
        TEST_ASSERT(migration::decodeFrame(frame, back, session)
                        && back.request_id == 7001 && back.player_id == 42
                        && back.ok && back.new_epoch == 9
                        && back.error_message.empty() && back.freeze_ms == 3
                        && back.handoff_ms == 21 && back.total_ms == 24,
                    "943 roundtrip carries step timings");
    }
    return true;
}

// 真链路 ②：937 带源地址 → 新 zone 现场拨老 zone 拉 936（五步② wire 落
// 地：MigrationPuller active 拨 M1 passive，老的已先 flush 再发）。
// Windows 桩树 M1 面缺席（start false）——护栏跳过（tcp 测试同款）。
#if !defined(_WIN32)
bool test_player_migration_pull_on_handoff() {
    std::cout << "Running: test_player_migration_pull_on_handoff..." << std::endl;
    namespace migration = player_store::migration;
    namespace protocol = apollo::protocol;
    namespace link = apollo::net::link;

    // 空闲端口探针（拿完即关——重用窗口内 M1 面接手）
    std::uint16_t m1Port = 0;
    {
        link::TcpListener probe;
        TEST_ASSERT(probe.start("127.0.0.1", 0), "port probe binds ephemeral");
        m1Port = probe.port();
    }

    MigrationDir oldDir("base_pull_old_data");
    MigrationDir newDir("base_pull_new_data");

    player_store::PlayerStoreConfig oldCfg;
    oldCfg.workerThreads = 1;
    oldCfg.autoSaveIntervalMs = 10;
    oldCfg.host = "127.0.0.1";
    oldCfg.port = 39106;  // 旧 REP 面已退役，占位端口（stub 树惯例高位段）
    oldCfg.dataDir = oldDir.path + "/players";
    oldCfg.journalPath = oldDir.path + "/journal/persist.log";
    oldCfg.m1Port = m1Port;
    oldCfg.m1RouteJson = APOLLO_BASEAPP_CONTRACT_ROUTE_JSON;
    std::filesystem::create_directories(oldCfg.dataDir);
    std::filesystem::create_directories(oldDir.path + "/journal");

    // 老 zone：播种 + 激活（脏锚——② 先 flush 再发的供数）+ 起 M1 面
    {
        player_store::PlayerData seed;
        seed.playerId = 42;
        seed.username = "pulled-me";
        seed.level = 7;
        player_store::DatabaseService seeder(oldCfg);
        TEST_ASSERT(seeder.savePlayer(seed), "old side seed archive written");
    }
    player_store::PlayerStore oldServer(oldCfg);
    oldServer.start();
    TEST_ASSERT(oldServer.m1_listen_port() == m1Port, "m1 face on probed port");
    {
        auto anchor = oldServer.activatePlayer(42);
        TEST_ASSERT(anchor != nullptr, "old side anchor activated");
        apollo::game::session::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(oldServer.bindSession(42, binding), "old side session bound");
        anchor = oldServer.findAnchor(42);
        anchor->set_home_zone_id(2);
        anchor->mark_dirty("pull_drill");
    }
    // ① 停写先行（五步序：编排面先 933 再 937——snapshot 前置 Transferring）
    {
        auto reply = oldServer.dispatchRequest(
            migration::encodeFrame(migration::FreezeRequest{42, 1}));
        protocol::SessionID session = 0;
        migration::FreezeReply frozen;
        TEST_ASSERT(migration::decodeFrame(reply, frozen, session) && frozen.accepted,
                    "old side frozen before pull");
    }

    // 新 zone：不装 M1 面——937 带源地址，handler 现场拨老 zone 拉 936
    player_store::PlayerStoreConfig newCfg = oldCfg;
    newCfg.dataDir = newDir.path + "/players";
    newCfg.journalPath = newDir.path + "/journal/persist.log";
    newCfg.m1Port = 0;
    std::filesystem::create_directories(newCfg.dataDir);
    std::filesystem::create_directories(newDir.path + "/journal");
    player_store::PlayerStore newServer(newCfg);
    {
        migration::HandoffRequest req{42, 9, 8, 777, 3, "127.0.0.1", m1Port};
        auto reply = newServer.dispatchRequest(migration::encodeFrame(req));
        protocol::SessionID session = 0;
        migration::HandoffReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session), "938 reply decodes");
        TEST_ASSERT(out.accepted && out.player_id == 42 && out.request_id == 9,
                    "handoff accepted after on-the-fly pull");

        auto anchor = newServer.findAnchor(42);
        TEST_ASSERT(anchor != nullptr
                        && anchor->state() == apollo::game::session::AnchorState::Online,
                    "new side anchor online");
        TEST_ASSERT(anchor->session_binding().session_id == 777,
                    "binding restored from pulled snapshot");
        TEST_ASSERT(anchor->home_zone_id() == 2, "home zone from pulled snapshot");

        player_store::PlayerData migrated;
        player_store::DatabaseService reader(newCfg);
        TEST_ASSERT(reader.loadPlayer(42, migrated) && migrated.level == 7
                        && migrated.username == "pulled-me",
                    "flushed profile landed on new side");
    }

    // 源不可达：937 指黑洞端口 → 拒且 accepted=false（信封不炸）
    {
        migration::HandoffRequest req{43, 10, 8, 0, 0, "127.0.0.1", 1};
        auto reply = newServer.dispatchRequest(migration::encodeFrame(req));
        protocol::SessionID session = 0;
        migration::HandoffReply out;
        TEST_ASSERT(migration::decodeFrame(reply, out, session) && !out.accepted,
                    "unreachable source rejected in-band");
    }

    oldServer.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}
#endif // !_WIN32

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
    run(test_reward_collect_persists_profile);
    run(test_reward_survives_restart_no_double_count);
    run(test_freeze_collects_pending_reward);
    run(test_player_migration_faces);
#if !defined(_WIN32)
    run(test_player_migration_pull_on_handoff);
#endif

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
