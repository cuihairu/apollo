#pragma once

#include "apollo/core/config/config_manager.h"

namespace apollo::core::config {
using ConfigManager = ConfigManager;
using ConfigFormat = ConfigFormat;
using ConfigChangeListener = ConfigChangeListener;

// Convenience macros
#define APOLLO_CONFIG_INT(key, def) \
    apollo::core::config::ConfigManager::instance().getInt(key, def)
#define APOLLO_CONFIG_STR(key, def) \
    apollo::core::config::ConfigManager::instance().getString(key, def)
#define APOLLO_CONFIG_BOOL(key, def) \
    apollo::core::config::ConfigManager::instance().getBool(key, def)
#define APOLLO_CONFIG_DOUBLE(key, def) \
    apollo::core::config::ConfigManager::instance().getDouble(key, def)
}
