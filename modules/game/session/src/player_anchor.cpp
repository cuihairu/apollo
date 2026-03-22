#include "apollo/game/session/player_anchor.hpp"

namespace apollo::game::session {

PlayerAnchor::PlayerAnchor(std::uint64_t player_id)
    : player_id_(player_id) {
}

std::uint64_t PlayerAnchor::player_id() const noexcept {
    return player_id_;
}

AnchorState PlayerAnchor::state() const noexcept {
    return state_;
}

void PlayerAnchor::set_state(AnchorState state) noexcept {
    state_ = state;
}

const SessionBinding& PlayerAnchor::session_binding() const noexcept {
    return session_binding_;
}

void PlayerAnchor::bind_session(const SessionBinding& binding) {
    session_binding_ = binding;
}

void PlayerAnchor::unbind_session(std::uint64_t session_id) {
    if (session_binding_.session_id == session_id) {
        session_binding_ = {};
    }
}

const WorldAssignment& PlayerAnchor::world_assignment() const noexcept {
    return world_assignment_;
}

void PlayerAnchor::assign_world(const WorldAssignment& assignment) noexcept {
    world_assignment_ = assignment;
}

void PlayerAnchor::clear_world_assignment() noexcept {
    world_assignment_ = {};
}

void PlayerAnchor::mark_dirty(std::string_view reason) {
    dirty_ = true;
    if (!reason.empty()) {
        dirty_reasons_.emplace_back(reason);
    }
}

bool PlayerAnchor::needs_save() const noexcept {
    return dirty_;
}

void PlayerAnchor::clear_dirty() noexcept {
    dirty_ = false;
    dirty_reasons_.clear();
}

const std::vector<std::string>& PlayerAnchor::dirty_reasons() const noexcept {
    return dirty_reasons_;
}

} // namespace apollo::game::session
