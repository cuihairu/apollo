#pragma once

#include "apollo/core/log/logger.hpp"
#include "apollo/core/log/log_level.h"
#include "apollo/core/log/appender.h"
#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/console_appender.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志管理器配置
 */
struct LogManagerConfig {
    std::string defaultLoggerName = "root";   ///< 默认日志器名称
    LogLevel defaultLevel = LogLevel::Info;   ///< 默认日志级别

    // 控制台输出配置
    bool consoleEnabled = true;               ///< 是否启用控制台输出
    ConsoleAppenderConfig consoleConfig;      ///< 控制台追加器配置

    // 文件输出配置
    bool fileEnabled = false;                 ///< 是否启用文件输出
    FileAppenderConfig fileConfig;            ///< 文件追加器配置

    /**
     * @brief 创建默认配置
     */
    static LogManagerConfig createDefault() {
        return LogManagerConfig{};
    }

    /**
     * @brief 创建仅控制台配置
     */
    static LogManagerConfig createConsoleOnly(LogLevel level = LogLevel::Info) {
        LogManagerConfig cfg;
        cfg.defaultLevel = level;
        cfg.consoleEnabled = true;
        cfg.fileEnabled = false;
        return cfg;
    }

    /**
     * @brief 创建仅文件配置
     */
    static LogManagerConfig createFileOnly(
        const std::string& filePath,
        LogLevel level = LogLevel::Info)
    {
        LogManagerConfig cfg;
        cfg.defaultLevel = level;
        cfg.consoleEnabled = false;
        cfg.fileEnabled = true;
        cfg.fileConfig.baseName = filePath;
        return cfg;
    }

    /**
     * @brief 创建组合配置（控制台+文件）
     */
    static LogManagerConfig createCombined(
        const std::string& filePath,
        LogLevel level = LogLevel::Info)
    {
        LogManagerConfig cfg;
        cfg.defaultLevel = level;
        cfg.consoleEnabled = true;
        cfg.fileEnabled = true;
        cfg.fileConfig.baseName = filePath;
        return cfg;
    }
};

/**
 * @brief 日志管理器
 *
 * 全局单例，管理所有日志器和默认日志器
 */
class LogManager {
public:
    /**
     * @brief 获取单例实例
     */
    static LogManager& instance();

    // 禁止拷贝和移动
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    /**
     * @brief 初始化日志管理器
     */
    void initialize(const LogManagerConfig& config = LogManagerConfig::createDefault());

    /**
     * @brief 关闭日志管理器
     */
    void shutdown();

    /**
     * @brief 获取默认日志器
     */
    LoggerPtr getDefaultLogger();

    /**
     * @brief 获取指定名称的日志器
     */
    LoggerPtr getLogger(const std::string& name);

    /**
     * @brief 创建或获取日志器
     */
    LoggerPtr createLogger(const std::string& name, LogLevel level = LogLevel::All);

    /**
     * @brief 移除日志器
     */
    void removeLogger(const std::string& name);

    /**
     * @brief 检查日志器是否存在
     */
    bool hasLogger(const std::string& name) const;

    /**
     * @brief 获取所有日志器名称
     */
    std::vector<std::string> getLoggerNames() const;

    /**
     * @brief 设置默认日志级别
     */
    void setDefaultLevel(LogLevel level);

    /**
     * @brief 获取默认日志级别
     */
    LogLevel getDefaultLevel() const { return defaultLevel_; }

    /**
     * @brief 刷新所有日志器
     */
    void flushAll();

private:
    LogManager() = default;
    ~LogManager() = default;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    LogLevel defaultLevel_ = LogLevel::Info;
    std::string defaultLoggerName_ = "root";
    std::unordered_map<std::string, LoggerPtr> loggers_;

    /**
     * @brief 创建控制台追加器
     */
    IAppenderPtr createConsoleAppender(const ConsoleAppenderConfig& config);

    /**
     * @brief 创建文件追加器
     */
    IAppenderPtr createFileAppender(const FileAppenderConfig& config);
};

} // namespace log
} // namespace core
} // namespace apollo

//==============================================================================
// 便捷宏定义
//==============================================================================

/**
 * @brief 获取默认日志器
 */
#define APOLLO_LOG() apollo::core::log::LogManager::instance().getDefaultLogger()

/**
 * @brief 获取指定名称的日志器
 */
#define APOLLO_LOG_GET(name) apollo::core::log::LogManager::instance().getLogger(name)

/**
 * @brief 日志宏 - Debug
 */
#define APOLLO_LOG_DEBUG(msg) \
    do { \
        auto logger = APOLLO_LOG(); \
        if (logger && logger->isEnabled(apollo::core::log::LogLevel::Debug)) \
            logger->debug(msg); \
    } while (0)

/**
 * @brief 日志宏 - Info
 */
#define APOLLO_LOG_INFO(msg) \
    do { \
        auto logger = APOLLO_LOG(); \
        if (logger && logger->isEnabled(apollo::core::log::LogLevel::Info)) \
            logger->info(msg); \
    } while (0)

/**
 * @brief 日志宏 - Warning
 */
#define APOLLO_LOG_WARN(msg) \
    do { \
        auto logger = APOLLO_LOG(); \
        if (logger && logger->isEnabled(apollo::core::log::LogLevel::Warning)) \
            logger->warning(msg); \
    } while (0)

/**
 * @brief 日志宏 - Error
 */
#define APOLLO_LOG_ERROR(msg) \
    do { \
        auto logger = APOLLO_LOG(); \
        if (logger && logger->isEnabled(apollo::core::log::LogLevel::Error)) \
            logger->error(msg); \
    } while (0)

/**
 * @brief 日志宏 - Critical
 */
#define APOLLO_LOG_CRITICAL(msg) \
    do { \
        auto logger = APOLLO_LOG(); \
        if (logger && logger->isEnabled(apollo::core::log::LogLevel::Critical)) \
            logger->critical(msg); \
    } while (0)

/**
 * @brief 格式化日志宏 - Debug
 */
#define APOLLO_LOG_DEBUG_F(fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
        APOLLO_LOG_DEBUG(buf); \
    } while (0)

/**
 * @brief 格式化日志宏 - Info
 */
#define APOLLO_LOG_INFO_F(fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
        APOLLO_LOG_INFO(buf); \
    } while (0)

/**
 * @brief 格式化日志宏 - Warning
 */
#define APOLLO_LOG_WARN_F(fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
        APOLLO_LOG_WARN(buf); \
    } while (0)

/**
 * @brief 格式化日志宏 - Error
 */
#define APOLLO_LOG_ERROR_F(fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
        APOLLO_LOG_ERROR(buf); \
    } while (0)

/**
 * @brief 格式化日志宏 - Critical
 */
#define APOLLO_LOG_CRITICAL_F(fmt, ...) \
    do { \
        char buf[512]; \
        snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
        APOLLO_LOG_CRITICAL(buf); \
    } while (0)
