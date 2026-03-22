#pragma once

#include "apollo/game/session/world_assignment.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::game::session {

enum class AnchorState : std::uint8_t {
    Loading = 0,
    Online,
    Transferring,
    Disconnected,
    Saving,
    Offline,
};

struct SessionBinding {
    std::uint64_t session_id = 0;
    std::uint32_t gateway_id = 0;
    std::string gateway_addr;
    std::int64_t bind_time_ms = 0;

    [[nodiscard]] bool is_bound() const noexcept {
        return session_id != 0;
    }
};

class PlayerAnchor {
public:
    explicit PlayerAnchor(std::uint64_t player_id);

    [[nodiscard]] std::uint64_t player_id() const noexcept;
    [[nodiscard]] AnchorState state() const noexcept;
    void set_state(AnchorState state) noexcept;

    [[nodiscard]] const SessionBinding& session_binding() const noexcept;
    void bind_session(const SessionBinding& binding);
    void unbind_session(std::uint64_t session_id);

    [[nodiscard]] const WorldAssignment& world_assignment() const noexcept;
    void assign_world(const WorldAssignment& assignment) noexcept;
    void clear_world_assignment() noexcept;

    void mark_dirty(std::string_view reason);
    [[nodiscard]] bool needs_save() const noexcept;
    void clear_dirty() noexcept;
    [[nodiscard]] const std::vector<std::string>& dirty_reasons() const noexcept;

private:
    std::uint64_t player_id_ = 0;
    AnchorState state_ = AnchorState::Loading;
    SessionBinding session_binding_{};
    WorldAssignment world_assignment_{};
    bool dirty_ = false;
    std::vector<std::string> dirty_reasons_;
};

} // namespace apollo::game::session
