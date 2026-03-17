#pragma once

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::core::log {

enum class LogLevel {
    Debug = 0,
    Info,
    Warn,
    Error,
};

struct LogEntry {
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
};

class LogManager {
public:
    void set_console_enabled(bool enabled);
    void clear();
    void write(LogLevel level, std::string category, std::string message);
    std::vector<LogEntry> snapshot() const;

private:
    mutable std::mutex mutex_;
    bool console_enabled_ = false;
    std::vector<LogEntry> entries_;
};

LogManager& global_log_manager();
std::string_view to_string(LogLevel level);

} // namespace apollo::core::log
