#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace apollo {
namespace core {
namespace metrics {

/// 时间单位
enum class TimeUnit {
    Nanoseconds,
    Microseconds,
    Milliseconds,
    Seconds
};

/// 指标类型
enum class MetricType {
    Gauge,      // 仪表（可增可减的值）
    Counter,    // 计数器（只增不减）
    Histogram,  // 直方图（分布统计）
    Summary,    // 摘要（百分位统计）
    Meter       // 仪表（速率统计）
};

//==============================================================================
// 指标接口
//==============================================================================

class IMetric {
public:
    virtual ~IMetric() = default;
    virtual std::string getName() const = 0;
    virtual std::string getHelp() const = 0;
    virtual MetricType getType() const = 0;
};

//==============================================================================
// 计数器
//==============================================================================

class Counter : public IMetric {
public:
    Counter(const std::string& name, const std::string& help = "")
        : name_(name), help_(help), count_(0) {}

    std::string getName() const override { return name_; }
    std::string getHelp() const override { return help_; }
    MetricType getType() const override { return MetricType::Counter; }

    void increment() {
        ++count_;
    }

    void increment(uint64_t delta) {
        count_ += delta;
    }

    uint64_t get() const {
        return count_;
    }

    void reset() {
        count_ = 0;
    }

private:
    std::string name_;
    std::string help_;
    std::atomic<uint64_t> count_;
};

//==============================================================================
// 仪表
//==============================================================================

class Gauge : public IMetric {
public:
    Gauge(const std::string& name, const std::string& help = "")
        : name_(name), help_(help), value_(0) {}

    std::string getName() const override { return name_; }
    std::string getHelp() const override { return help_; }
    MetricType getType() const override { return MetricType::Gauge; }

    void set(int64_t value) {
        value_ = value;
    }

    void increment() {
        ++value_;
    }

    void decrement() {
        --value_;
    }

    void add(int64_t delta) {
        value_ += delta;
    }

    int64_t get() const {
        return value_;
    }

private:
    std::string name_;
    std::string help_;
    std::atomic<int64_t> value_;
};

//==============================================================================
// 直方图（统计分布）
//==============================================================================

class Histogram : public IMetric {
public:
    Histogram(const std::string& name, const std::string& help = "")
        : name_(name), help_(help)
        , count_(0)
        , sum_(0)
        , min_(INT64_MAX)
        , max_(INT64_MIN) {}

    std::string getName() const override { return name_; }
    std::string getHelp() const override { return help_; }
    MetricType getType() const override { return MetricType::Histogram; }

    void record(int64_t value) {
        count_.fetch_add(1);
        sum_.fetch_add(value);

        // 更新最小值
        int64_t currentMin = min_.load();
        while (value < currentMin && !min_.compare_exchange_weak(currentMin, value)) {
            currentMin = min_.load();
        }

        // 更新最大值
        int64_t currentMax = max_.load();
        while (value > currentMax && !max_.compare_exchange_weak(currentMax, value)) {
            currentMax = max_.load();
        }
    }

    uint64_t getCount() const { return count_; }
    int64_t getSum() const { return sum_; }
    int64_t getMin() const { return (min_ == INT64_MAX) ? 0 : min_; }
    int64_t getMax() const { return (max_ == INT64_MIN) ? 0 : max_; }

    double getAvg() const {
        uint64_t c = count_;
        return c > 0 ? static_cast<double>(sum_) / c : 0.0;
    }

private:
    std::string name_;
    std::string help_;
    std::atomic<uint64_t> count_;
    std::atomic<int64_t> sum_;
    std::atomic<int64_t> min_;
    std::atomic<int64_t> max_;
};

//==============================================================================
// 百分位统计
//==============================================================================

class Summary : public IMetric {
public:
    Summary(const std::string& name, const std::string& help = "",
            size_t windowSize = 100)
        : name_(name), help_(help), windowSize_(windowSize) {
        values_.reserve(windowSize);
    }

