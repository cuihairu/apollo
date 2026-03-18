#include "apollo/core/config/config_registry.hpp"

#include <charconv>
#include <mutex>
#include <shared_mutex>

namespace apollo::core::config {

void ConfigRegistry::set(std::string key, std::string value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    values_[std::move(key)] = std::move(value);
}

void ConfigRegistry::set(std::string key, const char* value) {
    set(std::move(key), std::string(value != nullptr ? value : ""));
}

void ConfigRegistry::set(std::string key, int64_t value) {
    set(std::move(key), std::to_string(value));
}

void ConfigRegistry::set(std::string key, bool value) {
    set(std::move(key), value ? "true" : "false");
}

bool ConfigRegistry::has(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return values_.find(key) != values_.end();
}

std::string ConfigRegistry::get_string(const std::string& key, std::string default_value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    return it == values_.end() ? std::move(default_value) : it->second;
}

int64_t ConfigRegistry::get_int64(const std::string& key, int64_t default_value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }

    int64_t result = default_value;
    const auto* begin = it->second.data();
    const auto* end = begin + it->second.size();
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end) {
        return default_value;
    }
    return result;
}

bool ConfigRegistry::get_bool(const std::string& key, bool default_value) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = values_.find(key);
    if (it == values_.end()) {
        return default_value;
    }
    if (it->second == "true" || it->second == "1" || it->second == "yes") {
        return true;
    }
    if (it->second == "false" || it->second == "0" || it->second == "no") {
        return false;
    }
    return default_value;
}

ConfigRegistry& global_config() {
    static ConfigRegistry registry;
    return registry;
}

} // namespace apollo::core::config
