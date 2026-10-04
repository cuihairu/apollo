#include "apollo/game/world/scene_aoi.hpp"

#include <algorithm>
#include <cmath>

namespace apollo::game::world {

namespace {
float distance_squared(const SceneAoi::Vec3& a, const SceneAoi::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
} // namespace

SceneAoi::SceneAoi(float width, float height, float grid_size, float view_radius)
    : width_(width)
    , height_(height)
    , grid_size_(grid_size > 0.0f ? grid_size : 1.0f)
    , view_radius_(view_radius) {
    grid_cols_ = static_cast<int>(std::ceil(width_ / grid_size_));
    grid_rows_ = static_cast<int>(std::ceil(height_ / grid_size_));
    if (grid_cols_ <= 0) {
        grid_cols_ = 1;
    }
    if (grid_rows_ <= 0) {
        grid_rows_ = 1;
    }
    grids_.resize(static_cast<std::size_t>(grid_cols_) * static_cast<std::size_t>(grid_rows_));
}

void SceneAoi::set_event_sink(EventSink sink) {
    sink_ = std::move(sink);
}

void SceneAoi::enter(apollo::game::core::EntityId id, const Vec3& position) {
    if (!id.is_valid() || entity_grid_.count(id.value()) != 0) {
        return;
    }
    const int gid = grid_id(position);
    grids_[static_cast<std::size_t>(gid)].push_back(id);
    entity_grid_[id.value()] = gid;
    positions_[id.value()] = position;

    // 事件分发（Enter/Sync/LEAVE 三面，见类注）：id 的初始视野铺底——
    // 半径内既有实体逐个 Sync 给 id；对称地，各既有实体收 id 的 Enter。
    // 水位先记账再分发（观察面 visible 与事件序列一致）。
    viewer_state_.reset(id);
    for (const auto other : viewers_at(position)) {
        if (other == id) {
            continue;
        }
        viewer_state_.add(id, other);
        viewer_state_.add(other, id);
        if (sink_) {
            sink_(Event{Event::Kind::Sync, id, other});
            sink_(Event{Event::Kind::Enter, other, id});
        }
    }
}

void SceneAoi::move(apollo::game::core::EntityId id, const Vec3& new_position) {
    auto it = entity_grid_.find(id.value());
    if (it == entity_grid_.end()) {
        return;
    }
    const int new_gid = grid_id(new_position);
    if (new_gid != it->second) {
        auto& old_grid = grids_[static_cast<std::size_t>(it->second)];
        old_grid.erase(std::remove(old_grid.begin(), old_grid.end(), id), old_grid.end());
        grids_[static_cast<std::size_t>(new_gid)].push_back(id);
        it->second = new_gid;
    }
    positions_[id.value()] = new_position;

    if (!viewer_state_.tracked(id)) {
        return;  // 无水位（未经 enter 登记的条目）——纯网格维护
    }

    // 差集分发（半径对称：o ∈ 新邻居集 ⇔ id ∈ o 的新邻居集）
    std::vector<apollo::game::core::EntityId> others;
    for (const auto other : viewers_at(new_position)) {
        if (other != id) {
            others.push_back(other);
        }
    }

    // observer=id 面：出视野 Leave → 新进 Enter → 仍在视野 Sync
    const auto before = viewer_state_.visible_set(id);
    for (const auto subject : before) {
        if (std::find(others.begin(), others.end(), subject) == others.end()) {
            viewer_state_.remove(id, subject);
            if (sink_) {
                sink_(Event{Event::Kind::Leave, id, subject});
            }
        }
    }
    for (const auto subject : others) {
        if (!viewer_state_.visible(id, subject)) {
            viewer_state_.add(id, subject);
            if (sink_) {
                sink_(Event{Event::Kind::Enter, id, subject});
            }
        } else if (sink_) {
            sink_(Event{Event::Kind::Sync, id, subject});
        }
    }

    // observer=other 面（对称）：受影响观察者 = 新邻居 ∪ 旧视野
    // （旧视野里不在新邻居集的：id 已走出其半径 → Leave）
    std::vector<apollo::game::core::EntityId> affected = others;
    for (const auto subject : before) {
        if (std::find(affected.begin(), affected.end(), subject) == affected.end()) {
            affected.push_back(subject);
        }
    }
    for (const auto observer : affected) {
        const bool sees_id =
            std::find(others.begin(), others.end(), observer) != others.end();
        if (sees_id) {
            if (viewer_state_.add(observer, id)) {
                if (sink_) {
                    sink_(Event{Event::Kind::Enter, observer, id});
                }
            } else if (sink_) {
                sink_(Event{Event::Kind::Sync, observer, id});
            }
        } else if (viewer_state_.remove(observer, id)) {
            if (sink_) {
                sink_(Event{Event::Kind::Leave, observer, id});
            }
        }
    }
}

void SceneAoi::leave(apollo::game::core::EntityId id) {
    auto it = entity_grid_.find(id.value());
    if (it == entity_grid_.end()) {
        return;
    }
    auto& grid = grids_[static_cast<std::size_t>(it->second)];
    grid.erase(std::remove(grid.begin(), grid.end(), id), grid.end());
    entity_grid_.erase(it);
    positions_.erase(id.value());

    // 事件分发：id 视野内的主体收 id 的 Leave（观察者侧），注销 id 自身
    // 视野；再扫全局水位把 id 从一切观察者视野里摘除（各自收 Leave）。
    if (sink_) {
        for (const auto subject : viewer_state_.visible_set(id)) {
            sink_(Event{Event::Kind::Leave, id, subject});
        }
    }
    viewer_state_.drop(id);
    if (sink_) {
        for (const auto observer : viewer_state_.remove_everywhere(id)) {
            sink_(Event{Event::Kind::Leave, observer, id});
        }
    } else {
        (void)viewer_state_.remove_everywhere(id);
    }
}

std::vector<apollo::game::core::EntityId> SceneAoi::viewers_of(
    apollo::game::core::EntityId id) const {
    auto pos_it = positions_.find(id.value());
    if (pos_it == positions_.end()) {
        return {};
    }
    auto result = viewers_at(pos_it->second);
    // viewers_at 含自身；语义「视野内实体」保留自身（调用方可自行过滤）
    return result;
}

std::vector<apollo::game::core::EntityId> SceneAoi::viewers_at(const Vec3& position) const {
    std::vector<apollo::game::core::EntityId> result;
    const int cx = grid_x(position.x);
    const int cy = grid_y(position.y);
    const float radius_sq = view_radius_ * view_radius_;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const int gx = cx + dx;
            const int gy = cy + dy;
            if (gx < 0 || gx >= grid_cols_ || gy < 0 || gy >= grid_rows_) {
                continue;
            }
            for (const auto& id : grids_[static_cast<std::size_t>(gy) *
                                         static_cast<std::size_t>(grid_cols_) +
                                         static_cast<std::size_t>(gx)]) {
                const auto pos_it = positions_.find(id.value());
                if (pos_it != positions_.end() &&
                    distance_squared(pos_it->second, position) <= radius_sq) {
                    result.push_back(id);
                }
            }
        }
    }
    return result;
}

bool SceneAoi::contains(apollo::game::core::EntityId id) const noexcept {
    return entity_grid_.count(id.value()) != 0;
}

std::size_t SceneAoi::entity_count() const noexcept {
    return entity_grid_.size();
}

int SceneAoi::grid_x(float x) const noexcept {
    int gx = static_cast<int>(std::floor(x / grid_size_));
    gx = std::max(0, std::min(gx, grid_cols_ - 1));
    return gx;
}

int SceneAoi::grid_y(float y) const noexcept {
    int gy = static_cast<int>(std::floor(y / grid_size_));
    gy = std::max(0, std::min(gy, grid_rows_ - 1));
    return gy;
}

int SceneAoi::grid_id(const Vec3& position) const noexcept {
    return grid_y(position.y) * grid_cols_ + grid_x(position.x);
}

} // namespace apollo::game::world
