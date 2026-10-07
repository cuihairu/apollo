#pragma once

#include <cstdint>

namespace apollo::game::session {

struct WorldAssignment {
    std::uint32_t world_id = 0;
    std::uint64_t map_id = 0;
    std::uint64_t instance_id = 0;
    std::uint64_t space_id = 0;
    std::uint64_t route_version = 0;

    [[nodiscard]] bool is_assigned() const noexcept {
        return world_id != 0 || instance_id != 0 || space_id != 0;
    }

    // 值语义比较（镜像 wire 往返断言/落点迁移测试复用）
    [[nodiscard]] bool operator==(const WorldAssignment&) const noexcept = default;
};

} // namespace apollo::game::session
