#pragma once

#include "apollo/network/transport/socket.hpp"  // Buffer
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace apollo::net {

/// 消息头（4字节）
///
/// 布局（host endian 的 32-bit packed 值）：
/// - low  24 bits: length（最大 16MB）
/// - high  8 bits: type（0-255）
struct MessageHeader {
    uint32_t packed = 0;

    uint32_t GetLength() const { return packed & 0x00FFFFFFu; }
    uint32_t GetType() const { return (packed >> 24) & 0xFFu; }

    void SetLength(uint32_t len) {
        packed = (packed & 0xFF000000u) | (len & 0x00FFFFFFu);
    }

    void SetType(uint32_t t) {
        packed = (packed & 0x00FFFFFFu) | ((t & 0xFFu) << 24);
    }
};

static_assert(sizeof(MessageHeader) == 4, "MessageHeader must be 4 bytes");

/// 消息基类
class Message {
public:
    using Ptr = std::shared_ptr<Message>;

    Message() = default;
    virtual ~Message() = default;

    virtual uint32_t GetType() const = 0;
    virtual bool Serialize(Buffer& buffer) const = 0;
    virtual bool Deserialize(Buffer& buffer) = 0;
    virtual std::shared_ptr<Message> Clone() const = 0;
};

/// 消息处理器接口
class IMessageHandler {
public:
    virtual ~IMessageHandler() = default;
    virtual void HandleMessage(const Message::Ptr& msg) = 0;
};

/// 消息分发器
class MessageDispatcher {
public:
    static MessageDispatcher& Instance() {
        static MessageDispatcher instance;
        return instance;
    }

    /// 注册消息处理器
    bool RegisterHandler(uint32_t msgType, std::shared_ptr<IMessageHandler> handler);

    /// 分发消息
    void Dispatch(const Message::Ptr& msg);

    /// 处理原始数据
    bool HandleRawData(const void* data, size_t len);

private:
    std::unordered_map<uint32_t, std::shared_ptr<IMessageHandler>> handlers_;
    std::mutex mutex_;
};

/// 消息工厂
class MessageFactory {
public:
    static MessageFactory& Instance() {
        static MessageFactory instance;
        return instance;
    }

    /// 注册消息类型
    template<typename T>
    void Register(uint32_t msgType) {
        creators_[msgType] = []() { return std::make_shared<T>(); };
    }

    /// 创建消息
    Message::Ptr Create(uint32_t msgType) {
        auto it = creators_.find(msgType);
        if (it != creators_.end()) {
            return it->second();
        }
        return nullptr;
    }

private:
    std::unordered_map<uint32_t, std::function<Message::Ptr()>> creators_;
};

/// 简单的消息定义宏
#define DECLARE_MESSAGE(MessageType, MessageClass) \
public: \
    static const uint32_t TYPE = MessageType; \
    uint32_t GetType() const override { return TYPE; } \
    std::shared_ptr<Message> Clone() const override { \
        return std::make_shared<MessageClass>(*this); \
    }

}  // namespace apollo::net
