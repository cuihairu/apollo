/**
 * @file log_manager.cpp
 * @brief 日志管理器实现 (基于 spdlog)
 */

#include "apollo/core/log/log_manager.h"

// 检测是否使用 spdlog
#ifdef SPDLOG_HEADER_ONLY
    #define APOLLO_USE_SPDLOG 1
#elif defined(SPDLOG_COMPILED_LIB)
    #define APOLLO_USE_SPDLOG 1
#endif

#ifdef APOLLO_USE_SPDLOG

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <mutex>

namespace apollo {
namespace core {
namespace log {

//==============================================================================
// spdlog 日志级别转换
//==============================================================================

namespace {
    spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
        switch (level) {
            case LogLevel::Trace:   return spdlog::level::trace;
            case LogLevel::Debug:   return spdlog::level::debug;
            case LogLevel::Info:    return spdlog::level::info;
            case LogLevel::Warning: return spdlog::level::warn;
            case LogLevel::Error:   return spdlog::level::err;
            case LogLevel::Fatal:   return spdlog::level::critical;
            default:                return spdlog::level::info;
        }
    }
}

//==============================================================================
// LogManager 实现 (spdlog 版本)
//==============================================================================

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

void LogManager::initialize(const LogManagerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    defaultLevel_ = config.defaultLevel;
    defaultLoggerName_ = config.defaultLoggerName;

    // 设置 spdlog 全局格式
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
    spdlog::set_level(toSpdlogLevel(config.defaultLevel));

    // 创建 sinks
    std::vector<spdlog::sink_ptr> sinks;

    if (config.consoleEnabled) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        if (!config.consoleConfig.useColors) {
            console_sink->set_color_mode(spdlog::color_mode::off);
        }
        if (!config.consoleConfig.timestamp) {
            console_sink->set_pattern("[%^%l%$] %v");
        }
        sinks.push_back(console_sink);
    }

    if (config.fileEnabled && !config.fileConfig.filePath.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.fileConfig.filePath,
            config.fileConfig.maxSize,
            config.fileConfig.maxFiles
        );
        sinks.push_back(file_sink);
    }

    if (!sinks.empty()) {
        auto default_logger = std::make_shared<spdlog::logger>(config.defaultLoggerName, sinks.begin(), sinks.end());
        spdlog::register_logger(default_logger);
        spdlog::set_default_logger(default_logger);
    }

    // 自动刷新（如果启用）
    if (config.fileConfig.autoFlush) {
        spdlog::flush_on(spdlog::level::warn);
    }

    initialized_ = true;
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::shutdown();
    loggers_.clear();
    initialized_ = false;
}

std::shared_ptr<Logger> LogManager::getDefaultLogger() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        const_cast<LogManager*>(this)->initialize(LogManagerConfig::createDefault());
    }

    auto logger = spdlog::get(defaultLoggerName_);
    return std::shared_ptr<Logger>(logger.get(), [](Logger*) {});
}

std::shared_ptr<Logger> LogManager::getLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto logger = spdlog::get(name);
    return std::shared_ptr<Logger>(logger.get(), [](Logger*) {});
}

std::shared_ptr<Logger> LogManager::createLogger(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto logger = spdlog::get(name);
    if (!logger) {
        logger = std::make_shared<spdlog::logger>(name, spdlog::default_logger()->sinks());
        logger->set_level(toSpdlogLevel(level));
        spdlog::register_logger(logger);
    }
    return std::shared_ptr<Logger>(logger.get(), [](Logger*) {});
}

void LogManager::removeLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (name != defaultLoggerName_) {
        spdlog::drop(name);
        loggers_.erase(name);
    }
}

bool LogManager::hasLogger(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return spdlog::get(name) != nullptr;
}

std::vector<std::string> LogManager::getLoggerNames() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return spdlog::get_names();
}

void LogManager::setDefaultLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    defaultLevel_ = level;
    spdlog::set_level(toSpdlogLevel(level));
}

void LogManager::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
        logger->flush();
    });
}

std::shared_ptr<IAppender> LogManager::createConsoleAppender(const ConsoleAppenderConfig& config) {
    // spdlog 没有 Appender 概念，返回 nullptr
    return nullptr;
}

std::shared_ptr<IAppender> LogManager::createFileAppender(const FileAppenderConfig& config) {
    // spdlog 没有 Appender 概念，返回 nullptr
    return nullptr;
}

void LogManager::write(LogLevel level, std::string logger, std::string message) {
    auto spdlog_level = toSpdlogLevel(level);
    auto log = spdlog::get(logger);
    if (log) {
        log->log(spdlog_level, "{}", message);
    } else {
        spdlog::default_logger()->log(spdlog_level, "[{}] {}", logger, message);
    }
}

} // namespace log
} // namespace core
} // namespace apollo

#else // !APOLLO_USE_SPDLOG

// 使用内置实现的占位符
#include <iostream>
#include <mutex>

namespace apollo {
namespace core {
namespace log {

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

void LogManager::initialize(const LogManagerConfig&) { initialized_ = true; }
void LogManager::shutdown() { initialized_ = false; }
std::shared_ptr<Logger> LogManager::getDefaultLogger() { return nullptr; }
std::shared_ptr<Logger> LogManager::getLogger(const std::string&) { return nullptr; }
std::shared_ptr<Logger> LogManager::createLogger(const std::string&, LogLevel) { return nullptr; }
void LogManager::removeLogger(const std::string&) {}
bool LogManager::hasLogger(const std::string&) const { return false; }
std::vector<std::string> LogManager::getLoggerNames() const { return {}; }
void LogManager::setDefaultLevel(LogLevel) {}
void LogManager::flushAll() {}
std::shared_ptr<IAppender> LogManager::createConsoleAppender(const ConsoleAppenderConfig&) { return nullptr; }
std::shared_ptr<IAppender> LogManager::createFileAppender(const FileAppenderConfig&) { return nullptr; }
void LogManager::write(LogLevel, std::string, std::string message) {
    std::cerr << "[LOG] " << message << std::endl;
}

} // namespace log
} // namespace core
} // namespace apollo

#endif // APOLLO_USE_SPDLOG

namespace apollo {
namespace core {
namespace log {

LogManager& global_log_manager() {
    return LogManager::instance();
}

} // namespace log
} // namespace core
} // namespace apollo
