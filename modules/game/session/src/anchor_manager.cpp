#include "apollo/game/session/anchor_manager.hpp"

namespace apollo::game::session {

AnchorManager::AnchorPtr AnchorManager::activate(std::uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& anchor = anchors_[player_id];
    if (!anchor) {
        anchor = std::make_shared<PlayerAnchor>(player_id);
    }
    return anchor;
}

AnchorManager::AnchorPtr AnchorManager::find(std::uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = anchors_.find(player_id);
    return it == anchors_.end() ? nullptr : it->second;
}

void AnchorManager::deactivate(std::uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    anchors_.erase(player_id);
}

std::size_t AnchorManager::anchor_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return anchors_.size();
}

} // namespace apollo::game::session
