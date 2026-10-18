#include "database/client.hpp"
#include "database/server.hpp"
#include "player_store/database_service.hpp"
#include "player_store/player_store.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/protocol/codec.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

#define CHECK(condition) do { if (!(condition)) \
    throw std::runtime_error(std::string(#condition) + " @" + std::to_string(__LINE__)); } while (false)
namespace fs = std::filesystem;
namespace protocol = apollo::protocol;

struct Fixture {
    fs::path path = fs::temp_directory_path() /
        ("apollo_base_database_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    database::ServerConfig config;
    Fixture() {
        config.host = "127.0.0.1";
        config.port = 0;
        config.route_json = APOLLO_TEST_ROUTE_JSON;
        config.storage.dataDir = (path / "database" / "players").string();
        config.storage.journalPath = (path / "database" / "journal.log").string();
    }
    ~Fixture() { std::error_code ec; fs::remove_all(path, ec); }
    database::ClientConfig client(std::uint16_t port) {
        database::ClientConfig c;
        c.port = port;
        c.route_json = config.route_json;
        return c;
    }
};

void roundtrip_and_base_restart() {
    Fixture f;
    database::Server server(f.config);
    CHECK(server.start());
    auto observer = f.client(server.port());
    observer.self_name = "storage-test-observer";
    auto client = database::makeClient(observer);
    CHECK(client->initialize());
    player_store::PlayerData profile{};
    profile.playerId = 81;
    profile.username = "base-owned";
    profile.exp = 9876543210LL;
    CHECK(client->savePlayer(profile));
    player_store::PlayerData loaded{};
    CHECK(client->loadPlayer(81, loaded));
    CHECK(loaded.exp == profile.exp);
    CHECK(!client->loadPlayer(999, loaded));

    player_store::PlayerStoreConfig baseConfig;
    // These are deliberately unusable storage paths. Remote Base must not touch them.
    fs::create_directories(f.path);
    const auto blocked = f.path / "not-a-directory";
    std::ofstream(blocked) << "blocked";
    baseConfig.dataDir = (blocked / "players").string();
    baseConfig.journalPath = (blocked / "journal.log").string();
    baseConfig.autoSaveIntervalMs = 60000;
    {
        player_store::PlayerStore base(baseConfig, database::makeClient(f.client(server.port())));
        base.start();
        auto anchor = base.activatePlayer(81);
        CHECK(anchor);
        CHECK(anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey) == profile.exp);
        anchor->add_progress(apollo::game::session::AnchorRewardSink::kExpKey, 37);
        base.stop();
    }
    {
        player_store::PlayerStore base(baseConfig, database::makeClient(f.client(server.port())));
        base.start();
        auto anchor = base.activatePlayer(81);
        CHECK(anchor);
        CHECK(anchor->progress(apollo::game::session::AnchorRewardSink::kExpKey) == profile.exp + 37);
        base.stop();
    }
    CHECK(!fs::exists(f.path / "data"));
    CHECK(client->loadPlayer(81, loaded)); // Stopping Base must not stop database.
    CHECK(loaded.exp == profile.exp + 37);
    client->shutdown();
    server.stop();

    // Read from a fresh backend, bypassing every RPC/process cache.
    player_store::DatabaseService disk(f.config.storage);
    CHECK(disk.initialize());
    CHECK(disk.loadPlayer(81, loaded));
    CHECK(loaded.exp == profile.exp + 37);
}

void reject_bad_requests_and_failures() {
    Fixture f;
    database::Server server(f.config);
    CHECK(server.start());
    protocol::DbSaveRequest bad{};
    bad.playerId = 10;
    bad.tableName = "players";
    player_store::PlayerData wrong{};
    wrong.playerId = 11;
    wrong.username = "wrong-owner";
    bad.jsonData = wrong.toJson();
    auto reply = server.dispatch(protocol::MessageCodec::encode(bad));
    auto header = protocol::MessageCodec::parseHeader(reply);
    CHECK(header.type == static_cast<std::uint16_t>(protocol::DbSaveResponse::TYPE));
    auto result = protocol::MessageCodec::decodeBody<protocol::DbSaveResponse>(
        {reply.begin() + sizeof(header), reply.end()});
    CHECK(!result.success);
    CHECK(!fs::exists(fs::path(f.config.storage.dataDir) / "player_11.json"));
    auto rejected = server.dispatch({1, 2, 3});
    CHECK(protocol::MessageCodec::parseHeader(rejected).type ==
          static_cast<std::uint16_t>(protocol::ErrorMessage::TYPE));
    auto config = f.client(server.port());
    server.stop();
    config.timeout = std::chrono::milliseconds(50);
    auto disconnected = database::makeClient(config);
    CHECK(!disconnected->initialize());
    CHECK(!disconnected->savePlayer(wrong));

    // A failed journal path is a readiness failure, never an empty "healthy" service.
    const auto blocker = f.path / "blocker";
    std::ofstream(blocker) << "x";
    f.config.storage.journalPath = (blocker / "journal.log").string();
    database::Server broken(f.config);
    CHECK(!broken.start());
    CHECK(!broken.port());
}

// L25 崩溃恢复契约（重试/失败/重启三态各自可辨）：database 运行中被终止 →
// 保存如实失败（传输失败，不假受理成功）、锚点真值保留 → database 同存储
// 同端口重启 → base 进程亦随编队重启（新存储客户端）→ 载入的是最后一次
// 已落盘真值而非断链期的未落档增量，断链期失败的写入不得复活，且复活后
// 新的写入照常落盘。跨崩溃未落档增量的重试缓冲在远程模式下不存在（无本地
// journal），此为可证明的边界而非回退成功。
void database_crash_survives_restart() {
    Fixture f;
    // 直接落盘播种（不经 server，杜绝缓存参与）
    {
        player_store::DatabaseService seed(f.config.storage);
        CHECK(seed.initialize());
        player_store::PlayerData p;
        p.playerId = 501;
        p.username = "crash-restart";
        p.exp = 1;
        CHECK(seed.savePlayer(p));
    }

    database::Server server(f.config);
    CHECK(server.start());
    const auto realPort = server.port();

    auto readerCfg = f.client(realPort);
    readerCfg.timeout = std::chrono::milliseconds(400);  // 断链期快败
    readerCfg.self_name = "crash-restart-reader";  // 同名互斥：读面独立命名，防被 base 链顶替
    auto reader = database::makeClient(readerCfg);
    CHECK(reader->initialize());

    auto clientCfg = f.client(realPort);
    clientCfg.timeout = std::chrono::milliseconds(400);  // 断链期快败

    player_store::PlayerStoreConfig baseCfg;
    baseCfg.dataDir = (f.path / "blocked" / "players").string();  // 远程 base 不触本地档案
    baseCfg.journalPath = (f.path / "blocked" / "journal.log").string();
    baseCfg.autoSaveIntervalMs = 60000;  // 手动驱动保存，排除自动保存干扰
    baseCfg.workerThreads = 1;
    baseCfg.shutdownFlushTimeoutMs = 4000;
    const auto key = apollo::game::session::AnchorRewardSink::kExpKey;
    const auto dispatch_save = [](std::unique_ptr<player_store::PlayerStore>& base,
                                  std::uint64_t id, std::int64_t exp) {
        protocol::DbSaveRequest request;
        request.playerId = id;
        request.tableName = "players";
        player_store::PlayerData data;
        data.playerId = id;
        data.exp = exp;
        request.jsonData = data.toJson();
        const auto reply = base->dispatchRequest(protocol::MessageCodec::encode(request, 0));
        const auto header = protocol::MessageCodec::parseHeader(reply);
        CHECK(header.type == static_cast<std::uint16_t>(protocol::DbSaveResponse::TYPE));
        return protocol::MessageCodec::decodeBody<protocol::DbSaveResponse>(
            {reply.begin() + sizeof(header), reply.end()});
    };

    // ① 正常路径：已落盘真值 100
    {
        std::unique_ptr<player_store::PlayerStore> base(
            new player_store::PlayerStore(baseCfg, database::makeClient(clientCfg)));
        base->start();
        auto anchor = base->activatePlayer(501);
        CHECK(anchor);
        CHECK(anchor->progress(key) == 1);
        anchor->add_progress(key, 99);  // 真值 100
        CHECK(dispatch_save(base, 501, anchor->progress(key)).success);
        base->stop();
    }
    {
        player_store::PlayerData landed;
        CHECK(reader->loadPlayer(501, landed));
        CHECK(landed.exp == 100);
    }

    // ② 断链：base 先在活链上载入，再终止 server——保存如实失败，锚点真值保留
    std::unique_ptr<player_store::PlayerStore> base(
        new player_store::PlayerStore(baseCfg, database::makeClient(clientCfg)));
    base->start();
    auto anchor = base->activatePlayer(501);
    CHECK(anchor);
    CHECK(anchor->progress(key) == 100);  // 断链前真值已可载回
    server.stop();
    anchor->add_progress(key, 50);  // 未落档增量：真值 150
    player_store::PlayerData ghost;
    ghost.playerId = 501;
    ghost.exp = 999;  // 断链期必败写入，此后不得复活
    CHECK(!reader->savePlayer(ghost));  // 传输失败：无回执不假成功
    const auto failed = dispatch_save(base, 501, anchor->progress(key));
    CHECK(!failed.success);  // 持久失败如实回执
    CHECK(anchor->progress(key) == 150);  // 失败不吞锚点真值
    base->stop();

    // ③ 复活：同存储同端口重建 database
    auto revivedCfg = f.config;
    revivedCfg.port = realPort;
    database::Server revived(revivedCfg);
    CHECK(revived.start());

    // ④ base 随编队重启（新存储客户端）：载入最后一次已落盘真值
    {
        std::unique_ptr<player_store::PlayerStore> base2(
            new player_store::PlayerStore(baseCfg, database::makeClient(clientCfg)));
        base2->start();
        auto restored = base2->activatePlayer(501);
        CHECK(restored);
        CHECK(restored->progress(key) == 100);  // 未落档的 150 不复活，已落盘的 100 存活
        restored->add_progress(key, 5);  // 复活后新的写入
        CHECK(dispatch_save(base2, 501, restored->progress(key)).success);
        base2->stop();
    }

    // ⑤ 终局盘上真值（全新后端，绕过一切缓存）
    player_store::DatabaseService disk(f.config.storage);
    CHECK(disk.initialize());
    player_store::PlayerData final;
    CHECK(disk.loadPlayer(501, final));
    CHECK(final.exp == 105);
    CHECK(final.exp != 999);
    reader->shutdown();
    revived.stop();
}

void pending_journal_recovery() {
    Fixture f;
    fs::create_directories(fs::path(f.config.storage.journalPath).parent_path());
    player_store::PlayerData profile{};
    profile.playerId = 52;
    profile.username = "recover";
    profile.exp = 123;
    {
        apollo::data::journal::PersistJournal journal(f.config.storage.journalPath);
        CHECK(journal.open());
        CHECK(journal.append("52", profile.toJson(), 1));
    }
    database::Server server(f.config);
    CHECK(server.start());
    auto client = database::makeClient(f.client(server.port()));
    CHECK(client->initialize());
    player_store::PlayerData recovered{};
    CHECK(client->loadPlayer(52, recovered));
    CHECK(recovered.exp == 123);
}

// L23 写入代次（database 半）：写端（Base）自持每玩家单调写入代次，
// database 只验单调不分配——旧进程迟到写（代次更低）如实拒绝、档案不动；
// 同代次重试幂等回执且不重复落盘（first-landed truth wins）；更高代次落盘；
// 无版本写端（writeGen=0）兼容受理且不清代次地板（旧代次仍拒）；
// database 重启后地板从档案恢复，跨重启旧代次依旧被拒。
void write_generation_ordering() {
    Fixture f;
    database::Server server(f.config);
    CHECK(server.start());
    const auto realPort = server.port();
    auto readerCfg = f.client(realPort);
    readerCfg.self_name = "write-gen-reader";  // 独立读面，防被主链顶替
    auto reader = database::makeClient(readerCfg);
    CHECK(reader->initialize());

    // 直发帧助手：显式 writeGen（模拟代次感知写端）；server 实例随步骤显式传入
    const auto dispatch_gen_save = [](database::Server& srv, std::uint64_t gen,
                                      std::int64_t exp) {
        protocol::DbSaveRequest request;
        request.playerId = 601;
        request.tableName = "players";
        request.writeGen = gen;
        player_store::PlayerData data;
        data.playerId = 601;
        data.exp = exp;
        request.jsonData = data.toJson();
        const auto reply = srv.dispatch(protocol::MessageCodec::encode(request));
        const auto header = protocol::MessageCodec::parseHeader(reply);
        CHECK(header.type == static_cast<std::uint16_t>(protocol::DbSaveResponse::TYPE));
        return protocol::MessageCodec::decodeBody<protocol::DbSaveResponse>(
            {reply.begin() + sizeof(header), reply.end()});
    };

    // ① 更高代次受理落盘：gen 5 → exp 10，档案 writeGen=5
    auto first = dispatch_gen_save(server, 5, 10);
    CHECK(first.success);
    player_store::PlayerData landed{};
    CHECK(reader->loadPlayer(601, landed));
    CHECK(landed.exp == 10);
    CHECK(landed.writeGen == 5);

    // ② 旧进程迟到写（gen 3 < 地板 5）拒绝，档案不动
    auto stale = dispatch_gen_save(server, 3, 999);
    CHECK(!stale.success);
    CHECK(reader->loadPlayer(601, landed));
    CHECK(landed.exp == 10 && landed.writeGen == 5);

    // ③ 同代次重试（回执丢失重放）：幂等成功但不重复落盘
    auto replay = dispatch_gen_save(server, 5, 999);
    CHECK(replay.success);
    CHECK(reader->loadPlayer(601, landed));
    CHECK(landed.exp == 10 && landed.writeGen == 5);

    // ④ 更高代次继续落盘：gen 6 → exp 20
    auto next = dispatch_gen_save(server, 6, 20);
    CHECK(next.success);
    CHECK(reader->loadPlayer(601, landed));
    CHECK(landed.exp == 20 && landed.writeGen == 6);

    // ⑤ database 重启：地板从档案恢复，跨重启旧代次仍拒
    reader->shutdown();
    server.stop();
    auto revivedCfg = f.config;
    revivedCfg.port = realPort;
    database::Server revived(revivedCfg);
    CHECK(revived.start());
    auto reader2 = database::makeClient(readerCfg);
    CHECK(reader2->initialize());
    CHECK(reader2->loadPlayer(601, landed));
    CHECK(landed.exp == 20 && landed.writeGen == 6);  // 重启读回带代次
    auto crossRestart = dispatch_gen_save(revived, 2, 999);
    CHECK(!crossRestart.success);
    CHECK(reader2->loadPlayer(601, landed));
    CHECK(landed.exp == 20);

    // ⑥ 无版本写端（writeGen=0）兼容受理且不清代次地板
    auto legacy = dispatch_gen_save(revived, 0, 30);
    CHECK(legacy.success);
    CHECK(reader2->loadPlayer(601, landed));
    CHECK(landed.exp == 30);
    CHECK(landed.writeGen == 6);  // 地板保持，后续旧代次仍拒
    auto staleAfterLegacy = dispatch_gen_save(revived, 4, 999);
    CHECK(!staleAfterLegacy.success);

    // ⑦ 代次感知客户端：PlayerData.writeGen 透传落盘、load 读回
    auto clientCfg = f.client(realPort);
    clientCfg.self_name = "write-gen-writer";
    auto client = database::makeClient(clientCfg);
    CHECK(client->initialize());
    player_store::PlayerData genAware{};
    genAware.playerId = 602;
    genAware.username = "gen-aware";
    genAware.writeGen = 7;
    CHECK(client->savePlayer(genAware));
    player_store::PlayerData round{};
    CHECK(client->loadPlayer(602, round));
    CHECK(round.writeGen == 7);

    client->shutdown();
    reader2->shutdown();
    revived.stop();
}

// L64 Base 半：写端代次自发分配——store 激活首载播种地板、每次落库
// 自增代次（database 半只验单调）；重启续代；接管后旧进程迟到写
//（代次 ≤ 地板）被 database 半拒，长期资料不动。
void base_write_generation_self_managed() {
    Fixture f;
    database::Server server(f.config);
    CHECK(server.start());
    const auto realPort = server.port();
    auto observerCfg = f.client(realPort);
    observerCfg.self_name = "base-gen-observer";  // 独立读面 + 旧进程替身
    auto observer = database::makeClient(observerCfg);
    CHECK(observer->initialize());

    // 建号（无版本写端兼容受理，地板 0）
    player_store::PlayerData profile{};
    profile.playerId = 51;
    profile.username = "base-gen-seed";
    profile.exp = 100;
    CHECK(observer->savePlayer(profile));

    player_store::PlayerStoreConfig baseConfig;
    fs::create_directories(f.path);
    baseConfig.dataDir = (f.path / "blocked" / "players").string();
    baseConfig.journalPath = (f.path / "blocked" / "journal.log").string();
    baseConfig.autoSaveIntervalMs = 60000;

    // 磁盘直读（绕开一切 RPC/进程缓存）
    const auto read_archive = [&](std::uint64_t& gen, std::int64_t& exp) {
        player_store::DatabaseService disk(f.config.storage);
        CHECK(disk.initialize());
        player_store::PlayerData landed{};
        CHECK(disk.loadPlayer(51, landed));
        gen = landed.writeGen;
        exp = landed.exp;
        disk.shutdown();
    };
    std::uint64_t gen = 0;
    std::int64_t exp = 0;

    // ① 本进程落库：激活首载播种地板 0 → 停机 flush 落库代次 1
    {
        player_store::PlayerStore base(baseConfig, database::makeClient(f.client(realPort)));
        base.start();
        auto anchor = base.activatePlayer(51);
        CHECK(anchor);
        anchor->add_progress(apollo::game::session::AnchorRewardSink::kExpKey, 5);
        base.stop();
    }
    read_archive(gen, exp);
    CHECK(gen == 1);
    CHECK(exp == 105);

    // ② 重启续代：首载播种地板 1 → flush 落库代次 2（单调续进）
    {
        player_store::PlayerStore base(baseConfig, database::makeClient(f.client(realPort)));
        base.start();
        auto anchor = base.activatePlayer(51);
        CHECK(anchor);
        anchor->add_progress(apollo::game::session::AnchorRewardSink::kExpKey, 7);
        base.stop();
    }
    read_archive(gen, exp);
    CHECK(gen == 2);
    CHECK(exp == 112);

    // ③ 旧进程迟到写（接管后旧 Base 复活持旧代次写）：≤ 地板 → database 半拒
    player_store::PlayerData stale{};
    stale.playerId = 51;
    stale.username = "base-gen-seed";
    stale.exp = 1;
    stale.writeGen = 1;
    CHECK(!observer->savePlayer(stale));
    read_archive(gen, exp);
    CHECK(gen == 2);
    CHECK(exp == 112);

    observer->shutdown();
    server.stop();
}

int main(int argc, char** argv) {
    try {
        if (argc == 5 && std::string(argv[1]) == "--probe") {
            database::ClientConfig config;
            config.port = static_cast<std::uint16_t>(std::stoul(argv[2]));
            config.peer_name = argv[3];
            config.self_name = "process-smoke-probe";
            config.route_json = APOLLO_TEST_ROUTE_JSON;
            auto client = database::makeClient(config);
            CHECK(client->initialize());
            player_store::PlayerData profile{};
            profile.playerId = 900001;
            profile.username = "process-persistence";
            profile.exp = 9876543210LL;
            if (std::string(argv[4]) == "seed") CHECK(client->savePlayer(profile));
            player_store::PlayerData loaded{};
            CHECK(client->loadPlayer(profile.playerId, loaded));
            CHECK(loaded.exp == profile.exp);
            return 0;
        }
        roundtrip_and_base_restart();
        reject_bad_requests_and_failures();
        database_crash_survives_restart();
        pending_journal_recovery();
        write_generation_ordering();
        base_write_generation_self_managed();
        std::cout << "Base/database TCP, persistence, isolation, failures, crash restart and replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
