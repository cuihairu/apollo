#pragma once

#include "apollo/core/log/log_manager.h"

namespace world_app::detail {

// world_app 拆分单元共享日志出口（cat=world_app；单定义在 world_app.cpp——
// 各拆分 TU 经匿名转发保持调用点不变）
apollo::core::log::Logger& world_log();

} // namespace world_app::detail
