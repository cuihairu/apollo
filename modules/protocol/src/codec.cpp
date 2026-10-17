#include "apollo/protocol/codec.hpp"
#include <nlohmann/json.hpp>
#include <atomic>

namespace apollo {
namespace protocol {

//==============================================================================
// 字符串编码辅助
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const LoginRequest& msg) {
    nlohmann::json j;
    j["username"] = msg.username;
    j["password"] = msg.password;
    j["clientVersion"] = msg.clientVersion;
    j["timestamp"] = msg.timestamp;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, LoginRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.username = j.value("username", "");
    msg.password = j.value("password", "");
    msg.clientVersion = j.value("clientVersion", "");
    msg.timestamp = j.value("timestamp", uint64_t(0));
}

std::vector<uint8_t> MessageCodec::encodeBody(const LoginResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["sessionId"] = msg.sessionId;
    j["playerId"] = msg.playerId;
    j["gatewayHost"] = msg.gatewayHost;
    j["gatewayPort"] = msg.gatewayPort;
    j["loginTicket"] = msg.loginTicket;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, LoginResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.sessionId = j.value("sessionId", SessionID(0));
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.gatewayHost = j.value("gatewayHost", "");
    msg.gatewayPort = j.value("gatewayPort", uint16_t(0));
    msg.loginTicket = j.value("loginTicket", "");
    msg.errorMessage = j.value("errorMessage", "");
}

//==============================================================================
// Gateway 消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const GatewayAssignRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["sessionId"] = msg.sessionId;
    j["loginTicket"] = msg.loginTicket;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, GatewayAssignRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.sessionId = j.value("sessionId", SessionID(0));
    msg.loginTicket = j.value("loginTicket", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const GatewayAssignResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["gatewayHost"] = msg.gatewayHost;
    j["gatewayPort"] = msg.gatewayPort;
    j["token"] = msg.token;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, GatewayAssignResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.gatewayHost = j.value("gatewayHost", "");
    msg.gatewayPort = j.value("gatewayPort", uint16_t(0));
    msg.token = j.value("token", "");
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const GatewayClientDisconnect& msg) {
    std::vector<uint8_t> result(sizeof(SessionID) + sizeof(PlayerID) + sizeof(uint8_t));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.sessionId, sizeof(SessionID)); offset += sizeof(SessionID);
    std::memcpy(result.data() + offset, &msg.playerId, sizeof(PlayerID)); offset += sizeof(PlayerID);
    const uint8_t normalClose = msg.normalClose ? 1 : 0;
    std::memcpy(result.data() + offset, &normalClose, sizeof(uint8_t));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, GatewayClientDisconnect& msg) {
    constexpr size_t kBodySize = sizeof(SessionID) + sizeof(PlayerID) + sizeof(uint8_t);
    const uint8_t* ptr = data.data();
    const uint8_t* end = data.data() + data.size();
    if (ptr + kBodySize > end) throw std::runtime_error("Invalid data size");
    std::memcpy(&msg.sessionId, ptr, sizeof(SessionID)); ptr += sizeof(SessionID);
    std::memcpy(&msg.playerId, ptr, sizeof(PlayerID)); ptr += sizeof(PlayerID);
    uint8_t normalClose = 0;
    std::memcpy(&normalClose, ptr, sizeof(uint8_t));
    msg.normalClose = normalClose != 0;
}

//==============================================================================
// WorldApp 消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const CellCreateEntity& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) + sizeof(EntityType) + sizeof(SpaceID) + sizeof(Position));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.entityId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.entityType, sizeof(EntityType)); offset += sizeof(EntityType);
    std::memcpy(result.data() + offset, &msg.spaceId, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(result.data() + offset, &msg.position, sizeof(Position));
    return result;
}

std::vector<uint8_t> MessageCodec::encodeBody(const CellDestroyEntity& msg) {
    std::vector<uint8_t> result(sizeof(EntityID));
    std::memcpy(result.data(), &msg.entityId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CellCreateEntity& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.entityId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.entityType, ptr + offset, sizeof(EntityType)); offset += sizeof(EntityType);
    std::memcpy(&msg.spaceId, ptr + offset, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(&msg.position, ptr + offset, sizeof(Position));
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CellDestroyEntity& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.entityId, ptr + offset, sizeof(EntityID));
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CellCrossBorder& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.entityId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.fromSpace, ptr + offset, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(&msg.toSpace, ptr + offset, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(&msg.position, ptr + offset, sizeof(Position));
}

std::vector<uint8_t> MessageCodec::encodeBody(const CellCrossBorder& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) + sizeof(SpaceID) * 2 + sizeof(Position));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.entityId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.fromSpace, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(result.data() + offset, &msg.toSpace, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(result.data() + offset, &msg.position, sizeof(Position));
    return result;
}

