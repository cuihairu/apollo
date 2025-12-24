#pragma once

#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include <string>
#include <memory>
#include <string_view>
#include <source_location>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志器抽象接口
 *
 * 定义所有日志器必须实现的基本功能
 */
class ILogger {
public:
    virtual ~ILogger() = default;

    /**
     * @brief 写入日志记录
     */
    virtual void log(const LogRecord& record) = 0;

    /**
     * @brief 刷新缓冲区
     */
    virtual void flush() = 0;

    /**
     * @brief 获取日志器名称
     */
    virtual const std::string& getName() const = 0;

    /**
     * @brief 设置日志级别
     */
    virtual void setLevel(LogLevel level) = 0;

    /**
     * @brief 获取日志级别
     */
    virtual LogLevel getLevel() const = 0;

    /**
     * @brief 检查是否启用指定级别
     */
    virtual bool isEnabled(LogLevel level) const = 0;

    // 便捷日志方法
    void debug(const std::string& msg,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Debug, msg, loc);
    }

    void info(const std::string& msg,
              std::source_location loc = std::source_location::current()) {
        log(LogLevel::Info, msg, loc);
    }

    void warning(const std::string& msg,
                 std::source_location loc = std::source_location::current()) {
        log(LogLevel::Warning, msg, loc);
    }

    void error(const std::string& msg,
               std::source_location loc = std::source_location::current()) {
        log(LogLevel::Error, msg, loc);
    }

    void critical(const std::string& msg,
                  std::source_location loc = std::source_location::current()) {
        log(LogLevel::Critical, msg, loc);
    }

protected:
    void log(LogLevel level, const std::string& msg,
             std::source_location loc) {
        if (!isEnabled(level)) return;
        LogRecord record(level, msg, getName(),
                        loc.file_name(), loc.line(), loc.function_name());
        log(record);
    }
};

/**
 * @brief 日志器智能指针类型
 */
using ILoggerPtr = std::shared_ptr<ILogger>;

} // namespace log
} // namespace core
} // namespace apollo
