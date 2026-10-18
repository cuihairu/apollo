#include "login/login_server.hpp"
#include "login/m1_base_client.hpp"
#include "gateway/m1_route_client.hpp"
#include "player_store/database_service.hpp"
#include "player_store/player_store.hpp"
#include "apollo/protocol/codec.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace protocol = apollo::protocol;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Client> void wait_ready(Client& client) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!client.peerUp() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(5ms);
    require(client.peerUp(), "RPC peer did not connect");
}
template<class Response, class Client, class Request>
Response call(Client& client, const Request& request) {
    auto frame = client.call(protocol::MessageCodec::encode(request, 1), 2s);
    require(frame.has_value(), "RPC did not return a frame");
    auto header = protocol::MessageCodec::parseHeader(*frame);
    require(header.type == static_cast<decltype(header.type)>(Response::TYPE), "Wrong RPC response type");
    return protocol::MessageCodec::decodeBody<Response>(
        std::vector<std::uint8_t>(frame->begin() + sizeof(protocol::MessageHeader), frame->end()));
}
struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-hosted-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};

void hosted_services() {
    TemporaryDirectory files;
    // 档案宿主 = Base（L19 资料归属切换）：store M1 面独立进程壳装配，
    // 不再经由 WorldApp 宿主。
    player_store::PlayerStoreConfig store_config;
    store_config.host = "127.0.0.1";
    store_config.m1Enabled = true;
    store_config.m1Port = 0;  // 内核分配
    store_config.m1SelfName = "base";
    store_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_config.dataDir = (files.path / "players").string();
    store_config.journalPath = (files.path / "journal/persist.log").string();
    {
        player_store::DatabaseService database(store_config);
        require(database.initialize(), "Profile database initialization failed");
        player_store::PlayerData profile{};
        profile.playerId = 1001;
        profile.username = "player1";
        profile.exp = 10;
        require(database.createPlayer(profile), "Profile creation failed");
        database.shutdown();
    }
    player_store::PlayerStore base(store_config);
    base.start();
    require(base.m1_listen_port(), "Base did not host player store");

    login::LoginConfig config;
    config.host = "127.0.0.1";
    config.m1Enabled = true;
    config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    config.directoryEnabled = true;
    config.directoryEpochPath = (files.path / "login/epochs.json").string();
    config.worldStoreM1Port = base.m1_listen_port();
    config.initialWorldId = 33;
    login::LoginServer login(config);
    login.start();
    require(login.admissionPort() && login.onlineDirectory()->listenPort(), "Login did not host admission/directory");

    login::M1BaseClientConfig client_config;
    client_config.self_name = "gateway";
    client_config.peer_name = "login";
    client_config.port = login.admissionPort();
    client_config.route_json = APOLLO_TEST_ROUTE_JSON;
    auto client = login::makeM1BaseClient(client_config);
    require(bool(client), "Admission client did not start");
    wait_ready(*client);
    protocol::LoginRequest request{};
    request.username = "player1";
    request.password = "pass1";
    // Both outbound and inbound links are asynchronous; wait for admission readiness
    // by retrying the authenticated request, without creating a synthetic session.
    protocol::LoginResponse response{};
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    do {
        response = call<protocol::LoginResponse>(*client, request);
        if (!response.success) std::this_thread::sleep_for(10ms);
    } while (!response.success && std::chrono::steady_clock::now() < deadline);
    require(response.success && response.playerId == 1001 && !response.loginTicket.empty(), "Real login failed");
    auto avatar = base.findAvatar(1001);
    require(avatar && avatar->session_binding().session_id == response.sessionId, "World did not bind session");

    gateway::M1RouteClientConfig route_config;
    route_config.port = login.onlineDirectory()->listenPort();
    route_config.route_json = APOLLO_TEST_ROUTE_JSON;
    auto route = gateway::makeM1RouteClient(route_config);
    require(bool(route), "Route client did not start");
    wait_ready(*route);
    protocol::PlayerResolveRouteRequest query{};
    query.sessionId = response.sessionId;
    auto resolved = call<protocol::PlayerResolveRouteResponse>(*route, query);
    require(resolved.success && resolved.playerId == 1001 && resolved.worldId == 33,
            "Gateway cannot resolve hosted Login directory");
    require(resolved.sessionId == response.sessionId && resolved.gatewayAddr == "tcp://127.0.0.1:8888",
            "Resolved session binding differs from login");

    request.password = "invalid";
    require(!call<protocol::LoginResponse>(*client, request).success, "Wrong password accepted");
    request.username = "player2";
    request.password = "pass2";
    require(!call<protocol::LoginResponse>(*client, request).success, "Missing profile fabricated");

    route->stop();
    client->stop();
    const auto before_stop = std::chrono::steady_clock::now();
    login.stop();
    require(std::chrono::steady_clock::now() - before_stop < 1s, "Login shutdown blocked on cleanup interval");
    std::uint64_t epoch_floor = 0;
    std::ifstream(config.directoryEpochPath) >> epoch_floor;
    require(epoch_floor > 1, "Directory authority epoch was not persisted");
    {
        login::LoginServer restarted(config);
        restarted.start();
        require(restarted.onlineDirectory()->directory().epoch_store_ok(), "Directory epoch recovery failed");
        require(restarted.onlineDirectory()->bindSession(1001, avatar->session_binding()), "Recovered directory could not bind session");
        std::uint64_t recovered_floor = 0;
        std::ifstream(config.directoryEpochPath) >> recovered_floor;
        require(recovered_floor > epoch_floor, "Restart regressed authority epoch");
        restarted.stop();
    }
    base.stop();
    {
        player_store::DatabaseService database(store_config);
        require(database.initialize(), "Profile reload initialization failed");
        player_store::PlayerData profile{};
        require(database.loadPlayer(1001, profile) && profile.exp == 10 && profile.username == "player1",
                "Base shutdown damaged persistent profile");
        database.shutdown();
    }
    login::LoginConfig unavailable;
    unavailable.gatewayUrls = config.gatewayUrls;
    login::LoginServer no_store(unavailable);
    request.username = "player1";
    request.password = "pass1";
    auto rejected = no_store.dispatchRequest(protocol::MessageCodec::encode(request, 1));
    auto denied = protocol::MessageCodec::decodeBody<protocol::LoginResponse>(
        std::vector<std::uint8_t>(rejected.begin() + sizeof(protocol::MessageHeader), rejected.end()));
    require(!denied.success && denied.loginTicket.empty(), "Unavailable store issued login ticket");
}
}
int main() {
#if defined(_WIN32)
    std::cout << "SKIP: hosted service transport requires POSIX TCP\n";
    return 0;
#else
    try { hosted_services(); }
    catch (const std::exception& ex) { std::cerr << "FAIL: " << ex.what() << '\n'; return 1; }
    std::cout << "Hosted World player store and Login directory passed\n";
    return 0;
#endif
}
