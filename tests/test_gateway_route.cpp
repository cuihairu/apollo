#include "gateway/gateway_server.hpp"
#include "gateway/ingress/gateway_connection_registry.hpp"
#include "gateway/ingress/client_packet_dispatcher.hpp"
#include "gateway/session_manager.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <iostream>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool test_gateway_route_snapshot_lifecycle() {
    std::cout << "Running: test_gateway_route_snapshot_lifecycle..." << std::endl;

    gateway::SessionManager manager;
    const auto sessionId = manager.createSession("127.0.0.1", 5000);

    gateway::RouteSnapshot routeSnapshot;
    routeSnapshot.worldId = 2;
    routeSnapshot.mapId = 101;
    routeSnapshot.instanceId = 202;
    routeSnapshot.spaceId = 303;
    routeSnapshot.routeVersion = 12;
    routeSnapshot.worldServerUrl = "tcp://127.0.0.1:9100";

    auto session = manager.bindPlayerAndRoute(sessionId, 1001, routeSnapshot);
    TEST_ASSERT(session != nullptr, "session exists after bind");
    TEST_ASSERT(session->playerId == 1001, "player bound");
    TEST_ASSERT(session->state == gateway::SessionState::IN_GAME, "in game after route assigned");
    TEST_ASSERT(session->routeSnapshot.routeVersion == 12, "route version stored");
    TEST_ASSERT(session->routeSnapshot.worldServerUrl == "tcp://127.0.0.1:9100", "route url stored");

    manager.clearRoute(sessionId);
    session = manager.getSession(sessionId);
    TEST_ASSERT(session != nullptr, "session still exists");
    TEST_ASSERT(session->state == gateway::SessionState::AUTHENTICATED, "back to authenticated after route clear");
    TEST_ASSERT(!session->routeSnapshot.isAssigned(), "route cleared");

    manager.assignRoute(sessionId, routeSnapshot);
    session = manager.getSession(sessionId);
    TEST_ASSERT(session->state == gateway::SessionState::IN_GAME, "in game after route reassigned");
    TEST_ASSERT(session->routeSnapshot.worldId == 2, "world id restored");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_authenticate_session() {
    std::cout << "Running: test_gateway_authenticate_session..." << std::endl;

    gateway::GatewayConfig config;
    gateway::GatewayServer server(config);

    const auto sessionId = server.createPendingSession("127.0.0.1", 6000);
    TEST_ASSERT(sessionId != 0, "pending session created");
    TEST_ASSERT(server.getRegisteredConnectionCount() == 1, "pending session registered a connection");

    const auto endpoint = server.findClientEndpoint(sessionId);
    TEST_ASSERT(endpoint.has_value(), "endpoint resolved from connection registry");
    TEST_ASSERT(endpoint->address == "127.0.0.1", "endpoint address preserved");
    TEST_ASSERT(endpoint->port == 6000, "endpoint port preserved");

    TEST_ASSERT(
        server.authenticateSession(sessionId, 1001, "ticket-abc123"),
        "session authenticated with ticket"
    );

    TEST_ASSERT(
        !server.authenticateSession(sessionId, 1001, ""),
        "empty ticket rejected"
    );

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_connection_registry_bind_and_remove() {
    std::cout << "Running: test_gateway_connection_registry_bind_and_remove..." << std::endl;

    gateway::GatewayConnectionRegistry registry;
    const auto connectionId = registry.registerConnection({"10.0.0.8", 7000});
    TEST_ASSERT(connectionId != 0, "connection id allocated");
    TEST_ASSERT(registry.getConnectionCount() == 1, "connection stored");
    TEST_ASSERT(registry.attachSession(connectionId, 88), "session attached");

    const auto endpoint = registry.findEndpointBySession(88);
    TEST_ASSERT(endpoint.has_value(), "session endpoint resolved");
    TEST_ASSERT(endpoint->address == "10.0.0.8", "session endpoint address");
    TEST_ASSERT(endpoint->port == 7000, "session endpoint port");

    TEST_ASSERT(registry.markClosed(connectionId), "connection closed");
    TEST_ASSERT(registry.removeConnection(connectionId), "connection removed");
    TEST_ASSERT(registry.getConnectionCount() == 0, "connection registry empty after removal");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_client_packet_dispatcher_routes_message_kinds() {
    std::cout << "Running: test_client_packet_dispatcher_routes_message_kinds..." << std::endl;

    gateway::ClientPacketDispatcher dispatcher;

    apollo::protocol::Ping ping;
    ping.timestamp = 42;
    const auto pingEncoded = apollo::protocol::MessageCodec::encode(ping, 7);
    const auto pingResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(pingEncoded.data(), pingEncoded.size()));
    TEST_ASSERT(pingResult.kind == gateway::ClientPacketKind::Heartbeat, "ping classified as heartbeat");

    apollo::protocol::ChatMessage chat;
    chat.sessionId = 8;
    chat.playerId = 1001;
    chat.playerName = "tester";
    chat.channel = apollo::protocol::ChatChannel::PRIVATE;
    chat.content = "hello";
    const auto chatEncoded = apollo::protocol::MessageCodec::encode(chat, 8);
    const auto chatResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(chatEncoded.data(), chatEncoded.size()));
    TEST_ASSERT(chatResult.kind == gateway::ClientPacketKind::Chat, "chat classified as chat");

    apollo::protocol::CombatSkillCast cast;
    cast.casterId = 1;
    cast.targetId = 2;
    cast.skillId = 3;
    cast.casterPos = apollo::protocol::Position(1.0f, 2.0f, 3.0f);
    cast.targetPos = apollo::protocol::Position(4.0f, 5.0f, 6.0f);
    const auto worldEncoded = apollo::protocol::MessageCodec::encode(cast, 9);
    const auto worldResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(worldEncoded.data(), worldEncoded.size()));
    TEST_ASSERT(worldResult.kind == gateway::ClientPacketKind::World, "combat classified as world");

    apollo::protocol::PlayerActivateRequest activate;
    activate.playerId = 1001;
    const auto baseEncoded = apollo::protocol::MessageCodec::encode(activate, 10);
    const auto baseResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(baseEncoded.data(), baseEncoded.size()));
    TEST_ASSERT(baseResult.kind == gateway::ClientPacketKind::Base, "activate classified as base");

    const std::vector<std::uint8_t> invalid{1, 2, 3};
    const auto invalidResult = dispatcher.dispatch(
        std::span<const std::uint8_t>(invalid.data(), invalid.size()));
    TEST_ASSERT(invalidResult.kind == gateway::ClientPacketKind::Invalid, "short payload invalid");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo Gateway Route Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_gateway_route_snapshot_lifecycle);
    run(test_gateway_authenticate_session);
    run(test_gateway_connection_registry_bind_and_remove);
    run(test_client_packet_dispatcher_routes_message_kinds);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
