#pragma once

// CellApp 拆分实现单元（LAY-001）共用的内部出口——实现单元私有，
// 不进公共安装头；单一定义在 cell_app.cpp。
namespace apollo::core::log {
class Logger;
}

namespace world_app::cell_detail {

// 库面日志出口：cat=world_app（§5.1 cat= 检索键；主程已接文件面）
apollo::core::log::Logger& world_log();

} // namespace world_app::cell_detail
