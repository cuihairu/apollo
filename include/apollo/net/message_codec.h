#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include "apollo/utils/loop_buffer.h"

namespace apollo {
namespace net {

/// 消息异常
class MessageException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// 消息头
struct MessageHeader {
    uint32_t length;      // 消息长度（不含头）
    uint16_t msgId;       // 消息 ID
    uint32_t seq;         // 序列号
    uint16_t flags;       // 标志位

    enum Flags {
        None = 0,
        Compressed = 0x0001,
        Encrypted = 0x0002,
        Urgent = 0x0004,
        Fragmented = 0x0008
    };

    static constexpr size_t SIZE = 12;  // 4 + 2 + 4 + 2

    MessageHeader()
        : length(0), msgId(0), seq(0), flags(None) {}

    MessageHeader(uint16_t id, uint32_t s = 0, uint16_t f = None)
        : length(0), msgId(id), seq(s), flags(f) {}

    /// 序列化到缓冲区
    bool encode(uint8_t* buffer, size_t size) const {
        if (size < SIZE) {
            return false;
        }

        // 大端序写入
        buffer[0] = (length >> 24) & 0xFF;
        buffer[1] = (length >> 16) & 0xFF;
        buffer[2] = (length >> 8) & 0xFF;
        buffer[3] = length & 0xFF;

        buffer[4] = (msgId >> 8) & 0xFF;
        buffer[5] = msgId & 0xFF;

        buffer[6] = (seq >> 24) & 0xFF;
        buffer[7] = (seq >> 16) & 0xFF;
        buffer[8] = (seq >> 8) & 0xFF;
        buffer[9] = seq & 0xFF;

        buffer[10] = (flags >> 8) & 0xFF;
        buffer[11] = flags & 0xFF;

        return true;
    }

    /// 从缓冲区解码
    bool decode(const uint8_t* buffer, size_t size) {
        if (size < SIZE) {
            return false;
        }

        length = (static_cast<uint32_t>(buffer[0]) << 24) |
                 (static_cast<uint32_t>(buffer[1]) << 16) |
                 (static_cast<uint32_t>(buffer[2]) << 8) |
                 static_cast<uint32_t>(buffer[3]);

        msgId = (static_cast<uint16_t>(buffer[4]) << 8) |
                static_cast<uint16_t>(buffer[5]);

        seq = (static_cast<uint32_t>(buffer[6]) << 24) |
              (static_cast<uint32_t>(buffer[7]) << 16) |
              (static_cast<uint32_t>(buffer[8]) << 8) |
              static_cast<uint32_t>(buffer[9]);

        flags = (static_cast<uint16_t>(buffer[10]) << 8) |
                static_cast<uint16_t>(buffer[11]);

        return true;
    }
};

/// 消息接口
class IMessage {
public:
    virtual ~IMessage() = default;

    /// 获取消息 ID
    virtual uint16_t getMessageId() const = 0;

    /// 序列化
    virtual bool encode(std::vector<uint8_t>& buffer) const = 0;

    /// 反序列化
    virtual bool decode(const uint8_t* buffer, size_t length) = 0;

    /// 获取消息名称（用于调试）
    virtual const char* getMessageName() const = 0;
};

/// 消息包装器
template<typename T>
class MessageWrapper : public IMessage {
public:
    explicit MessageWrapper(const T& msg = T()) : message_(msg) {}

    T& get() { return message_; }
    const T& get() const { return message_; }

private:
    T message_;
};

/// Protobuf 消息类型（前向声明）
namespace google {
namespace protobuf {
class Message;
}
}

/// Protobuf 消息包装器
class ProtobufMessage : public IMessage {
public:
    explicit ProtobufMessage(std::unique_ptr<google::protobuf::Message> message,
                            uint16_t msgId);

    uint16_t getMessageId() const override { return msgId_; }
    bool encode(std::vector<uint8_t>& buffer) const override;
    bool decode(const uint8_t* buffer, size_t length) override;
    const char* getMessageName() const override;

    google::protobuf::Message* getProtoMessage() { return protoMessage_.get(); }
    const google::protobuf::Message* getProtoMessage() const { return protoMessage_.get(); }

private:
    std::unique_ptr<google::protobuf::Message> protoMessage_;
    uint16_t msgId_;
};

/// 原始字节消息
class RawMessage : public IMessage {
public:
    RawMessage(uint16_t msgId, const void* data = nullptr, size_t length = 0);

    uint16_t getMessageId() const override { return msgId_; }
    bool encode(std::vector<uint8_t>& buffer) const override;
    bool decode(const uint8_t* buffer, size_t length) override;
    const char* getMessageName() const override { return "RawMessage"; }

    const std::vector<uint8_t>& getData() const { return data_; }
    std::vector<uint8_t>& getData() { return data_; }

private:
    uint16_t msgId_;
    std::vector<uint8_t> data_;
};

/// 消息处理器
using MessageHandler = std::function<void(std::shared_ptr<IMessage>)>;

/// 消息编解码器
class MessageCodec {
public:
    MessageCodec();
    ~MessageCodec() = default;

    /// 注册消息处理器
    void registerHandler(uint16_t msgId, MessageHandler handler);

    /// 取消注册
    void unregisterHandler(uint16_t msgId);

    /// 编码消息（添加消息头）
    bool encodeMessage(const IMessage& message, std::vector<uint8_t>& output,
                       uint32_t seq = 0, uint16_t flags = MessageHeader::None);

    /// 编码原始数据（添加消息头）
    bool encode(uint16_t msgId, const void* data, size_t length,
                std::vector<uint8_t>& output, uint32_t seq = 0,
                uint16_t flags = MessageHeader::None);

    /// 解码数据流
    bool decode(const uint8_t* data, size_t length,
                std::vector<std::shared_ptr<IMessage>>& outMessages);

    /// 处理接收到的数据
    size_t handleReceivedData(const uint8_t* data, size_t length);

    /// 设置默认消息处理器
    void setDefaultHandler(MessageHandler handler) {
        defaultHandler_ = handler;
    }

    /// 获取接收缓冲区
    ByteLoopBuffer& getRecvBuffer() { return recvBuffer_; }

    /// 清空接收缓冲区
    void clear() { recvBuffer_.clear(); }

    /// 设置下一个序列号
    void setNextSequence(uint32_t seq) { nextSeq_ = seq; }

    /// 获取下一个序列号
    uint32_t getNextSequence() const { return nextSeq_; }

    /// 分配序列号
    uint32_t allocateSequence() { return nextSeq_++; }

private:
    std::unordered_map<uint16_t, MessageHandler> handlers_;
    MessageHandler defaultHandler_;
    ByteLoopBuffer recvBuffer_;
    uint32_t nextSeq_;

    // 处理单个完整消息
    bool processCompleteMessage(const MessageHeader& header);
};

/// 消息工厂
class MessageFactory {
public:
    using Creator = std::function<std::shared_ptr<IMessage>()>;

    static MessageFactory& instance();

    /// 注册消息创建器
    void registerMessage(uint16_t msgId, Creator creator);

    /// 创建消息
    std::shared_ptr<IMessage> createMessage(uint16_t msgId);

    /// 注册 Protobuf 消息
    template<typename T>
    void registerProtobufMessage(uint16_t msgId) {
        registerMessage(msgId, [msgId]() {
            return std::make_shared<ProtobufMessage>(
                std::make_unique<T>(), msgId);
        });
    }

private:
    MessageFactory() = default;

    std::unordered_map<uint16_t, Creator> creators_;
};

} // namespace net
} // namespace apollo
