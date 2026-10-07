#include "apollo/core/metrics/metric_registry.h"

#include <algorithm>

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

std::vector<MetricRegistry::Sample> MetricRegistry::snapshot() const {
    std::vector<Sample> samples;
    std::lock_guard<std::mutex> lock(mutex_);
    samples.reserve(counters_.size() + gauges_.size());
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
