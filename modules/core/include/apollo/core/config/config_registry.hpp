#pragma once

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace apollo::core::config {

class ConfigRegistry {
public:
    void set(std::string key, std::string value);
    void set(std::string key, const char* value);
    void set(std::string key, int64_t value);
    void set(std::string key, bool value);

    bool has(const std::string& key) const;
    std::string get_string(const std::string& key, std::string default_value = {}) const;
    int64_t get_int64(const std::string& key, int64_t default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
};

ConfigRegistry& global_config();

} // namespace apollo::core::config