std::vector<uint8_t> MessageCodec::encodeBody(const CellEntityMove& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) + sizeof(Position) * 2);
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.entityId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.oldPos, sizeof(Position)); offset += sizeof(Position);
    std::memcpy(result.data() + offset, &msg.newPos, sizeof(Position));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CellEntityMove& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.entityId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.oldPos, ptr + offset, sizeof(Position)); offset += sizeof(Position);
    std::memcpy(&msg.newPos, ptr + offset, sizeof(Position));
}

//==============================================================================
// 战斗消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const CombatSkillCast& msg) {
    std::vector<uint8_t> result(
        sizeof(EntityID) * 2 + sizeof(uint32_t) + sizeof(Position) * 2
    );
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.casterId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.targetId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.skillId, sizeof(uint32_t)); offset += sizeof(uint32_t);
    std::memcpy(result.data() + offset, &msg.casterPos, sizeof(Position)); offset += sizeof(Position);
    std::memcpy(result.data() + offset, &msg.targetPos, sizeof(Position));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CombatSkillCast& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.casterId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.targetId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.skillId, ptr + offset, sizeof(uint32_t)); offset += sizeof(uint32_t);
    std::memcpy(&msg.casterPos, ptr + offset, sizeof(Position)); offset += sizeof(Position);
    std::memcpy(&msg.targetPos, ptr + offset, sizeof(Position));
}

std::vector<uint8_t> MessageCodec::encodeBody(const CombatDamage& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) * 2 + sizeof(int32_t));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.targetId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.damage, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.sourceId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CombatDamage& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.targetId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.damage, ptr + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.sourceId, ptr + offset, sizeof(EntityID));
}

//==============================================================================
// 聊天消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const ChatMessage& msg) {
    nlohmann::json j;
    j["sessionId"] = msg.sessionId;
    j["playerId"] = msg.playerId;
    j["playerName"] = msg.playerName;
    j["channel"] = static_cast<uint8_t>(msg.channel);
    j["content"] = msg.content;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, ChatMessage& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.sessionId = j.value("sessionId", SessionID(0));
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.playerName = j.value("playerName", "");
    msg.channel = static_cast<ChatChannel>(j.value("channel", 0));
    msg.content = j.value("content", "");
}

//==============================================================================
// 系统消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const Ping& msg) {
    std::vector<uint8_t> result(sizeof(uint64_t));
    std::memcpy(result.data(), &msg.timestamp, sizeof(uint64_t));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, Ping& msg) {
    if (data.size() < sizeof(uint64_t)) throw std::runtime_error("Invalid data size");
    std::memcpy(&msg.timestamp, data.data(), sizeof(uint64_t));
}

std::vector<uint8_t> MessageCodec::encodeBody(const Pong& msg) {
    std::vector<uint8_t> result(sizeof(uint64_t));
    std::memcpy(result.data(), &msg.timestamp, sizeof(uint64_t));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, Pong& msg) {
    if (data.size() < sizeof(uint64_t)) throw std::runtime_error("Invalid data size");
    std::memcpy(&msg.timestamp, data.data(), sizeof(uint64_t));
}

std::vector<uint8_t> MessageCodec::encodeBody(const ErrorMessage& msg) {
    nlohmann::json j;
    j["code"] = msg.code;
    j["message"] = msg.message;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, ErrorMessage& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.code = j.value("code", uint32_t(0));
    msg.message = j.value("message", "");
}

