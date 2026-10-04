#pragma once

#include "apollo/game/session/player_anchor.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace apollo::game::session {

class AnchorManager {
public:
    using AnchorPtr = std::shared_ptr<PlayerAnchor>;

    AnchorPtr activate(std::uint64_t player_id);
    AnchorPtr find(std::uint64_t player_id) const;
    void deactivate(std::uint64_t player_id);
    [[nodiscard]] std::size_t anchor_count() const;
    // 快照（P0-4）：关闭 flush / 自动保存遍历脏锚点用；锁内拷贝指针，
    // 遍历期间不持锁
    [[nodiscard]] std::vector<AnchorPtr> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, AnchorPtr> anchors_;
};

} // namespace apollo::game::session
