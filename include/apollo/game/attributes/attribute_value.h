#pragma once

#include "apollo/game/attributes/comval.h"
#include "apollo/game/attributes/attribute_id.h"
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace apollo {
namespace game {

/**
 * @brief 属性变更事件
 */
struct AttributeChangeEvent {
    uint64_t objectId;       ///< 对象ID
    uint32_t attributeId;    ///< 属性ID
    ComVal oldValue;         ///< 旧值
    ComVal newValue;         ///< 新值
    bool fromServer;         ///< 是否来自服务端
};

/**
 * @brief 属性变更回调类型
 */
using AttributeChangeCallback = std::function<void(const AttributeChangeEvent&)>;

/**
 * @brief 属性值包装
 *
 * 对应服务端的 ProperValue 结构
 * 包含属性值和脏标志(用于同步)
 */
struct AttributeValue {
    uint32_t id;             ///< 属性ID
    ComVal value;            ///< 属性值
    bool dirty;              ///< 是否需要同步(对应服务端的 bUpdate)

    AttributeValue() : id(0), dirty(false) {}

    AttributeValue(uint32_t attrId, const ComVal& val, bool isDirty = false)
        : id(attrId), value(val), dirty(isDirty) {}

    AttributeValue(const AttributeValue& other)
        : id(other.id), value(other.value), dirty(other.dirty) {}

    AttributeValue& operator=(const AttributeValue& other) {
        if (this != &other) {
            id = other.id;
            value = other.value;
            dirty = other.dirty;
        }
        return *this;
    }

    void reset() {
        id = 0;
        value.reset();
        dirty = false;
    }
};

/**
 * @brief 属性容器
 *
 * 管理一个对象的所有属性，支持：
 * - 属性存取
 * - 变更跟踪(dirty flag)
 * - 变更事件通知
 * - 序列化/反序列化
 */
class AttributeContainer {
public:
    explicit AttributeContainer(uint64_t objectId);
    ~AttributeContainer() = default;

    // 禁止拷贝
    AttributeContainer(const AttributeContainer&) = delete;
    AttributeContainer& operator=(const AttributeContainer&) = delete;

    // mutex makes this type non-movable; state stays behind shared_ptr ownership.
    AttributeContainer(AttributeContainer&&) = delete;
    AttributeContainer& operator=(AttributeContainer&&) = delete;

    /// 获取对象ID
    uint64_t objectId() const { return objectId_; }

    /// 获取属性数量
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return attributes_.size();
    }

    /// 是否有需要同步的属性
    bool hasDirty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& kv : attributes_) {
            if (kv.second.dirty) return true;
        }
        return false;
    }

    // ========== 属性设置 ==========

    /**
     * @brief 设置属性
     * @param attrId 属性ID
     * @param value 新值
     * @param fromServer 是否来自服务端
     * @return 是否设置成功(值发生变化)
     */
    bool setAttribute(uint32_t attrId, const ComVal& value, bool fromServer = false);

    /**
     * @brief 批量设置属性
     * @param attrs 属性列表
     * @param fromServer 是否来自服务端
     * @return 实际变更的属性数量
     */
    int setAttributes(const std::unordered_map<uint32_t, ComVal>& attrs, bool fromServer = false);

    // ========== 属性获取 ==========

    /**
     * @brief 获取属性
     * @param attrId 属性ID
     * @param value 输出值
     * @return 是否存在该属性
     */
    bool getAttribute(uint32_t attrId, ComVal& value) const;

    /**
     * @brief 获取属性(带默认值)
     */
    ComVal getAttribute(uint32_t attrId, const ComVal& defaultValue = ComVal()) const;

    /**
     * @brief 类型安全的获取方法
     */
    int32_t getInt(uint32_t attrId, int32_t defaultValue = 0) const;
    int64_t getInt64(uint32_t attrId, int64_t defaultValue = 0) const;
    uint32_t getUInt(uint32_t attrId, uint32_t defaultValue = 0) const;
    double getDouble(uint32_t attrId, double defaultValue = 0.0) const;
    bool getBool(uint32_t attrId, bool defaultValue = false) const;
    std::string getString(uint32_t attrId, const std::string& defaultValue = "") const;

    // ========== 属性操作 ==========

    /**
     * @brief 增加数值(仅数值类型有效)
     */
    ComVal addValue(uint32_t attrId, const ComVal& delta);

    /**
     * @brief 检查属性是否存在
     */
    bool hasAttribute(uint32_t attrId) const;

    /**
     * @brief 移除属性
     */
    bool removeAttribute(uint32_t attrId);

    /**
     * @brief 清空所有属性
     */
    void clear();

    // ========== 同步相关 ==========

    /**
     * @brief 获取需要同步的属性ID列表
     */
    std::vector<uint32_t> getDirtyAttributes() const;

    /**
     * @brief 获取需要同步的属性
     */
    std::unordered_map<uint32_t, ComVal> getDirtyValues() const;

    /**
     * @brief 清除所有脏标志
     */
    void clearDirtyFlags();

    /**
     * @brief 清除指定属性的脏标志
     */
    void clearDirtyFlag(uint32_t attrId);

    // ========== 序列化 ==========

    /**
     * @brief 序列化所有属性到字节数组(用于全量同步)
     * 格式: [属性数量(2)] [属性ID(4)] [值长度(2)] [值数据]...
     */
    std::vector<uint8_t> serialize() const;

    /**
     * @brief 从字节数组反序列化
     */
    bool deserialize(const std::vector<uint8_t>& data);

    // ========== 事件 ==========

    /**
     * @brief 设置属性变更监听器
     */
    void setChangeListener(AttributeChangeCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        changeListener_ = std::move(callback);
    }

private:
    void notifyChange(uint32_t attrId, const ComVal& oldValue, const ComVal& newValue, bool fromServer);

    uint64_t objectId_;
    std::unordered_map<uint32_t, AttributeValue> attributes_;
    mutable std::mutex mutex_;
    AttributeChangeCallback changeListener_;
};

/**
 * @brief 属性容器管理器
 *
 * 管理多个对象的属性容器
 */
class AttributeContainerManager {
public:
    static AttributeContainerManager& instance();

    /**
     * @brief 获取或创建属性容器
     */
    std::shared_ptr<AttributeContainer> getOrCreate(uint64_t objectId);

    /**
     * @brief 获取属性容器
     */
    std::shared_ptr<AttributeContainer> get(uint64_t objectId);

    /**
     * @brief 移除属性容器
     */
    void remove(uint64_t objectId);

    /**
     * @brief 清空所有容器
     */
    void clear();

    /**
     * @brief 获取所有对象ID
     */
    std::vector<uint64_t> getAllObjectIds() const;

private:
    AttributeContainerManager() = default;
    ~AttributeContainerManager() = default;

    std::unordered_map<uint64_t, std::shared_ptr<AttributeContainer>> containers_;
    mutable std::mutex mutex_;
};

} // namespace game
} // namespace apollo
