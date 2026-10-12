#pragma once

// 指标 push 客户端（P3-3 批：metrics admin 通道发送面——logging.md §5.1
// 三禁下的上行管线源端，与 logpush（L4 日志面）成对）。
//
// 语义分工（对比 log_push_client.hpp）：日志=事件流→kReliable+有界队列
// 补投恰一次；指标=采样流→kDroppable+无队列——快照整体自含，背压/断连
// 丢本拍，下拍新快照自然追赶（最新者胜，不补投）。周期推（默认 5s），
// provider 注入渲染闭包（默认 = MetricRegistry::instance() 快照渲染）。
// 收集永不构成运行依赖：推不阻塞、不抛、零业务反馈。
//
// 线程纪律：provider 供内部泵线程每拍调用一次（MetricRegistry 自持锁，
// 快照免调用方同步）；链路收发/重连全在泵线程（单写者）。链路面 =
// InterServerLink active 端点 → admin exporter（apps/admin serve）。
//
// Windows：link TCP 底座不进本批（POSIX only）——退空件（start 真、
// 无推面），调用面无 #if。

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "apollo/core/metrics/metric_registry.h"

#if !defined(_WIN32)
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_table.hpp"
#include <thread>
#endif

namespace apollo::net::metricspush {

struct MetricsPushClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 9106;    // admin metrics link 监听口
    std::string route_json;       // 契约路由 JSON 路径（空 = start 失败显形）
    std::string self_name;        // 本进程组件名（握手身份 = proc 键同源）
    std::uint32_t push_interval_ms = 5000;  // 采样流周期推（丢拍不积压）
    std::uint32_t pump_interval_ms = 50;
};

// 渲染闭包：每拍拉一次当前快照的 exposition 文本（已嵌 proc 标签——源端
// 渲染，exporter 零解析直拼接）。返回空串 = 本拍无样本（仍发空体帧，保
// proc_up 存活面）。
using SnapshotProvider = std::function<std::string()>;

class MetricsPushClient {
public:
    explicit MetricsPushClient(MetricsPushClientConfig config,
                               SnapshotProvider provider);
    ~MetricsPushClient();

    MetricsPushClient(const MetricsPushClient&) = delete;
    MetricsPushClient& operator=(const MetricsPushClient&) = delete;

    // 装配（路由表缺失/坏路径 = false 降级为无推面，不 throw）
    bool start();
    void stop();

    // 观察面
    bool connected() const;
    std::uint64_t pushed() const;   // 已被链路受理的快照帧
    std::uint64_t dropped() const;  // 未受理（背压/断连/编码失败丢拍）

private:
    void run();

    MetricsPushClientConfig config_;
    SnapshotProvider provider_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> pushed_{0};
    std::atomic<std::uint64_t> dropped_{0};

#if !defined(_WIN32)
    std::uint64_t last_push_ms_ = 0;  // 泵线程私有（丢拍也推进，不积压）
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::thread pump_thread_;
#endif
};

// MetricRegistry 快照 → Prometheus exposition 文本（自拼不引
// prometheus-cpp，logging.md §5.1）。counter/gauge 逐样本两行：
// `# TYPE <name> counter|gauge` + `<name>{proc="<proc>",<标签...>} <值>`；
// histogram 样本展开一族：`# TYPE <name> histogram` + `<name>_bucket{
// le="..."} 累计`（含 +Inf 尾档）+ `<name>_sum` + `<name>_count`。同名
// 相邻样本 TYPE 行只出一次（快照稳定排序保同名相邻）。标签值转义 \ " \n。
std::string render_exposition(
    const std::vector<apollo::core::metrics::MetricRegistry::Sample>& samples,
    const std::string& proc);

} // namespace apollo::net::metricspush
