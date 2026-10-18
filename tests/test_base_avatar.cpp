#include "player_store/player_store.hpp"
#include "player_store/migration_wire.hpp"
#include "apollo/game/base/avatar_reward_sink.hpp"
#include "apollo/game/base/avatar.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/net/link/tcp_transport.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <thread>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

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

// baseapp（数据面）锚点生命周期测试。
// 落点裁决/目录查询（assignWorld/ResolveRoute 族）已拆至 apps/baseappmgr，
// 见 test_baseappmgr.cpp。
bool test_base_server_avatar_lifecycle() {
    std::cout << "Running: test_base_server_avatar_lifecycle..." << std::endl;

    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.dataDir = "base_avatar_test_data";  // 隔离落盘目录
    const MigrationDir dataGuard(config.dataDir);  // 预清残留 + 退出回收（失败路径不泄漏）

    // P1-4 口径：loadPlayer 只读已存在档案，不再凭空 bootstrap——先播种档案
    player_store::PlayerData seed;
    seed.playerId = 1001;
    seed.username = "avatar-test";
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    player_store::PlayerStore server(config);

    auto avatar = server.activatePlayer(1001);
    TEST_ASSERT(avatar != nullptr, "avatar activated");
    TEST_ASSERT(avatar->player_id() == 1001, "player id preserved");
    TEST_ASSERT(avatar->state() == apollo::game::base::AvatarState::Online, "avatar online after activate");

    apollo::game::base::SessionBinding binding;
    binding.session_id = 9001;
    binding.gateway_id = 7;
    binding.gateway_addr = "gateway://127.0.0.1:8888";

    TEST_ASSERT(server.bindSession(1001, binding), "session bound");

    avatar = server.findAvatar(1001);
    TEST_ASSERT(avatar != nullptr, "avatar found after bind");
    TEST_ASSERT(avatar->session_binding().session_id == 9001, "session stored on avatar");

    const auto playerBySession = server.findPlayerBySession(9001);
    TEST_ASSERT(playerBySession.has_value(), "player resolved by session");
    TEST_ASSERT(*playerBySession == 1001, "resolved player id matches");

    TEST_ASSERT(server.unbindSession(9001), "session unbound");
    TEST_ASSERT(avatar->state() == apollo::game::base::AvatarState::Disconnected, "avatar disconnected after unbind");
    TEST_ASSERT(!avatar->session_binding().is_bound(), "binding removed from avatar");
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
    config.dataDir = "base_avatar_test_gate_data/players";
    config.journalPath = "base_avatar_test_gate_data/journal/persist.log";
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

    std::filesystem::remove_all("base_avatar_test_gate_data");
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
    config.dataDir = "base_avatar_test_stop_data/players";
    config.journalPath = "base_avatar_test_stop_data/journal/persist.log";
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

    auto avatar = server.activatePlayer(1002);
    TEST_ASSERT(avatar != nullptr, "avatar activated");
    avatar->mark_dirty("shutdown_test");

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

    std::filesystem::remove_all("base_avatar_test_stop_data");
    return true;
}

// ---- 真实奖励持久化（P2-4）：奖励落 Avatar 长期态，flush 载荷收集自
// Avatar（exp 覆盖写回）而非仅重写旧档案；重启播种不双记；冻结迁移
// 路径同样收集。----

