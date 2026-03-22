#include "apollo/game/world/world_session.hpp"

namespace apollo::game::world {

WorldSession::WorldSession(SessionId session_id, PlayerId player_id)
    : session_id_(session_id)
    , player_id_(player_id) {
}

WorldSession::SessionId WorldSession::session_id() const {
    return session_id_;
}

WorldSession::PlayerId WorldSession::player_id() const {
    return player_id_;
}

void WorldSession::bind_avatar(apollo::game::core::EntityId avatar_entity_id) {
    avatar_entity_id_ = avatar_entity_id;
}

apollo::game::core::EntityId WorldSession::avatar_entity_id() const {
    return avatar_entity_id_;
}

void WorldSession::assign_map_instance(MapInstance::InstanceId map_instance_id) {
    map_instance_id_ = map_instance_id;
}

MapInstance::InstanceId WorldSession::map_instance_id() const {
    return map_instance_id_;
}

void WorldSession::assign_world(std::uint32_t world_id) {
    world_id_ = world_id;
}

std::uint32_t WorldSession::world_id() const {
    return world_id_;
}

void WorldSession::assign_space(std::uint64_t space_id) {
    space_id_ = space_id;
}

std::uint64_t WorldSession::space_id() const {
    return space_id_;
}

void WorldSession::set_state(WorldSessionState state) {
    state_ = state;
}

WorldSessionState WorldSession::state() const {
    return state_;
}

void WorldSession::set_route_version(std::uint64_t route_version) {
    route_version_ = route_version;
}

std::uint64_t WorldSession::route_version() const {
    return route_version_;
}

void WorldSession::suspend() {
    state_ = WorldSessionState::Suspended;
}

void WorldSession::resume() {
    state_ = WorldSessionState::Active;
}

void WorldSession::begin_transfer(
    std::uint32_t target_world_id,
    MapInstance::InstanceId target_map_instance_id,
    std::uint64_t target_space_id,
    bool inbound
) {
    pending_world_id_ = target_world_id;
    pending_map_instance_id_ = target_map_instance_id;
    pending_space_id_ = target_space_id;
    state_ = inbound ? WorldSessionState::TransferringIn : WorldSessionState::TransferringOut;
}

void WorldSession::complete_transfer() {
    if (pending_world_id_ != 0) {
        world_id_ = pending_world_id_;
    }
    if (pending_map_instance_id_ != 0) {
        map_instance_id_ = pending_map_instance_id_;
    }
    if (pending_space_id_ != 0) {
        space_id_ = pending_space_id_;
    }

    pending_world_id_ = 0;
    pending_map_instance_id_ = 0;
    pending_space_id_ = 0;
    state_ = WorldSessionState::Active;
}

void WorldSession::begin_leave() {
    state_ = WorldSessionState::Leaving;
}

std::uint32_t WorldSession::pending_world_id() const {
    return pending_world_id_;
}

MapInstance::InstanceId WorldSession::pending_map_instance_id() const {
    return pending_map_instance_id_;
}

std::uint64_t WorldSession::pending_space_id() const {
    return pending_space_id_;
}

std::string_view WorldSession::to_string(WorldSessionState state) {
    switch (state) {
    case WorldSessionState::Entering:
        return "Entering";
    case WorldSessionState::Active:
        return "Active";
    case WorldSessionState::Suspended:
        return "Suspended";
    case WorldSessionState::TransferringOut:
        return "TransferringOut";
    case WorldSessionState::TransferringIn:
        return "TransferringIn";
    case WorldSessionState::Leaving:
        return "Leaving";
    case WorldSessionState::Closed:
        return "Closed";
    }

    return "Unknown";
}

} // namespace apollo::game::world
