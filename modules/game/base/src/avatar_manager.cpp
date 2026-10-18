#include "apollo/game/base/avatar_manager.hpp"

namespace apollo::game::base {

AvatarManager::AvatarPtr AvatarManager::activate(std::uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& avatar = avatars_[player_id];
    if (!avatar) {
        avatar = std::make_shared<Avatar>(player_id);
    }
    return avatar;
}

AvatarManager::AvatarPtr AvatarManager::find(std::uint64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = avatars_.find(player_id);
    return it == avatars_.end() ? nullptr : it->second;
}

void AvatarManager::deactivate(std::uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    avatars_.erase(player_id);
}

std::size_t AvatarManager::avatar_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return avatars_.size();
}

std::vector<AvatarManager::AvatarPtr> AvatarManager::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AvatarPtr> result;
    result.reserve(avatars_.size());
    for (const auto& [id, avatar] : avatars_) {
        (void)id;
        result.push_back(avatar);
    }
    return result;
}

} // namespace apollo::game::base
