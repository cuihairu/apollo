#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <stdexcept>
#include <cstring>

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace apollo {
namespace actor {

//==============================================================================
// 消息序列化格式
//==============================================================================

enum class MessageFormat : uint8_t {
    FlatBuffers = 0,  // 默认：零拷贝，高性能
    JSON         = 1,  // 调试：人类可读
    Protobuf     = 2,  // 兼容：外部系统集成
    Binary       = 3,  // 原始：最高性能（POD 类型）
    Auto         = 255 // 自动检测
};

// 格式转字符串
inline const char* formatToString(MessageFormat format) {
    switch (format) {
        case MessageFormat::FlatBuffers: return "flatbuffers";
        case MessageFormat::JSON:         return "json";
        case MessageFormat::Protobuf:     return "protobuf";
        case MessageFormat::Binary:       return "binary";
        default: return "auto";
    }
}

//==============================================================================
// 消息类型标识（用于路由和反序列化）
//==============================================================================

struct MessageType {
    std::string name;       // 消息类型名
    uint32_t hash;          // 类型哈希（用于快速匹配）
    MessageFormat format;   // 序列化格式

    MessageType() : hash(0), format(MessageFormat::FlatBuffers) {}

    MessageType(const std::string& n, MessageFormat f = MessageFormat::FlatBuffers)
        : name(n), format(f) {
        hash = std::hash<std::string>{}(name);
    }

    bool operator==(const MessageType& other) const {
        return hash == other.hash;
    }
};

//==============================================================================
// 序列化接口
//==============================================================================

class IMessageSerializer {
public:
    virtual ~IMessageSerializer() = default;

    // 序列化
    virtual std::vector<uint8_t> serialize(const void* msg) const = 0;

    // 反序列化
    virtual bool deserialize(const std::vector<uint8_t>& data, void* out) const = 0;

    // 获取消息类型
    virtual MessageType getType() const = 0;

    // 创建空消息对象
    virtual void* createMessage() const = 0;

    // 销毁消息对象
    virtual void destroyMessage(void* msg) const = 0;
};

// 类型安全的序列化器基类
template<typename T>
class MessageSerializer : public IMessageSerializer {
public:
    // 类型安全的序列化
    virtual std::vector<uint8_t> serialize(const T& msg) const = 0;

    // 类型安全的反序列化
    virtual T deserialize(const std::vector<uint8_t>& data) const = 0;

    // IMessageSerializer 接口实现
    std::vector<uint8_t> serialize(const void* msg) const override {
        return serialize(*static_cast<const T*>(msg));
    }

    bool deserialize(const std::vector<uint8_t>& data, void* out) const override {
        try {
            *static_cast<T*>(out) = deserialize(data);
            return true;
        } catch (...) {
            return false;
        }
    }

    void* createMessage() const override {
        return new T();
    }

    void destroyMessage(void* msg) const override {
        delete static_cast<T*>(msg);
    }
};

//==============================================================================
// FlatBuffers 序列化器（默认）
//==============================================================================

template<typename T>
class FlatBuffersSerializer : public MessageSerializer<T> {
public:
    std::vector<uint8_t> serialize(const T& msg) const override {
        // 用户需要为每个消息类型特化此函数
        // 或者使用 FlatBuffers 的自动生成代码
        return T::serialize(msg);
    }

    T deserialize(const std::vector<uint8_t>& data) const override {
        return T::deserialize(data);
    }

    MessageType getType() const override {
        return MessageType(T::typeName(), MessageFormat::FlatBuffers);
    }
};

//==============================================================================
// JSON 序列化器（用于调试）
//==============================================================================

template<typename T>
class JsonSerializer : public MessageSerializer<T> {
public:
    std::vector<uint8_t> serialize(const T& msg) const override {
        // 使用 nlohmann/json 或其他 JSON 库
#ifdef HAVE_NLOHMANN_JSON
        auto json = nlohmann::json(msg);
        std::string str = json.dump();
        return std::vector<uint8_t>(str.begin(), str.end());
#else
        (void)msg;
        throw std::runtime_error("JSON serializer not available (missing nlohmann-json)");
#endif
    }

    T deserialize(const std::vector<uint8_t>& data) const override {
#ifdef HAVE_NLOHMANN_JSON
        std::string str(data.begin(), data.end());
        auto json = nlohmann::json::parse(str);
        return json.get<T>();
#else
        (void)data;
        throw std::runtime_error("JSON serializer not available (missing nlohmann-json)");
#endif
    }

    MessageType getType() const override {
        return MessageType(T::typeName(), MessageFormat::JSON);
    }
};

//==============================================================================
// 原始二进制序列化器（POD 类型）
//==============================================================================

template<typename T>
class BinarySerializer : public MessageSerializer<T> {
public:
    std::vector<uint8_t> serialize(const T& msg) const override {
        const uint8_t* begin = reinterpret_cast<const uint8_t*>(&msg);
        const uint8_t* end = begin + sizeof(T);
        return std::vector<uint8_t>(begin, end);
    }

    T deserialize(const std::vector<uint8_t>& data) const override {
        if (data.size() != sizeof(T)) {
            throw std::runtime_error("Binary deserialize size mismatch");
        }
        T result;
        std::memcpy(&result, data.data(), sizeof(T));
        return result;
    }

    MessageType getType() const override {
        return MessageType(T::typeName(), MessageFormat::Binary);
    }
};

//==============================================================================
// 消息包装器（支持任意格式）
//==============================================================================

class Message {
public:
    Message() = default;

    template<typename T>
    Message(T&& msg, MessageFormat format = MessageFormat::Auto)
        : format_(format == MessageFormat::Auto ? getDefaultFormat<T>() : format) {

        if (format_ == MessageFormat::FlatBuffers) {
            serializer_ = std::make_shared<FlatBuffersSerializer<std::decay_t<T>>>();
        } else if (format_ == MessageFormat::JSON) {
            serializer_ = std::make_shared<JsonSerializer<std::decay_t<T>>>();
        } else if (format_ == MessageFormat::Binary) {
            serializer_ = std::make_shared<BinarySerializer<std::decay_t<T>>>();
        }

        type_ = serializer_->getType();
        data_ = serializer_->serialize(msg);
    }

    // 获取消息类型
    const MessageType& getType() const { return type_; }

    // 获取序列化格式
    MessageFormat getFormat() const { return format_; }

    // 获取原始数据
    const std::vector<uint8_t>& getData() const { return data_; }

    // 反序列化为指定类型
    template<typename T>
    T as() const {
        if (!serializer_) {
            throw std::runtime_error("No serializer available");
        }
        auto typed = static_cast<MessageSerializer<T>*>(serializer_.get());
        return typed->deserialize(data_);
    }

    // 检查是否为指定类型
    template<typename T>
    bool is() const {
        return type_.name == T::typeName();
    }

    // 转换为其他格式
    Message convertTo(MessageFormat targetFormat) const;

    // 序列化为字节
    std::vector<uint8_t> toBytes() const;

    // 从字节解析
    static Message fromBytes(const std::vector<uint8_t>& bytes);

private:
    template<typename T>
    static MessageFormat getDefaultFormat() {
        // 默认使用 FlatBuffers
        return MessageFormat::FlatBuffers;
    }

    MessageType type_;
    MessageFormat format_;
    std::vector<uint8_t> data_;
    std::shared_ptr<IMessageSerializer> serializer_;
};

//==============================================================================
// 消息注册表（类型名 <-> 序列化器）
//==============================================================================

class MessageRegistry {
public:
    static MessageRegistry& instance() {
        static MessageRegistry instance;
        return instance;
    }

    // 注册消息类型和序列化器
    template<typename T>
    void registerType() {
        std::type_index idx(typeid(T));
        serializers_[idx] = std::make_shared<FlatBuffersSerializer<T>>();
    }

    // 获取序列化器
    std::shared_ptr<IMessageSerializer> getSerializer(const std::string& typeName) {
        auto it = nameSerializers_.find(typeName);
        return it != nameSerializers_.end() ? it->second : nullptr;
    }

private:
    MessageRegistry() = default;

    std::unordered_map<std::type_index, std::shared_ptr<IMessageSerializer>> serializers_;
    std::unordered_map<std::string, std::shared_ptr<IMessageSerializer>> nameSerializers_;
};

//==============================================================================
// 消息类型声明宏
//==============================================================================

// 声明消息类型（FlatBuffers 风格）
#define APOLLO_MESSAGE_TYPE(Type) \
    static const char* typeName() { return #Type; } \
    static std::vector<uint8_t> serialize(const Type& msg); \
    static Type deserialize(const std::vector<uint8_t>& data);

// 声明 POD 消息类型（二进制序列化）
#define APOLLO_POD_MESSAGE(Type) \
    static const char* typeName() { return #Type; } \
    static constexpr bool isPod = true;

} // namespace actor
} // namespace apollo
