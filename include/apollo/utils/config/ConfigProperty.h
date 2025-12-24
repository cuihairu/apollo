#pragma once

#include "apollo/framework/ioc/ConfigManager.h"
#include <string>
#include <functional>
#include <sstream>
#include <type_traits>
#include <vector>

namespace Apollo {

template<typename T>
class ConfigProperty {
public:
    ConfigProperty(const std::string& key, const T& defaultValue = T{})
        : key_(key), value_(defaultValue), hasValue_(false) {}

    ConfigProperty& operator=(const T& value) {
        value_ = value;
        hasValue_ = true;

        if (ConfigManager* configMgr = ConfigManager::getInstance()) {
            configMgr->setValue(key_, value);
        }
        return *this;
    }

    operator T() const {
        if (!hasValue_) {
            loadFromConfig();
        }
        return value_;
    }

    T get() const {
        return static_cast<T>(*this);
    }

    void set(const T& value) {
        *this = value;
    }

    const std::string& getKey() const {
        return key_;
    }

    void reset() {
        hasValue_ = false;
        value_ = T{};
    }

    void addChangeListener(std::function<void(const T&)> listener) {
        listeners_.push_back(listener);
    }

    void notifyListeners() {
        T currentValue = get();
        for (auto& listener : listeners_) {
            listener(currentValue);
        }
    }

private:
    void loadFromConfig() const {
        // Forward declaration to avoid circular dependency
        // Implementation in ConfigManager.cpp
        if (ConfigManager* configMgr = ConfigManager::getInstance()) {
            if (configMgr->hasValue(key_)) {
                value_ = configMgr->getValue<T>(key_);
                hasValue_ = true;
            }
        }
    }

    std::string key_;
    mutable T value_;
    mutable bool hasValue_;
    std::vector<std::function<void(const T&)>> listeners_;
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const ConfigProperty<T>& prop) {
    os << prop.get();
    return os;
}

#define CONFIG_PROPERTY(Type, Name, Key, Default) \
    mutable Apollo::ConfigProperty<Type> Name##_{Key, Default}; \
    const Apollo::ConfigProperty<Type>& Name = Name##_; \
    Apollo::ConfigProperty<Type>& Name##Writable = Name##_

}
