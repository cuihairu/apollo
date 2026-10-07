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
// watcher 树/TraceBridge；histogram 缺位（帧耗时类原语待接线批定口径）；
// 指标生命周期 = 进程期，不支持注销——注册返回的引用在注册表存续期恒稳定，
// owning 模块应存引用、热路径免查找。
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

    /// 快照（exporter 聚合出口）：按 name + 标签规范序稳定排序，确定性输出。
    struct Sample {
        std::string name;
        Labels labels;
        bool is_counter = false;
        std::uint64_t counter_value = 0;
        std::int64_t gauge_value = 0;
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

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<CounterEntry>> counters_;
    std::unordered_map<std::string, std::unique_ptr<GaugeEntry>> gauges_;
};

} // namespace metrics
} // namespace core
} // namespace apollo
