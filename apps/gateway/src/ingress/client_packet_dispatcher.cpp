#include "gateway/ingress/client_packet_dispatcher.hpp"

#include "apollo/protocol/codec.hpp"

namespace gateway {

ClientPacketDispatchResult ClientPacketDispatcher::dispatch(std::span<const std::uint8_t> payload) const {
    if (payload.size() < sizeof(apollo::protocol::MessageHeader)) {
        return {};
    }

    const std::vector<std::uint8_t> buffer(payload.begin(), payload.end());
    apollo::protocol::MessageHeader header;
    try {
        header = apollo::protocol::MessageCodec::parseHeader(buffer);
    } catch (const std::exception&) {
        return {};
    }
    const auto messageType = static_cast<apollo::protocol::MessageType>(header.type);

    if (messageType == apollo::protocol::MessageType::PING) {
        return {ClientPacketKind::Heartbeat, messageType};
    }

    if (messageType == apollo::protocol::MessageType::CHAT_MESSAGE) {
        return {ClientPacketKind::Chat, messageType};
    }

    // 客户端准入（登录串联入口帧）：客户端凭 login 签发的票据向 gateway
    // 换取会话绑定（login-flow：login 端 LOGIN_REQUEST → 票据 → gateway 端
    // GATEWAY_ASSIGN_REQUEST 准入）。LOGIN_REQUEST 归 login 进程——gateway
    // 端显式 decline（不静默当 Base 帧落地），客户端可读原因。
    if (messageType == apollo::protocol::MessageType::GATEWAY_ASSIGN_REQUEST ||
        messageType == apollo::protocol::MessageType::LOGIN_REQUEST) {
        return {ClientPacketKind::Login, messageType};
    }

    if (isWorldMessage(messageType)) {
        return {ClientPacketKind::World, messageType};
    }

    return {ClientPacketKind::Base, messageType};
}

bool ClientPacketDispatcher::isWorldMessage(apollo::protocol::MessageType messageType) {
    using MessageType = apollo::protocol::MessageType;

    switch (messageType) {
        case MessageType::CELL_CREATE_ENTITY:
        case MessageType::CELL_DESTROY_ENTITY:
        case MessageType::CELL_ENTITY_ENTER:
        case MessageType::CELL_ENTITY_LEAVE:
        case MessageType::CELL_ENTITY_MOVE:
        case MessageType::CELL_ENTITY_PROPERTY:
        case MessageType::CELL_CROSS_BORDER:
        case MessageType::COMBAT_SKILL_CAST:
        case MessageType::COMBAT_DAMAGE:
        case MessageType::COMBAT_HEAL:
        case MessageType::COMBAT_DEATH:
        // 队伍与小队副本族（P2 真实链路）：经 World 受理面执行。
        case MessageType::PARTY_CREATE_REQUEST:
        case MessageType::PARTY_JOIN_REQUEST:
        case MessageType::SQUAD_LAUNCH:
        case MessageType::SQUAD_ENTER:
        case MessageType::SQUAD_START:
        case MessageType::SQUAD_KILL:
            return true;

        default:
            return false;
    }
}

} // namespace gateway
