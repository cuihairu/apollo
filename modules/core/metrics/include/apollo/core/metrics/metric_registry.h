#pragma once
//
// MetricRegistry 最小集（P3-3 批 B）。
//
// 设计依据：
//   - observability-watcher §14 对象模型：MetricRegistry = 趋势面
//     （「metrics=趋势、watcher=现状」，现状面归 watcher 树后续批）；
//   - net-abstraction §7 / G-5 两截：检测原语内嵌 owning 模块、聚合归
//     apps/exporter（依赖面 ConsoleEvent/IConsoleEventSource）；
//   - scripting-lua §6.1：onScriptFault O(1) 三件事之一 = 计数器自增，
//     标签语义 `script_fault_total{module,kind}`；
//   - logging.md §5.1：单一 exporter（admin 吐 /metrics）消费本注册表
//     快照、Prometheus 文本格式自拼（不引 prometheus-cpp）。
//
// 边界（本批不做）：control 通道上行、admin exporter、owning 模块接线、
// watcher 树/TraceBridge；指标生命周期 = 进程期，不支持注销——注册返回
// 的引用在注册表存续期恒稳定，owning 模块应存引用、热路径免查找。
//
// 注意：同名同标签分别以 counter 与 gauge 注册属调用方错误（两表分居不
// 裁决，快照两行并出，由出口侧发现）。

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apollo {
namespace core {
namespace metrics {

/// 标签集（Prometheus exposition 语义；存入前按键序规范化）
using Labels = std::vector<std::pair<std::string, std::string>>;

/// 只增计数器（`*_total` 类：错误计数/预算告警计数）
class Counter {
public:
    void increment(std::uint64_t n = 1) noexcept {
        value_.fetch_add(n, std::memory_order_relaxed);
    }

    std::uint64_t load() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> value_{0};
};

/// 绝对值原子 → Counter 推进（owning 接线：既有链路/egress 计数为进程期
/// 只增原子，读出即绝对值——直接 increment 会重复计底数）。调用方持
/// `last` 影子（上一拍读数），每拍推进增量：`counter_advance_to(c, last,
/// now)`。now <= last（首拍 last=0 与只增原子不回退之外兜底）不动。
inline void counter_advance_to(Counter& counter, std::uint64_t& last,
                               std::uint64_t now) noexcept {
    if (now > last) {
        counter.increment(now - last);
        last = now;
    }
}

/// 帧耗时类直方图（Prometheus histogram 语义：bucket 计数 + sum/count，
/// 归桶边界 le=小于等于；出口面展开 `_bucket{le=...}` 累计 + `_sum` +
/// `_count`）。桶位口径由消费侧定（cell 帧耗时 = ms 档，20ms 对齐
/// ADR-016 消息段预算锚点）；bounds 须严格递增——不验不纠，非递增归桶
/// 静默错位，与同名跨类型注册同款调用方错误立场。尾桶 +Inf 隐含不录入
/// bounds；桶存非累计（observe O(1) 热路径），累计在快照拼装。
class Histogram {
public:
    explicit Histogram(std::vector<double> bounds)
        : bounds_(std::move(bounds)), buckets_(bounds_.size() + 1) {}

    void observe(double v) noexcept {
        std::size_t idx = buckets_.size() - 1;  // 兜底落 +Inf 尾桶
        for (std::size_t i = 0; i < bounds_.size(); ++i) {
            if (v <= bounds_[i]) {
                idx = i;
                break;
            }
        }
        buckets_[idx].fetch_add(1, std::memory_order_relaxed);
        count_.fetch_add(1, std::memory_order_relaxed);
        // atomic<double>::fetch_add 是 C++20 面（c++17 档不可恃），FP
        // 累加走 CAS 环跨标准可用
        double expected = sum_.load(std::memory_order_relaxed);
        while (!sum_.compare_exchange_weak(expected, expected + v,
                                           std::memory_order_relaxed)) {
        }
    }

    /// 桶边界（不含 +Inf 尾桶；首建注册时定格）
    const std::vector<double>& bounds() const noexcept { return bounds_; }

    /// 单桶非累计计数（i ∈ [0, 桶数)；桶数 = bounds 数 + 1 尾桶）
    std::uint64_t bucket(std::size_t i) const noexcept {
        return buckets_[i].load(std::memory_order_relaxed);
    }

    std::uint64_t count() const noexcept {
        return count_.load(std::memory_order_relaxed);
    }

    double sum() const noexcept { return sum_.load(std::memory_order_relaxed); }

private:
    std::vector<double> bounds_;
    std::vector<std::atomic<std::uint64_t>> buckets_;  // 非累计单桶
    std::atomic<std::uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
};

/// 可增减仪表（队列水位/实体计数/心跳版本号类瞬时值）
class Gauge {
public:
    void add(std::int64_t delta) noexcept {
        value_.fetch_add(delta, std::memory_order_relaxed);
    }

    void store(std::int64_t v) noexcept {
        value_.store(v, std::memory_order_relaxed);
    }

    std::int64_t load() const noexcept {
        return value_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::int64_t> value_{0};
};

class MetricRegistry {
public:
    /// 局部实例可用（测试/独立聚合域）；进程级默认注册表走 instance()
    MetricRegistry() = default;

    /// 进程级默认注册表（LogManager::instance 同款惯例）
    static MetricRegistry& instance();

    /// 查找或创建计数器。标签顺序不影响同一性（规范化后按键序比对）。
    Counter& counter(const std::string& name, Labels labels = {});

    /// 查找或创建仪表。同上。
    Gauge& gauge(const std::string& name, Labels labels = {});

    /// 查找或创建直方图（帧耗时类）。bounds 仅首建生效——重复查找返回
    /// 既有实例（引用稳定性同 counter/gauge，调用方存引用入热路径）。
    Histogram& histogram(const std::string& name, Labels labels,
                         std::vector<double> bounds);

    /// 快照（exporter 聚合出口）：按 name + 标签规范序稳定排序，确定性输出。
    struct Sample {
        std::string name;
        Labels labels;
        bool is_counter = false;
        std::uint64_t counter_value = 0;
        std::int64_t gauge_value = 0;
        // 直方图三件（is_histogram 时有效）：bounds 不含 +Inf；buckets
        // 为累计计数（升档，末元素=全量含尾桶）
        bool is_histogram = false;
        std::vector<double> histogram_bounds;
        std::vector<std::uint64_t> histogram_buckets;
        double histogram_sum = 0.0;
        std::uint64_t histogram_count = 0;
    };
    std::vector<Sample> snapshot() const;

private:
    struct CounterEntry {
        std::string name;
        Labels labels;
        Counter metric;
    };
    struct GaugeEntry {
        std::string name;
        Labels labels;
        Gauge metric;
    };
    // Histogram 不可移动（atomic 成员）——entry 就地构造
    struct HistogramEntry {
        HistogramEntry(const std::string& n, Labels l, std::vector<double> b)
            : name(n), labels(std::move(l)), metric(std::move(b)) {}
        std::string name;
        Labels labels;
        Histogram metric;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CounterEntry>> counters_;
    std::unordered_map<std::string, std::unique_ptr<GaugeEntry>> gauges_;
    std::unordered_map<std::string, std::unique_ptr<HistogramEntry>> histograms_;
};

} // namespace metrics
} // namespace core
} // namespace apollo
