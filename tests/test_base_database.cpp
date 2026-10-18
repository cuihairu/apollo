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
#include <stdexcept>

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(#condition); } while (false)
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
        pending_journal_recovery();
        std::cout << "Base/database TCP, persistence, isolation, failures and replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAILED: " << error.what() << '\n';
        return 1;
    }
}
