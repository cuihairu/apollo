/**
 * @file logger.cpp
 * @brief 标准日志器实现
 */

#include "apollo/core/log/logger.hpp"
#include <algorithm>

namespace apollo {
namespace core {
namespace log {

//==============================================================================
// Logger 实现
//==============================================================================

Logger::Logger(std::string name, LogLevel level)
    : name_(std::move(name))
    , level_(level)
{
}

void Logger::log(const LogRecord& record) {
    // 检查级别
    if (!isEnabled(record.getLevel())) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 创建副本并设置日志器名称
    LogRecord newRecord = record;
    if (newRecord.getLoggerName().empty()) {
        newRecord.setLoggerName(name_);
    }

    // 发送到所有追加器
    for (auto& appender : appenders_) {
        if (appender->isEnabled(record.getLevel())) {
            appender->append(newRecord);
        }
    }
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& appender : appenders_) {
        appender->flush();
    }
}

void Logger::addAppender(IAppenderPtr appender) {
    if (!appender) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // 检查是否已存在
    auto it = std::find(appenders_.begin(), appenders_.end(), appender);
    if (it == appenders_.end()) {
        appenders_.push_back(appender);
    }
}

void Logger::removeAppender(IAppenderPtr appender) {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.erase(
        std::remove(appenders_.begin(), appenders_.end(), appender),
        appenders_.end()
    );
}

void Logger::clearAppenders() {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.clear();
}

} // namespace log
} // namespace core
} // namespace apollo
