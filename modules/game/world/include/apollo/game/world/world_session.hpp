#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/map_instance.hpp"

#include <cstdint>
#include <string_view>

namespace apollo::game::world {

enum class WorldSessionState : std::uint8_t {
    Entering = 0,
    Active,
    Suspended,
    TransferringOut,
    TransferringIn,
    Leaving,
    Closed,
};

class WorldSession {
public:
    using SessionId = std::uint64_t;
    using PlayerId = std::uint64_t;

    WorldSession(SessionId session_id, PlayerId player_id);

    SessionId session_id() const;
    PlayerId player_id() const;

    void bind_avatar(apollo::game::core::EntityId avatar_entity_id);
    apollo::game::core::EntityId avatar_entity_id() const;

    void assign_map_instance(MapInstance::InstanceId map_instance_id);
    MapInstance::InstanceId map_instance_id() const;

    void assign_world(std::uint32_t world_id);
    std::uint32_t world_id() const;

    void assign_space(std::uint64_t space_id);
    std::uint64_t space_id() const;

    void set_state(WorldSessionState state);
    WorldSessionState state() const;

    void set_route_version(std::uint64_t route_version);
    std::uint64_t route_version() const;

    void suspend();
    void resume();
    void begin_transfer(std::uint32_t target_world_id,
                        MapInstance::InstanceId target_map_instance_id,
                        std::uint64_t target_space_id,
                        bool inbound = false);
    void complete_transfer();
    void begin_leave();

    std::uint32_t pending_world_id() const;
    MapInstance::InstanceId pending_map_instance_id() const;
    std::uint64_t pending_space_id() const;

    static std::string_view to_string(WorldSessionState state);

private:
    SessionId session_id_ = 0;
    PlayerId player_id_ = 0;
    apollo::game::core::EntityId avatar_entity_id_{};    
    MapInstance::InstanceId map_instance_id_ = 0;
    std::uint32_t world_id_ = 0;
    std::uint64_t space_id_ = 0;
    WorldSessionState state_ = WorldSessionState::Entering;
    std::uint64_t route_version_ = 0;
    std::uint32_t pending_world_id_ = 0;
    MapInstance::InstanceId pending_map_instance_id_ = 0;
    std::uint64_t pending_space_id_ = 0;
};

} // namespace apollo::game::world
