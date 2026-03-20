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

private:
    LogManager() = default;
    ~LogManager() = default;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    LogLevel defaultLevel_ = LogLevel::Info;
    std::string defaultLoggerName_ = "root";
    std::unordered_map<std::string, LoggerPtr> loggers_;

    IAppenderPtr createConsoleAppender(const ConsoleAppenderConfig& config);
    IAppenderPtr createFileAppender(const FileAppenderConfig& config);
};

} // namespace log
} // namespace core
} // namespace apollo
