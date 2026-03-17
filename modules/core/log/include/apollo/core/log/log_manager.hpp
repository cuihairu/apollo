#pragma once

#include "apollo/core/log/log_manager.h"
#include "apollo/core/log/logger.hpp"

namespace apollo::core::log {
using LogManager = LogManager;
using LogManagerConfig = LogManagerConfig;
using LoggerPtr = LoggerPtr;

// Re-export convenience macros
#define APOLLO_LOG() apollo::core::log::LogManager::instance().getDefaultLogger()
#define APOLLO_LOG_GET(name) apollo::core::log::LogManager::instance().getLogger(name)
}
