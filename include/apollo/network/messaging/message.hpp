#pragma once

#include "apollo/network/transport/net_common.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>

namespace apollo::net {

/// 消息头（4字节）
struct MessageHeader {
    uint32_t length : 24;    // 消息长度（最大16MB）
    uint32_t type : 8;       // 消息类型（0-255）

    uint32_t GetLength() const { return length; }
    uint32_t GetType() const { return type; }
    void SetLength(uint32_t len) { length = len & 0xFFFFFF; }
    void SetType(uint32_t t) { type = t & 0xFF; }
};

/// 消息基类
class Message {
public:
    using Ptr = std::shared_ptr<Message>;

    Message() = default;
    virtual ~Message() = default;

    virtual uint32_t GetType() const = 0;
    virtual bool Serialize(Buffer& buffer) const = 0;
    virtual bool Deserialize(const Buffer& buffer) = 0;
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