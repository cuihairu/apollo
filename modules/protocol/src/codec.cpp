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
    msg.errorMessage = j.value("errorMessage", "");
}

//==============================================================================
// Gateway 消息
//==============================================================================

std::vector<uint8_t> MessageCodec::encodeBody(const GatewayAssignRequest& msg) {
    std::vector<uint8_t> result(sizeof(PlayerID) + sizeof(SessionID));
    size_t offset = 0;
    std::memcpy(result.data() + offset, &msg.playerId, sizeof(PlayerID)); offset += sizeof(PlayerID);
    std::memcpy(result.data() + offset, &msg.sessionId, sizeof(SessionID));
    return result;
}

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, GatewayAssignRequest& msg) {
    const uint8_t* ptr = data.data();
    const uint8_t* end = data.data() + data.size();
    if (ptr + sizeof(PlayerID) + sizeof(SessionID) > end) throw std::runtime_error("Invalid data size");
    std::memcpy(&msg.playerId, ptr, sizeof(PlayerID)); ptr += sizeof(PlayerID);
    std::memcpy(&msg.sessionId, ptr, sizeof(SessionID));
}

std::vector<uint8_t> MessageCodec::encodeBody(const GatewayAssignResponse& msg) {
    nlohmann::json j;
    j["success"] = msg.success;
    j["gatewayHost"] = msg.gatewayHost;
    j["gatewayPort"] = msg.gatewayPort;
    j["token"] = msg.token;
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
}

//==============================================================================
// CellApp 消息
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

void MessageCodec::decodeBody(const std::vector<uint8_t>& data, CellCreateEntity& msg) {
    const uint8_t* ptr = data.data();
    size_t offset = 0;
    std::memcpy(&msg.entityId, ptr + offset, sizeof(EntityID)); offset += sizeof(EntityID);
    std::memcpy(&msg.entityType, ptr + offset, sizeof(EntityType)); offset += sizeof(EntityType);
    std::memcpy(&msg.spaceId, ptr + offset, sizeof(SpaceID)); offset += sizeof(SpaceID);
    std::memcpy(&msg.position, ptr + offset, sizeof(Position));
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

} // namespace protocol
} // namespace apollo
