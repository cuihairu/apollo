#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace apollo::core {

/// 服务器类型枚举
enum class ServerType : uint16_t {
    UNKNOWN         = 0,     // 未知类型
    LOGIN_GATE      = 1,     // 登录网关
    LOGIN_SERVER    = 2,     // 登录服务器
    DB_SERVER       = 3,     // 数据库服务器
    GAME_GATE       = 4,     // 游戏网关
    NEWM_SERVER     = 5,     // 辅助服务器 (组队/公会)
    GAME_SERVER     = 6,     // 游戏逻辑服务器
    LOG_SERVER      = 7,     // 日志服务器
    WORLD_SERVER    = 8,     // 跨服世界服务器
    CHAT_SERVER     = 9,     // 聊天服务器
    AGENT_SERVER    = 10,    // 代理服务器

    // 1000+: 用户自定义服务类型
    USER_DEFINED    = 1000,
};

/// 获取服务器类型名称
inline std::string_view GetServerTypeName(ServerType type) {
    switch (type) {
        case ServerType::LOGIN_GATE:    return "LoginGate";
        case ServerType::LOGIN_SERVER:  return "LoginServer";
        case ServerType::DB_SERVER:     return "DBServer";
        case ServerType::GAME_GATE:     return "GameGate";
        case ServerType::NEWM_SERVER:   return "NewMServer";
        case ServerType::GAME_SERVER:   return "GameServer";
        case ServerType::LOG_SERVER:    return "LogServer";
        case ServerType::WORLD_SERVER:  return "WorldServer";
        case ServerType::CHAT_SERVER:   return "ChatServer";
        case ServerType::AGENT_SERVER:  return "AgentServer";
        default:                        return "Unknown";
    }
}

/// 服务器状态枚举
enum class ServerStatus : uint8_t {
    OFFLINE = 0,     // 离线
    STARTING,        // 启动中
    RUNNING,         // 运行中
    STOPPING,        // 停止中
    MAINTENANCE,      // 维护中
    ERROR            // 错误状态
};

/// 获取服务器状态名称
inline std::string_view GetServerStatusName(ServerStatus status) {
    switch (status) {
        case ServerStatus::OFFLINE:    return "Offline";
        case ServerStatus::STARTING:   return "Starting";
        case ServerStatus::RUNNING:    return "Running";
        case ServerStatus::STOPPING:   return "Stopping";
        case ServerStatus::MAINTENANCE:return "Maintenance";
        case ServerStatus::ERROR:      return "Error";
        default:                       return "Unknown";
    }
}

}  // namespace apollo::core