bool test_reward_collect_persists_profile() {
    std::cout << "Running: test_reward_collect_persists_profile..." << std::endl;

    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.host = "127.0.0.1";
    config.port = 39004;
    config.dataDir = "base_avatar_test_reward_data/players";
    config.journalPath = "base_avatar_test_reward_data/journal/persist.log";
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

        auto avatar = server.activatePlayer(1003);
        TEST_ASSERT(avatar != nullptr, "avatar activated");
        // 结算奖励正路径：AvatarRewardSink 语义（exp 键 + mark_dirty）
        apollo::game::base::AvatarRewardSink sink(*avatar);
        sink.on_reward(1003, 50);
        TEST_ASSERT(avatar->needs_save(), "reward marks avatar dirty");
        TEST_ASSERT(
            avatar->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 150,
            "avatar carries profile exp plus reward");

        server.stop();
    }

    // flush 载荷收集自 Avatar：档案 exp = 播种 100 + 奖励 50（旧路径只
    // 重写旧档案——奖励随进程消亡）
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1003, loaded), "archive readable");
        TEST_ASSERT(loaded.exp == 150, "reward collected into archive on flush");
    }

    std::cout << "  PASSED" << std::endl;
    std::filesystem::remove_all("base_avatar_test_reward_data");
    return true;
}

bool test_reward_survives_restart_no_double_count() {
    std::cout << "Running: test_reward_survives_restart_no_double_count..." << std::endl;

    const std::string root = "base_avatar_test_restart_data";
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
        auto avatar = server.activatePlayer(1004);
        TEST_ASSERT(avatar != nullptr, "gen1 avatar activated");
        apollo::game::base::AvatarRewardSink(*avatar).on_reward(1004, 50);
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.loadPlayer(1004, loaded) && loaded.exp == 150,
                    "gen1 flush collected reward");
    }

    // 代次二：重启播种（avatar exp = 档案 150）再 +30 → 落档 180
    {
        player_store::PlayerStore server(config);
        server.start();
        auto avatar = server.activatePlayer(1004);
        TEST_ASSERT(avatar != nullptr, "gen2 avatar activated");
        TEST_ASSERT(
            avatar->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 150,
            "restart seeds avatar from archive (no double count)");
        apollo::game::base::AvatarRewardSink(*avatar).on_reward(1004, 30);
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
        auto avatar = server.activatePlayer(1004);
        TEST_ASSERT(avatar != nullptr, "gen3 avatar activated");
        TEST_ASSERT(
            avatar->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 180,
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

    const std::string root = "base_avatar_test_freeze_data";
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
    auto avatar = server.activatePlayer(1005);
    TEST_ASSERT(avatar != nullptr, "avatar activated");
    apollo::game::base::AvatarRewardSink(*avatar).on_reward(1005, 50);

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
    auto adopted = server.findAvatar(1006);
    TEST_ASSERT(adopted != nullptr, "adopted avatar online");
    TEST_ASSERT(adopted->progress(apollo::game::base::AvatarRewardSink::kExpKey) == 150,
                "adopt seeds avatar exp from adopted profile");

    server.stop();

    std::cout << "  PASSED" << std::endl;
    std::filesystem::remove_all(root);
    return true;
}

// ---- 玩家迁移受理面（P0-2 路 2 第一步，backup-revive §9.2 五步 zone 侧；
// wire = 契约 933-938，migration_wire 全帧经 dispatchRequest 分流）----

// 未知/未受理消息型契约（L69 兼容面）：新增代次消息型（如 squad 族后续
// 迭代）到达未升级受理面 = 结构化 ERROR 回包（非崩溃、非断链），会话号
// 保留，且后续已知类型受理不受污染——增量协议演进的受理面底线。
bool test_dispatch_unknown_type_contract() {
    std::cout << "Running: test_dispatch_unknown_type_contract..." << std::endl;

    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 60000;
    config.host = "127.0.0.1";
    config.port = 39008;  // 高位端口：stub 树 start() 无传输面，避撞
    config.dataDir = "base_avatar_test_unknown_data/players";
    config.journalPath = "base_avatar_test_unknown_data/journal/persist.log";
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));

    player_store::PlayerStore server(config);
    server.start();

    // 两类「新代型」：未受理的既有枚举成员 + 枚举之外的裸值
    const uint16_t unknown_types[] = {
        static_cast<uint16_t>(apollo::protocol::MessageType::SQUAD_LAUNCH),
        0x07FF,
    };
    for (const auto type : unknown_types) {
        apollo::protocol::MessageHeader header{};
        header.magic = apollo::protocol::MessageHeader::MAGIC;
        header.version = apollo::protocol::MessageHeader::CURRENT_VERSION;
        header.type = type;
        header.length = 4;
        header.sequence = 1;
        header.sessionId = 13;
        std::vector<uint8_t> frame(sizeof(header) + 4, 0x5a);
        std::memcpy(frame.data(), &header, sizeof(header));

        const auto reply = server.dispatchRequest(frame);
        const auto replyHeader = apollo::protocol::MessageCodec::parseHeader(reply);
        TEST_ASSERT(replyHeader.type ==
                        static_cast<uint16_t>(apollo::protocol::MessageType::ERROR),
                    "unknown type answered with ERROR envelope");
        TEST_ASSERT(replyHeader.sessionId == 13, "unknown type reply keeps session id");

        apollo::protocol::ErrorMessage err;
        apollo::protocol::MessageCodec::decodeBody(
            {reply.begin() + sizeof(replyHeader), reply.end()}, err);
        TEST_ASSERT(!err.message.empty(), "unknown type error carries reason");
    }

    // 受理面未被污染：已知类型照常受理
    apollo::protocol::Ping ping;
    ping.timestamp = 7;
    const auto alive =
        server.dispatchRequest(apollo::protocol::MessageCodec::encode(ping, 13));
    TEST_ASSERT(apollo::protocol::MessageCodec::parseHeader(alive).type ==
                    static_cast<uint16_t>(apollo::protocol::MessageType::PONG),
                "known type still served after unknown type");

    server.stop();
    std::filesystem::remove_all("base_avatar_test_unknown_data");
    std::cout << "  PASSED" << std::endl;
    return true;
}

