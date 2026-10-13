#include "apollo/game/attrsync/delta_builder.hpp"

#include <algorithm>
#include <unordered_map>

namespace apollo::game::attrsync {

AttrSyncFrame DeltaBuilder::build(std::uint64_t entity_id,
                                   const ChangeHistory& history,
                                   const ViewerAttrState& viewer) const {
    AttrSyncFrame frame;
    frame.entity_id = entity_id;

    auto world_seq = history.world_seq();
    auto acked = viewer.acked_seq(entity_id);
    frame.base_seq = acked;

    // ── 快照切换条件（§3.3）──
    // 1. 实体无变更
    if (world_seq == 0) {
        frame.is_snapshot = false;
        return frame;
    }

    // 2. 首次进入视野（acked_seq == 0 且 world_seq > 0）
    if (acked == 0) {
        frame.is_snapshot = true;
        return frame;
    }

    // 3. 历史已覆盖不了对方进度（world_seq - acked > 容量）
    if (world_seq - acked > history.capacity()) {
        frame.is_snapshot = true;
        return frame;
    }

    // ── 计算 delta ──
    auto entries = history.entries_since(acked);

    // last-write-wins：同 attr_id 只保留最后一条
    std::unordered_map<std::uint32_t, std::int64_t> last_values;
    std::unordered_map<std::uint32_t, std::uint32_t> change_counts;
    for (const auto& entry : entries) {
        last_values[entry.attr_id] = entry.value;
        change_counts[entry.attr_id]++;
    }

    // 4. delta 属性数超过阈值 → 直接快照
    if (last_values.size() > delta_attr_threshold_) {
        frame.is_snapshot = true;
        return frame;
    }

    // 组装 delta 条目
    frame.deltas.reserve(last_values.size());
    for (const auto& [attr_id, value] : last_values) {
        AttrDelta delta;
        delta.attr_id = attr_id;
        delta.value = value;
        delta.change_n = change_counts[attr_id];
        frame.deltas.push_back(delta);
    }

    frame.is_snapshot = false;
    return frame;
}

}  // namespace apollo::game::attrsync
