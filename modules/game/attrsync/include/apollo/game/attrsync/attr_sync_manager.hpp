#pragma once
//
// AttrSyncManager — per-viewer delta 管线集成（attribute-sync §3.2/§4.1）。
//
// 职责：
//   - 每实体 EntityAttrs（属性存储 + ChangeHistory）
//   - 每 (viewer, entity) ViewerAttrState（acked_seq 水位）
//   - AOI Enter → 建 ViewerState + 快照；AOI Leave → 回收
//   - Broadcast tick → 收集 per-viewer delta 帧
//   - ACK → 推进 acked_seq
//
// 单写者：归 scene 线程，无内锁。
//

#include <cstdint>
#include <unordered_map>

#include "apollo/game/attrsync/delta_builder.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"

namespace apollo::game::attrsync {

class AttrSyncManager {
public:
    // ---- 实体注册 ----
    void register_entity(std::uint64_t entity_id);
    [[nodiscard]] bool has_entity(std::uint64_t entity_id) const;

    // ---- 属性写路径 ----
    void set_attribute(std::uint64_t entity_id, std::uint32_t attr_id, std::int64_t value);
    [[nodiscard]] std::int64_t get_attribute(std::uint64_t entity_id, std::uint32_t attr_id) const;

    // ---- AOI 事件接线（§4.1）----
    // Viewer 进入实体视野：建 ViewerState（不组帧，快照由 collect 面出）
    void on_viewer_enter(std::uint64_t viewer, std::uint64_t entity_id);
    // Viewer 进入实体视野：建 ViewerState + 返回快照帧
    std::vector<AttrSyncFrame> on_viewer_enter_collect(std::uint64_t viewer, std::uint64_t entity_id);
    // Viewer 离开实体视野：回收 ViewerState
    void on_viewer_leave(std::uint64_t viewer, std::uint64_t entity_id);

    // ---- 查询 ----
    [[nodiscard]] bool has_viewer(std::uint64_t viewer, std::uint64_t entity_id) const;

    // ---- Broadcast tick 收集（§3.2）----
    // 返回 per-viewer delta 帧（空 = 无变更）
    [[nodiscard]] std::vector<AttrSyncFrame> collect(std::uint64_t viewer, std::uint64_t entity_id);

    // ---- ACK（§3.2 第 4 步）----
    void ack(std::uint64_t viewer, std::uint64_t entity_id, std::uint64_t seq);

    // ---- 内部访问（测试用）----
    [[nodiscard]] const EntityAttrs& entity_attrs(std::uint64_t entity_id) const;

private:
    std::unordered_map<std::uint64_t, EntityAttrs> entities_;
    // (viewer, entity) → ViewerAttrState 展平为单键
    std::unordered_map<std::uint64_t, ViewerAttrState> viewers_;
};

}  // namespace apollo::game::attrsync
