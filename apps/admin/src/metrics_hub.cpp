#include "admin/metrics_hub.hpp"

#include <sstream>
#include <utility>
#include <vector>

namespace apollo::admin {

namespace {

// 标签值转义（Prometheus 文本格式：\\ \" \n——与源端 render_exposition
// 同款，跨进程口径必须一致，否则 exporter 拼接出的 proc 标签会错行）
std::string escape_label_value(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

}  // namespace

MetricsHub::MetricsHub(MetricsHubConfig config) : config_(config) {}

void MetricsHub::ingest(std::string proc, std::string body, std::uint64_t seen_ms) {
    if (proc.empty() || body.size() > config_.max_body_bytes) {
        ++rejected_;  // 超限不覆盖既有快照（last-wins 只被「合格的更新」覆盖）
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = procs_.find(proc);
    if (it == procs_.end()) {
        // key 由 move 后的 proc 构造（map 持有副本，本缝不再读 proc）
        it = procs_.emplace(std::move(proc), ProcSnapshot{}).first;
    }
    it->second.body = std::move(body);
    it->second.seen_ms = seen_ms;
    ++ingested_;
}

std::string MetricsHub::render(std::uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream out;
    out << "# TYPE apollo_proc_up gauge\n";
    for (const auto& kv : procs_) {
        const ProcSnapshot& snap = kv.second;
        // 时钟回拨（宿主注入时钟换源）按未陈旧处理，不判死
        const bool fresh = (snap.seen_ms > now_ms)
                               ? true
                               : (now_ms - snap.seen_ms <= config_.stale_ms);
        out << "apollo_proc_up{proc=\"" << escape_label_value(kv.first) << "\"} "
            << (fresh ? 1 : 0) << '\n';
        if (fresh && !snap.body.empty()) {
            out << snap.body;  // 已嵌 proc 标签（源端渲染），零解析直拼接
        }
    }
    return out.str();
}

std::vector<std::pair<std::string, bool>> MetricsHub::proc_states(
    std::uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, bool>> states;
    states.reserve(procs_.size());
    for (const auto& kv : procs_) {
        const ProcSnapshot& snap = kv.second;
        // 与 render 同款：时钟回拨（宿主注入时钟换源）按未陈旧处理，不判死
        const bool fresh = (snap.seen_ms > now_ms)
                               ? true
                               : (now_ms - snap.seen_ms <= config_.stale_ms);
        states.emplace_back(kv.first, fresh);
    }
    return states;
}

std::size_t MetricsHub::procs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return procs_.size();
}

std::uint64_t MetricsHub::ingested() const {
    return ingested_.load(std::memory_order_relaxed);
}

std::uint64_t MetricsHub::rejected() const {
    return rejected_.load(std::memory_order_relaxed);
}

}  // namespace apollo::admin
