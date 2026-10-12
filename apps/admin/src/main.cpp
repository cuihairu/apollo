// admin——指标 exporter 进程壳入口（metrics admin 通道，logging.md §5.1
// 单一 exporter 定案）。
//
// v1 形态 = serve 受面子命令：link passive "metrics"(9106) 收 931 快照 +
// HTTP(9107) 吐 /metrics。admin 不进业务路径（三禁），crash 采集面照常。

#include "apollo/core/log/log_manager.h"
#include "apollo/runtime/crash_capture.hpp"
#include "admin/admin_app.hpp"

int main(int argc, char* argv[]) {
    // stdout 是查询结果面（serve 起始行）——admin 自身不配任何日志出口
    // （ring 仅内存），惰性默认初始化会装 console appender 污染输出，
    // 故先显式 initialize。
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.consoleEnabled = false;
    config.fileEnabled = false;
    logs.initialize(config);

    apollo::runtime::init_crash_capture(argc, argv, "admin");
    return apollo::admin::AdminApp().run(argc, argv);
}
