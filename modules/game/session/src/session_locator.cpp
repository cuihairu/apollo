#include "apollo/game/session/session_locator.hpp"

namespace apollo::game::session {

void SessionLocator::bind(std::uint64_t player_id, const SessionBinding& binding) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto existing = by_player_.find(player_id);
    if (existing != by_player_.end() && existing->second.session_id != 0) {
        by_session_.erase(existing->second.session_id);
    }

    by_player_[player_id] = binding;
    if (binding.session_id != 0) {
        by_session_[binding.session_id] = player_id;
    }
}

void SessionLocator::unbind_player(std::uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = by_player_.find(player_id);
    if (it == by_player_.end()) {
        return;
    }

    by_session_.erase(it->second.session_id);
    by_player_.erase(it);
}

void SessionLocator::unbind_session(std::uint64_t session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = by_session_.find(session_id);
    if (it == by_session_.end()) {
        return;
    }

    by_player_.erase(it->second);
    by_session_.erase(it);
}

std::optional<SessionBinding> SessionLocator::find_by_player(std::uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = by_player_.find(player_id);
    if (it == by_player_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::uint64_t> SessionLocator::find_player_by_session(std::uint64_t session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = by_session_.find(session_id);
    if (it == by_session_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t SessionLocator::binding_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return by_player_.size();
}

} // namespace apollo::game::session