// L69 解码兼容面：合法帧头下的新旧/畸形消息体结构化边界。未升级客户端的
// 旧形消息体（缺字段 → 默认值容忍）、携未知未来字段的档案载荷（忽略不污染
// 已知字段）都不得炸受理面；畸形 body（非 JSON）必须结构化 ERROR 回包
// （会话号保留），绝不向泵线程抛异常。
bool test_decode_compat_structured_faces() {
    std::cout << "Running: test_decode_compat_structured_faces..." << std::endl;
    namespace protocol = apollo::protocol;
    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 60000;
    config.host = "127.0.0.1";
    config.port = 39009;  // 高位端口：stub 树 start() 无传输面，避撞
    config.dataDir = "base_avatar_test_compat_data/players";
    config.journalPath = "base_avatar_test_compat_data/journal/persist.log";
    std::filesystem::create_directories(config.dataDir);
    std::filesystem::create_directories(
        config.journalPath.substr(0, config.journalPath.find_last_of('/')));
    player_store::PlayerStore server(config);
    server.start();

    // ① 畸形 body（非 JSON）：结构化 ERROR 回包，会话号保留，不向泵抛异常
    {
        protocol::MessageHeader header{};
        header.magic = protocol::MessageHeader::MAGIC;
        header.version = protocol::MessageHeader::CURRENT_VERSION;
        header.type = static_cast<uint16_t>(protocol::MessageType::DB_LOAD_REQUEST);
        header.length = 8;
        header.sequence = 1;
        header.sessionId = 21;
        std::vector<uint8_t> frame(sizeof(header) + 8, 0);
        std::memcpy(frame.data(), &header, sizeof(header));
        std::memcpy(frame.data() + sizeof(header), "not json", 8);
        const auto reply = server.dispatchRequest(frame);
        const auto replyHeader = protocol::MessageCodec::parseHeader(reply);
        TEST_ASSERT(replyHeader.type ==
                        static_cast<uint16_t>(protocol::MessageType::ERROR),
                    "malformed body answered with ERROR envelope");
        TEST_ASSERT(replyHeader.sessionId == 21, "malformed body keeps session id");
        protocol::ErrorMessage err;
        protocol::MessageCodec::decodeBody(
            {reply.begin() + sizeof(replyHeader), reply.end()}, err);
        TEST_ASSERT(!err.message.empty(), "malformed body error carries reason");
    }

    // ② 旧形消息体（缺字段 → 默认值容忍）：结构化响应而非异常
    {
        protocol::DbLoadRequest legacy{};
        const auto reply =
            server.dispatchRequest(protocol::MessageCodec::encode(legacy, 22));
        const auto replyHeader = protocol::MessageCodec::parseHeader(reply);
        TEST_ASSERT(replyHeader.type ==
                        static_cast<uint16_t>(protocol::DbLoadResponse::TYPE),
                    "legacy minimal body still decodes to response");
        auto resp = protocol::MessageCodec::decodeBody<protocol::DbLoadResponse>(
            {reply.begin() + sizeof(replyHeader), reply.end()});
        TEST_ASSERT(!resp.success, "playerId 0 load rejected structurally");
    }

    // ③ 档案载荷前后兼容：未知字段忽略不污染、缺字段默认不炸
    {
        player_store::PlayerData data = player_store::PlayerData::fromJson(
            R"({"playerId":31,"username":"compat","futureField":123,"futureStr":"x"})");
        TEST_ASSERT(data.playerId == 31 && data.username == "compat",
                    "unknown fields do not pollute known fields");
        player_store::PlayerData minimal =
            player_store::PlayerData::fromJson(R"({"playerId":32})");
        TEST_ASSERT(minimal.playerId == 32 && minimal.username.empty() &&
                        minimal.exp == 0,
                    "old minimal archive decodes with defaults");
    }

    server.stop();
    std::filesystem::remove_all("base_avatar_test_compat_data");
    std::cout << "  PASSED" << std::endl;
    return true;
}

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
        auto avatar = oldServer.activatePlayer(42);
        TEST_ASSERT(avatar != nullptr, "old side avatar activated");
        apollo::game::base::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(oldServer.bindSession(42, binding), "old side session bound");
        avatar = oldServer.findAvatar(42);
        avatar->set_home_zone_id(2);
        TEST_ASSERT(avatar->needs_save(), "old side avatar dirty before freeze");
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
        auto avatar = oldServer.findAvatar(42);
        TEST_ASSERT(avatar->state()
                        == apollo::game::base::AvatarState::Transferring,
                    "avatar read-only after freeze");
        TEST_ASSERT(!avatar->needs_save(), "journal segment drained (not dirty)");
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
                    "avatar metadata carried");
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

        auto avatar = newServer.findAvatar(42);
        TEST_ASSERT(avatar != nullptr
                        && avatar->state() == apollo::game::base::AvatarState::Online,
                    "new side avatar online");
        TEST_ASSERT(avatar->session_binding().session_id == 777
                        && avatar->session_binding().gateway_id == 3,
                    "binding restored on new side");
        TEST_ASSERT(avatar->home_zone_id() == 2, "home zone from snapshot");

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
// 存档串行化 + 故障重启（P2 存档竞争清账验收）：「自动保存、RPC 保存、
// 保存回调」三线并跑——同玩家不并发落盘、旧快照不回卷新数据、exp 以锚点
// 为真源；停机五阶段排平后重启读回 level/exp 恒定，journal 追平不残留。
bool test_save_race_and_restart_persistence() {
    std::cout << "Running: test_save_race_and_restart_persistence..." << std::endl;

    namespace fs = std::filesystem;
    const std::string root = "base_avatar_test_race_data";
    fs::remove_all(root);
    fs::create_directories(root + "/players");
    fs::create_directories(root + "/journal");

    player_store::PlayerStoreConfig config;
    config.workerThreads = 4;
    config.autoSaveIntervalMs = 20;
    config.host = "127.0.0.1";
    config.port = 39007;
    config.dataDir = root + "/players";
    config.journalPath = root + "/journal/persist.log";

    player_store::PlayerData seed;
    seed.playerId = 9001;
    seed.username = "race-test";
    seed.level = 1;
    seed.exp = 100;
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    player_store::PlayerStore server(config);
    server.start();
    const auto avatar = server.activatePlayer(9001);
    TEST_ASSERT(avatar != nullptr, "avatar activated");
    avatar->add_progress(apollo::game::base::AvatarRewardSink::kExpKey, 150);
    avatar->mark_dirty("race_seed");

    // 线①（模拟权威）：奖励单向落锚点——exp 真源的唯一写者
    std::atomic<bool> done{false};
    std::thread sim([&] {
        for (int i = 0; i < 20; ++i) {
            avatar->add_progress(apollo::game::base::AvatarRewardSink::kExpKey, 10);
            avatar->mark_dirty("sim_tick");
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
        done.store(true);
    });

    // 线②（RPC 受理面）：载荷带 level=7 与旧 exp=9999——旧 exp 回放必须被
    // 锚点真源挡下，level 字段必须不被 flush 旧基线回卷
    std::thread rpc([&] {
        for (int i = 0; i < 20; ++i) {
            player_store::PlayerData payload = seed;
            payload.playerId = 9001;
            payload.level = 7;
            payload.exp = 9999;
            apollo::protocol::DbSaveRequest req;
            req.playerId = 9001;
            req.jsonData = payload.toJson();
            server.dispatchRequest(apollo::protocol::MessageCodec::encode(req, 0));
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    });

    sim.join();
    rpc.join();

    // 停机五阶段：flush → 队列排干 → journal 追平（故障重启的优雅面）
    server.stop();

    const std::int64_t expectedExp = 100 + 150 + 20 * 10;
    {
        player_store::DatabaseService reader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reader.loadPlayer(9001, loaded), "archive readable after restart");
        TEST_ASSERT(loaded.level == 7, "RPC payload field survived (no stale rollback)");
        TEST_ASSERT(loaded.exp == expectedExp,
                    "avatar-truth exp survived (stale replay blocked, no loss/no dup)");
    }

    // 重启播种一致：新代次锚点 exp == 档案 exp（不叠加、不丢）
    player_store::PlayerStore next(config);
    const auto reactivated = next.activatePlayer(9001);
    TEST_ASSERT(reactivated != nullptr, "reactivated after restart");
    TEST_ASSERT(reactivated->progress(apollo::game::base::AvatarRewardSink::kExpKey)
                    == expectedExp,
                "restart seed equals archive exp");
    next.stop();

    fs::remove_all(root);
    std::cout << "  PASSED" << std::endl;
    return true;
}
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
        auto avatar = oldServer.activatePlayer(42);
        TEST_ASSERT(avatar != nullptr, "old side avatar activated");
        apollo::game::base::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(oldServer.bindSession(42, binding), "old side session bound");
        avatar = oldServer.findAvatar(42);
        avatar->set_home_zone_id(2);
        avatar->mark_dirty("pull_drill");
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

        auto avatar = newServer.findAvatar(42);
        TEST_ASSERT(avatar != nullptr
                        && avatar->state() == apollo::game::base::AvatarState::Online,
                    "new side avatar online");
        TEST_ASSERT(avatar->session_binding().session_id == 777,
                    "binding restored from pulled snapshot");
        TEST_ASSERT(avatar->home_zone_id() == 2, "home zone from pulled snapshot");

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
    std::cout << "=== Apollo Base Avatar Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_base_server_avatar_lifecycle);
    run(test_shutdown_maintenance_gate);
    run(test_shutdown_flushes_journal_and_archive);
    run(test_reward_collect_persists_profile);
    run(test_reward_survives_restart_no_double_count);
    run(test_freeze_collects_pending_reward);
    run(test_save_race_and_restart_persistence);
    run(test_dispatch_unknown_type_contract);
    run(test_decode_compat_structured_faces);
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
