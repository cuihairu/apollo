#include <gtest/gtest.h>
#include "apollo/protocol/nng_wrapper.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/socket.hpp"

#include <thread>
#include <chrono>

using namespace apollo::protocol;

//==============================================================================
// NngWrapper 测试
//==============================================================================

TEST(NngWrapperTest, UrlCreation) {
    EXPECT_EQ(make_tcp_url("127.0.0.1", 8080), "tcp://127.0.0.1:8080");
    EXPECT_EQ(make_ipc_url("/tmp/test"), "ipc:///tmp/test");
    EXPECT_EQ(make_inproc_url("test"), "inproc://test");
}

//==============================================================================
// MessageHeader 测试
//==============================================================================

TEST(MessageHeaderTest, SizeCheck) {
    EXPECT_EQ(sizeof(MessageHeader), 26); // magic(4) + version(2) + type(2) + length(4) + sequence(8) + sessionId(8)
}

TEST(MessageHeaderTest, DefaultValues) {
    MessageHeader header;
    header.magic = MessageHeader::MAGIC;
    header.version = MessageHeader::CURRENT_VERSION;
    header.type = static_cast<uint16_t>(MessageType::PING);
    header.length = 100;
    header.sequence = 12345;
    header.sessionId = 67890;

    EXPECT_EQ(header.magic, 0x42575452);
    EXPECT_EQ(header.version, 1);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::PING));
    EXPECT_EQ(header.length, 100);
}

//==============================================================================
// MessageCodec 测试
//==============================================================================

TEST(MessageCodecTest, EncodeDecodePing) {
    Ping ping;
    ping.timestamp = 1234567890ULL;

    auto data = MessageCodec::encode(ping);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::PING));
    EXPECT_EQ(header.length, sizeof(uint64_t));

    Ping decoded = MessageCodec::decodeBody<Ping>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.timestamp, 1234567890ULL);
}

TEST(MessageCodecTest, EncodeDecodePong) {
    Pong pong;
    pong.timestamp = 9876543210ULL;

    auto data = MessageCodec::encode(pong);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::PONG));

    Pong decoded = MessageCodec::decodeBody<Pong>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.timestamp, 9876543210ULL);
}

TEST(MessageCodecTest, EncodeDecodeLoginRequest) {
    LoginRequest req;
    req.username = "testuser";
    req.password = "testpass";
    req.clientVersion = "1.0.0";
    req.timestamp = 123456;

    auto data = MessageCodec::encode(req);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::LOGIN_REQUEST));

    LoginRequest decoded = MessageCodec::decodeBody<LoginRequest>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.username, "testuser");
    EXPECT_EQ(decoded.password, "testpass");
    EXPECT_EQ(decoded.clientVersion, "1.0.0");
    EXPECT_EQ(decoded.timestamp, 123456);
}

TEST(MessageCodecTest, EncodeDecodeLoginResponse) {
    LoginResponse resp;
    resp.success = true;
    resp.sessionId = 11111;
    resp.playerId = 22222;
    resp.gatewayHost = "192.168.1.100";
    resp.gatewayPort = 8888;

    auto data = MessageCodec::encode(resp);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::LOGIN_RESPONSE));

    LoginResponse decoded = MessageCodec::decodeBody<LoginResponse>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_TRUE(decoded.success);
    EXPECT_EQ(decoded.sessionId, 11111);
    EXPECT_EQ(decoded.playerId, 22222);
    EXPECT_EQ(decoded.gatewayHost, "192.168.1.100");
    EXPECT_EQ(decoded.gatewayPort, 8888);
}

TEST(MessageCodecTest, EncodeDecodeCellCreateEntity) {
    CellCreateEntity msg;
    msg.entityId = 12345;
    msg.entityType = EntityType::PLAYER;
    msg.spaceId = 100;
    msg.position = Position{10.5f, 20.5f, 30.5f};

    auto data = MessageCodec::encode(msg);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::CELL_CREATE_ENTITY));

    CellCreateEntity decoded = MessageCodec::decodeBody<CellCreateEntity>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.entityId, 12345);
    EXPECT_EQ(decoded.entityType, EntityType::PLAYER);
    EXPECT_EQ(decoded.spaceId, 100);
    EXPECT_FLOAT_EQ(decoded.position.x, 10.5f);
    EXPECT_FLOAT_EQ(decoded.position.y, 20.5f);
    EXPECT_FLOAT_EQ(decoded.position.z, 30.5f);
}

