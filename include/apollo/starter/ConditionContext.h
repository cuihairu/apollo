#pragma once

#include "Starter.h"
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <sstream>
#include <memory>

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace Apollo::Starter {

/**
 * @brief 条件上下文实现
 *
 * 提供 Starter 条件判断所需的上下文信息
 *
 * 设计原则：
 * - SOLID-S: 单一职责，只提供条件查询接口
 * - SOLID-I: 接口隔离，轻量级的查询接口
 * - DRY: 统一的条件检查逻辑
 */
class DefaultConditionContext : public ConditionContext {
public:
    DefaultConditionContext() = default;
    ~DefaultConditionContext() override = default;

    /**
     * @brief 设置配置属性
     */
    void setProperty(const std::string& key, const std::string& value) {
        properties_[key] = value;
    }

    void setProperty(const std::string& key, bool value) {
        properties_[key] = value ? "true" : "false";
    }

    void setProperty(const std::string& key, int value) {
        properties_[key] = std::to_string(value);
    }

    /**
     * @brief 从 key=value 格式的字符串加载配置
     */
    void loadFromString(const std::string& config) {
        std::istringstream iss(config);
        std::string line;
        while (std::getline(iss, line)) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                // 去除空格
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                properties_[key] = value;
            }
        }
    }

    /**
     * @brief 从 JSON 格式加载配置
     */
#ifdef HAVE_NLOHMANN_JSON
    void loadFromJson(const std::string& jsonStr) {
        try {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.value().is_string()) {
                    properties_[it.key()] = it.value().get<std::string>();
                } else if (it.value().is_boolean()) {
                    properties_[it.key()] = it.value().get<bool>() ? "true" : "false";
                } else if (it.value().is_number_integer()) {
                    properties_[it.key()] = std::to_string(it.value().get<int>());
                } else if (it.value().is_number()) {
                    properties_[it.key()] = std::to_string(it.value().get<double>());
                }
            }
        } catch (const std::exception& e) {
            // 静默失败，保持现有配置
        }
    }
#endif

    bool getProperty(const std::string& key, bool defaultValue) const override {
        auto it = properties_.find(key);
        if (it == properties_.end()) {
            return defaultValue;
        }
        const std::string& value = it->second;
        return value == "true" || value == "1" || value == "yes" || value == "on";
    }

    int getProperty(const std::string& key, int defaultValue) const override {
        auto it = properties_.find(key);
        if (it == properties_.end()) {
            return defaultValue;
        }
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }

    std::string getProperty(const std::string& key, const std::string& defaultValue) const override {
        auto it = properties_.find(key);
        return it != properties_.end() ? it->second : defaultValue;
    }

    /**
     * @brief 标记类为可用
     */
    void markClassAvailable(const std::string& className) {
        availableClasses_.insert(className);
    }

    bool isClassAvailable(const std::string& className) const override {
        return availableClasses_.find(className) != availableClasses_.end();
    }

    /**
     * @brief 标记 Bean 为存在
     */
    void markBeanPresent(const std::string& beanName) {
        presentBeans_.insert(beanName);
    }

    bool isBeanPresent(const std::string& beanName) const override {
        return presentBeans_.find(beanName) != presentBeans_.end();
    }

    /**
     * @brief 标记 Starter 为启用
     */
    void markStarterEnabled(const std::string& starterName) {
        enabledStarters_.insert(starterName);
    }

    bool isStarterEnabled(const std::string& starterName) const override {
        return enabledStarters_.find(starterName) != enabledStarters_.end();
    }

    /**
     * @brief 获取所有配置属性
     */
    const std::unordered_map<std::string, std::string>& getAllProperties() const {
        return properties_;
    }

private:
    std::unordered_map<std::string, std::string> properties_;
    std::unordered_set<std::string> availableClasses_;
    std::unordered_set<std::string> presentBeans_;
    std::unordered_set<std::string> enabledStarters_;
};

} // namespace Apollo::Starter
