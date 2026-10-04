#pragma once

#include "apollo/game/core/entity.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace apollo::game::world {

// SceneAoi（P0-3）：Scene 的兴趣管理成员（契约 §2.8：AOI 归 scene、scene_id 隔离）。
//
// 九宫格网格 AOI，与实体类型解耦（只认 EntityId + 位置值）——语义自
// cell::AOIManager 移植（P1-3 收敛后 cell 侧退役）。每个 Scene 独享一个
// SceneAoi 实例，天然按 scene 隔离。
class SceneAoi {
public:
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // 默认 1x1 退化网格（可 enter/move/leave，查询退化为全局半径过滤）
    SceneAoi()
        : SceneAoi(0.0f, 0.0f, 1.0f, 20.0f) {
    }
    SceneAoi(float width, float height, float grid_size, float view_radius);

    // ---- 集合维护 ----
    void enter(apollo::game::core::EntityId id, const Vec3& position);
    void move(apollo::game::core::EntityId id, const Vec3& new_position);
    void leave(apollo::game::core::EntityId id);

    // ---- 查询 ----
    // 视野内的实体（含自身；九宫格 + 半径过滤）
    [[nodiscard]] std::vector<apollo::game::core::EntityId> viewers_of(
        apollo::game::core::EntityId id) const;
    [[nodiscard]] std::vector<apollo::game::core::EntityId> viewers_at(
        const Vec3& position) const;
    [[nodiscard]] bool contains(apollo::game::core::EntityId id) const noexcept;
    [[nodiscard]] std::size_t entity_count() const noexcept;

private:
    int grid_x(float x) const noexcept;
    int grid_y(float y) const noexcept;
    int grid_id(const Vec3& position) const noexcept;

    float width_ = 0.0f;
    float height_ = 0.0f;
    float grid_size_ = 1.0f;
    float view_radius_ = 20.0f;
    int grid_cols_ = 0;
    int grid_rows_ = 0;

    std::vector<std::vector<apollo::game::core::EntityId>> grids_;
    std::unordered_map<std::uint64_t, int> entity_grid_;
    std::unordered_map<std::uint64_t, Vec3> positions_;
};

} // namespace apollo::game::world