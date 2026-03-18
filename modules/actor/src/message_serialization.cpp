/**
 * @file message_serialization.cpp
 * @brief 消息序列化实现
 */

#include "apollo/actor/message.h"
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <type_traits>
#include <stdexcept>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace apollo {
namespace actor {

//==============================================================================
// Message 实现
//==============================================================================

namespace {

#pragma pack(push, 1)
struct MessageWireHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
    uint32_t typeHash;
    uint8_t format;
    uint8_t reserved[7];
};
#pragma pack(pop)

static constexpr uint32_t MESSAGE_MAGIC = 0x41435452;        // "ACTR"
static constexpr uint32_t MESSAGE_VERSION = 1;

} // namespace

std::vector<uint8_t> Message::toBytes() const {
    MessageWireHeader header{};
    header.magic = MESSAGE_MAGIC;
    header.version = MESSAGE_VERSION;
    header.length = static_cast<uint32_t>(data_.size());
    header.format = static_cast<uint8_t>(format_);
    header.typeHash = std::hash<std::string>{}(type_.name);

    std::vector<uint8_t> out(sizeof(MessageWireHeader) + data_.size());
    std::memcpy(out.data(), &header, sizeof(header));
    if (!data_.empty()) {
        std::memcpy(out.data() + sizeof(MessageWireHeader), data_.data(), data_.size());
    }
    return out;
}

Message Message::fromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < sizeof(MessageWireHeader)) {
        return Message{};
    }

    MessageWireHeader header{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != MESSAGE_MAGIC || header.version != MESSAGE_VERSION) {
        return Message{};
    }
    if (bytes.size() != sizeof(MessageWireHeader) + header.length) {
        return Message{};
    }

    Message msg;
    msg.format_ = static_cast<MessageFormat>(header.format);
    if (header.length > 0) {
        msg.data_.assign(bytes.begin() + sizeof(MessageWireHeader), bytes.end());
    }

    // TODO: 根据 typeHash 查找类型
    msg.type_.name = "";  // 需要类型注册表
    msg.type_.hash = header.typeHash;
    msg.type_.format = msg.format_;

    return msg;
}

Message Message::convertTo(MessageFormat targetFormat) const {
    if (format_ == targetFormat) {
        return *this;
    }

    // TODO: 实现格式转换
    // 目前只支持相同格式
    return *this;
}

//==============================================================================
// JSON 序列化器实现
//==============================================================================

namespace json {

// 简单的 JSON 转义
std::string escape(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c < 32) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    result += buf;
                } else {
                    result += c;
                }
        }
    }
    return result;
}

// 将任意类型序列化为 JSON（简化版）
template<typename T>
std::string toJson(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + escape(value) + "\"";
    } else if constexpr (std::is_same_v<T, int> ||
                         std::is_same_v<T, int32_t> ||
                         std::is_same_v<T, int64_t> ||
                         std::is_same_v<T, uint32_t> ||
                         std::is_same_v<T, uint64_t>) {
        return std::to_string(value);
    } else if constexpr (std::is_same_v<T, bool>) {
        return value ? "true" : "false";
    } else {
        return "{}";  // TODO: 完整实现
    }
}

} // namespace json

//==============================================================================
// 二进制序列化器实现
//==============================================================================

namespace binary {

// 将 POD 类型序列化为字节
template<typename T>
std::vector<uint8_t> toBytes(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable for binary serialization");

    const uint8_t* begin = reinterpret_cast<const uint8_t*>(&value);
    const uint8_t* end = begin + sizeof(T);
    return std::vector<uint8_t>(begin, end);
}

// 从字节反序列化 POD 类型
template<typename T>
T fromBytes(const std::vector<uint8_t>& data) {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Type must be trivially copyable for binary serialization");

    if (data.size() != sizeof(T)) {
        throw std::runtime_error("Binary data size mismatch");
    }

    T result;
    std::memcpy(&result, data.data(), sizeof(T));
    return result;
}

} // namespace binary

//==============================================================================
// 类型注册表（用于反序列化）
//==============================================================================

namespace {

class TypeRegistry {
public:
    static TypeRegistry& instance() {
        static TypeRegistry registry;
        return registry;
    }

    // 注册类型
    void registerType(const std::string& name,
                     std::function<std::shared_ptr<void>()> factory,
                     std::function<std::vector<uint8_t>(const void*)> serializer,
                     std::function<bool(const std::vector<uint8_t>&, void*)> deserializer) {
        TypeInfo info;
        info.name = name;
        info.factory = std::move(factory);
        info.serializer = std::move(serializer);
        info.deserializer = std::move(deserializer);

        types_[name] = std::move(info);
    }

    // 创建对象
    std::shared_ptr<void> create(const std::string& name) const {
        auto it = types_.find(name);
        if (it != types_.end() && it->second.factory) {
            return it->second.factory();
        }
        return nullptr;
    }

    // 序列化
    std::vector<uint8_t> serialize(const std::string& name, const void* obj) const {
        auto it = types_.find(name);
        if (it != types_.end() && it->second.serializer) {
            return it->second.serializer(obj);
        }
        return {};
    }

    // 反序列化
    bool deserialize(const std::string& name,
                    const std::vector<uint8_t>& data,
                    void* obj) const {
        auto it = types_.find(name);
        if (it != types_.end() && it->second.deserializer) {
            return it->second.deserializer(data, obj);
        }
        return false;
    }

private:
    struct TypeInfo {
        std::string name;
        std::function<std::shared_ptr<void>()> factory;
        std::function<std::vector<uint8_t>(const void*)> serializer;
        std::function<bool(const std::vector<uint8_t>&, void*)> deserializer;
    };

    std::unordered_map<std::string, TypeInfo> types_;
};

} // anonymous namespace

//==============================================================================
// 全局序列化函数
//==============================================================================

namespace serialization {

// 快捷函数：序列化为 JSON
template<typename T>
std::string toJson(const T& value) {
    std::ostringstream oss;
    // 简化实现
    if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
        oss << value;
    } else if constexpr (std::is_same_v<T, std::string>) {
        oss << "\"" << json::escape(value) << "\"";
    } else {
        oss << "{}";
    }
    return oss.str();
}

// 快捷函数：从 JSON 解析
template<typename T>
T fromJson(const std::string& json) {
    (void)json;
    T result;
    // TODO: 完整 JSON 解析
    return result;
}

// 快捷函数：序列化为二进制
template<typename T>
std::vector<uint8_t> toBinary(const T& value) {
    return binary::toBytes(value);
}

// 快捷函数：从二进制解析
template<typename T>
T fromBinary(const std::vector<uint8_t>& data) {
    return binary::fromBytes<T>(data);
}

} // namespace serialization

} // namespace actor
} // namespace apollo