//==============================================================================
// 数据库消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const DbLoadRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["tableName"] = msg.tableName;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbLoadRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.tableName = j.value("tableName", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const DbLoadResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["jsonData"] = msg.jsonData;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbLoadResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.jsonData = j.value("jsonData", "");
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const DbSaveRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["tableName"] = msg.tableName;
    j["jsonData"] = msg.jsonData;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbSaveRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.tableName = j.value("tableName", "");
    msg.jsonData = j.value("jsonData", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const DbSaveResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbSaveResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const DbQueryRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["tableName"] = msg.tableName;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbQueryRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.tableName = j.value("tableName", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const DbQueryResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["jsonData"] = msg.jsonData;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, DbQueryResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.jsonData = j.value("jsonData", "");
    msg.errorMessage = j.value("errorMessage", "");
}

//==============================================================================
// 玩家生命周期与落点消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerActivateRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerActivateRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerActivateResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["playerId"] = msg.playerId;
    j["state"] = msg.state;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerActivateResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.state = j.value("state", "");
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerBindSessionRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["sessionId"] = msg.sessionId;
    j["gatewayId"] = msg.gatewayId;
    j["gatewayAddr"] = msg.gatewayAddr;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerBindSessionRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.sessionId = j.value("sessionId", SessionID(0));
    msg.gatewayId = j.value("gatewayId", uint32_t(0));
    msg.gatewayAddr = j.value("gatewayAddr", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerBindSessionResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerBindSessionResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerAssignWorldRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["worldId"] = msg.worldId;
    j["mapId"] = msg.mapId;
    j["instanceId"] = msg.instanceId;
    j["spaceId"] = msg.spaceId;
    j["routeVersion"] = msg.routeVersion;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerAssignWorldRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.worldId = j.value("worldId", uint32_t(0));
    msg.mapId = j.value("mapId", uint64_t(0));
    msg.instanceId = j.value("instanceId", uint64_t(0));
    msg.spaceId = j.value("spaceId", uint64_t(0));
    msg.routeVersion = j.value("routeVersion", uint64_t(0));
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerAssignWorldResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["routeVersion"] = msg.routeVersion;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerAssignWorldResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.routeVersion = j.value("routeVersion", uint64_t(0));
    msg.errorMessage = j.value("errorMessage", "");
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerResolveRouteRequest& msg) {
    nlohmann::json j;
    j["playerId"] = msg.playerId;
    j["sessionId"] = msg.sessionId;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerResolveRouteRequest& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.sessionId = j.value("sessionId", SessionID(0));
}

std::vector<uint8_t> MessageCodec::encodeBody(const PlayerResolveRouteResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["playerId"] = msg.playerId;
    j["sessionId"] = msg.sessionId;
    j["gatewayId"] = msg.gatewayId;
    j["gatewayAddr"] = msg.gatewayAddr;
    j["worldId"] = msg.worldId;
    j["mapId"] = msg.mapId;
    j["instanceId"] = msg.instanceId;
    j["spaceId"] = msg.spaceId;
    j["routeVersion"] = msg.routeVersion;
    j["errorMessage"] = msg.errorMessage;
    std::string str = j.dump();
    return std::vector<uint8_t>(str.begin(), str.end());
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PlayerResolveRouteResponse& msg) {
    std::string str(data.begin(), data.end());
    auto j = nlohmann::json::parse(str);
    msg.success = j.value("success", false);
    msg.playerId = j.value("playerId", PlayerID(0));
    msg.sessionId = j.value("sessionId", SessionID(0));
    msg.gatewayId = j.value("gatewayId", uint32_t(0));
    msg.gatewayAddr = j.value("gatewayAddr", "");
    msg.worldId = j.value("worldId", uint32_t(0));
    msg.mapId = j.value("mapId", uint64_t(0));
    msg.instanceId = j.value("instanceId", uint64_t(0));
    msg.spaceId = j.value("spaceId", uint64_t(0));
    msg.routeVersion = j.value("routeVersion", uint64_t(0));
    msg.errorMessage = j.value("errorMessage", "");
}

//==============================================================================
// 队伍与小队副本（P2 真实链路；二进制 body，同 CELL/COMBAT 约定）
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const PartyCreateRequest& /*msg*/) {
    return {};
}

