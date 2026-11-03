#pragma once

// Dashboard 状态快照聚合器（DASH-1 阶段 2 数据源接线）。
// 从各模块已有只读状态接口采集并渲染 /api/state JSON：
//   process  → /proc/self/{stat,status}（Linux）+ 启动时刻
//   links    → M1LinkServer::snapshot_links()（代理 LinkManager）
//   script   → ScriptRuntime::active_modules()（owning 线程拍内采集）
//   load     → MetricRegistry::instance().snapshot()（进程级，锁安全）
//   fleet    → dev_fleet 运行目录文件面（.run/<role>.pid + logs/<role>.log）
//   orders   → OrderStore（本批生产进程无实例 → 空表）
//
// 线程纪律：collector 内部 mutex 守快照拷贝；采集函数由数据源 owning 线程
// 调用（world/cell 的 pre_tick 或 manager 泵拍），render 由 dashboard HTTP
// 泵调用（主循环线程）——两向只经锁交换，不跨线程直接触碰业务对象。

#include "apollo/net/link/link_manager.hpp"
#include "apollo/core/metrics/metric_registry.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace world_app {

struct DashboardProcessInfo {
    std::string role;
    std::uint64_t started_ms = 0;
    // 编队运行目录（dev_fleet FLEET_DIR）：读 .run/<role>.pid + logs/<role>.log
    // 文件面。空 = 不读（standalone 形态）。
    std::string fleet_dir;
};

class DashboardDataCollector {
public:
    explicit DashboardDataCollector(DashboardProcessInfo info = {});

    // 采集面（数据源 owning 线程调用——每拍或按需）
    void collect_links(const std::vector<apollo::net::link::LinkSnapshot>& links);
    void collect_script(const std::map<std::string, std::uint64_t>& modules);
    void collect_metrics(const std::vector<apollo::core::metrics::MetricRegistry::Sample>& samples);
    void collect_fleet(const std::vector<apollo::net::link::LinkSnapshot>& links);

    // 渲染面（dashboard HTTP 泵线程调用）——返回 /api/state JSON 文本
    std::string render() const;

private:
    struct Data {
        std::vector<apollo::net::link::LinkSnapshot> links;
        std::map<std::string, std::uint64_t> script_modules;
        std::vector<apollo::core::metrics::MetricRegistry::Sample> metrics;
    };

    DashboardProcessInfo info_;
    mutable std::mutex mutex_;
    Data data_;
};

}  // namespace world_app
