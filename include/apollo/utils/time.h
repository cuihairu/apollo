#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <ctime>
#include <iostream>
#include <functional>

namespace apollo {
namespace utils {

//==============================================================================
// Time - 时间工具类
//==============================================================================

/**
 * @brief 时间工具类
 *
 * 提供高精度时间测量、时间格式化等功能
 */
class Time {
public:
    /**
     * @brief 时间戳类型（毫秒）
     */
    using Timestamp = int64_t;

    /**
     * @brief 获取当前时间戳（毫秒）
     */
    static Timestamp now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief 获取当前时间戳（微秒）
     */
    static int64_t nowMicroseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief 获取当前时间戳（纳秒）
     */
    static int64_t nowNanoseconds() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    /**
     * @brief 获取系统时间（Unix时间戳，秒）
     */
    static time_t unixTime() {
        return std::time(nullptr);
    }

    /**
     * @brief 格式化时间
     *
     * @param timestamp 时间戳（毫秒）
     * @param format 格式字符串
     * @return 格式化后的时间字符串
     */
    static std::string format(time_t timestamp, const char* format = "%Y-%m-%d %H:%M:%S") {
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &timestamp);
#else
        localtime_r(&timestamp, &tm);
#endif
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), format, &tm);
        return std::string(buffer);
    }

    /**
     * @brief 格式化当前时间
     */
    static std::string formatNow(const char* format = "%Y-%m-%d %H:%M:%S") {
        return format(unixTime(), format);
    }

    /**
     * @brief 获取日期字符串
     */
    static std::string dateStr(time_t timestamp) {
        return format(timestamp, "%Y-%m-%d");
    }

    /**
     * @brief 获取时间字符串
     */
    static std::string timeStr(time_t timestamp) {
        return format(timestamp, "%H:%M:%S");
    }

    /**
     * @brief 获取日期时间字符串（文件名安全）
     */
    static std::string dateTimeStr(time_t timestamp) {
        return format(timestamp, "%Y%m%d_%H%M%S");
    }

    /**
     * @brief 解析时间字符串
     *
     * @param str 时间字符串
     * @param format 格式字符串
     * @return Unix 时间戳
     */
    static time_t parse(const std::string& str, const char* format = "%Y-%m-%d %H:%M:%S") {
        std::tm tm = {};
#ifdef _WIN32
        sscanf_s(str.c_str(), format,
            &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
            &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
        tm.tm_year -= 1900;
        tm.tm_mon -= 1;
#else
        strptime(str.c_str(), format, &tm);
#endif
        return std::mktime(&tm);
    }

    /**
     * @brief 时间戳转可读字符串
     */
    static std::string toString(Timestamp ms) {
        time_t sec = static_cast<time_t>(ms / 1000);
        int millis = static_cast<int>(ms % 1000);
        std::string base = format(sec);
        char buffer[8];
        snprintf(buffer, sizeof(buffer), ".%03d", millis);
        return base + buffer;
    }
};

//==============================================================================
// Timer - 高精度计时器
//==============================================================================

/**
 * @brief 高精度计时器
 *
 * 用于测量代码执行时间
 */
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    /**
     * @brief 构造函数，自动开始计时
     */
    Timer() : start_(Clock::now()) {}

    /**
     * @brief 重置计时器
     */
    void reset() {
        start_ = Clock::now();
    }

    /**
     * @brief 获取经过的时间（秒）
     */
    double elapsed() const {
        return std::chrono::duration_cast<Duration>(Clock::now() - start_).count();
    }

    /**
     * @brief 获取经过的时间（毫秒）
     */
    double elapsedMillis() const {
        return elapsed() * 1000.0;
    }

    /**
     * @brief 获取经过的时间（微秒）
     */
    double elapsedMicros() const {
        return elapsed() * 1000000.0;
    }

private:
    TimePoint start_;
};

//==============================================================================
// ScopeTimer - 作用域计时器
//==============================================================================

/**
 * @brief 作用域计时器
 *
 * 析构时自动输出耗时
 */
class ScopeTimer {
public:
    using Callback = std::function<void(double)>;

    /**
     * @brief 构造函数
     *
     * @param name 计时器名称
     * @param output 是否输出到标准输出
     */
    explicit ScopeTimer(const std::string& name, bool output = true)
        : name_(name), output_(output) {}

    /**
     * @brief 构造函数（自定义回调）
     */
    explicit ScopeTimer(Callback callback)
        : callback_(std::move(callback)) {}

    ~ScopeTimer() {
        double elapsed = timer_.elapsedMillis();
        if (callback_) {
            callback_(elapsed);
        } else if (output_) {
            std::cout << "[" << name_ << "] " << elapsed << " ms" << std::endl;
        }
    }

private:
    std::string name_;
    bool output_;
    Callback callback_;
    Timer timer_;
};

//==============================================================================
// FpsCalculator - FPS 计算器
//==============================================================================

/**
 * @brief FPS 计算器
 *
 * 用于游戏渲染等场景的帧率计算
 */
class FpsCalculator {
public:
    /**
     * @brief 构造函数
     *
     * @param updateInterval 更新间隔（毫秒）
     */
    explicit FpsCalculator(Time::Timestamp updateInterval = 500)
        : updateInterval_(updateInterval)
        , lastUpdateTime_(Time::now())
        , frameCount_(0)
        , fps_(0.0f) {}

    /**
     * @brief 更新帧计数
     */
    void update() {
        ++frameCount_;
        Time::Timestamp now = Time::now();

        if (now - lastUpdateTime_ >= updateInterval_) {
            double elapsed = static_cast<double>(now - lastUpdateTime_) / 1000.0;
            fps_ = static_cast<float>(frameCount_ / elapsed);
            frameCount_ = 0;
            lastUpdateTime_ = now;
        }
    }

    /**
     * @brief 获取 FPS
     */
    float getFps() const {
        return fps_;
    }

    /**
     * @brief 获取帧时间（毫秒）
     */
    float getFrameTime() const {
        return (fps_ > 0.0f) ? (1000.0f / fps_) : 0.0f;
    }

private:
    Time::Timestamp updateInterval_;
    Time::Timestamp lastUpdateTime_;
    uint32_t frameCount_;
    float fps_;
};

} // namespace utils
} // namespace apollo