std::vector<uint8_t> MessageCodec::encodeBody(const PartyJoinRequest& msg) {
    std::vector<uint8_t> result(sizeof(EntityID));
    std::memcpy(result.data(), &msg.partyId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PartyJoinRequest& msg) {
    if (data.size() < sizeof(EntityID)) throw std::runtime_error("body truncated");
    std::memcpy(&msg.partyId, data.data(), sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadLaunch& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) * 2);
    std::memcpy(result.data(), &msg.partyId, sizeof(EntityID));
    std::memcpy(result.data() + sizeof(EntityID), &msg.mapId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadLaunch& msg) {
    if (data.size() < sizeof(EntityID) * 2) throw std::runtime_error("body truncated");
    std::memcpy(&msg.partyId, data.data(), sizeof(EntityID));
    std::memcpy(&msg.mapId, data.data() + sizeof(EntityID), sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadEnter& msg) {
    std::vector<uint8_t> result(sizeof(EntityID));
    std::memcpy(result.data(), &msg.partyId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadEnter& msg) {
    if (data.size() < sizeof(EntityID)) throw std::runtime_error("body truncated");
    std::memcpy(&msg.partyId, data.data(), sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadStart& msg) {
    std::vector<uint8_t> result(sizeof(EntityID));
    std::memcpy(result.data(), &msg.partyId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadStart& msg) {
    if (data.size() < sizeof(EntityID)) throw std::runtime_error("body truncated");
    std::memcpy(&msg.partyId, data.data(), sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadKill& msg) {
    std::vector<uint8_t> result(sizeof(EntityID) * 2);
    std::memcpy(result.data(), &msg.partyId, sizeof(EntityID));
    std::memcpy(result.data() + sizeof(EntityID), &msg.monsterEntityId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadKill& msg) {
    if (data.size() < sizeof(EntityID) * 2) throw std::runtime_error("body truncated");
    std::memcpy(&msg.partyId, data.data(), sizeof(EntityID));
    std::memcpy(&msg.monsterEntityId, data.data() + sizeof(EntityID), sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const PartyResult& msg) {
    std::vector<uint8_t> result(sizeof(int32_t) * 2 + sizeof(EntityID));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.op, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.code, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.partyId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, PartyResult& msg) {
    if (data.size() < sizeof(int32_t) * 2 + sizeof(EntityID)) throw std::runtime_error("body truncated");
    size_t offset = 0;
    std::memcpy(&msg.op, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.code, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.partyId, data.data() + offset, sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadResult& msg) {
    std::vector<uint8_t> result(sizeof(int32_t) * 2 + sizeof(EntityID) * 3 + sizeof(uint32_t));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.op, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.code, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.partyId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.instanceId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.worldId, sizeof(uint32_t)); offset += sizeof(uint32_t);
    std::memcpy(result.data() + offset, &msg.sceneId, sizeof(EntityID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadResult& msg) {
    constexpr size_t kBody = sizeof(int32_t) * 2 + sizeof(EntityID) * 3 + sizeof(uint32_t);
    if (data.size() < kBody) throw std::runtime_error("body truncated");
    size_t offset = 0;
    std::memcpy(&msg.op, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.code, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.partyId, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.instanceId, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.worldId, data.data() + offset, sizeof(uint32_t)); offset += sizeof(uint32_t);
    std::memcpy(&msg.sceneId, data.data() + offset, sizeof(EntityID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const SquadState& msg) {
    // 尾变长段（存活怪列表）：u32 计数 + n×8B，同契约 Int64List 线形
    std::vector<uint8_t> result(sizeof(EntityID) * 4 + sizeof(int32_t) * 2 +
                                 sizeof(uint32_t) + msg.monsters.size() * sizeof(EntityID));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.partyId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.instanceId, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.phase, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(result.data() + offset, &msg.monstersDefeated, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.ticks, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(result.data() + offset, &msg.goalSet, sizeof(int32_t)); offset += sizeof(int32_t);
    const auto count = static_cast<uint32_t>(msg.monsters.size());
    std::memcpy(result.data() + offset, &count, sizeof(uint32_t)); offset += sizeof(uint32_t);
    for (const auto monster : msg.monsters) {
        std::memcpy(result.data() + offset, &monster, sizeof(EntityID)); offset += sizeof(EntityID);
    }
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, SquadState& msg) {
    constexpr size_t kBody = sizeof(EntityID) * 4 + sizeof(int32_t) * 2;
    if (data.size() < kBody + sizeof(uint32_t)) throw std::runtime_error("body truncated");
    size_t offset = 0;
    std::memcpy(&msg.partyId, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.instanceId, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.phase, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    std::memcpy(&msg.monstersDefeated, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.ticks, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.goalSet, data.data() + offset, sizeof(int32_t)); offset += sizeof(int32_t);
    uint32_t count = 0;
    std::memcpy(&count, data.data() + offset, sizeof(uint32_t)); offset += sizeof(uint32_t);
    if (data.size() < offset + count * sizeof(EntityID)) throw std::runtime_error("body truncated");
    msg.monsters.resize(count);
    for (auto& monster : msg.monsters) {
        std::memcpy(&monster, data.data() + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    }
}

} // namespace protocol
} // namespace apollo
