#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "apollo/core/log/log_level.hpp"

namespace apollo {
namespace core {
namespace log {

class Logger;
class IAppender;

struct ConsoleAppenderConfig {
    bool useColors = true;
    bool timestamp = true;
};

struct FileAppenderConfig {
    std::string filePath;
    size_t maxSize = 10 * 1024 * 1024;  // 10 MB
    int maxFiles = 5;
    bool autoFlush = true;
};

struct LogManagerConfig {
    LogLevel defaultLevel = LogLevel::Info;
    std::string defaultLoggerName = "default";
    bool consoleEnabled = true;
    bool fileEnabled = false;
    ConsoleAppenderConfig consoleConfig;
    FileAppenderConfig fileConfig;

    static LogManagerConfig createDefault() {
        return LogManagerConfig{};
    }
};

class LogManager {
public:
    static LogManager& instance();

    void initialize(const LogManagerConfig& config);
    void shutdown();

    std::shared_ptr<Logger> getDefaultLogger();
    std::shared_ptr<Logger> getLogger(const std::string& name);
    std::shared_ptr<Logger> createLogger(const std::string& name, LogLevel level);
    void removeLogger(const std::string& name);

    bool hasLogger(const std::string& name) const;
    std::vector<std::string> getLoggerNames() const;

    void setDefaultLevel(LogLevel level);
    void flushAll();

    std::shared_ptr<IAppender> createConsoleAppender(const ConsoleAppenderConfig& config);
    std::shared_ptr<IAppender> createFileAppender(const FileAppenderConfig& config);

    // Write log message directly
    void write(LogLevel level, std::string logger, std::string message);

private:
    LogManager() = default;
    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    LogLevel defaultLevel_ = LogLevel::Info;
    std::string defaultLoggerName_ = "default";
    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
};

// Global accessor function
LogManager& global_log_manager();

} // namespace log
} // namespace core
} // namespace apollo
