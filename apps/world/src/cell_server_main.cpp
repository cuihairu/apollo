// cellServer——cell 执行端独立进程边界（L104 首片收口）。
//
// 与 world 共用二进制的 manager 角色不同，本目标不编译 WorldApp 管理壳：
// 只编执行端源（cell_main 入口 + cell_app 内核 + m1_link_server +
// cell_register_client + 协议编解码），进程内不再携带世界管理/注册/拓扑/
// 容量的管理职责代码。模拟循环、WorldHost/Scene/Avatar、AOI、战斗与 Lua
// 全部由 CellApp 内核装配（见 cell_main.cpp 的 run_cell_entry）。world
// --role=cell 委托同一 run_cell_entry_body，执行端装配单一来源。
#include "world_app/cell_main.hpp"

int main(int argc, char* argv[]) {
    return world_app::run_cell_entry(argc, argv);
}
