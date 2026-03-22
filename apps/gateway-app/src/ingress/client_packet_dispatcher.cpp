#include "gateway/ingress/client_packet_dispatcher.hpp"

#include "apollo/protocol/codec.hpp"

namespace gateway {

ClientPacketDispatchResult ClientPacketDispatcher::dispatch(std::span<const std::uint8_t> payload) const {
    if (payload.size() < sizeof(apollo::protocol::MessageHeader)) {
        return {};
    }

    const std::vector<std::uint8_t> buffer(payload.begin(), payload.end());
    const auto header = apollo::protocol::MessageCodec::parseHeader(buffer);
    const auto messageType = static_cast<apollo::protocol::MessageType>(header.type);

    if (messageType == apollo::protocol::MessageType::PING) {
        return {ClientPacketKind::Heartbeat, messageType};
    }

    if (messageType == apollo::protocol::MessageType::CHAT_MESSAGE) {
        return {ClientPacketKind::Chat, messageType};
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
            return true;

        default:
            return false;
    }
}

} // namespace gateway
