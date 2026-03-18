#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace apollo {
namespace protocol {

//==============================================================================
// 消息类型定义
//==============================================================================

enum class MessageType : uint16_t {
    // 登录相关
    LOGIN_REQUEST           = 0x0001,
    LOGIN_RESPONSE          = 0x0002,
    LOGOUT_NOTIFY           = 0x0003,

    // 网关相关
    GATEWAY_ASSIGN_REQUEST  = 0x0010,
    GATEWAY_ASSIGN_RESPONSE = 0x0011,
    GATEWAY_HEARTBEAT       = 0x0012,
    GATEWAY_CLIENT_CONNECT  = 0x0013,
    GATEWAY_CLIENT_DISCONNECT = 0x0014,

    // 数据库相关
    DB_LOAD_REQUEST         = 0x0020,
    DB_LOAD_RESPONSE        = 0x0021,
    DB_SAVE_REQUEST         = 0x0022,
    DB_SAVE_RESPONSE        = 0x0023,
    DB_QUERY_REQUEST        = 0x0024,
    DB_QUERY_RESPONSE       = 0x0025,

    // CellApp 相关
    CELL_CREATE_ENTITY      = 0x0030,
    CELL_DESTROY_ENTITY     = 0x0031,
    CELL_ENTITY_ENTER       = 0x0032,
    CELL_ENTITY_LEAVE       = 0x0033,
    CELL_ENTITY_MOVE        = 0x0034,
    CELL_ENTITY_PROPERTY    = 0x0035,
    CELL_CROSS_BORDER       = 0x0036,

    // 战斗相关
    COMBAT_SKILL_CAST       = 0x0040,
    COMBAT_DAMAGE           = 0x0041,
    COMBAT_HEAL             = 0x0042,
    COMBAT_DEATH            = 0x0043,

    // 聊天相关
    CHAT_MESSAGE            = 0x0050,
    CHAT_BROADCAST          = 0x0051,

    // 系统相关
    PING                    = 0x1000,
    PONG                    = 0x1001,
    SHUTDOWN                = 0x1002,
    ERROR                   = 0x1FFF,
};

//==============================================================================
// 实体类型定义
//==============================================================================

enum class EntityType : uint8_t {
    UNKNOWN   = 0,
    PLAYER    = 1,
    NPC       = 2,
    MONSTER   = 3,
    PET       = 4,
    ITEM      = 5,
    TELEPORTER = 6,
};

//==============================================================================
// 基础数据结构
//==============================================================================

// 唯一ID
using EntityID = uint64_t;
using SpaceID = uint32_t;
using PlayerID = uint64_t;
using SessionID = uint64_t;

// 位置
struct Position {
    float x;
    float y;
    float z;

    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

// 旋转
struct Rotation {
    float x;
    float y;
    float z;
    float w;

    Rotation() : x(0), y(0), z(0), w(1) {}
};

// 向量3
struct Vector3 {
    float x;
    float y;
    float z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

//==============================================================================
// 登录消息
//==============================================================================

struct LoginRequest {
    std::string username;
    std::string password;
    std::string clientVersion;
    uint64_t timestamp;

    static constexpr MessageType TYPE = MessageType::LOGIN_REQUEST;
};

struct LoginResponse {
    bool success;
    SessionID sessionId;
    PlayerID playerId;
    std::string gatewayHost;
    uint16_t gatewayPort;
    std::string errorMessage;

    static constexpr MessageType TYPE = MessageType::LOGIN_RESPONSE;
};

//==============================================================================
// 网关消息
//==============================================================================

struct GatewayAssignRequest {
    PlayerID playerId;
    SessionID sessionId;

    static constexpr MessageType TYPE = MessageType::GATEWAY_ASSIGN_REQUEST;
};

struct GatewayAssignResponse {
    bool success;
    std::string gatewayHost;
    uint16_t gatewayPort;
    std::string token;

    static constexpr MessageType TYPE = MessageType::GATEWAY_ASSIGN_RESPONSE;
};

struct GatewayClientConnect {
    SessionID sessionId;
    PlayerID playerId;
    std::string clientIP;

    static constexpr MessageType TYPE = MessageType::GATEWAY_CLIENT_CONNECT;
};

struct GatewayClientDisconnect {
    SessionID sessionId;
    PlayerID playerId;
    bool normalClose;  // true=正常退出, false=异常掉线

    static constexpr MessageType TYPE = MessageType::GATEWAY_CLIENT_DISCONNECT;
};

//==============================================================================
// 数据库消息
//==============================================================================

struct DbLoadRequest {
    PlayerID playerId;
    std::string tableName;

    static constexpr MessageType TYPE = MessageType::DB_LOAD_REQUEST;
};

struct DbLoadResponse {
    bool success;
    std::string jsonData;  // JSON 格式的玩家数据
    std::string errorMessage;

    static constexpr MessageType TYPE = MessageType::DB_LOAD_RESPONSE;
};

struct DbSaveRequest {
    PlayerID playerId;
    std::string tableName;
    std::string jsonData;  // JSON 格式的玩家数据

    static constexpr MessageType TYPE = MessageType::DB_SAVE_REQUEST;
};

struct DbSaveResponse {
    bool success;
    std::string errorMessage;

