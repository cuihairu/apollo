/**
 * @file message_codec.cpp
 * @brief 消息编解码器实现
 */

#include "apollo/net/message_codec.h"
#include <google/protobuf/message.h>
#include <algorithm>
#include <cstring>

namespace apollo {
namespace net {

//==============================================================================
// ProtobufMessage 实现
//==============================================================================

ProtobufMessage::ProtobufMessage(std::unique_ptr<::google::protobuf::Message> message,
                                 uint16_t msgId)
    : protoMessage_(std::move(message)), msgId_(msgId) {
}

bool ProtobufMessage::encode(std::vector<uint8_t>& buffer) const {
    if (!protoMessage_) {
        return false;
    }

    size_t size = protoMessage_->ByteSizeLong();
    buffer.resize(size);

    return protoMessage_->SerializeToArray(buffer.data(), static_cast<int>(size));
}

bool ProtobufMessage::decode(const uint8_t* buffer, size_t length) {
    if (!protoMessage_) {
        return false;
    }

    return protoMessage_->ParseFromArray(buffer, static_cast<int>(length));
}

const char* ProtobufMessage::getMessageName() const {
    if (protoMessage_) {
        // GetTypeName() may return std::string_view in newer protobuf versions,
        // so we materialize it into a stable thread-local buffer.
        static thread_local std::string typeName;
        typeName = std::string(protoMessage_->GetTypeName());
        return typeName.c_str();
    }
    return "ProtobufMessage";
}

//==============================================================================
// RawMessage 实现
//==============================================================================

RawMessage::RawMessage(uint16_t msgId, const void* data, size_t length)
    : msgId_(msgId) {
    if (data && length > 0) {
        data_.resize(length);
        std::memcpy(data_.data(), data, length);
    }
}

bool RawMessage::encode(std::vector<uint8_t>& buffer) const {
    buffer = data_;
    return true;
}

bool RawMessage::decode(const uint8_t* buffer, size_t length) {
    if (!buffer || length == 0) {
        return true;
    }

    data_.assign(buffer, buffer + length);
    return true;
}

//==============================================================================
// MessageCodec 实现
//==============================================================================

MessageCodec::MessageCodec()
    : recvBuffer_(64 * 1024)
    , nextSeq_(1) {
}

void MessageCodec::registerHandler(uint16_t msgId, MessageHandler handler) {
    handlers_[msgId] = handler;
}

void MessageCodec::unregisterHandler(uint16_t msgId) {
    handlers_.erase(msgId);
}

bool MessageCodec::encodeMessage(const IMessage& message, std::vector<uint8_t>& output,
                                  uint32_t seq, uint16_t flags) {
    // 序列化消息体
    std::vector<uint8_t> body;
    if (!message.encode(body)) {
        return false;
    }

    // 构建消息头
    MessageHeader header;
    header.msgId = message.getMessageId();
    header.length = static_cast<uint32_t>(body.size());
    header.seq = seq;
    header.flags = flags;

    // 分配输出缓冲区
    output.resize(MessageHeader::SIZE + body.size());

    // 编码消息头
    if (!header.encode(output.data(), output.size())) {
        return false;
    }

    // 复制消息体
    if (!body.empty()) {
        std::memcpy(output.data() + MessageHeader::SIZE, body.data(), body.size());
    }

    return true;
}

bool MessageCodec::encode(uint16_t msgId, const void* data, size_t length,
                          std::vector<uint8_t>& output,
                          uint32_t seq, uint16_t flags) {
    // 构建消息头
    MessageHeader header(msgId, seq, flags);
    header.length = static_cast<uint32_t>(length);

    // 分配输出缓冲区
    output.resize(MessageHeader::SIZE + length);

    // 编码消息头
    if (!header.encode(output.data(), output.size())) {
        return false;
    }

    // 复制消息体
    if (data && length > 0) {
        std::memcpy(output.data() + MessageHeader::SIZE, data, length);
    }

    return true;
}

bool MessageCodec::decode(const uint8_t* data, size_t length,
                          std::vector<std::shared_ptr<IMessage>>& outMessages) {
    if (!data || length == 0) {
        return false;
    }

    // 写入接收缓冲区
    size_t written = recvBuffer_.write(data, length);
    if (written != length) {
        // 缓冲区溢出
        recvBuffer_.clear();
        return false;
    }

    // 处理缓冲区中的消息
    while (recvBuffer_.availableRead() >= MessageHeader::SIZE) {
        // Peek 消息头
        uint8_t headerBytes[MessageHeader::SIZE];
        size_t peeked = recvBuffer_.peek(headerBytes, MessageHeader::SIZE);

        MessageHeader header;
        if (!header.decode(headerBytes, peeked)) {
            recvBuffer_.clear();
            return false;
        }

        // 检查消息长度
        if (header.length > 10 * 1024 * 1024) {  // 10MB 限制
            recvBuffer_.clear();
            return false;
        }

        size_t totalLength = MessageHeader::SIZE + header.length;

        // 检查是否接收完整
        if (recvBuffer_.availableRead() < totalLength) {
            // 数据不完整，等待更多数据
            break;
        }

        // 跳过消息头
        recvBuffer_.skip(MessageHeader::SIZE);

        // 读取消息体
        if (header.length > 0) {
            std::vector<uint8_t> body(header.length);
            recvBuffer_.read(body.data(), header.length);

            // 创建消息对象
            auto message = std::make_shared<RawMessage>(header.msgId, body.data(), body.size());
            outMessages.push_back(message);

            // 调用处理器
            auto it = handlers_.find(header.msgId);
            if (it != handlers_.end()) {
                it->second(message);
            } else if (defaultHandler_) {
                defaultHandler_(message);
            }
        }
    }

    return true;
}

size_t MessageCodec::handleReceivedData(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return 0;
    }

    std::vector<std::shared_ptr<IMessage>> messages;
    if (!decode(data, length, messages)) {
        return 0;
    }

    return messages.size();
}

bool MessageCodec::processCompleteMessage(const MessageHeader&) {
    // 这个方法由 decode 内部调用处理
    return true;
}

//==============================================================================
// MessageFactory 实现
//==============================================================================

MessageFactory& MessageFactory::instance() {
    static MessageFactory instance;
    return instance;
}

void MessageFactory::registerMessage(uint16_t msgId, Creator creator) {
    creators_[msgId] = creator;
}

std::shared_ptr<IMessage> MessageFactory::createMessage(uint16_t msgId) {
    auto it = creators_.find(msgId);
    if (it != creators_.end()) {
        return it->second();
    }
    return nullptr;
}

} // namespace net
} // namespace apollo
