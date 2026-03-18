/**
 * @file log_manager.cpp
 * @brief 日志管理器实现
 */

#include "apollo/core/log/log_manager.h"
#include <iostream>
#include <mutex>

namespace apollo {
namespace core {
namespace log {

//==============================================================================
// LogManager 实现
//==============================================================================

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

void LogManager::initialize(const LogManagerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return; // 已初始化
    }

    defaultLevel_ = config.defaultLevel;
    defaultLoggerName_ = config.defaultLoggerName;
    initialized_ = true;
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.clear();
    initialized_ = false;
}

std::shared_ptr<Logger> LogManager::getDefaultLogger() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        // 自动初始化为默认配置
        const_cast<LogManager*>(this)->initialize(LogManagerConfig::createDefault());
    }

    auto it = loggers_.find(defaultLoggerName_);
    if (it != loggers_.end()) {
        return it->second;
    }

    // Stub: return null if logger not found
    return nullptr;
}

std::shared_ptr<Logger> LogManager::getLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    // Stub: return null if logger not found
    return nullptr;
}

std::shared_ptr<Logger> LogManager::createLogger(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Stub: not implemented
    return nullptr;
}

void LogManager::removeLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (name == defaultLoggerName_) {
        return; // 不允许删除默认日志器
    }

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        loggers_.erase(it);
    }
}

bool LogManager::hasLogger(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loggers_.find(name) != loggers_.end();
}

std::vector<std::string> LogManager::getLoggerNames() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(loggers_.size());
    for (const auto& pair : loggers_) {
        names.push_back(pair.first);
    }
    return names;
}

void LogManager::setDefaultLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    defaultLevel_ = level;
}

void LogManager::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    // Stub: nothing to flush
}

std::shared_ptr<IAppender> LogManager::createConsoleAppender(const ConsoleAppenderConfig& config) {
    // Stub: not implemented
    return nullptr;
}

std::shared_ptr<IAppender> LogManager::createFileAppender(const FileAppenderConfig& config) {
    // Stub: not implemented
    return nullptr;
}

void LogManager::write(LogLevel level, std::string logger, std::string message) {
    // Stub: just output to stderr
    std::cerr << "[" << static_cast<int>(level) << "] " << logger << ": " << message << std::endl;
}

//==============================================================================
// Global functions
//==============================================================================

LogManager& global_log_manager() {
    return LogManager::instance();
}

} // namespace log
} // namespace core
} // namespace apollo
