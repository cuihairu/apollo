/**
 * @file log_manager.cpp
 * @brief 日志管理器实现
 */

#include "apollo/core/log/log_manager.h"
#include <iostream>

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

    // 创建默认日志器
    auto defaultLogger = std::make_shared<Logger>(defaultLoggerName_, defaultLevel_);

    // 添加控制台追加器
    if (config.consoleEnabled) {
        auto consoleAppender = std::make_shared<ConsoleAppender>(config.consoleConfig);
        consoleAppender->setLevel(defaultLevel_);
        defaultLogger->addAppender(consoleAppender);
    }

    // 添加文件追加器
    if (config.fileEnabled) {
        auto fileAppender = std::make_shared<FileAppender>(config.fileConfig);
        fileAppender->setLevel(defaultLevel_);
        defaultLogger->addAppender(fileAppender);
    }

    loggers_[defaultLoggerName_] = defaultLogger;
    initialized_ = true;
}

void LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 刷新所有日志器
    for (auto& pair : loggers_) {
        pair.second->flush();
    }

    loggers_.clear();
    initialized_ = false;
}

LoggerPtr LogManager::getDefaultLogger() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        // 自动初始化为默认配置
        const_cast<LogManager*>(this)->initialize(LogManagerConfig::createDefault());
    }

    auto it = loggers_.find(defaultLoggerName_);
    if (it != loggers_.end()) {
        return it->second;
    }

    // 如果默认日志器不存在，创建一个
    auto logger = std::make_shared<Logger>(defaultLoggerName_, defaultLevel_);
    loggers_[defaultLoggerName_] = logger;
    return logger;
}

LoggerPtr LogManager::getLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    // 不存在则创建一个新的
    auto logger = std::make_shared<Logger>(name, defaultLevel_);

    // 继承默认日志器的追加器
    auto defaultLogger = getDefaultLogger();
    if (defaultLogger) {
        for (auto& appender : defaultLogger->getAppenders()) {
            logger->addAppender(appender);
        }
    }

    loggers_[name] = logger;
    return logger;
}

LoggerPtr LogManager::createLogger(const std::string& name, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto logger = std::make_shared<Logger>(name, level);
    loggers_[name] = logger;
    return logger;
}

void LogManager::removeLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (name == defaultLoggerName_) {
        return; // 不允许删除默认日志器
    }

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        it->second->flush();
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

    // 更新所有日志器的级别
    for (auto& pair : loggers_) {
        pair.second->setLevel(level);
    }
}

void LogManager::flushAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& pair : loggers_) {
        pair.second->flush();
    }
}

IAppenderPtr LogManager::createConsoleAppender(const ConsoleAppenderConfig& config) {
    return std::make_shared<ConsoleAppender>(config);
}

IAppenderPtr LogManager::createFileAppender(const FileAppenderConfig& config) {
    return std::make_shared<FileAppender>(config);
}

} // namespace log
} // namespace core
} // namespace apollo
