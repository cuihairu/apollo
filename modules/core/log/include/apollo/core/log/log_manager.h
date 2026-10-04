#pragma once

#include "apollo/core/log/logger.hpp"
#include "apollo/core/log/log_level.h"
#include "apollo/core/log/appender.h"
#include "apollo/core/log/file_appender.h"
#include "apollo/core/log/console_appender.h"
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace apollo {
namespace core {
namespace log {

// 兼容别名（P0-6 双树收敛）：旧内存版 LogLevel 有 Warn，活树是 Warning
inline constexpr LogLevel Warn = LogLevel::Warning;

// 旧内存版 API 面（P0-6 并入）：to_string 自由函数（log_level.h 提供驼峰
// toString，这里补 snake_case 拼写）
inline std::string_view to_string(LogLevel level) {
    return toString(level);
}

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
};

struct LogManagerConfig {
    std::string defaultLoggerName = "root";
    LogLevel defaultLevel = LogLevel::Info;

    bool consoleEnabled = true;
    ConsoleAppenderConfig consoleConfig;

    bool fileEnabled = false;
    FileAppenderConfig fileConfig;

    static LogManagerConfig createDefault() {
        return LogManagerConfig{};
    }

    static LogManagerConfig createConsoleOnly(LogLevel level = LogLevel::Info) {
        LogManagerConfig cfg;
        cfg.defaultLevel = level;
        cfg.consoleEnabled = true;
        cfg.fileEnabled = false;
        return cfg;
    }

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

class LogManager {
public:
    static LogManager& instance();

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    void initialize(const LogManagerConfig& config = LogManagerConfig::createDefault());
    void shutdown();

    LoggerPtr getDefaultLogger();
    LoggerPtr getLogger(const std::string& name);
    LoggerPtr createLogger(const std::string& name, LogLevel level = LogLevel::All);
    void removeLogger(const std::string& name);

    bool hasLogger(const std::string& name) const;
    std::vector<std::string> getLoggerNames() const;

    void setDefaultLevel(LogLevel level);
    LogLevel getDefaultLevel() const { return defaultLevel_; }

    void flushAll();
    void write(LogLevel level, std::string logger, std::string message);

    // ---- 收敛面（P0-6）：旧内存版四方法并入活实现，ODR 消除 ----
    // write() 的控制台镜像开关（不改变 appender 链；默认关）
    void set_console_enabled(bool enabled);
    // 清空快照环形缓冲
    void clear();
    // 近期条目快照（环形上限 1024，drop-oldest）
    std::vector<LogEntry> snapshot() const;

private:
    LogManager() = default;
    ~LogManager() = default;

    void record_entry(LogLevel level, const std::string& logger, const std::string& message);

    mutable std::mutex mutex_;
    bool initialized_ = false;
    LogLevel defaultLevel_ = LogLevel::Info;
    std::string defaultLoggerName_ = "root";
    std::unordered_map<std::string, LoggerPtr> loggers_;

    bool console_mirror_ = false;
    static constexpr std::size_t kEntryRingCapacity = 1024;
    std::vector<LogEntry> entry_ring_;

    IAppenderPtr createConsoleAppender(const ConsoleAppenderConfig& config);
    IAppenderPtr createFileAppender(const FileAppenderConfig& config);
};

// 全局单例入口（P0-6 补声明：此前只有 .cpp 定义、头无声明——链接全靠旧头
// 的 ODR 撞名撑着）
LogManager& global_log_manager();

} // namespace log
} // namespace core
} // namespace apollo
