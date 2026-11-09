#pragma once

#include "apollo/core/log/log_manager.h"

namespace gateway::server_detail {

// gateway_server 拆分单元共享日志出口（cat=gateway_server；单定义在
// gateway_server.cpp——各拆分 TU 经匿名转发保持调用点不变）
apollo::core::log::Logger& glog();

} // namespace gateway::server_detail
