#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>

namespace apollo {
namespace core {
namespace config {

/**
 * @brief 配置值类型
 */
using ConfigValue = std::variant<
    std::monostate,   // 空值
    bool,             // 布尔值
    int64_t,          // 整数
    double,           // 浮点数
    std::string,      // 字符串
    std::vector<std::string>  // 字符串数组
>;

/**
 * @brief 配置异常
 */
class ConfigException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief 配置节点
 *
 * 支持层级结构的配置访问
 */
class ConfigNode {
public:
    ConfigNode() = default;
    explicit ConfigNode(ConfigValue value) : value_(std::move(value)) {}

    // 类型检查
    bool isNull() const { return std::holds_alternative<std::monostate>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isInt64() const { return std::holds_alternative<int64_t>(value_); }
    bool isDouble() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<std::vector<std::string>>(value_); }

    // 获取值（带默认值）
    bool asBool(bool defaultValue = false) const {
        if (isBool()) return std::get<bool>(value_);
        if (isString()) {
            const auto& s = std::get<std::string>(value_);
            return s == "true" || s == "1" || s == "yes";
        }
        return defaultValue;
    }

    int64_t asInt64(int64_t defaultValue = 0) const {
        if (isInt64()) return std::get<int64_t>(value_);
        if (isDouble()) return static_cast<int64_t>(std::get<double>(value_));
        if (isString()) {
            try { return std::stoll(std::get<std::string>(value_)); }
            catch (...) { return defaultValue; }
        }
        return defaultValue;
    }

    int asInt(int defaultValue = 0) const {
        return static_cast<int>(asInt64(defaultValue));
    }

    double asDouble(double defaultValue = 0.0) const {
        if (isDouble()) return std::get<double>(value_);
        if (isInt64()) return static_cast<double>(std::get<int64_t>(value_));
        if (isString()) {
            try { return std::stod(std::get<std::string>(value_)); }
            catch (...) { return defaultValue; }
        }
        return defaultValue;
    }

    std::string asString(const std::string& defaultValue = "") const {
        if (isString()) return std::get<std::string>(value_);
        if (isBool()) return std::get<bool>(value_) ? "true" : "false";
        if (isInt64()) return std::to_string(std::get<int64_t>(value_));
        if (isDouble()) return std::to_string(std::get<double>(value_));
        return defaultValue;
    }

    std::vector<std::string> asArray() const {
        if (isArray()) return std::get<std::vector<std::string>>(value_);
        if (isString()) {
            // 尝试按逗号分割
            std::string s = asString();
            std::vector<std::string> result;
            std::string::size_type start = 0, pos;
            while ((pos = s.find(',', start)) != std::string::npos) {
                result.push_back(s.substr(start, pos - start));
                start = pos + 1;
            }
            result.push_back(s.substr(start));
            return result;
        }
        return {};
    }

    // 设置值
    void setBool(bool v) { value_ = v; }
    void setInt64(int64_t v) { value_ = v; }
    void setInt(int v) { value_ = static_cast<int64_t>(v); }
    void setDouble(double v) { value_ = v; }
    void setString(const std::string& v) { value_ = v; }
    void setArray(const std::vector<std::string>& v) { value_ = v; }

    // 子节点操作
    bool hasChild(const std::string& key) const {
        return children_.find(key) != children_.end();
    }

    ConfigNode& getChild(const std::string& key) {
        return children_[key];
    }

    const ConfigNode& getChild(const std::string& key) const {
        static ConfigNode nullNode;
        auto it = children_.find(key);
        return (it != children_.end()) ? it->second : nullNode;
    }

    void setChild(const std::string& key, ConfigNode node) {
        children_[key] = std::move(node);
    }

    // 路径访问（支持 "section.key" 格式）
    ConfigNode* getByPath(const std::string& path);

    const ConfigNode* getByPath(const std::string& path) const;

    // 获取所有子节点键名
    std::vector<std::string> getKeys() const {
        std::vector<std::string> keys;
        for (const auto& pair : children_) {
            keys.push_back(pair.first);
        }
        return keys;
    }

    // 合并另一个节点
    void merge(const ConfigNode& other);

    // 转换为字符串（调试用）
    std::string toString(int indent = 0) const;

private:
    ConfigValue value_;
    std::map<std::string, ConfigNode> children_;
};

} // namespace config
} // namespace core
} // namespace apollo
