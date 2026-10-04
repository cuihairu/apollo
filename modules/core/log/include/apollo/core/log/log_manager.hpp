#pragma once

// .hpp 拼写入口（P0-6 双树收敛后唯一残留拼写）：直接转发活实现，
// 不再有任何本地类型声明（同名类双定义的 ODR 违规已随旧内存版
// log_manager.hpp/.cpp 删除一并消除）。
#include "apollo/core/log/log_manager.h"

// Re-export convenience macros
#define APOLLO_LOG() apollo::core::log::LogManager::instance().getDefaultLogger()
#define APOLLO_LOG_GET(name) apollo::core::log::LogManager::instance().getLogger(name)