    static constexpr MessageType TYPE = MessageType::DB_SAVE_RESPONSE;
};

//==============================================================================
// CellApp 消息
//==============================================================================

struct CellCreateEntity {
    EntityID entityId;
    EntityType entityType;
    SpaceID spaceId;
    Position position;

    static constexpr MessageType TYPE = MessageType::CELL_CREATE_ENTITY;
};

struct CellDestroyEntity {
    EntityID entityId;

    static constexpr MessageType TYPE = MessageType::CELL_DESTROY_ENTITY;
};

struct CellEntityMove {
    EntityID entityId;
    Position oldPos;
    Position newPos;

    static constexpr MessageType TYPE = MessageType::CELL_ENTITY_MOVE;
};

struct CellEntityProperty {
    EntityID entityId;
    std::string propertyName;
    std::string propertyValue;  // JSON 格式

    static constexpr MessageType TYPE = MessageType::CELL_ENTITY_PROPERTY;
};

struct CellCrossBorder {
    EntityID entityId;
    SpaceID fromSpace;
    SpaceID toSpace;
    Position position;

    static constexpr MessageType TYPE = MessageType::CELL_CROSS_BORDER;
};

//==============================================================================
// 战斗消息
//==============================================================================

struct CombatSkillCast {
    EntityID casterId;
    EntityID targetId;
    uint32_t skillId;
    Position casterPos;
    Position targetPos;

    static constexpr MessageType TYPE = MessageType::COMBAT_SKILL_CAST;
};

struct CombatDamage {
    EntityID targetId;
    int32_t damage;
    EntityID sourceId;

    static constexpr MessageType TYPE = MessageType::COMBAT_DAMAGE;
};

struct CombatDeath {
    EntityID entityId;
    EntityID killerId;

    static constexpr MessageType TYPE = MessageType::COMBAT_DEATH;
};

//==============================================================================
// 聊天消息
//==============================================================================

enum class ChatChannel : uint8_t {
    CURRENT  = 0,  // 当前频道
    WORLD    = 1,  // 世界频道
    PRIVATE  = 2,  // 私聊
    GUILD    = 3,  // 公会
    PARTY    = 4,  // 队伍
    SYSTEM   = 5,  // 系统
};

struct ChatMessage {
    SessionID sessionId;
    PlayerID playerId;
    std::string playerName;
    ChatChannel channel;
    std::string content;

    static constexpr MessageType TYPE = MessageType::CHAT_MESSAGE;
};

//==============================================================================
// 系统消息
//==============================================================================

struct Ping {
    uint64_t timestamp;

    static constexpr MessageType TYPE = MessageType::PING;
};

struct Pong {
    uint64_t timestamp;

    static constexpr MessageType TYPE = MessageType::PONG;
};

struct ErrorMessage {
    uint32_t code;
    std::string message;

    static constexpr MessageType TYPE = MessageType::ERROR;
};

//==============================================================================
// 消息头 (网络传输)
//==============================================================================

#pragma pack(push, 1)
struct MessageHeader {
    uint32_t magic;        // 魔数 0x42575452 ("BWTR")
    uint16_t version;      // 版本号
    uint16_t type;         // 消息类型
    uint32_t length;       // 消息体长度
    uint64_t sequence;     // 序列号
    SessionID sessionId;   // 会话ID

    static constexpr uint32_t MAGIC = 0x42575452;
    static constexpr uint16_t CURRENT_VERSION = 1;
};
#pragma pack(pop)

//==============================================================================
// 辅助函数
//==============================================================================

inline const char* toString(MessageType type) {
    switch (type) {
        case MessageType::LOGIN_REQUEST: return "LOGIN_REQUEST";
        case MessageType::LOGIN_RESPONSE: return "LOGIN_RESPONSE";
        case MessageType::DB_LOAD_REQUEST: return "DB_LOAD_REQUEST";
        case MessageType::DB_LOAD_RESPONSE: return "DB_LOAD_RESPONSE";
        case MessageType::CELL_CREATE_ENTITY: return "CELL_CREATE_ENTITY";
        case MessageType::CELL_ENTITY_MOVE: return "CELL_ENTITY_MOVE";
        case MessageType::COMBAT_SKILL_CAST: return "COMBAT_SKILL_CAST";
        case MessageType::COMBAT_DAMAGE: return "COMBAT_DAMAGE";
        case MessageType::PING: return "PING";
        case MessageType::PONG: return "PONG";
        default: return "UNKNOWN";
    }
}

inline const char* toString(EntityType type) {
    switch (type) {
        case EntityType::PLAYER: return "PLAYER";
        case EntityType::NPC: return "NPC";
        case EntityType::MONSTER: return "MONSTER";
        case EntityType::PET: return "PET";
        case EntityType::ITEM: return "ITEM";
        default: return "UNKNOWN";
    }
}

inline const char* toString(ChatChannel channel) {
    switch (channel) {
        case ChatChannel::CURRENT: return "CURRENT";
        case ChatChannel::WORLD: return "WORLD";
        case ChatChannel::PRIVATE: return "PRIVATE";
        case ChatChannel::GUILD: return "GUILD";
        case ChatChannel::PARTY: return "PARTY";
        case ChatChannel::SYSTEM: return "SYSTEM";
        default: return "UNKNOWN";
    }
}

} // namespace protocol
} // namespace apollo
