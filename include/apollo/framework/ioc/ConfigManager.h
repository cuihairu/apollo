#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <memory>
#include <type_traits>
#include <any>
#include <sstream>

namespace Apollo {

class FileWatcher;

class ConfigManager {
public:
    static ConfigManager* getInstance() {
        static ConfigManager instance;
        return &instance;
    }

    bool loadFromFile(const std::string& filename);
    bool saveToFile(const std::string& filename);
    bool loadFromJson(const std::string& jsonStr);
    std::string saveToJson() const;

    // Convenience overload: avoid template deduction as `char[N]` for string literals.
    void setValue(const std::string& key, const char* value) {
        setValue<std::string>(key, value ? std::string(value) : std::string());
    }

    template<typename T>
    void setValue(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string strValue = toString(value);
        values_[key] = strValue;

        auto it = typedValues_.find(key);
        if (it != typedValues_.end()) {
            it->second = std::make_any<T>(value);
        }
        notifyChangeListeners(key);
    }

    template<typename T>
    T getValue(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = values_.find(key);
        if (it == values_.end()) {
            return T{};
        }

        auto typedIt = typedValues_.find(key);
        if (typedIt != typedValues_.end()) {
            try {
                return std::any_cast<T>(typedIt->second);
            } catch (const std::bad_any_cast&) {}
        }

        return fromString<T>(it->second);
    }

    template<typename T>
    T getValue(const std::string& key, const T& defaultValue) const {
        if (!hasValue(key)) {
            return defaultValue;
        }
        return getValue<T>(key);
    }

    bool hasValue(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return values_.find(key) != values_.end();
    }

    void removeValue(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.erase(key);
        typedValues_.erase(key);
        notifyChangeListeners(key);
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        values_.clear();
        typedValues_.clear();
    }

    std::vector<std::string> getAllKeys() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> keys;
        for (const auto& pair : values_) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    void addChangeListener(const std::string& key, std::function<void(const std::string&)> listener);
    void removeChangeListener(const std::string& key);
    void addGlobalChangeListener(std::function<void(const std::string&)> listener);
    void removeGlobalChangeListener(std::function<void(const std::string&)> listener);

    bool enableAutoReload(const std::string& configFile);
    void disableAutoReload();
    bool isAutoReloadEnabled() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    template<typename T>
    std::string toString(const T& value) const {
        if constexpr (std::is_same_v<T, std::string>) {
            return value;
        } else if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    }

    template<typename T>
    T fromString(const std::string& str) const {
        if constexpr (std::is_same_v<T, std::string>) {
            return str;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(str);
        } else if constexpr (std::is_same_v<T, long>) {
            return std::stol(str);
        } else if constexpr (std::is_same_v<T, long long>) {
            return std::stoll(str);
        } else if constexpr (std::is_same_v<T, unsigned int>) {
            return static_cast<unsigned int>(std::stoul(str));
        } else if constexpr (std::is_same_v<T, unsigned long>) {
            return std::stoul(str);
        } else if constexpr (std::is_same_v<T, unsigned long long>) {
            return std::stoull(str);
        } else if constexpr (std::is_same_v<T, float>) {
            return std::stof(str);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(str);
        } else if constexpr (std::is_same_v<T, bool>) {
            return str == "true" || str == "1" || str == "yes" || str == "on";
        } else {
            std::istringstream iss(str);
            T value;
            iss >> value;
            return value;
        }
    }

    void notifyChangeListeners(const std::string& key);
    void onFileChanged(const std::string& filename);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> values_;
    std::unordered_map<std::string, std::any> typedValues_;
    std::unordered_map<std::string, std::vector<std::function<void(const std::string&)>>> changeListeners_;
    std::vector<std::function<void(const std::string&)>> globalChangeListeners_;
    FileWatcher* fileWatcher_;
    std::string watchedFile_;
};

}
