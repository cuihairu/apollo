#pragma once

#include "apollo/core/log/log_level.h"
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>
#include <source_location>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief 日志记录
 *
 * 存储单条日志的完整信息
 */
class LogRecord {
public:
    LogRecord() = default;

    LogRecord(LogLevel level,
              const std::string& message,
              const std::string& loggerName = "",
              const std::string& file = "",
              int line = 0,
              const std::string& function = "")
        : level_(level)
        , message_(message)
        , loggerName_(loggerName)
        , file_(file)
        , line_(line)
        , function_(function)
        , timestamp_(std::chrono::system_clock::now())
    {}

    // Getter 方法
    LogLevel getLevel() const { return level_; }
    const std::string& getMessage() const { return message_; }
    const std::string& getLoggerName() const { return loggerName_; }
    const std::string& getFile() const { return file_; }
    int getLine() const { return line_; }
    const std::string& getFunction() const { return function_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }

    // Setter 方法
    void setLevel(LogLevel level) { level_ = level; }
    void setMessage(const std::string& msg) { message_ = msg; }
    void setLoggerName(const std::string& name) { loggerName_ = name; }
    void setLocation(const std::string& file, int line, const std::string& func) {
        file_ = file;
        line_ = line;
        function_ = func;
    }

    /**
     * @brief 格式化日志记录为字符串
     */
    std::string format() const {
        return format("[%Y-%m-%d %H:%M:%S.%f] [%l] [%n] %v");
    }

    /**
     * @brief 按指定格式格式化日志记录
     *
     * 支持的占位符：
     *   %Y - 年份（4位）
     *   %m - 月份（2位）
     *   %d - 日期（2位）
     *   %H - 小时（2位）
     *   %M - 分钟（2位）
     *   %S - 秒（2位）
     *   %f - 毫秒（3位）
     *   %l - 日志级别
     *   %n - 日志器名称
     *   %v - 日志消息
     *   %F - 源文件名
     *   %L - 行号
     *   %C - 函数名
     *   %t - 线程ID
     */
    std::string format(const std::string& pattern) const {
        std::string result = pattern;
        std::tm tm = getLocalTime(timestamp_);

        // 替换时间相关占位符
        replaceAll(result, "%Y", formatInt(tm.tm_year + 1900, 4));
        replaceAll(result, "%m", formatInt(tm.tm_mon + 1, 2));
        replaceAll(result, "%d", formatInt(tm.tm_mday, 2));
        replaceAll(result, "%H", formatInt(tm.tm_hour, 2));
        replaceAll(result, "%M", formatInt(tm.tm_min, 2));
        replaceAll(result, "%S", formatInt(tm.tm_sec, 2));
        replaceAll(result, "%f", formatMillis(timestamp_));
        replaceAll(result, "%l", toString(level_));
        replaceAll(result, "%n", loggerName_.empty() ? "-" : loggerName_);
        replaceAll(result, "%v", message_);
        replaceAll(result, "%F", file_.empty() ? "-" : extractFileName(file_));
        replaceAll(result, "%L", line_ > 0 ? std::to_string(line_) : "-");
        replaceAll(result, "%C", function_.empty() ? "-" : function_);
        replaceAll(result, "%t", getThreadId());

        return result;
    }

    /**
     * @brief 获取简短格式（仅时间、级别、消息）
     */
    std::string toShortString() const {
        return format("[%H:%M:%S.%f] [%l] %v");
    }

    /**
     * @brief 获取完整格式（包含文件位置）
     */
    std::string toFullString() const {
        return format("[%Y-%m-%d %H:%M:%S.%f] [%l] [%n:%F:%L] %v");
    }

private:
    LogLevel level_ = LogLevel::Info;
    std::string message_;
    std::string loggerName_;
    std::string file_;
    int line_ = 0;
    std::string function_;
    std::chrono::system_clock::time_point timestamp_;

    static std::tm getLocalTime(const std::chrono::system_clock::time_point& tp) {
        time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        return tm;
    }

    static std::string formatInt(int value, int width) {
        std::ostringstream oss;
        oss << std::setw(width) << std::setfill('0') << value;
        return oss.str();
    }

    static std::string formatMillis(const std::chrono::system_clock::time_point& tp) {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()) % 1000;
        return formatInt(static_cast<int>(ms.count()), 3);
    }

    static std::string getThreadId() {
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        return oss.str();
    }

    static std::string extractFileName(const std::string& path) {
        size_t pos = path.find_last_of("/\\");
        return (pos != std::string::npos) ? path.substr(pos + 1) : path;
    }

    static void replaceAll(std::string& str, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            str.replace(pos, from.length(), to);
            pos += to.length();
        }
    }
};

} // namespace log
} // namespace core
} // namespace apollo
