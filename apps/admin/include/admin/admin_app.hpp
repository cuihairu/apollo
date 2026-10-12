#pragma once

// AdminApp——指标 exporter 进程壳（metrics admin 通道；logging.md §5.1
// 单一 exporter 定案）。v1 = serve 受面子命令：link passive "metrics"
// 收 931 快照 → MetricsHub 聚合（last-wins + proc_up 陈旧判据）→ HTTP
// /metrics 吐 Prometheus 文本。
//
// 不做的：告警 notifier（logging.md §5.2 层 B：规则表 + webhook，后续批）；
// admin 自身指标反向暴露（v1 不做）。

#include <cstdint>

namespace apollo::admin {

class AdminApp {
public:
    int run(int argc, char* argv[]);
};

}  // namespace apollo::admin
