#include "apollo/core/metrics/metric_registry.h"

#include <algorithm>
#include <cstddef>

namespace apollo {
namespace core {
namespace metrics {

namespace {

// 标签按键序规范化（原地排序），返回用于查表的规范键：
// `<name>|<k1>=<v1>|<k2>=<v2>`。键序规范化保证 {a=1,b=2} 与 {b=2,a=1}
// 同一性，也使快照输出确定性（同名条目标签序稳定）。
std::string canonical_key(const std::string& name, Labels& labels) {
    std::sort(labels.begin(), labels.end());
    std::string key = name;
    for (const auto& kv : labels) {
        key.push_back('|');
        key += kv.first;
        key.push_back('=');
        key += kv.second;
    }
    return key;
}

} // namespace

MetricRegistry& MetricRegistry::instance() {
    static MetricRegistry registry;
    return registry;
}

Counter& MetricRegistry::counter(const std::string& name, Labels labels) {
    const std::string key = canonical_key(name, labels);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = counters_.find(key);
    if (it != counters_.end()) {
        return it->second->metric;
    }
    auto entry = std::make_unique<CounterEntry>();
    entry->name = name;
    entry->labels = std::move(labels);
    Counter& ref = entry->metric;
    counters_.emplace(key, std::move(entry));
    return ref;
}

Gauge& MetricRegistry::gauge(const std::string& name, Labels labels) {
    const std::string key = canonical_key(name, labels);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = gauges_.find(key);
    if (it != gauges_.end()) {
        return it->second->metric;
    }
    auto entry = std::make_unique<GaugeEntry>();
    entry->name = name;
    entry->labels = std::move(labels);
    Gauge& ref = entry->metric;
    gauges_.emplace(key, std::move(entry));
    return ref;
}

Histogram& MetricRegistry::histogram(const std::string& name, Labels labels,
                                     std::vector<double> bounds) {
    const std::string key = canonical_key(name, labels);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = histograms_.find(key);
    if (it != histograms_.end()) {
        return it->second->metric;
    }
    auto entry = std::make_unique<HistogramEntry>(name, std::move(labels),
                                                  std::move(bounds));
    Histogram& ref = entry->metric;
    histograms_.emplace(key, std::move(entry));
    return ref;
}

std::vector<MetricRegistry::Sample> MetricRegistry::snapshot() const {
    std::vector<Sample> samples;
    std::lock_guard<std::mutex> lock(mutex_);
    samples.reserve(counters_.size() + gauges_.size());
    for (const auto& kv : histograms_) {
        Sample s;
        s.name = kv.second->name;
        s.labels = kv.second->labels;
        s.is_histogram = true;
        const Histogram& h = kv.second->metric;
        s.histogram_bounds = h.bounds();
        const std::size_t n = s.histogram_bounds.size();
        s.histogram_buckets.reserve(n + 1);
        std::uint64_t cum = 0;
        for (std::size_t i = 0; i < n; ++i) {
            cum += h.bucket(i);
            s.histogram_buckets.push_back(cum);
        }
        s.histogram_buckets.push_back(h.count());  // +Inf 尾桶 = 全量
        s.histogram_sum = h.sum();
        s.histogram_count = h.count();
        samples.push_back(std::move(s));
    }
    for (const auto& kv : counters_) {
        Sample s;
        s.name = kv.second->name;
        s.labels = kv.second->labels;
        s.is_counter = true;
        s.counter_value = kv.second->metric.load();
        samples.push_back(std::move(s));
    }
    for (const auto& kv : gauges_) {
        Sample s;
        s.name = kv.second->name;
        s.labels = kv.second->labels;
        s.is_counter = false;
        s.gauge_value = kv.second->metric.load();
        samples.push_back(std::move(s));
    }
    // 稳定序：name 升序，同名按标签字典序（标签已规范序，逐位比较即可）。
    std::sort(samples.begin(), samples.end(),
              [](const Sample& a, const Sample& b) {
                  if (a.name != b.name) {
                      return a.name < b.name;
                  }
                  return a.labels < b.labels;
              });
    return samples;
}

} // namespace metrics
} // namespace core
} // namespace apollo
