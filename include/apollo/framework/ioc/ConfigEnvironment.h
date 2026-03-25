#pragma once

#include "ConfigManager.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Apollo {

enum class ConfigSourcePriority : int {
    Defaults = 0,
    BaseConfig = 100,
    EnvironmentConfig = 200,
    OverrideConfig = 300,
    EnvironmentVariables = 400,
    CommandLine = 500,
    InMemoryOverride = 600
};

struct ConfigValueOrigin {
    std::string source;
    std::string value;
    int priority = 0;
};

class IConfigEnvironment {
public:
    virtual ~IConfigEnvironment() = default;
    virtual bool has(std::string_view key) const = 0;
    virtual std::string getString(std::string_view key,
                                  std::string_view fallback = "") const = 0;
    virtual int64_t getInt(std::string_view key, int64_t fallback = 0) const = 0;
    virtual bool getBool(std::string_view key, bool fallback = false) const = 0;
    virtual std::vector<std::string> getList(std::string_view key) const = 0;
    virtual std::vector<ConfigValueOrigin> explain(std::string_view key) const = 0;
};

class ConfigEnvironment : public IConfigEnvironment {
public:
    void set(std::string source,
             ConfigSourcePriority priority,
             std::string key,
             std::string value) {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_[std::move(key)].push_back(
            ConfigValueOrigin{std::move(source), std::move(value), static_cast<int>(priority)});
    }

    void remove(std::string_view key, std::string_view source = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = storage_.find(std::string(key));
        if (it == storage_.end()) {
            return;
        }

        if (source.empty()) {
            storage_.erase(it);
            return;
        }

        auto& values = it->second;
        values.erase(std::remove_if(values.begin(), values.end(),
            [&](const ConfigValueOrigin& origin) {
                return origin.source == source;
            }), values.end());

        if (values.empty()) {
            storage_.erase(it);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_.clear();
    }

    void clearSource(std::string_view source) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = storage_.begin(); it != storage_.end();) {
            auto& values = it->second;
            values.erase(std::remove_if(values.begin(), values.end(),
                [&](const ConfigValueOrigin& origin) {
                    return origin.source == source;
                }), values.end());

            if (values.empty()) {
                it = storage_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool has(std::string_view key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return storage_.find(std::string(key)) != storage_.end();
    }

    std::string getString(std::string_view key,
                          std::string_view fallback = "") const override {
        auto value = resolveValue(key);
        return value.empty() ? std::string(fallback) : value;
    }

    int64_t getInt(std::string_view key, int64_t fallback = 0) const override {
        auto value = resolveValue(key);
        if (value.empty()) {
            return fallback;
        }
        try {
            return std::stoll(value);
        } catch (...) {
            return fallback;
        }
    }

    bool getBool(std::string_view key, bool fallback = false) const override {
        auto value = resolveValue(key);
        if (value.empty()) {
            return fallback;
        }
        return value == "true" || value == "1" || value == "yes" || value == "on";
    }

    std::vector<std::string> getList(std::string_view key) const override {
        auto value = resolveValue(key);
        std::vector<std::string> result;
        if (value.empty()) {
            return result;
        }

        std::stringstream ss(value);
        std::string item;
        while (std::getline(ss, item, ',')) {
            trimInPlace(item);
            if (!item.empty()) {
                result.push_back(item);
            }
        }
        return result;
    }

    std::vector<ConfigValueOrigin> explain(std::string_view key) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = storage_.find(std::string(key));
        if (it == storage_.end()) {
            return {};
        }

        auto values = it->second;
        std::sort(values.begin(), values.end(),
            [](const ConfigValueOrigin& lhs, const ConfigValueOrigin& rhs) {
                if (lhs.priority != rhs.priority) {
                    return lhs.priority > rhs.priority;
                }
                return lhs.source < rhs.source;
            });
        return values;
    }

    std::vector<std::string> getAllKeys() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> keys;
        keys.reserve(storage_.size());
        for (const auto& [key, _] : storage_) {
            keys.push_back(key);
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    void syncToConfigManager(ConfigManager& configManager) const {
        std::lock_guard<std::mutex> lock(mutex_);
        configManager.clear();
        for (const auto& [key, origins] : storage_) {
            if (origins.empty()) {
                continue;
            }
            const auto winner = std::max_element(origins.begin(), origins.end(),
                [](const ConfigValueOrigin& lhs, const ConfigValueOrigin& rhs) {
                    if (lhs.priority != rhs.priority) {
                        return lhs.priority < rhs.priority;
                    }
                    return lhs.source > rhs.source;
                });
            configManager.setValue(key, winner->value);
        }
    }

private:
    std::string resolveValue(std::string_view key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = storage_.find(std::string(key));
        if (it == storage_.end() || it->second.empty()) {
            return {};
        }

        const auto winner = std::max_element(it->second.begin(), it->second.end(),
            [](const ConfigValueOrigin& lhs, const ConfigValueOrigin& rhs) {
                if (lhs.priority != rhs.priority) {
                    return lhs.priority < rhs.priority;
                }
                return lhs.source > rhs.source;
            });
        return winner->value;
    }

    static void trimInPlace(std::string& value) {
        auto notSpace = [](unsigned char ch) { return std::isspace(ch) == 0; };
        value.erase(value.begin(),
            std::find_if(value.begin(), value.end(), notSpace));
        value.erase(
            std::find_if(value.rbegin(), value.rend(), notSpace).base(),
            value.end());
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::vector<ConfigValueOrigin>> storage_;
};

} // namespace Apollo
