/**
 * @file config_value.cpp
 * @brief 配置节点实现
 */

#include "apollo/core/config/config_value.h"
#include <sstream>
#include <algorithm>

namespace apollo {
namespace core {
namespace config {

//==============================================================================
// ConfigNode 实现
//==============================================================================

const ConfigNode* ConfigNode::getByPath(const std::string& path) const {
    if (path.empty()) {
        return this;
    }

    // 查找第一个点或斜杠
    std::string::size_type pos = path.find('.');
    std::string::size_type pos2 = path.find('/');

    if (pos == std::string::npos && pos2 == std::string::npos) {
        // 没有分隔符，直接查找子节点
        return &getChild(path);
    }

    // 使用较早出现的分隔符
    std::string::size_type sep = pos;
    if (pos == std::string::npos) {
        sep = pos2;
    } else if (pos2 != std::string::npos && pos2 < pos) {
        sep = pos2;
    }

    std::string key = path.substr(0, sep);
    std::string rest = (sep + 1 < path.size()) ? path.substr(sep + 1) : "";

    auto it = children_.find(key);
    if (it == children_.end()) {
        return nullptr;
    }

    return rest.empty() ? &it->second : it->second.getByPath(rest);
}

ConfigNode* ConfigNode::getByPath(const std::string& path) {
    return const_cast<ConfigNode*>(const_cast<const ConfigNode*>(this)->getByPath(path));
}

void ConfigNode::merge(const ConfigNode& other) {
    // 合并值（如果有）
    if (!other.isNull()) {
        value_ = other.value_;
    }

    // 合并子节点
    for (const auto& pair : other.children_) {
        auto it = children_.find(pair.first);
        if (it != children_.end()) {
            it->second.merge(pair.second);
        } else {
            children_[pair.first] = pair.second;
        }
    }
}

std::string ConfigNode::toString(int indent) const {
    std::ostringstream oss;
    std::string ind(indent, ' ');

    // 输出值
    if (isNull()) {
        // 无值，只有子节点
    } else if (isBool()) {
        oss << ind << asBool() << "\n";
    } else if (isInt64()) {
        oss << ind << asInt64() << "\n";
    } else if (isDouble()) {
        oss << ind << asDouble() << "\n";
    } else if (isString()) {
        oss << ind << "\"" << asString() << "\"\n";
    } else if (isArray()) {
        auto arr = asArray();
        oss << ind << "[";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << "\"" << arr[i] << "\"";
        }
        oss << "]\n";
    }

    // 输出子节点
    for (const auto& pair : children_) {
        oss << ind << pair.first << ":\n";
        oss << pair.second.toString(indent + 2);
    }

    return oss.str();
}

} // namespace config
} // namespace core
} // namespace apollo
