#pragma once

#include "apollo/game/base/avatar.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace apollo::game::base {

class AvatarManager {
public:
    using AvatarPtr = std::shared_ptr<Avatar>;

    AvatarPtr activate(std::uint64_t player_id);
    AvatarPtr find(std::uint64_t player_id) const;
    void deactivate(std::uint64_t player_id);
    [[nodiscard]] std::size_t avatar_count() const;
    // 快照（P0-4）：关闭 flush / 自动保存遍历脏Avatar用；锁内拷贝指针，
    // 遍历期间不持锁
    [[nodiscard]] std::vector<AvatarPtr> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, AvatarPtr> avatars_;
};

} // namespace apollo::game::base
