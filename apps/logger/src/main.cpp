// logger——日志收集进程壳入口（ADR-013 立项 / logger-app.md §6 批 L3）。
//
// v1 形态 = 只读汇聚 CLI（scan 检索 / follow 跟随），进程可入编队
// （dev_fleet roster 行，无端口——三禁）。push 出口留接缝不实现
//（logger/push_sink.hpp，collector 面 M1 后换 InterServerLink）。

#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/log/log_manager.h"
#include "logger/logger_app.hpp"

int main(int argc, char* argv[]) {
    // stdout 是查询结果面——logger 自身不配任何日志出口（ring 仅内存），
    // 惰性默认初始化会装 console appender 污染输出，故先显式 initialize。
    // crash 采集面照常（dump 落 crashdumps/，可见化行直写 stderr）。
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.consoleEnabled = false;
    config.fileEnabled = false;
    logs.initialize(config);

    apollo::runtime::init_crash_capture(argc, argv, "logger");
    return logger::LoggerApp().run(argc, argv);
}
