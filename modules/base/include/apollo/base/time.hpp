#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>

namespace apollo::base {

class Time {
public:
    using Timestamp = int64_t;

    static Timestamp now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static int64_t now_microseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static int64_t now_nanoseconds() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    static time_t unix_time() {
        return std::time(nullptr);
    }

    static std::string format(time_t timestamp, const char* pattern = "%Y-%m-%d %H:%M:%S") {
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &timestamp);
#else
        localtime_r(&timestamp, &tm);
#endif
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), pattern, &tm);
        return std::string(buffer);
    }

    static std::string format_now(const char* pattern = "%Y-%m-%d %H:%M:%S") {
        return format(unix_time(), pattern);
    }

    static std::string date_str(time_t timestamp) {
        return format(timestamp, "%Y-%m-%d");
    }

    static std::string time_str(time_t timestamp) {
        return format(timestamp, "%H:%M:%S");
    }

    static std::string datetime_str(time_t timestamp) {
        return format(timestamp, "%Y%m%d_%H%M%S");
    }

    static std::string to_string(Timestamp milliseconds) {
        const time_t seconds = static_cast<time_t>(milliseconds / 1000);
        const int millis = static_cast<int>(milliseconds % 1000);
        std::string text = format(seconds);
        char buffer[8];
#ifdef _WIN32
        sprintf_s(buffer, sizeof(buffer), ".%03d", millis);
#else
        std::snprintf(buffer, sizeof(buffer), ".%03d", millis);
#endif
        return text + buffer;
    }
};

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    Timer() : start_(Clock::now()) {}

    void reset() {
        start_ = Clock::now();
    }

    double elapsed() const {
        return std::chrono::duration_cast<Duration>(Clock::now() - start_).count();
    }

    double elapsed_millis() const {
        return elapsed() * 1000.0;
    }

    double elapsed_micros() const {
        return elapsed() * 1000000.0;
    }

private:
    TimePoint start_;
};

class ScopeTimer {
public:
    using Callback = std::function<void(double)>;

    explicit ScopeTimer(std::string name, bool output = true)
        : name_(std::move(name)), output_(output) {}

    explicit ScopeTimer(Callback callback)
        : callback_(std::move(callback)) {}

    ~ScopeTimer() {
        const double milliseconds = timer_.elapsed_millis();
        if (callback_) {
            callback_(milliseconds);
        } else if (output_) {
            std::cout << "[" << name_ << "] " << milliseconds << " ms" << std::endl;
        }
    }

private:
    std::string name_;
    bool output_ = false;
    Callback callback_;
    Timer timer_;
};

class FpsCalculator {
public:
    explicit FpsCalculator(Time::Timestamp update_interval = 500)
        : update_interval_(update_interval), last_update_time_(Time::now()) {}

    void update() {
        ++frame_count_;
        const auto now = Time::now();
        if (now - last_update_time_ >= update_interval_) {
            const double elapsed = static_cast<double>(now - last_update_time_) / 1000.0;
            fps_ = elapsed > 0.0 ? static_cast<float>(frame_count_ / elapsed) : 0.0f;
            frame_count_ = 0;
            last_update_time_ = now;
        }
    }

    float fps() const {
        if (fps_ > 0.0f) {
            return fps_;
        }

        if (frame_count_ == 0) {
            return 0.0f;
        }

        const auto elapsed_ms = Time::now() - last_update_time_;
        if (elapsed_ms <= 0) {
            return 0.0f;
        }

        const double elapsed = static_cast<double>(elapsed_ms) / 1000.0;
        return elapsed > 0.0 ? static_cast<float>(frame_count_ / elapsed) : 0.0f;
    }

    float frame_time_ms() const {
        const float current_fps = fps();
        return current_fps > 0.0f ? 1000.0f / current_fps : 0.0f;
    }

private:
    Time::Timestamp update_interval_ = 500;
    Time::Timestamp last_update_time_ = 0;
    uint32_t frame_count_ = 0;
    float fps_ = 0.0f;
};

} // namespace apollo::base
