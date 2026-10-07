#pragma once

#include "apollo/core/log/appender.h"
#include <mutex>
#include <iostream>
#include <string>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 控制台颜色支持
 */
enum class ConsoleColor {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

/**
 * @brief 控制台追加器配置
 */
struct ConsoleAppenderConfig {
    bool useColor = true;              ///< 是否使用颜色
    bool useStderr = false;            ///< 输出到 stderr 而非 stdout
    bool flushOnWrite = false;         ///< 写入后立即刷新

    // 颜色映射
    ConsoleColor debugColor = ConsoleColor::Cyan;
    ConsoleColor infoColor = ConsoleColor::Green;
    ConsoleColor warningColor = ConsoleColor::Yellow;
    ConsoleColor errorColor = ConsoleColor::Red;
    ConsoleColor criticalColor = ConsoleColor::Magenta;
};

/**
 * @brief 控制台日志追加器
 *
 * 将日志输出到标准输出或标准错误
 * 支持彩色输出（可选）
 */
class ConsoleAppender : public IAppender {
public:
    /**
     * @brief 构造函数
     */
    explicit ConsoleAppender(const ConsoleAppenderConfig& config = ConsoleAppenderConfig{});

    /**
     * @brief 析构函数
     */
    ~ConsoleAppender() override = default;

    /**
     * @brief 追加日志记录
     */
    void append(const LogRecord& record) override;

    /**
     * @brief 刷新缓冲区
     */
    void flush() override;

    /**
     * @brief 获取配置
     */
    const ConsoleAppenderConfig& getConfig() const { return config_; }

    /**
     * @brief 设置配置
     */
    void setConfig(const ConsoleAppenderConfig& config) { config_ = config; }

private:
    ConsoleAppenderConfig config_;
    mutable std::mutex mutex_;

    /**
     * @brief 设置控制台颜色
     */
    void setColor(ConsoleColor color);

    /**
     * @brief 重置控制台颜色
     */
    void resetColor();

    /**
     * @brief 获取级别对应的颜色
     */
    ConsoleColor getColorForLevel(LogLevel level) const;

    /**
     * @brief 获取输出流
     */
    std::ostream& getStream();
};

/**
 * @brief 控制台追加器智能指针类型
 */
using ConsoleAppenderPtr = std::shared_ptr<ConsoleAppender>;

} // namespace log
} // namespace core
} // namespace apollo
