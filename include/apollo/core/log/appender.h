#pragma once

#include "apollo/core/log/log_level.h"
#include "apollo/core/log/log_record.h"
#include <string>
#include <memory>
#include <vector>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志追加器抽象接口
 *
 * 定义日志输出的目标（文件、控制台、网络等）
 */
class IAppender {
public:
    virtual ~IAppender() = default;

    /**
     * @brief 追加日志记录
     */
    virtual void append(const LogRecord& record) = 0;

    /**
     * @brief 刷新缓冲区
     */
    virtual void flush() = 0;

    /**
     * @brief 设置日志级别过滤器
     */
    virtual void setLevel(LogLevel level) {
        level_ = level;
    }

    /**
     * @brief 获取日志级别
     */
    virtual LogLevel getLevel() const {
        return level_;
    }

    /**
     * @brief 设置格式模式
     */
    virtual void setPattern(const std::string& pattern) {
        pattern_ = pattern;
    }

    /**
     * @brief 获取格式模式
     */
    virtual const std::string& getPattern() const {
        return pattern_;
    }

    /**
     * @brief 检查是否启用指定级别
     */
    bool isEnabled(LogLevel level) const {
        return (static_cast<uint32_t>(level) & static_cast<uint32_t>(level_)) != 0;
    }

    /**
     * @brief 格式化日志记录
     */
    std::string format(const LogRecord& record) const {
        return record.format(pattern_);
    }

protected:
    LogLevel level_ = LogLevel::All;
    std::string pattern_ = "[%Y-%m-%d %H:%M:%S.%f] [%l] [%n] %v";
};

/**
 * @brief 追加器智能指针类型
 */
using IAppenderPtr = std::shared_ptr<IAppender>;

/**
 * @brief 追加器列表类型
 */
using AppenderList = std::vector<IAppenderPtr>;

} // namespace log
} // namespace core
} // namespace apollo
