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

void WorldSession::assign_map_instance(Instance::InstanceId map_instance_id) {
    map_instance_id_ = map_instance_id;
}

Instance::InstanceId WorldSession::map_instance_id() const {
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

bool WorldSession::suspend() {
    if (state_ != WorldSessionState::Active) {
        return false;  // 仅 Active 可挂起
    }
    state_ = WorldSessionState::Suspended;
    return true;
}

bool WorldSession::resume() {
    if (state_ != WorldSessionState::Suspended && state_ != WorldSessionState::Entering) {
        return false;  // Suspended 恢复 / Entering 激活；Closed/Leaving 拒绝
    }
    state_ = WorldSessionState::Active;
    resume_deadline_tick_ = 0;
    resume_token_ = 0;
    return true;
}

bool WorldSession::suspend_window(std::uint64_t now_tick, std::uint64_t window_ticks,
                                  std::uint64_t resume_token) {
    if (!suspend()) {
        return false;
    }
    resume_deadline_tick_ = now_tick + window_ticks;
    resume_token_ = resume_token;
    return true;
}

bool WorldSession::resume(std::uint64_t resume_token, std::uint64_t now_tick) {
    if (state_ != WorldSessionState::Suspended) {
        return false;
    }
    if (resume_token_ == 0 || resume_token != resume_token_) {
        return false;  // token 死/不匹配（双键失配即拒）
    }
    if (resume_window_expired(now_tick)) {
        return false;  // 窗口满（终结由 sweep 收口）
    }
    return resume();
}

bool WorldSession::resume_window_expired(std::uint64_t now_tick) const noexcept {
    return resume_deadline_tick_ != 0 && now_tick > resume_deadline_tick_;
}

std::uint64_t WorldSession::resume_token() const noexcept {
    return resume_token_;
}

std::uint64_t WorldSession::resume_deadline_tick() const noexcept {
    return resume_deadline_tick_;
}

bool WorldSession::begin_transfer(
    std::uint32_t target_world_id,
    Instance::InstanceId target_map_instance_id,
    std::uint64_t target_space_id,
    bool inbound
) {
    if (state_ != WorldSessionState::Active) {
        return false;  // 准入：仅 Active 可发起转移
    }
    pending_world_id_ = target_world_id;
    pending_map_instance_id_ = target_map_instance_id;
    pending_space_id_ = target_space_id;
    pre_transfer_state_ = state_;
    state_ = inbound ? WorldSessionState::TransferringIn : WorldSessionState::TransferringOut;
    return true;
}

bool WorldSession::suspend_from_transfer() {
    if (state_ != WorldSessionState::TransferringOut &&
        state_ != WorldSessionState::TransferringIn) {
        return false;  // 仅转移中可断线挂起（普通断线走 suspend）
    }
    state_ = WorldSessionState::Suspended;  // pending 保留：重连后收口
    return true;
}

bool WorldSession::can_resolve_pending_transfer() const noexcept {
    return state_ == WorldSessionState::TransferringOut ||
           state_ == WorldSessionState::TransferringIn ||
           (state_ == WorldSessionState::Suspended && pending_world_id_ != 0);
}

bool WorldSession::complete_transfer() {
    if (!can_resolve_pending_transfer()) {
        return false;  // 仅转移中（含断线挂机带 pending）可确认
    }
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
    return true;
}

bool WorldSession::abort_transfer() {
    if (!can_resolve_pending_transfer()) {
        return false;  // 仅转移中（含断线挂机带 pending）可回滚
    }
    pending_world_id_ = 0;
    pending_map_instance_id_ = 0;
    pending_space_id_ = 0;
    state_ = pre_transfer_state_;
    return true;
}

bool WorldSession::begin_leave() {
    if (state_ == WorldSessionState::Leaving || state_ == WorldSessionState::Closed) {
        return false;  // 离场不重入、Closed 终态封死
    }
    state_ = WorldSessionState::Leaving;
    return true;
}

std::uint32_t WorldSession::pending_world_id() const {
    return pending_world_id_;
}

Instance::InstanceId WorldSession::pending_map_instance_id() const {
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
