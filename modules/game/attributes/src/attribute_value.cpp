#include "apollo/game/attributes/attribute_value.h"
#include <algorithm>

namespace apollo {
namespace game {

// ========== AttributeContainer ==========

AttributeContainer::AttributeContainer(uint64_t objectId)
    : objectId_(objectId) {
}

bool AttributeContainer::setAttribute(uint32_t attrId, const ComVal& value, bool fromServer) {
    ComVal oldValue;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = attributes_.find(attrId);
        if (it != attributes_.end()) {
            oldValue = it->second.value;

            // 值未变化
            if (oldValue == value) {
                return false;
            }
        }

        // 设置新值
        attributes_[attrId].id = attrId;
        attributes_[attrId].value = value;
        attributes_[attrId].dirty = !fromServer; // 来自服务端的值不需要同步回去
        changed = true;
    }

    if (changed) {
        notifyChange(attrId, oldValue, value, fromServer);
    }

    return changed;
}

int AttributeContainer::setAttributes(const std::unordered_map<uint32_t, ComVal>& attrs, bool fromServer) {
    if (attrs.empty()) return 0;

    int changeCount = 0;
    std::vector<AttributeChangeEvent> events;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& kv : attrs) {
            uint32_t attrId = kv.first;
            const ComVal& newValue = kv.second;

            ComVal oldValue;
            bool exists = false;

            auto it = attributes_.find(attrId);
            if (it != attributes_.end()) {
                oldValue = it->second.value;
                exists = true;

                // 值未变化
                if (oldValue == newValue) {
                    continue;
                }
            }

            // 设置新值
            attributes_[attrId].id = attrId;
            attributes_[attrId].value = newValue;
            attributes_[attrId].dirty = !fromServer;

            events.push_back({objectId_, attrId, oldValue, newValue, fromServer});
            changeCount++;
        }
    }

    // 触发事件
    for (const auto& evt : events) {
        notifyChange(evt.attributeId, evt.oldValue, evt.newValue, evt.fromServer);
    }

    return changeCount;
}

bool AttributeContainer::getAttribute(uint32_t attrId, ComVal& value) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        value = it->second.value;
        return true;
    }
    return false;
}

ComVal AttributeContainer::getAttribute(uint32_t attrId, const ComVal& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value;
    }
    return defaultValue;
}

int32_t AttributeContainer::getInt(uint32_t attrId, int32_t defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getInt(defaultValue);
    }
    return defaultValue;
}

int64_t AttributeContainer::getInt64(uint32_t attrId, int64_t defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getInt64(defaultValue);
    }
    return defaultValue;
}

uint32_t AttributeContainer::getUInt(uint32_t attrId, uint32_t defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getDword(defaultValue);
    }
    return defaultValue;
}

double AttributeContainer::getDouble(uint32_t attrId, double defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getDouble(defaultValue);
    }
    return defaultValue;
}

bool AttributeContainer::getBool(uint32_t attrId, bool defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getBool(defaultValue);
    }
    return defaultValue;
}

std::string AttributeContainer::getString(uint32_t attrId, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        return it->second.value.getStringStd(defaultValue);
    }
    return defaultValue;
}

ComVal AttributeContainer::addValue(uint32_t attrId, const ComVal& delta) {
    ComVal current;
    ComVal newValue;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = attributes_.find(attrId);
        if (it != attributes_.end()) {
            current = it->second.value;
        }

        newValue = current + delta;
        attributes_[attrId].id = attrId;
        attributes_[attrId].value = newValue;
        attributes_[attrId].dirty = true;
    }

    notifyChange(attrId, current, newValue, false);

    return newValue;
}

bool AttributeContainer::hasAttribute(uint32_t attrId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attributes_.find(attrId) != attributes_.end();
}

bool AttributeContainer::removeAttribute(uint32_t attrId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return attributes_.erase(attrId) > 0;
}

void AttributeContainer::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    attributes_.clear();
}

std::vector<uint32_t> AttributeContainer::getDirtyAttributes() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint32_t> result;
    for (const auto& kv : attributes_) {
        if (kv.second.dirty) {
            result.push_back(kv.first);
        }
    }
    return result;
}

std::unordered_map<uint32_t, ComVal> AttributeContainer::getDirtyValues() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unordered_map<uint32_t, ComVal> result;
    for (const auto& kv : attributes_) {
        if (kv.second.dirty) {
            result[kv.first] = kv.second.value;
        }
    }
    return result;
}

void AttributeContainer::clearDirtyFlags() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& kv : attributes_) {
        kv.second.dirty = false;
    }
}

void AttributeContainer::clearDirtyFlag(uint32_t attrId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        it->second.dirty = false;
    }
}

uint64_t AttributeContainer::addChangeListener(AttributeChangeCallback callback) {
    if (!callback) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t listenerId = nextListenerId_++;
    changeListeners_.push_back({listenerId, std::move(callback)});
    return listenerId;
}

bool AttributeContainer::removeChangeListener(uint64_t listenerId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto oldSize = changeListeners_.size();
    changeListeners_.erase(
        std::remove_if(changeListeners_.begin(),
                       changeListeners_.end(),
                       [listenerId](const auto& entry) {
                           return entry.first == listenerId;
                       }),
        changeListeners_.end());
    return changeListeners_.size() != oldSize;
}

