#pragma once

#include "message.hpp"
#include <google/protobuf/message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <memory>

namespace apollo::net {

/// Protobuf消息基类
class ProtobufMessage : public Message {
public:
    using Ptr = std::shared_ptr<ProtobufMessage>;

    ProtobufMessage() = default;
    ~ProtobufMessage() override = default;

    /// 获取Protobuf消息
    virtual google::protobuf::Message* GetProtoMessage() = 0;
    virtual const google::protobuf::Message* GetProtoMessage() const = 0;

    /// 序列化到Buffer
    bool Serialize(Buffer& buffer) const override {
        const google::protobuf::Message* msg = GetProtoMessage();
        if (!msg) {
            return false;
        }

        // 计算序列化后的长度
        size_t size = msg->ByteSizeLong();
        if (size > 0xFFFFFF) {  // 16MB limit
            return false;
        }

        // 写入消息头
        MessageHeader header;
        header.SetLength(static_cast<uint32_t>(size));
        header.SetType(GetType());

        buffer.Write(&header, sizeof(header));

        // 写入消息体
        if (size > 0) {
            std::vector<uint8_t> data(size);
            if (!msg->SerializeToArray(data.data(), static_cast<int>(size))) {
                return false;
            }
            buffer.Write(data.data(), size);
        }

        return true;
    }

    /// 从Buffer反序列化
    bool Deserialize(Buffer& buffer) override {
        google::protobuf::Message* msg = GetProtoMessage();
        if (!msg) {
            return false;
        }

        // 读取消息头
        MessageHeader header;
        if (buffer.Read(&header, sizeof(header)) != sizeof(header)) {
            return false;
        }

        // 验证消息类型
        if (header.GetType() != GetType()) {
            return false;
        }

        // 读取消息体
        uint32_t length = header.GetLength();
        if (length == 0) {
            return true;  // 空消息
        }

        std::vector<uint8_t> data(length);
        if (buffer.Read(data.data(), length) != length) {
            return false;
        }

        return msg->ParseFromArray(data.data(), static_cast<int>(length));
    }
};

/// 模板类，用于包装任何Protobuf消息
template<typename T>
class ProtobufMessageWrapper : public ProtobufMessage {
public:
    static_assert(std::is_base_of<google::protobuf::Message, T>::value,
                  "T must be a protobuf message");

    using Ptr = std::shared_ptr<ProtobufMessageWrapper<T>>;

    ProtobufMessageWrapper() = default;
    explicit ProtobufMessageWrapper(const T& msg) : message_(msg) {}
    explicit ProtobufMessageWrapper(T&& msg) : message_(std::move(msg)) {}

    uint32_t GetType() const override {
        // 使用消息类型的哈希值作为类型ID
        static const uint32_t type = static_cast<uint32_t>(
            std::hash<std::string>{}(std::string(T::descriptor()->full_name())) & 0xFFu
        );
        return type;
    }

    std::shared_ptr<Message> Clone() const override {
        return std::make_shared<ProtobufMessageWrapper<T>>(*this);
    }

    google::protobuf::Message* GetProtoMessage() override {
        return &message_;
    }

    const google::protobuf::Message* GetProtoMessage() const override {
        return &message_;
    }

    /// 获取具体的消息类型
    T* GetTypedMessage() { return &message_; }
    const T* GetTypedMessage() const { return &message_; }

    /// 方便的构造函数
    template<typename... Args>
    static Ptr Create(Args&&... args) {
        return std::make_shared<ProtobufMessageWrapper<T>>(std::forward<Args>(args)...);
    }

private:
    T message_;
};

/// Protobuf消息工厂
class ProtobufMessageFactory {
public:
    static ProtobufMessageFactory& Instance() {
        static ProtobufMessageFactory instance;
        return instance;
    }

    /// 注册消息类型
    template<typename T>
    void Register() {
        static_assert(std::is_base_of<google::protobuf::Message, T>::value,
                      "T must be a protobuf message");

        uint32_t type = static_cast<uint32_t>(
            std::hash<std::string>{}(std::string(T::descriptor()->full_name())) & 0xFFu
        );

        creators_[type] = []() -> Message::Ptr {
            return std::make_shared<ProtobufMessageWrapper<T>>();
        };

        names_[type] = std::string(T::descriptor()->full_name());
    }

    /// 创建消息
    Message::Ptr Create(uint32_t msgType) {
        auto it = creators_.find(msgType);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    /// 获取消息类型名称
    std::string GetMessageName(uint32_t msgType) const {
        auto it = names_.find(msgType);
        if (it != names_.end()) {
            return it->second;
        }
        return "";
    }

    /// 获取已注册的消息类型
    std::vector<uint32_t> GetRegisteredTypes() const {
        std::vector<uint32_t> types;
        types.reserve(creators_.size());

        for (const auto& pair : creators_) {
            types.push_back(pair.first);
        }

        return types;
    }

private:
    std::unordered_map<uint32_t, std::function<Message::Ptr()>> creators_;
    std::unordered_map<uint32_t, std::string> names_;
};

/// Protobuf消息编解码器
class ProtobufCodec {
public:
    /// 编码消息
    static bool Encode(const google::protobuf::Message& message,
                       uint32_t msgType,
                       Buffer& buffer) {
        size_t size = message.ByteSizeLong();
        if (size > 0xFFFFFF) {
            return false;
        }

        MessageHeader header;
        header.SetLength(static_cast<uint32_t>(size));
        header.SetType(msgType);

        buffer.Write(&header, sizeof(header));

        if (size > 0) {
            std::vector<uint8_t> data(size);
            if (!message.SerializeToArray(data.data(), static_cast<int>(size))) {
                return false;
            }
            buffer.Write(data.data(), size);
        }

        return true;
    }

    /// 解码消息
    static std::pair<uint32_t, std::vector<uint8_t>> Decode(Buffer& buffer) {
        MessageHeader header;
        if (buffer.GetReadableSize() < sizeof(header)) {
            return {0, {}};
        }

        std::memcpy(&header, buffer.Peek(), sizeof(header));

        uint32_t totalSize = sizeof(header) + header.GetLength();
        if (buffer.GetReadableSize() < totalSize) {
            return {0, {}};
        }

        buffer.Read(&header, sizeof(header));

        std::vector<uint8_t> data;
        if (header.GetLength() > 0) {
            data.resize(header.GetLength());
            buffer.Read(data.data(), header.GetLength());
        }

        return {header.GetType(), std::move(data)};
    }
};

/// 注册Protobuf消息的便捷宏
#define REGISTER_PROTOBUF_MESSAGE(MessageClass) \
    ProtobufMessageFactory::Instance().Register<MessageClass>()

/// 创建Protobuf消息的便捷宏
#define CREATE_PROTOBUF_MESSAGE(MessageClass) \
    std::make_shared<ProtobufMessageWrapper<MessageClass>>()

/// 获取消息类型ID的便捷函数
template<typename T>
inline uint32_t GetMessageType() {
    static_assert(std::is_base_of<google::protobuf::Message, T>::value,
                  "T must be a protobuf message");
    return static_cast<uint32_t>(
        std::hash<std::string>{}(std::string(T::descriptor()->full_name())) & 0xFFu
    );
}

}  // namespace apollo::net