    std::string getName() const override { return name_; }
    std::string getHelp() const override { return help_; }
    MetricType getType() const override { return MetricType::Summary; }

    void record(int64_t value) {
        std::lock_guard<std::mutex> lock(mutex_);

        values_.push_back(value);
        if (values_.size() > windowSize_) {
            values_.erase(values_.begin());
        }
    }

    uint64_t getCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.size();
    }

    double getAvg() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0.0;

        int64_t sum = 0;
        for (auto v : values_) {
            sum += v;
        }
        return static_cast<double>(sum) / values_.size();
    }

    int64_t getMax() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0;
        return *std::max_element(values_.begin(), values_.end());
    }

    int64_t getMin() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (values_.empty()) return 0;
        return *std::min_element(values_.begin(), values_.end());
    }

    /// 获取百分位数
    double getPercentile(double percentile) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (values_.empty()) {
            return 0.0;
        }

        std::vector<int64_t> sorted = values_;
        std::sort(sorted.begin(), sorted.end());

        size_t index = static_cast<size_t>(sorted.size() * percentile / 100.0);
        if (index >= sorted.size()) {
            index = sorted.size() - 1;
        }

        return static_cast<double>(sorted[index]);
    }

    double getP50() const { return getPercentile(50); }   // 中位数
    double getP90() const { return getPercentile(90); }
    double getP95() const { return getPercentile(95); }
    double getP99() const { return getPercentile(99); }

private:
    std::string name_;
    std::string help_;
    size_t windowSize_;
    std::vector<int64_t> values_;
    mutable std::mutex mutex_;
};

//==============================================================================
// 速率统计
//==============================================================================

class Meter : public IMetric {
public:
    Meter(const std::string& name, const std::string& help = "")
        : name_(name), help_(help)
        , count_(0)
        , lastCount_(0)
        , lastTime_(std::chrono::steady_clock::now())
        , oneMinuteRate_(0)
        , fiveMinuteRate_(0)
        , fifteenMinuteRate_(0) {
    }

    std::string getName() const override { return name_; }
    std::string getHelp() const override { return help_; }
    MetricType getType() const override { return MetricType::Meter; }

    void mark() {
        count_.fetch_add(1);
    }

    void mark(uint64_t n) {
        count_.fetch_add(n);
    }

    uint64_t getCount() const {
        return count_;
    }

    double getOneMinuteRate() {
        updateRates();
        return oneMinuteRate_;
    }

    double getFiveMinuteRate() {
        updateRates();
        return fiveMinuteRate_;
    }

    double getFifteenMinuteRate() {
        updateRates();
        return fifteenMinuteRate_;
    }

    double getMeanRate() {
        updateRates();
        return meanRate_;
    }

private:
    void updateRates() {
        auto now = std::chrono::steady_clock::now();
        uint64_t currentCount = count_;

        std::chrono::duration<double> elapsed = now - lastTime_;
        if (elapsed.count() < 1.0) {
            return;  // 每秒更新一次
        }

        uint64_t delta = currentCount - lastCount_;
        double rate = delta / elapsed.count();

        // EMA 指数移动平均
        const double alpha = 0.5;  // 平滑系数
        oneMinuteRate_ = alpha * rate + (1 - alpha) * oneMinuteRate_;
        fiveMinuteRate_ = alpha * rate + (1 - alpha) * fiveMinuteRate_;
        fifteenMinuteRate_ = alpha * rate + (1 - alpha) * fifteenMinuteRate_;
        meanRate_ = rate;

        lastCount_ = currentCount;
        lastTime_ = now;
    }

    std::string name_;
    std::string help_;
    std::atomic<uint64_t> count_;
    uint64_t lastCount_;
    std::chrono::steady_clock::time_point lastTime_;
    double oneMinuteRate_;
    double fiveMinuteRate_;
    double fifteenMinuteRate_;
    double meanRate_;
};

