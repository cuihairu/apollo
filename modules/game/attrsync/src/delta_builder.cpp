#include "apollo/game/attrsync/delta_builder.hpp"

#include <algorithm>
#include <map>
#include <unordered_map>

namespace apollo::game::attrsync {

AttrSyncFrame DeltaBuilder::build(std::uint64_t entity_id,
                                   std::uint64_t viewer_id,
                                   const EntityAttrs& attrs,
                                   const ViewerAttrState& viewer) const {
    AttrSyncFrame frame;
    frame.entity_id = entity_id;

    const auto& history = attrs.history();
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
        fill_snapshot(frame, viewer_id, entity_id, attrs);
        return frame;
    }

    // 3. 历史已覆盖不了对方进度（world_seq - acked > 容量）
    if (world_seq - acked > history.capacity()) {
        frame.is_snapshot = true;
        fill_snapshot(frame, viewer_id, entity_id, attrs);
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

    // 域裁剪（§2.2）：对该 viewer 不可见的 attr 剔除（阈值按裁剪后条数算）
    for (auto it = last_values.begin(); it != last_values.end();) {
        if (!visible(viewer_id, entity_id, it->first, attrs)) {
            it = last_values.erase(it);
        } else {
            ++it;
        }
    }

    // slice 保序例外（§3.4）：容器收缩即终态——同批内被新长度越过的死格
    // 条目丢弃（否则客户端先收长度再收越界格写）。组合 id 升序输出天然
    // 「长度（= base）先于 slot」——扩容向无需任何重排。
    for (auto it = last_values.begin(); it != last_values.end();) {
        const auto coord = attrs.slice_coord(it->first);
        if (!coord || coord->slot == EntityAttrs::kSliceLenSlot) {
            ++it;
            continue;
        }
        const auto len_it = last_values.find(
            EntityAttrs::slice_slot_id(coord->base, EntityAttrs::kSliceLenSlot));
        const auto live_len = (len_it != last_values.end())
            ? static_cast<std::uint16_t>(len_it->second)
            : attrs.slice_len(coord->base);
        if (coord->slot > live_len) {
            it = last_values.erase(it);
        } else {
            ++it;
        }
    }

    // 4. delta 属性数超过阈值 → 直接快照
    if (last_values.size() > delta_attr_threshold_) {
        frame.is_snapshot = true;
        fill_snapshot(frame, viewer_id, entity_id, attrs);
        return frame;
    }

    // 组装 delta 条目（attr_id 升序，输出确定性）
    frame.deltas.reserve(last_values.size());
    std::map<std::uint32_t, std::int64_t> ordered(last_values.begin(), last_values.end());
    for (const auto& [attr_id, value] : ordered) {
        AttrDelta delta;
        delta.attr_id = attr_id;
        delta.value = value;
        delta.change_n = change_counts[attr_id];
        frame.deltas.push_back(delta);
    }

    frame.is_snapshot = false;
    return frame;
}

void DeltaBuilder::fill_snapshot(AttrSyncFrame& frame, std::uint64_t viewer_id,
                                 std::uint64_t entity_id, const EntityAttrs& attrs) const {
    // 快照负载 = 全量当前值（§3.3；域裁剪后，域投影 §2.2）
    // base_seq = world_seq：快照即"把对方进度直接推到当前"
    frame.base_seq = attrs.history().world_seq();
    frame.deltas.clear();
    frame.deltas.reserve(attrs.values().size());
    // std::map 按 attr_id 升序（输出确定性）
    std::map<std::uint32_t, std::int64_t> ordered(attrs.values().begin(), attrs.values().end());
    for (const auto& [attr_id, value] : ordered) {
        if (!visible(viewer_id, entity_id, attr_id, attrs)) {
            continue;  // 域裁剪：对该观察者不可见
        }
        AttrDelta delta;
        delta.attr_id = attr_id;
        delta.value = value;
        delta.change_n = 1;  // 快照条目无合并语义
        frame.deltas.push_back(delta);
    }
}

bool DeltaBuilder::visible(std::uint64_t viewer_id, std::uint64_t entity_id,
                           std::uint32_t attr_id, const EntityAttrs& attrs) const {
    switch (attrs.domain(attr_id)) {
    case AttrDomain::kSelf:
        // 仅 owner 自身可见（§2.2 SELF：CASH、任务进度）
        return viewer_id == entity_id;
    case AttrDomain::kTeam:
    case AttrDomain::kGuild:
        // 队/会域经 group gate 判定（未设置 = 不可见，成员数据归宿主）
        return group_gate_ != nullptr && group_gate_(viewer_id, entity_id, attrs.domain(attr_id));
    case AttrDomain::kPublic:
    case AttrDomain::kWorld:
    default:
        return true;
    }
}

}  // namespace apollo::game::attrsync
