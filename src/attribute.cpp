#include "apollo/attribute.hpp"
#include <stdexcept>

namespace apollo {

bool AttributeContainer::SetAttribute(uint32_t attributeId, const AttributeValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    AttributeValue oldValue;
    bool hasOld = false;
    auto it = attributes_.find(attributeId);
    if (it != attributes_.end()) {
        oldValue = it->second;
        hasOld = true;
    }

    attributes_[attributeId] = value;

    if (listener_ && (!hasOld || oldValue != value)) {
        AttributeChangeEvent event;
        event.objectId = objectId_;
        event.attributeId = attributeId;
        event.oldValue = oldValue;
        event.newValue = value;
        listener_(event);
    }

    return true;
}

bool AttributeContainer::GetAttribute(uint32_t attributeId, AttributeValue& value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attributes_.find(attributeId);
    if (it != attributes_.end()) {
        value = it->second;
        return true;
    }
    return false;
}

bool AttributeContainer::AddAttribute(uint32_t attributeId, const AttributeValue& delta) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attributeId);
    if (it == attributes_.end()) {
        // 如果属性不存在，直接设置
        attributes_[attributeId] = delta;
    } else {
        // 根据类型进行加法操作
        try {
            if (std::holds_alternative<int32_t>(it->second)) {
                if (std::holds_alternative<int32_t>(delta)) {
                    it->second = std::get<int32_t>(it->second) + std::get<int32_t>(delta);
                }
            } else if (std::holds_alternative<int64_t>(it->second)) {
                if (std::holds_alternative<int64_t>(delta)) {
                    it->second = std::get<int64_t>(it->second) + std::get<int64_t>(delta);
                }
            } else if (std::holds_alternative<float>(it->second)) {
                if (std::holds_alternative<float>(delta)) {
                    it->second = std::get<float>(it->second) + std::get<float>(delta);
                }
            } else if (std::holds_alternative<double>(it->second)) {
                if (std::holds_alternative<double>(delta)) {
                    it->second = std::get<double>(it->second) + std::get<double>(delta);
                }
            }
        } catch (...) {
            return false;
        }
    }

    if (listener_) {
        AttributeChangeEvent event;
        event.objectId = objectId_;
        event.attributeId = attributeId;
        // 省略oldValue计算
        event.newValue = it->second;
        listener_(event);
    }

    return true;
}

void AttributeContainer::SetAttributes(const std::vector<std::pair<uint32_t, AttributeValue>>& attrs) {
    for (const auto& attr : attrs) {
        SetAttribute(attr.first, attr.second);
    }
}

const std::unordered_map<uint32_t, AttributeValue>& AttributeContainer::GetAllAttributes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attributes_;
}

void AttributeContainer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    attributes_.clear();
}

bool AttributeManager::RegisterAttribute(const AttributeDef& def) {
    std::lock_guard<std::mutex> lock(mutex_);
    attributeDefs_[def.id] = def;
    return true;
}

const AttributeDef* AttributeManager::GetAttributeDef(uint32_t attributeId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = attributeDefs_.find(attributeId);
    return it != attributeDefs_.end() ? &it->second : nullptr;
}

std::shared_ptr<AttributeContainer> AttributeManager::CreateContainer(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto container = std::make_shared<AttributeContainer>(objectId);
    containers_[objectId] = container;
    return container;
}

std::shared_ptr<AttributeContainer> AttributeManager::GetContainer(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = containers_.find(objectId);
    return it != containers_.end() ? it->second : nullptr;
}

void AttributeManager::DestroyContainer(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    containers_.erase(objectId);
}

bool AttributeManager::LoadFromConfig(const std::string& configFile) {
    // TODO: 从配置文件加载属性定义
    return true;
}

}  // namespace apollo