//==============================================================================
// 指标注册表
//==============================================================================

class MetricRegistry {
public:
    static MetricRegistry& instance() {
        static MetricRegistry registry;
        return registry;
    }

    /// 创建计数器
    Counter* counter(const std::string& name, const std::string& help = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& metric = counters_[name];
        if (!metric) {
            metric = std::make_unique<Counter>(name, help);
        }
        return metric.get();
    }

    /// 创建仪表
    Gauge* gauge(const std::string& name, const std::string& help = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& metric = gauges_[name];
        if (!metric) {
            metric = std::make_unique<Gauge>(name, help);
        }
        return metric.get();
    }

    /// 创建直方图
    Histogram* histogram(const std::string& name, const std::string& help = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& metric = histograms_[name];
        if (!metric) {
            metric = std::make_unique<Histogram>(name, help);
        }
        return metric.get();
    }

    /// 创建百分位统计
    Summary* summary(const std::string& name, const std::string& help = "",
                     size_t windowSize = 100) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& metric = summaries_[name];
        if (!metric) {
            metric = std::make_unique<Summary>(name, help, windowSize);
        }
        return metric.get();
    }

    /// 创建速率统计
    Meter* meter(const std::string& name, const std::string& help = "") {
        std::lock_guard<std::mutex> lock(mutex_);

        auto& metric = meters_[name];
        if (!metric) {
            metric = std::make_unique<Meter>(name, help);
        }
        return metric.get();
    }

    /// 获取所有指标
    std::vector<IMetric*> getAllMetrics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<IMetric*> result;

        for (const auto& pair : counters_) {
            result.push_back(pair.second.get());
        }
        for (const auto& pair : gauges_) {
            result.push_back(pair.second.get());
        }
        for (const auto& pair : histograms_) {
            result.push_back(pair.second.get());
        }
        for (const auto& pair : summaries_) {
            result.push_back(pair.second.get());
        }
        for (const auto& pair : meters_) {
            result.push_back(pair.second.get());
        }

        return result;
    }

private:
    MetricRegistry() = default;

    mutable std::mutex mutex_;

    std::unordered_map<std::string, std::unique_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::unordered_map<std::string, std::unique_ptr<Histogram>> histograms_;
    std::unordered_map<std::string, std::unique_ptr<Summary>> summaries_;
    std::unordered_map<std::string, std::unique_ptr<Meter>> meters_;
};

//==============================================================================
// 性能计时器
//==============================================================================

class ScopedTimer {
public:
    ScopedTimer(Summary* summary)
        : summary_(summary)
        , start_(std::chrono::steady_clock::now()) {}

    ScopedTimer(Histogram* histogram)
        : histogram_(histogram)
        , start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_).count();

        if (summary_) {
            summary_->record(duration);
        }
        if (histogram_) {
            histogram_->record(duration);
        }
    }

private:
    Summary* summary_ = nullptr;
    Histogram* histogram_ = nullptr;
    std::chrono::steady_clock::time_point start_;
};

//==============================================================================
// 辅助宏
//==============================================================================

#define METRIC_COUNTER(name) \
    apollo::core::metrics::MetricRegistry::instance().counter(name)

#define METRIC_GAUGE(name) \
    apollo::core::metrics::MetricRegistry::instance().gauge(name)

#define METRIC_HISTOGRAM(name) \
    apollo::core::metrics::MetricRegistry::instance().histogram(name)

#define METRIC_SUMMARY(name) \
    apollo::core::metrics::MetricRegistry::instance().summary(name)

#define METRIC_METER(name) \
    apollo::core::metrics::MetricRegistry::instance().meter(name)

#define METRIC_TIME(name, code) \
    apollo::core::metrics::ScopedTimer _timer_##name(METRIC_SUMMARY(name)); \
    code

} // namespace metrics
} // namespace core
} // namespace apollo
