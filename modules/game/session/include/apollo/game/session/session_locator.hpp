#pragma once

#include "apollo/game/session/player_anchor.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace apollo::game::session {

class SessionLocator {
public:
    void bind(std::uint64_t player_id, const SessionBinding& binding);
    void unbind_player(std::uint64_t player_id);
    void unbind_session(std::uint64_t session_id);

    [[nodiscard]] std::optional<SessionBinding> find_by_player(std::uint64_t player_id) const;
    [[nodiscard]] std::optional<std::uint64_t> find_player_by_session(std::uint64_t session_id) const;
    [[nodiscard]] std::size_t binding_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, SessionBinding> by_player_;
    std::unordered_map<std::uint64_t, std::uint64_t> by_session_;
};

} // namespace apollo::game::session
