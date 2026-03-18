#pragma once

#include <cstdint>
#include <string>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel : uint32_t {
    None     = 0x0000,  ///< 禁用日志
    Debug    = 0x0001,  ///< 调试信息
    Info     = 0x0002,  ///< 一般信息
    Warning  = 0x0004,  ///< 警告信息
    Error    = 0x0008,  ///< 错误信息
    Critical = 0x0010,  ///< 严重错误
    All      = 0xFFFF   ///< 所有级别
};

/**
 * @brief 日志级别转换为字符串
 */
inline const char* toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:    return "DEBUG";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warning:  return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        case LogLevel::None:
        case LogLevel::All:      return "NONE";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief 字符串转换为日志级别
 */
inline LogLevel fromString(const std::string& str) {
    if (str == "DEBUG")    return LogLevel::Debug;
    if (str == "INFO")     return LogLevel::Info;
    if (str == "WARN" || str == "WARNING") return LogLevel::Warning;
    if (str == "ERROR")    return LogLevel::Error;
    if (str == "CRITICAL" || str == "FATAL") return LogLevel::Critical;
    return LogLevel::Info;
}

/**
 * @brief 位运算支持（用于组合多个级别）
 */
inline LogLevel operator|(LogLevel a, LogLevel b) {
    return static_cast<LogLevel>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline LogLevel operator&(LogLevel a, LogLevel b) {
    return static_cast<LogLevel>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasLevel(LogLevel mask, LogLevel level) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(level)) != 0;
}

} // namespace log
} // namespace core
} // namespace apollo
