#pragma once

#include "apollo/core/log/log_manager.h"

namespace player_store::store_detail {

// player_store 拆分单元共享日志出口（cat=base_server；单定义在
// player_store.cpp——各拆分 TU 经匿名转发保持调用点不变）
apollo::core::log::Logger& blog();

} // namespace player_store::store_detail
