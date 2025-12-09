#pragma once

#include <unordered_map>
#include <variant>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>

namespace apollo {

/// 属性值类型
using AttributeValue = std::variant<
    int32_t, int64_t,
    float, double,
    bool, std::string
>;

/// 属性类型枚举
enum class AttributeType : uint8_t {
    INT32 = 0,
    INT64 = 1,
    FLOAT = 2,
    DOUBLE = 3,
    BOOL = 4,
    STRING = 5
};

/// 属性定义
struct AttributeDef {
    uint32_t id;                    // 属性ID
    std::string name;               // 属性名称
    AttributeType type;             // 属性类型
    AttributeValue defaultValue;    // 默认值
    AttributeValue minValue;        // 最小值
    AttributeValue maxValue;        // 最大值
    bool persistent;                // 是否持久化
    bool syncToClient;              // 是否同步到客户端
};

/// 属性变更事件
struct AttributeChangeEvent {
    uint64_t objectId;              // 对象ID
    uint32_t attributeId;           // 属性ID
    AttributeValue oldValue;        // 旧值
    AttributeValue newValue;        // 新值
};

/// 属性容器
class AttributeContainer {
public:
    AttributeContainer() = default;
    explicit AttributeContainer(uint64_t objectId) : objectId_(objectId) {}

    /// 设置属性
    bool SetAttribute(uint32_t attributeId, const AttributeValue& value);

    /// 获取属性
    bool GetAttribute(uint32_t attributeId, AttributeValue& value) const;

    /// 获取属性（带默认值）
    template<typename T>
    T GetAttribute(uint32_t attributeId, const T& defaultValue = T{}) const {
        AttributeValue value;
        if (GetAttribute(attributeId, value)) {
            try {
                return std::get<T>(value);
            } catch (...) {}
        }
        return defaultValue;
    }

    /// 增加属性值
    bool AddAttribute(uint32_t attributeId, const AttributeValue& delta);

    /// 批量设置属性
    void SetAttributes(const std::vector<std::pair<uint32_t, AttributeValue>>& attrs);

    /// 获取所有属性
    const std::unordered_map<uint32_t, AttributeValue>& GetAllAttributes() const;

    /// 清空属性
    void Clear();

    /// 设置属性变更监听器
    void SetChangeListener(std::function<void(const AttributeChangeEvent&)> listener) {
        listener_ = listener;
    }

private:
    uint64_t objectId_;
    std::unordered_map<uint32_t, AttributeValue> attributes_;
    std::function<void(const AttributeChangeEvent&)> listener_;
    mutable std::mutex mutex_;
};

/// 属性管理器
class AttributeManager {
public:
    static AttributeManager& Instance() {
        static AttributeManager instance;
        return instance;
    }

    /// 注册属性定义
    bool RegisterAttribute(const AttributeDef& def);

    /// 获取属性定义
    const AttributeDef* GetAttributeDef(uint32_t attributeId) const;

    /// 获取所有属性定义
    const std::unordered_map<uint32_t, AttributeDef>& GetAllAttributes() const {
        return attributeDefs_;
    }

    /// 创建属性容器
    std::shared_ptr<AttributeContainer> CreateContainer(uint64_t objectId);

    /// 获取属性容器
    std::shared_ptr<AttributeContainer> GetContainer(uint64_t objectId);

    /// 销毁属性容器
    void DestroyContainer(uint64_t objectId);

    /// 从配置加载属性定义
    bool LoadFromConfig(const std::string& configFile);

private:
    AttributeManager() = default;
    ~AttributeManager() = default;

    std::unordered_map<uint32_t, AttributeDef> attributeDefs_;
    std::unordered_map<uint64_t, std::shared_ptr<AttributeContainer>> containers_;
    mutable std::mutex mutex_;
};

/// 便捷的属性访问宏
#define GET_ATTR(container, id, type) \
    (container)->GetAttribute<type>(id)

#define SET_ATTR(container, id, value) \
    (container)->SetAttribute(id, value)

#define ADD_ATTR(container, id, value) \
    (container)->AddAttribute(id, value)

}  // namespace apollo