#include "apollo/utils/logging/logger.hpp"
#include <iomanip>
#include <cstdio>
#include <iostream>

namespace apollo {

void ConsoleAppender::Append(const LogEvent& event) {
    // 格式化时间
    auto tm_time = static_cast<time_t>(event.timestamp / 1000);
    auto ms = event.timestamp % 1000;
    auto tm = *std::localtime(&tm_time);

    // 线程ID转数字
    std::ostringstream oss;
    oss << event.threadId;

    std::cout << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms << "] "
              << "[" << std::setw(5) << oss.str() << "] "
              << "[" << GetLogLevelName(event.level) << "] "
              << "[" << event.loggerName << "] "
              << "[" << event.file << ":" << event.line << "] "
              << event.message << std::endl;

    if (event.level == LogLevel::FATAL) {
        std::cerr.flush();
    }
}

FileAppender::FileAppender(const std::string& filename, bool async)
    : async_(async) {
    file_.open(filename, std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("Failed to open log file: " + filename);
    }

    if (async_) {
        writeThread_ = std::thread(&FileAppender::AsyncWriteLoop, this);
    }
}

FileAppender::~FileAppender() {
    running_ = false;
    if (async_) {
        cond_.notify_all();
        if (writeThread_.joinable()) {
            writeThread_.join();
        }
    }
}

void FileAppender::Append(const LogEvent& event) {
    if (async_) {
        std::lock_guard<std::mutex> lock(mutex_);
        eventQueue_.push(event);
        cond_.notify_one();
    } else {
        WriteToFile(event);
    }
}

void FileAppender::WriteToFile(const LogEvent& event) {
    // 格式化时间
    auto tm_time = static_cast<time_t>(event.timestamp / 1000);
    auto ms = event.timestamp % 1000;
    auto tm = *std::localtime(&tm_time);

    // 线程ID转数字
    std::ostringstream oss;
    oss << event.threadId;

    std::lock_guard<std::mutex> lock(mutex_);
    file_ << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms << "] "
           << "[" << std::setw(5) << oss.str() << "] "
           << "[" << GetLogLevelName(event.level) << "] "
           << "[" << event.loggerName << "] "
           << "[" << event.file << ":" << event.line << "] "
           << event.message << std::endl;

    file_.flush();
}

void FileAppender::AsyncWriteLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !eventQueue_.empty() || !running_; });

        while (!eventQueue_.empty()) {
            LogEvent event = eventQueue_.front();
            eventQueue_.pop();
            lock.unlock();

            WriteToFile(event);

            lock.lock();
        }
    }
}

Logger::Logger(const std::string& name)
    : name_(name), level_(LogLevel::INFO) {
}

void Logger::AddAppender(std::shared_ptr<LogAppender> appender) {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.push_back(appender);
}

void Logger::ClearAppenders() {
    std::lock_guard<std::mutex> lock(mutex_);
    appenders_.clear();
}

void Logger::Log(LogLevel level, const std::string& file, int line,
                const std::string& function, const std::string& message) {
    if (level < level_) {
        return;
    }

    LogEvent event(level, name_, file, line, function, message);

    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& appender : appenders_) {
        appender->Append(event);
    }
}

std::shared_ptr<Logger> LogManager::GetLogger(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }

    auto logger = std::make_shared<Logger>(name);
    logger->SetLevel(globalLevel_);

    // 默认添加控制台输出器
    logger->AddAppender(std::make_shared<ConsoleAppender>());

    loggers_[name] = logger;
    return logger;
}

bool LogManager::Initialize(const std::string& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 这里可以根据配置文件进行初始化
    // 暂时使用默认配置

    initialized_ = true;
    return true;
}

void LogManager::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    loggers_.clear();
    initialized_ = false;
}

}  // namespace apollo