std::unordered_map<uint32_t, ComVal> AttributeContainer::getAllValues() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unordered_map<uint32_t, ComVal> result;
    result.reserve(attributes_.size());
    for (const auto& [attrId, attr] : attributes_) {
        result.emplace(attrId, attr.value);
    }
    return result;
}

std::vector<uint8_t> AttributeContainer::serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);

    // 计算需要的空间
    // 格式: [属性数量(2)] [属性ID(4) 值长度(2) 值数据...]
    size_t totalSize = 2; // 属性数量
    for (const auto& kv : attributes_) {
        totalSize += 4 + 2; // 属性ID + 值长度
        // 值数据大小估算
        switch (kv.second.value.getType()) {
            case EComValType::ECVT_BOOL:
            case EComValType::ECVT_BYTE:
                totalSize += 1;
                break;
            case EComValType::ECVT_WORD:
                totalSize += 2;
                break;
            case EComValType::ECVT_INT:
            case EComValType::ECVT_DWORD:
                totalSize += 4;
                break;
            case EComValType::ECVT_INT64:
            case EComValType::ECVT_DOUBLE:
                totalSize += 8;
                break;
            case EComValType::ECVT_STRING:
                totalSize += kv.second.value.getStringStd().size();
                break;
            default:
                break;
        }
    }

    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;

    // 写入属性数量
    uint16_t count = static_cast<uint16_t>(attributes_.size());
    std::memcpy(buffer.data() + offset, &count, 2);
    offset += 2;

    // 写入每个属性
    for (const auto& kv : attributes_) {
        // 写入属性ID
        std::memcpy(buffer.data() + offset, &kv.first, 4);
        offset += 4;

        // 写入值
        int valueOffset = static_cast<int>(offset + 2); // 跳过长度字段
        if (kv.second.value.saveToBuff(reinterpret_cast<char*>(buffer.data()), buffer.size(), valueOffset)) {
            uint16_t valueLen = static_cast<uint16_t>(valueOffset - offset - 2);
            std::memcpy(buffer.data() + offset, &valueLen, 2);
            offset = valueOffset;
        } else {
            // 写入空值
            uint16_t valueLen = 0;
            std::memcpy(buffer.data() + offset, &valueLen, 2);
            offset += 2;
        }
    }

    return buffer;
}

bool AttributeContainer::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 2) return false;

    int offset = 0;

    // 读取属性数量
    uint16_t count;
    std::memcpy(&count, data.data() + offset, 2);
    offset += 2;

    // 读取每个属性
    for (uint16_t i = 0; i < count; ++i) {
        if (offset + 6 > static_cast<int>(data.size())) return false;

        // 读取属性ID
        uint32_t attrId;
        std::memcpy(&attrId, data.data() + offset, 4);
        offset += 4;

        // 读取值长度
        uint16_t valueLen;
        std::memcpy(&valueLen, data.data() + offset, 2);
        offset += 2;

        // 读取值
        ComVal value;
        if (valueLen > 0 && offset + valueLen <= static_cast<int>(data.size())) {
            value.fromBuff(reinterpret_cast<const char*>(data.data()) + offset, valueLen, offset);
            // fromBuff 会更新 offset，但这里我们不需要继续使用
        }

        setAttribute(attrId, value, true); // 来自服务端
    }

    return true;
}

void AttributeContainer::notifyChange(uint32_t attrId, const ComVal& oldValue,
                                       const ComVal& newValue, bool fromServer) {
    std::vector<AttributeChangeCallback> listeners;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        listeners.reserve(changeListeners_.size());
        for (const auto& entry : changeListeners_) {
            listeners.push_back(entry.second);
        }
    }

    if (listeners.empty()) {
        return;
    }

    AttributeChangeEvent evt = {objectId_, attrId, oldValue, newValue, fromServer};
    for (const auto& listener : listeners) {
        if (listener) {
            listener(evt);
        }
    }
}

// ========== AttributeContainerManager ==========

AttributeContainerManager& AttributeContainerManager::instance() {
    static AttributeContainerManager instance;
    return instance;
}

std::shared_ptr<AttributeContainer> AttributeContainerManager::getOrCreate(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = containers_.find(objectId);
    if (it != containers_.end()) {
        return it->second;
    }

    auto container = std::make_shared<AttributeContainer>(objectId);
    containers_[objectId] = container;
    return container;
}

std::shared_ptr<AttributeContainer> AttributeContainerManager::get(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = containers_.find(objectId);
    if (it != containers_.end()) {
        return it->second;
    }
    return nullptr;
}

void AttributeContainerManager::remove(uint64_t objectId) {
    std::lock_guard<std::mutex> lock(mutex_);
    containers_.erase(objectId);
}

void AttributeContainerManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    containers_.clear();
}

std::vector<uint64_t> AttributeContainerManager::getAllObjectIds() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<uint64_t> result;
    result.reserve(containers_.size());
    for (const auto& kv : containers_) {
        result.push_back(kv.first);
    }
    return result;
}

} // namespace game
} // namespace apollo
