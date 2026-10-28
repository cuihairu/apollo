#pragma once

// cell 执行端独立入口（本切片：执行端进程边界 + 模拟循环迁出的最小可跑
// 骨架）。批⑳已把模拟内核从 WorldApp 抽入 CellApp；本面把执行端运行装配
// 从 world 共用二进制的内联角色分支显式化为可独立启动的入口，供真正的
// cellapp 可执行进程边界复用。
//
// 进程边界：cellapp 二进制只编译执行端源（cell_app/m1_link_server/
// cell_register_client + 协议编解码），不编译 WorldApp 管理壳——执行端
// 进程不再携带世界管理/注册/拓扑/容量的管理职责代码。world --role=cell
// 保留为兼容委托（既有 drill/编队口径零破坏，边界显式化不拆既有消费面）。

namespace world_app {

// 运行 cell 执行端并返回进程退出码。语义与旧 world --role=cell 完全一致：
// CellApp 内核 + 仅意图 M1 受理面（eval/编排/排空回调缺席→分发判空拒收）
// + 可选 World 注册拨号（--register-*）+ 可选编队注册（--machined-*）。
// 自持日志/崩溃捕获/配置解析（不依赖 manager 壳）。
int run_cell_entry(int argc, char* argv[]);

} // namespace world_app
