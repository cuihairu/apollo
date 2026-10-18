#pragma once

#include "apollo/protocol/messages.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace gateway {

enum class ClientPacketKind {
    Invalid,
    Heartbeat,
    Chat,
    World,
    Base,
};

struct ClientPacketDispatchResult {
    ClientPacketKind kind{ClientPacketKind::Invalid};
    std::optional<apollo::protocol::MessageType> messageType;
};

class ClientPacketDispatcher {
public:
    ClientPacketDispatcher() = default;
    ~ClientPacketDispatcher() = default;

    ClientPacketDispatchResult dispatch(std::span<const std::uint8_t> payload) const;

private:
    static bool isWorldMessage(apollo::protocol::MessageType messageType);
};

} // namespace gateway
