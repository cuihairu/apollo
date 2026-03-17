#include "apollo/core/log/log_manager.hpp"

#include <iostream>

namespace apollo::core::log {

void LogManager::set_console_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_enabled_ = enabled;
}

void LogManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

void LogManager::write(LogLevel level, std::string category, std::string message) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(LogEntry{level, std::move(category), std::move(message)});
    if (console_enabled_) {
        const auto& entry = entries_.back();
        std::cout << "[" << to_string(entry.level) << "] "
                  << entry.category << ": " << entry.message << std::endl;
    }
}

std::vector<LogEntry> LogManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

LogManager& global_log_manager() {
    static LogManager manager;
    return manager;
}

std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace apollo::core::log
