#pragma once

// 指标聚合缝（apps/admin exporter：logging.md §5.1 单一 exporter——消费各
// 进程 MetricRegistry 快照经 931 上行，聚合后自拼 Prometheus 文本，不引
// prometheus-cpp）。
//
// 语义（采样流，与发送面 kDroppable 无队列成对）：
//   last-wins per proc——迟到旧快照只会被更新覆盖，不回放（最新者胜）；
//   apollo_proc_up = 陈旧判据（默认 30s）——陈旧进程只出 proc_up=0 墓碑，
//   样本行不再出（该进程样本随墓碑一起离开 scrape 面，prom 侧 staleness
//   处理自然接管，不留驻旧值）；
//   体长上限（默认 4MB = Droppable 单拍容量口径）——超限即拒收计数、不
//   覆盖既有快照，防单个进程每拍驻留兆字节。
//
// 线程纪律：ingest 由宿主 serve 循环拍内调用（受理面单写者），render 由
// HTTP 面调用——两向都持锁，锁粒度 = 一次摄入/一次渲染，不跨拍。
// 平台无关（纯内存 + 字符串拼装），Windows 照常构建。
//
// 告警 notifier 独立面（alert_notifier.hpp，L7）：931 受理点直调不经
// hub；webhook 出站面已交付（alert_webhook.hpp）。proc 存活告警（L8）经
// proc_states() 读出口面同源判据。进程登记生命周期（陈旧不淘汰——
// proc_up=0 墓碑常驻即「进程已下线」的可观测面，运维靠重启清表）。

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace apollo::admin {

struct MetricsHubConfig {
    std::uint64_t stale_ms = 30000;               // apollo_proc_up 陈旧判据
    std::size_t max_body_bytes = 4 * 1024 * 1024;  // 快照体长上限
};

class MetricsHub {
public:
    explicit MetricsHub(MetricsHubConfig config = {});

    // 收一条 931 快照。proc 空/体长超限 = 拒收计数、不覆盖既有快照。
    // seen_ms = 注入时钟（宿主 serve 拍），免本缝读墙钟。
    void ingest(std::string proc, std::string body, std::uint64_t seen_ms);

    // 渲染当前 /metrics 文本（每 scrape 现算）。apollo_proc_up TYPE 行只出
    // 一次；逐 proc 按名排序；陈旧 = 墓碑（样本行不出）。
    std::string render(std::uint64_t now_ms) const;

    // 逐 proc 存活态（按名排序；up 判据与 render 出口面同源——proc_up 由
    // 出口面合成不进推送体，告警面经此读同一判据）。时钟回拨按未陈旧。
    std::vector<std::pair<std::string, bool>> proc_states(
        std::uint64_t now_ms) const;

    // 观察面（验收/断言）
    std::size_t procs() const;
    std::uint64_t ingested() const;
    std::uint64_t rejected() const;

private:
    struct ProcSnapshot {
        std::string body;
        std::uint64_t seen_ms = 0;
    };

    MetricsHubConfig config_;
    mutable std::mutex mutex_;
    std::map<std::string, ProcSnapshot> procs_;  // key = proc
    std::atomic<std::uint64_t> ingested_{0};
    std::atomic<std::uint64_t> rejected_{0};
};

}  // namespace apollo::admin
