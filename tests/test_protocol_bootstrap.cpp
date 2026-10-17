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

bool test_login_assign_codec() {
    std::cout << "Running: test_login_assign_codec..." << std::endl;

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

    TEST_ASSERT(decodedLogin.success, "success decoded from login response");
    TEST_ASSERT(decodedLogin.sessionId == 42, "session id decoded from login response");
    TEST_ASSERT(decodedLogin.gatewayHost == "127.0.0.1", "gateway host decoded");
    TEST_ASSERT(decodedLogin.gatewayPort == 8888, "gateway port decoded");
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

    TEST_ASSERT(decodedAssign.playerId == 1001, "player id decoded from gateway assign request");
    TEST_ASSERT(decodedAssign.sessionId == 42, "session id decoded from gateway assign request");
    TEST_ASSERT(decodedAssign.loginTicket == "ticket-abc123", "login ticket decoded from gateway assign request");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_gateway_client_disconnect_codec() {
    std::cout << "Running: test_gateway_client_disconnect_codec..." << std::endl;

    apollo::protocol::GatewayClientDisconnect message;
    message.sessionId = 9001;
    message.playerId = 1001;
    message.normalClose = false;

    const auto data = apollo::protocol::MessageCodec::encode(message, 666);
    const auto header = apollo::protocol::MessageCodec::parseHeader(data);

    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::GATEWAY_CLIENT_DISCONNECT),
        "message type encoded"
    );
    TEST_ASSERT(header.sessionId == 666, "session id encoded");

    const auto body = std::vector<uint8_t>(data.begin() + sizeof(apollo::protocol::MessageHeader), data.end());
    const auto decoded = apollo::protocol::MessageCodec::decodeBody<apollo::protocol::GatewayClientDisconnect>(body);

    TEST_ASSERT(decoded.sessionId == 9001, "session id decoded");
    TEST_ASSERT(decoded.playerId == 1001, "player id decoded");
    TEST_ASSERT(decoded.normalClose == false, "normal close decoded");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_squad_wire_codec() {
    std::cout << "Running: test_squad_wire_codec..." << std::endl;

    using apollo::protocol::EntityID;
    using apollo::protocol::MessageCodec;
    using apollo::protocol::MessageHeader;

    // C2S 上行族：party create（空 body）/ join / launch / enter / start / kill。
    const auto create = MessageCodec::encode(apollo::protocol::PartyCreateRequest{}, 42);
    auto header = MessageCodec::parseHeader(create);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::PARTY_CREATE_REQUEST),
        "party create type encoded"
    );
    TEST_ASSERT(header.sessionId == 42, "party create session encoded");
    TEST_ASSERT(
        create.size() == sizeof(MessageHeader),
        "party create empty body"
    );

    apollo::protocol::PartyJoinRequest join;
    join.partyId = 7001;
    const auto joinFrame = MessageCodec::encode(join, 43);
    header = MessageCodec::parseHeader(joinFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::PARTY_JOIN_REQUEST),
        "party join type encoded"
    );
    auto body = std::vector<uint8_t>(joinFrame.begin() + sizeof(MessageHeader), joinFrame.end());
    auto decodedJoin = MessageCodec::decodeBody<apollo::protocol::PartyJoinRequest>(body);
    TEST_ASSERT(decodedJoin.partyId == 7001, "party join id decoded");

    apollo::protocol::SquadLaunch launch;
    launch.partyId = 7001;
    launch.mapId = 4400;
    const auto launchFrame = MessageCodec::encode(launch, 44);
    header = MessageCodec::parseHeader(launchFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::SQUAD_LAUNCH),
        "squad launch type encoded"
    );
    body = std::vector<uint8_t>(launchFrame.begin() + sizeof(MessageHeader), launchFrame.end());
    auto decodedLaunch = MessageCodec::decodeBody<apollo::protocol::SquadLaunch>(body);
    TEST_ASSERT(decodedLaunch.partyId == 7001, "squad launch party decoded");
    TEST_ASSERT(decodedLaunch.mapId == 4400, "squad launch map decoded");

    apollo::protocol::SquadKill kill;
    kill.partyId = 7001;
    kill.monsterEntityId = 90002;
    const auto killFrame = MessageCodec::encode(kill, 45);
    header = MessageCodec::parseHeader(killFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::SQUAD_KILL),
        "squad kill type encoded"
    );
    body = std::vector<uint8_t>(killFrame.begin() + sizeof(MessageHeader), killFrame.end());
    auto decodedKill = MessageCodec::decodeBody<apollo::protocol::SquadKill>(body);
    TEST_ASSERT(decodedKill.partyId == 7001, "squad kill party decoded");
    TEST_ASSERT(decodedKill.monsterEntityId == 90002, "squad kill monster decoded");

    // S2C 下行族：party result / squad result / squad state。
    apollo::protocol::PartyResult partyResult;
    partyResult.op = static_cast<int32_t>(apollo::protocol::SquadOp::PARTY_CREATE);
    partyResult.code = 0;
    partyResult.partyId = 8801;
    const auto partyFrame = MessageCodec::encode(partyResult, 42);
    header = MessageCodec::parseHeader(partyFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::PARTY_RESULT),
        "party result type encoded"
    );
    body = std::vector<uint8_t>(partyFrame.begin() + sizeof(MessageHeader), partyFrame.end());
    auto decodedParty = MessageCodec::decodeBody<apollo::protocol::PartyResult>(body);
    TEST_ASSERT(decodedParty.op == 1, "party result op decoded");
    TEST_ASSERT(decodedParty.partyId == 8801, "party result id decoded");

    apollo::protocol::SquadResult result;
    result.op = static_cast<int32_t>(apollo::protocol::SquadOp::SQUAD_LAUNCH);
    result.code = 1101;
    result.partyId = 8801;
    result.instanceId = 5501;
    result.worldId = 3;
    result.sceneId = 6601;
    const auto resultFrame = MessageCodec::encode(result, 42);
    header = MessageCodec::parseHeader(resultFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::SQUAD_RESULT),
        "squad result type encoded"
    );
    body = std::vector<uint8_t>(resultFrame.begin() + sizeof(MessageHeader), resultFrame.end());
    auto decodedResult = MessageCodec::decodeBody<apollo::protocol::SquadResult>(body);
    TEST_ASSERT(decodedResult.op == 5, "squad result op decoded");
    TEST_ASSERT(decodedResult.code == 1101, "squad result code decoded");
    TEST_ASSERT(decodedResult.partyId == 8801, "squad result party decoded");
    TEST_ASSERT(decodedResult.instanceId == 5501, "squad result instance decoded");
    TEST_ASSERT(decodedResult.worldId == 3, "squad result world decoded");
    TEST_ASSERT(decodedResult.sceneId == 6601, "squad result scene decoded");

    apollo::protocol::SquadState state;
    state.partyId = 8801;
    state.instanceId = 5501;
    state.phase = 3;  // Running
    state.monstersDefeated = 2;
    state.ticks = 120;
    state.goalSet = 1;
    state.monsters = {91001, 91002, 91003};
    const auto stateFrame = MessageCodec::encode(state, 42);
    header = MessageCodec::parseHeader(stateFrame);
    TEST_ASSERT(
        header.type == static_cast<uint16_t>(apollo::protocol::MessageType::SQUAD_STATE),
        "squad state type encoded"
    );
    body = std::vector<uint8_t>(stateFrame.begin() + sizeof(MessageHeader), stateFrame.end());
    auto decodedState = MessageCodec::decodeBody<apollo::protocol::SquadState>(body);
    TEST_ASSERT(decodedState.partyId == 8801, "squad state party decoded");
    TEST_ASSERT(decodedState.phase == 3, "squad state phase decoded");
    TEST_ASSERT(decodedState.monstersDefeated == 2, "squad state kills decoded");
    TEST_ASSERT(decodedState.ticks == 120, "squad state ticks decoded");
    TEST_ASSERT(decodedState.goalSet == 1, "squad state goal decoded");
    TEST_ASSERT(decodedState.monsters.size() == 3 && decodedState.monsters[0] == 91001 &&
                    decodedState.monsters[2] == 91003,
                "squad state live monsters decoded");

    // 截断 body 必须拒收（短 body 不允许静默读越界）。
    bool threw = false;
    try {
        const std::vector<uint8_t> truncated(stateFrame.begin(), stateFrame.begin() + 10);
        MessageCodec::decodeBody<apollo::protocol::SquadState>(truncated);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_ASSERT(threw, "truncated squad state rejected");

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
    run(test_login_assign_codec);
    run(test_gateway_client_disconnect_codec);
    run(test_squad_wire_codec);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
