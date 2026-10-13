#pragma once
//
// collector 受理面（L4：logging.md §5 collector 形态——push 通道的接收
// 端）。net 依赖隔离在本单元：logger 壳（scan/follow 检索面）零链路依赖
// 不变，serve 子命令经此入口进 M1 面。
//

#include <cstdint>
#include <string>

namespace logger {

struct ServeOptions {
    std::string host = "0.0.0.0";
    std::uint16_t port = 9102;    // M1 passive 监听口（fleet 口径 9102）
    std::string archive_dir = "log/pushed";  // 归档目录（scan/follow 可直读）
    std::string route_json;       // 空 = 编译期默认（CMake 注入生成物路径）
};

// 阻塞受理：M1 passive 端点收 930 结构化行 → 原行归档 <archive_dir>/
// collector_<日期>.log（按日滚动 + 保留份数清理，ArchiveWriter——§5
// collector「磁盘滚动」；保真同 follow 透传；行内 proc= 键自识进程；
// 检索面按 *.log 目录序即时间序）。SIGINT/SIGTERM 退出，stderr 出受理
// 汇总。返回进程退出码。
int run_serve(const ServeOptions& options);

} // namespace logger
