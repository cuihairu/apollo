#pragma once

#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/nng_wrapper.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <memory>

namespace apollo {
namespace protocol {

//==============================================================================
// 消息编解码器
//==============================================================================

class MessageCodec {
public:
    //==========================================================================
    // 编码
    //==========================================================================

    // 编码消息头
    static std::vector<uint8_t> encodeHeader(const MessageHeader& header) {
        std::vector<uint8_t> buffer(sizeof(MessageHeader));
        std::memcpy(buffer.data(), &header, sizeof(MessageHeader));
        return buffer;
    }

    // 编码消息（自动添加头）
    template<typename T>
    static std::vector<uint8_t> encode(const T& msg, SessionID sessionId = 0) {
        // 序列化消息体
        auto body = encodeBody(msg);

        // 构建消息头
        MessageHeader header;
        header.magic = MessageHeader::MAGIC;
        header.version = MessageHeader::CURRENT_VERSION;
        header.type = static_cast<uint16_t>(T::TYPE);
        header.length = static_cast<uint32_t>(body.size());
        header.sequence = nextSequence();
        header.sessionId = sessionId;

        // 合并头和体
        std::vector<uint8_t> result;
        result.reserve(sizeof(MessageHeader) + body.size());
        auto headerData = encodeHeader(header);
        result.insert(result.end(), headerData.begin(), headerData.end());
        result.insert(result.end(), body.begin(), body.end());

        return result;
    }

    // 编码消息体
    static std::vector<uint8_t> encodeBody(const LoginRequest& msg);
    static std::vector<uint8_t> encodeBody(const LoginResponse& msg);
    static std::vector<uint8_t> encodeBody(const GatewayAssignRequest& msg);
    static std::vector<uint8_t> encodeBody(const GatewayAssignResponse& msg);
    static std::vector<uint8_t> encodeBody(const CellCreateEntity& msg);
    static std::vector<uint8_t> encodeBody(const CellEntityMove& msg);
    static std::vector<uint8_t> encodeBody(const CombatSkillCast& msg);
    static std::vector<uint8_t> encodeBody(const CombatDamage& msg);
    static std::vector<uint8_t> encodeBody(const ChatMessage& msg);
    static std::vector<uint8_t> encodeBody(const Ping& msg);
    static std::vector<uint8_t> encodeBody(const Pong& msg);
    static std::vector<uint8_t> encodeBody(const ErrorMessage& msg);

    //==========================================================================
    // 解码
    //==========================================================================

    // 解析消息头
    static MessageHeader parseHeader(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(MessageHeader)) {
            throw NngError(NNG_EINVAL);
        }
        MessageHeader header;
        std::memcpy(&header, data.data(), sizeof(MessageHeader));
        if (header.magic != MessageHeader::MAGIC) {
            throw NngError(NNG_EPROTO);
        }
        return header;
    }

    // 解析消息头（指针）
    static MessageHeader parseHeader(const uint8_t* data, size_t size) {
        if (size < sizeof(MessageHeader)) {
            throw NngError(NNG_EINVAL);
        }
        MessageHeader header;
        std::memcpy(&header, data, sizeof(MessageHeader));
        if (header.magic != MessageHeader::MAGIC) {
            throw NngError(NNG_EPROTO);
        }
        return header;
    }

    // 解码消息体
    template<typename T>
    static T decodeBody(const std::vector<uint8_t>& data) {
        T msg;
        decodeBody(data, msg);
        return msg;
    }

    // 解码消息体（引用）
    static void decodeBody(const std::vector<uint8_t>& data, LoginRequest& msg);
    static void decodeBody(const std::vector<uint8_t>& data, LoginResponse& msg);
    static void decodeBody(const std::vector<uint8_t>& data, GatewayAssignRequest& msg);
    static void decodeBody(const std::vector<uint8_t>& data, GatewayAssignResponse& msg);
    static void decodeBody(const std::vector<uint8_t>& data, CellCreateEntity& msg);
    static void decodeBody(const std::vector<uint8_t>& data, CellEntityMove& msg);
    static void decodeBody(const std::vector<uint8_t>& data, CombatSkillCast& msg);
    static void decodeBody(const std::vector<uint8_t>& data, CombatDamage& msg);
    static void decodeBody(const std::vector<uint8_t>& data, ChatMessage& msg);
    static void decodeBody(const std::vector<uint8_t>& data, Ping& msg);
    static void decodeBody(const std::vector<uint8_t>& data, Pong& msg);
    static void decodeBody(const std::vector<uint8_t>& data, ErrorMessage& msg);

    //==========================================================================
    // 辅助
    //==========================================================================

    static uint64_t nextSequence() {
        static std::atomic<uint64_t> seq{1};
        return seq.fetch_add(1);
    }

private:
    // 字符串编码
    static std::vector<uint8_t> encodeString(const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        std::vector<uint8_t> result(sizeof(uint32_t) + len);
        std::memcpy(result.data(), &len, sizeof(uint32_t));
        if (!str.empty()) {
            std::memcpy(result.data() + sizeof(uint32_t), str.data(), len);
        }
        return result;
    }

    // 字符串解码
    static std::string decodeString(const uint8_t*& ptr, const uint8_t* end) {
        if (ptr + sizeof(uint32_t) > end) {
            throw NngError(NNG_EINVAL);
        }
        uint32_t len;
        std::memcpy(&len, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        if (ptr + len > end) {
            throw NngError(NNG_EINVAL);
        }

        std::string result(ptr, ptr + len);
        ptr += len;
        return result;
    }
};

//==============================================================================
// 便捷宏
//==============================================================================

#define BW_ENCODE_REQUEST(Type, msg, sessionId) \
    apollo::protocol::MessageCodec::encode(msg, sessionId)

#define BW_DECODE_REQUEST(Type, data) \
    apollo::protocol::MessageCodec::decodeBody<Type>(data)

} // namespace protocol
} // namespace apollo
