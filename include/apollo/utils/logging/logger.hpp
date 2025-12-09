#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <atomic>

namespace apollo {

/// 日志级别
enum class LogLevel : int {
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
};

/// 获取日志级别名称
inline const char* GetLogLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:               return "UNKN ";
    }
}

/// 日志事件
struct LogEvent {
    LogLevel level;
    std::string loggerName;
    std::string file;
    int line;
    std::string function;
    std::string message;
    uint64_t timestamp;
    std::thread::id threadId;

    LogEvent(LogLevel level, const std::string& loggerName,
             const std::string& file, int line,
             const std::string& function, const std::string& message)
        : level(level), loggerName(loggerName), file(file), line(line),
          function(function), message(message) {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        threadId = std::this_thread::get_id();
    }
};

/// 日志输出器接口
class LogAppender {
public:
    virtual ~LogAppender() = default;
    virtual void Append(const LogEvent& event) = 0;
};

/// 控制台输出器
class ConsoleAppender : public LogAppender {
public:
    void Append(const LogEvent& event) override;
};

/// 文件输出器
class FileAppender : public LogAppender {
public:
    explicit FileAppender(const std::string& filename, bool async = false);
    ~FileAppender();

    void Append(const LogEvent& event) override;

private:
    void WriteToFile(const LogEvent& event);
    void AsyncWriteLoop();

    std::ofstream file_;
    std::mutex mutex_;
    bool async_;
    std::queue<LogEvent> eventQueue_;
    std::condition_variable cond_;
    std::thread writeThread_;
    std::atomic<bool> running_{true};
};

/// 日志器
class Logger {
public:
    explicit Logger(const std::string& name);
    ~Logger() = default;

    /// 设置日志级别
    void SetLevel(LogLevel level) { level_ = level; }

    /// 获取日志级别
    LogLevel GetLevel() const { return level_; }

    /// 添加输出器
    void AddAppender(std::shared_ptr<LogAppender> appender);

    /// 移除所有输出器
    void ClearAppenders();

    /// 日志记录接口
    void Log(LogLevel level, const std::string& file, int line,
             const std::string& function, const std::string& message);

    /// 便捷接口
    template<typename... Args>
    void Trace(const std::string& file, int line, const std::string& function,
               const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::TRACE) {
            Log(LogLevel::TRACE, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    void Debug(const std::string& file, int line, const std::string& function,
               const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::DEBUG) {
            Log(LogLevel::DEBUG, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    void Info(const std::string& file, int line, const std::string& function,
              const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::INFO) {
            Log(LogLevel::INFO, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    void Warn(const std::string& file, int line, const std::string& function,
              const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::WARN) {
            Log(LogLevel::WARN, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    void Error(const std::string& file, int line, const std::string& function,
               const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::ERROR) {
            Log(LogLevel::ERROR, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

    template<typename... Args>
    void Fatal(const std::string& file, int line, const std::string& function,
               const std::string& fmt, Args&&... args) {
        if (level_ <= LogLevel::FATAL) {
            Log(LogLevel::FATAL, file, line, function, Format(fmt, std::forward<Args>(args)...));
        }
    }

private:
    std::string name_;
    LogLevel level_;
    std::vector<std::shared_ptr<LogAppender>> appenders_;
    std::mutex mutex_;

    template<typename... Args>
    static std::string Format(const std::string& fmt, Args&&... args) {
        size_t size = snprintf(nullptr, 0, fmt.c_str(), args...) + 1;
        std::unique_ptr<char[]> buf(new char[size]);
        snprintf(buf.get(), size, fmt.c_str(), args...);
        return std::string(buf.get(), buf.get() + size - 1);
    }

    static std::string Format(const std::string& msg) {
        return msg;
    }
};

/// 日志管理器
class LogManager {
public:
    static LogManager& Instance() {
        static LogManager instance;
        return instance;
    }

    /// 获取或创建日志器
    std::shared_ptr<Logger> GetLogger(const std::string& name);

    /// 初始化日志系统
    bool Initialize(const std::string& config = "");

    /// 关闭日志系统
    void Shutdown();

private:
    LogManager() = default;
    ~LogManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Logger>> loggers_;
    std::mutex mutex_;
    LogLevel globalLevel_ = LogLevel::INFO;
    bool initialized_ = false;
};

/// 日志宏定义
#define LOG_TRACE(logger, ...) \
    logger->Trace(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_DEBUG(logger, ...) \
    logger->Debug(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_INFO(logger, ...) \
    logger->Info(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_WARN(logger, ...) \
    logger->Warn(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_ERROR(logger, ...) \
    logger->Error(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#define LOG_FATAL(logger, ...) \
    logger->Fatal(__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

/// 默认日志器
#define s_logger apollo::LogManager::Instance().GetLogger("default")

}  // namespace apollo