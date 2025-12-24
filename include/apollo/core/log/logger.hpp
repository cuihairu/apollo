#pragma once

#include "apollo/core/log/logger.h"
#include "apollo/core/log/appender.h"
#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <source_location>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 标准日志器实现
 *
 * 支持多个追加器，每个日志记录会发送到所有追加器
 */
class Logger : public ILogger {
public:
    /**
     * @brief 构造函数
     */
    Logger(std::string name, LogLevel level = LogLevel::All);

    /**
     * @brief 析构函数
     */
    ~Logger() override = default;

    /**
     * @brief 写入日志记录
     */
    void log(const LogRecord& record) override;

    /**
     * @brief 刷新缓冲区
     */
    void flush() override;

    /**
     * @brief 获取日志器名称
     */
    const std::string& getName() const override { return name_; }

    /**
     * @brief 设置日志级别
     */
    void setLevel(LogLevel level) override { level_ = level; }

    /**
     * @brief 获取日志级别
     */
    LogLevel getLevel() const override { return level_; }

    /**
     * @brief 检查是否启用指定级别
     */
    bool isEnabled(LogLevel level) const override {
        return (static_cast<uint32_t>(level) & static_cast<uint32_t>(level_)) != 0;
    }

    /**
     * @brief 添加追加器
     */
    void addAppender(IAppenderPtr appender);

    /**
     * @brief 移除追加器
     */
    void removeAppender(IAppenderPtr appender);

    /**
     * @brief 获取所有追加器
     */
    const AppenderList& getAppenders() const { return appenders_; }

    /**
     * @brief 清空所有追加器
     */
    void clearAppenders();

private:
    std::string name_;
    LogLevel level_;
    AppenderList appenders_;
    mutable std::mutex mutex_;
};

/**
 * @brief 日志器智能指针类型
 */
using LoggerPtr = std::shared_ptr<Logger>;

} // namespace log
} // namespace core
} // namespace apollo