TEST(MessageCodecTest, EncodeDecodeCellEntityMove) {
    CellEntityMove msg;
    msg.entityId = 54321;
    msg.oldPos = Position{0, 0, 0};
    msg.newPos = Position{100, 200, 300};

    auto data = MessageCodec::encode(msg);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::CELL_ENTITY_MOVE));

    CellEntityMove decoded = MessageCodec::decodeBody<CellEntityMove>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.entityId, 54321);
    EXPECT_FLOAT_EQ(decoded.oldPos.x, 0);
    EXPECT_FLOAT_EQ(decoded.oldPos.y, 0);
    EXPECT_FLOAT_EQ(decoded.oldPos.z, 0);
    EXPECT_FLOAT_EQ(decoded.newPos.x, 100);
    EXPECT_FLOAT_EQ(decoded.newPos.y, 200);
    EXPECT_FLOAT_EQ(decoded.newPos.z, 300);
}

TEST(MessageCodecTest, EncodeDecodeCombatDamage) {
    CombatDamage msg;
    msg.targetId = 100;
    msg.damage = -50;
    msg.sourceId = 200;

    auto data = MessageCodec::encode(msg);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::COMBAT_DAMAGE));

    CombatDamage decoded = MessageCodec::decodeBody<CombatDamage>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.targetId, 100);
    EXPECT_EQ(decoded.damage, -50);
    EXPECT_EQ(decoded.sourceId, 200);
}

TEST(MessageCodecTest, EncodeDecodeChatMessage) {
    ChatMessage msg;
    msg.sessionId = 999;
    msg.playerId = 888;
    msg.playerName = "TestPlayer";
    msg.channel = ChatChannel::WORLD;
    msg.content = "Hello World!";

    auto data = MessageCodec::encode(msg);

    auto header = MessageCodec::parseHeader(data);
    EXPECT_EQ(header.type, static_cast<uint16_t>(MessageType::CHAT_MESSAGE));

    ChatMessage decoded = MessageCodec::decodeBody<ChatMessage>(
        std::vector<uint8_t>(data.begin() + sizeof(MessageHeader), data.end())
    );
    EXPECT_EQ(decoded.sessionId, 999);
    EXPECT_EQ(decoded.playerId, 888);
    EXPECT_EQ(decoded.playerName, "TestPlayer");
    EXPECT_EQ(decoded.channel, ChatChannel::WORLD);
    EXPECT_EQ(decoded.content, "Hello World!");
}

//==============================================================================
// 辅助函数测试
//==============================================================================

TEST(MessageHelperTest, MessageTypeToString) {
    EXPECT_STREQ(toString(MessageType::PING), "PING");
    EXPECT_STREQ(toString(MessageType::PONG), "PONG");
    EXPECT_STREQ(toString(MessageType::LOGIN_REQUEST), "LOGIN_REQUEST");
    EXPECT_STREQ(toString(MessageType::CELL_CREATE_ENTITY), "CELL_CREATE_ENTITY");
}

TEST(MessageHelperTest, EntityTypeToString) {
    EXPECT_STREQ(toString(EntityType::PLAYER), "PLAYER");
    EXPECT_STREQ(toString(EntityType::NPC), "NPC");
    EXPECT_STREQ(toString(EntityType::MONSTER), "MONSTER");
}

TEST(MessageHelperTest, ChatChannelToString) {
    EXPECT_STREQ(toString(ChatChannel::WORLD), "WORLD");
    EXPECT_STREQ(toString(ChatChannel::PRIVATE), "PRIVATE");
    EXPECT_STREQ(toString(ChatChannel::GUILD), "GUILD");
}

//==============================================================================
// 主函数
//==============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
