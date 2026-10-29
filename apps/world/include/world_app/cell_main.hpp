#pragma once

#include <memory>

// cell 执行端独立入口（本切片：执行端进程边界 + 模拟循环迁出的最小可跑
// 骨架）。批⑳已把模拟内核从 WorldApp 抽入 CellApp；本面把执行端运行装配
// 从 world 共用二进制的内联角色分支显式化为可独立启动的入口，供真正的
// cellapp 可执行进程边界复用。
//
// 进程边界：cellServer 二进制只编译执行端源（cell_app/m1_link_server/
// cell_register_client + 协议编解码），不编译 WorldApp 管理壳——执行端
// 进程不再携带世界管理/注册/拓扑/容量的管理职责代码。world 不再保留
// --role=cell 委托（L104 收口片一），cellServer 是执行端唯一入口；执行端
// 装配来源 = run_cell_entry_body（本文件声明 + cell_main.cpp 定义）。

namespace apollo {
namespace core {
namespace log {
class Logger;
}
} // namespace core
} // namespace apollo

namespace world_app {

// 运行 cell 执行端并返回进程退出码（cellServer 独立入口）。语义：
// CellApp 内核 + 仅意图 M1 受理面（eval/编排/排空回调缺席→分发判空拒收）
// + 可选 World 注册拨号（--register-*）+ 可选编队注册（--machined-*）。
// 自持日志/崩溃捕获/配置解析（不依赖 manager 壳）。
int run_cell_entry(int argc, char* argv[]);

// 执行端装配与主循环（不含日志/崩溃捕获/信号注册——由调用方负责）。
// logger 由调用方注入：cellServer 独立入口自建 cell 日志。信号处理与本
// 进程停机面在本函数内装配（覆盖调用方既有注册，无竞态面——信号注册在
// 装配前单线程执行）。
int run_cell_entry_body(int argc, char* argv[],
                        const std::shared_ptr<apollo::core::log::Logger>& logger);

} // namespace world_app
