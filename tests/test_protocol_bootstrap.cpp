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

bool test_player_assign_world_codec() {
    std::cout << "Running: test_player_assign_world_codec..." << std::endl;

    apollo::protocol::PlayerAssignWorldRequest request;
    request.playerId = 1001;
    request.worldId = 2;
    request.mapId = 101;
    request.instanceId = 202;
    request.spaceId = 303;
    request.routeVersion = 9;

    const auto data = apollo::protocol::MessageCodec::encode(request, 777);
    const auto header = apollo::protocol::MessageCodec::parseHeader(data);

    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::PLAYER_ASSIGN_WORLD_REQUEST),
        "message type encoded"
    );
    TEST_ASSERT(header.sessionId == 777, "session id encoded");

    const auto body = std::vector<uint8_t>(data.begin() + sizeof(apollo::protocol::MessageHeader), data.end());
    const auto decoded = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::PlayerAssignWorldRequest>(body);

    TEST_ASSERT(decoded.playerId == 1001, "player id decoded");
    TEST_ASSERT(decoded.worldId == 2, "world id decoded");
    TEST_ASSERT(decoded.mapId == 101, "map id decoded");
    TEST_ASSERT(decoded.instanceId == 202, "instance id decoded");
    TEST_ASSERT(decoded.spaceId == 303, "space id decoded");
    TEST_ASSERT(decoded.routeVersion == 9, "route version decoded");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_cell_cross_border_codec() {
    std::cout << "Running: test_cell_cross_border_codec..." << std::endl;

    apollo::protocol::CellCrossBorder message;
    message.entityId = 5001;
    message.fromSpace = 10;
    message.toSpace = 11;
    message.position = apollo::protocol::Position{12.0f, 13.0f, 14.0f};

    const auto data = apollo::protocol::MessageCodec::encode(message, 888);
    const auto body = std::vector<uint8_t>(data.begin() + sizeof(apollo::protocol::MessageHeader), data.end());
    const auto decoded = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::CellCrossBorder>(body);

    TEST_ASSERT(decoded.entityId == 5001, "entity id decoded");
    TEST_ASSERT(decoded.fromSpace == 10, "from space decoded");
    TEST_ASSERT(decoded.toSpace == 11, "to space decoded");
    TEST_ASSERT(decoded.position.x == 12.0f, "position x decoded");
    TEST_ASSERT(decoded.position.y == 13.0f, "position y decoded");
    TEST_ASSERT(decoded.position.z == 14.0f, "position z decoded");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_player_resolve_route_codec() {
    std::cout << "Running: test_player_resolve_route_codec..." << std::endl;

    apollo::protocol::PlayerResolveRouteResponse response;
    response.success = true;
    response.playerId = 1001;
    response.sessionId = 777;
    response.gatewayId = 1;
    response.gatewayAddr = "tcp://127.0.0.1:8888";
    response.worldId = 2;
    response.mapId = 101;
    response.instanceId = 202;
    response.spaceId = 303;
    response.routeVersion = 5;

    const auto data = apollo::protocol::MessageCodec::encode(response, 777);
    const auto body = std::vector<uint8_t>(data.begin() + sizeof(apollo::protocol::MessageHeader), data.end());
    const auto decoded = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::PlayerResolveRouteResponse>(body);

    TEST_ASSERT(decoded.success, "response success decoded");
    TEST_ASSERT(decoded.playerId == 1001, "player id decoded");
    TEST_ASSERT(decoded.gatewayAddr == "tcp://127.0.0.1:8888", "gateway addr decoded");
    TEST_ASSERT(decoded.worldId == 2, "world id decoded");
    TEST_ASSERT(decoded.routeVersion == 5, "route version decoded");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_login_ticket_codec() {
    std::cout << "Running: test_login_ticket_codec..." << std::endl;

    apollo::protocol::LoginResponse loginResponse;
    loginResponse.success = true;
    loginResponse.sessionId = 42;
    loginResponse.playerId = 1001;
    loginResponse.gatewayHost = "127.0.0.1";
    loginResponse.gatewayPort = 8888;
    loginResponse.loginTicket = "ticket-abc123";

    const auto encodedLogin = apollo::protocol::MessageCodec::encode(loginResponse, 42);
    const auto loginBody = std::vector<uint8_t>(
        encodedLogin.begin() + sizeof(apollo::protocol::MessageHeader),
        encodedLogin.end());
    const auto decodedLogin =
        apollo::protocol::MessageCodec::decodeBody<apollo::protocol::LoginResponse>(loginBody);

    TEST_ASSERT(decodedLogin.loginTicket == "ticket-abc123", "login ticket decoded from login response");

    apollo::protocol::GatewayAssignRequest assignRequest;
    assignRequest.playerId = 1001;
    assignRequest.sessionId = 42;
    assignRequest.loginTicket = "ticket-abc123";

    const auto encodedAssign = apollo::protocol::MessageCodec::encode(assignRequest, 42);
    const auto assignBody = std::vector<uint8_t>(
        encodedAssign.begin() + sizeof(apollo::protocol::MessageHeader),
        encodedAssign.end());
    const auto decodedAssign =
        apollo::protocol::MessageCodec::decodeBody<apollo::protocol::GatewayAssignRequest>(assignBody);

    TEST_ASSERT(decodedAssign.loginTicket == "ticket-abc123", "login ticket decoded from gateway assign request");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo Protocol Bootstrap Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_player_assign_world_codec);
    run(test_cell_cross_border_codec);
    run(test_player_resolve_route_codec);
    run(test_login_ticket_codec);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
