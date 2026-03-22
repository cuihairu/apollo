#pragma once

#include "apollo/game/world/world_session.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace apollo::game::world {

class WorldSessionManager {
public:
    using SessionPtr = std::shared_ptr<WorldSession>;

    SessionPtr create_session(WorldSession::SessionId session_id, WorldSession::PlayerId player_id);
    SessionPtr find_session(WorldSession::SessionId session_id) const;
    SessionPtr find_by_player(WorldSession::PlayerId player_id) const;
    SessionPtr suspend_session(WorldSession::SessionId session_id);
    SessionPtr resume_session(WorldSession::SessionId session_id);
    SessionPtr transfer_session(WorldSession::SessionId session_id,
                                std::uint32_t target_world_id,
                                MapInstance::InstanceId target_map_instance_id,
                                std::uint64_t target_space_id,
                                bool inbound = false);
    SessionPtr complete_transfer(WorldSession::SessionId session_id);
    void close_session(WorldSession::SessionId session_id);
    [[nodiscard]] std::size_t session_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<WorldSession::SessionId, SessionPtr> sessions_;
    std::unordered_map<WorldSession::PlayerId, WorldSession::SessionId> player_index_;
};

} // namespace apollo::game::world
