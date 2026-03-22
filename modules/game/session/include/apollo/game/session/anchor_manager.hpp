#pragma once

#include "apollo/game/session/player_anchor.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace apollo::game::session {

class AnchorManager {
public:
    using AnchorPtr = std::shared_ptr<PlayerAnchor>;

    AnchorPtr activate(std::uint64_t player_id);
    AnchorPtr find(std::uint64_t player_id) const;
    void deactivate(std::uint64_t player_id);
    [[nodiscard]] std::size_t anchor_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, AnchorPtr> anchors_;
};

} // namespace apollo::game::session
