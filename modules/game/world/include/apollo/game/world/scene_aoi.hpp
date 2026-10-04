#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/viewer_state.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace apollo::game::world {

// SceneAoi（P0-3 建立；P1-3 收敛为全仓唯一 AOI 实现）：Scene 的兴趣管理
// 成员（契约 §2.8：AOI 归 scene、scene_id 隔离）。
//
// 九宫格网格 AOI，与实体类型解耦（只认 EntityId + 位置值）——语义自
// cell::AOIManager 移植（P1-3 后 cell 侧与 legacy aoi.cpp 双实现退役）。
// 每个 Scene 独享一个 SceneAoi 实例，天然按 scene 隔离。
//
// 事件面（P1-3 补全，契约 §13 ENTER/SYNC/LEAVE 三事件）：
//   - Enter：主体进入观察者视野（双向各发一侧）；
//   - Sync：主体在视野内位置更新（含进场时的初始视野铺底）；
//   - Leave：主体离开观察者视野（含 leave 集合时对全部观察者）。
// 差集基准是 ViewerState（逐 observer 水位）；sink 缺省静默，由 Scene/
// 宿主注入（下发网关面留 P3-2）。
class SceneAoi {
public:
    struct Vec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    // ---- 事件面（单进程形态；跨进程下发随 P3-2 网关面）----
    struct Event {
        enum class Kind : std::uint8_t { Enter = 0, Sync, Leave };
        Kind kind = Kind::Enter;
        apollo::game::core::EntityId observer{};  // 事件发给谁（观察者）
        apollo::game::core::EntityId subject{};   // 关于谁（主体）
    };
    using EventSink = std::function<void(const Event&)>;

    // 注册事件 sink（同步分发；缺省静默）
    void set_event_sink(EventSink sink);

    // 默认 1x1 退化网格（可 enter/move/leave，查询退化为全局半径过滤）
    SceneAoi()
        : SceneAoi(0.0f, 0.0f, 1.0f, 20.0f) {
    }
    SceneAoi(float width, float height, float grid_size, float view_radius);

    // ---- 集合维护（维护即分发：Enter/Sync/Leave 事件面见类注）----
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
    ViewerState viewer_state_;  // 逐 observer 视野水位（事件差集基准）
    EventSink sink_;
};

} // namespace apollo::game::world