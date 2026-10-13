#pragma once
//
// DeltaBuilder — per-viewer delta 组装（attribute-sync §3.2/§3.3）。
//
// 输入：EntityAttrs（属性存储 + ChangeHistory）+ ViewerAttrState（viewer 水位）
// 输出：AttrSyncFrame（delta 批 or 快照；快照时 deltas 已填全量当前值）
//
// 快照切换条件（§3.3）：
//   - world_seq == 0（实体无变更）
//   - world_seq - acked_seq > history 容量（历史已覆盖不了对方进度）
//   - delta 属性数 > 阈值（直接快照更省）
//   - 首次进入视野（acked_seq == 0 且 world_seq > 0）
//
// 域投影（§2.2）：快照与 delta 都按 viewer 可见域裁剪——
//   kPublic/kWorld 恒可见；kSelf 仅 owner（viewer_id == entity_id）；
//   kTeam/kGuild 经 group gate 判定（未设置 = 不可见）。
//

#include <cstdint>
#include <functional>
#include <vector>

#include "apollo/game/attrsync/attr_delta.hpp"
#include "apollo/game/attrsync/attr_domain.hpp"
#include "apollo/game/attrsync/change_history.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"
#include "apollo/game/attrsync/viewer_attr_state.hpp"

namespace apollo::game::attrsync {

struct AttrSyncFrame {
    std::uint64_t entity_id = 0;
    std::uint64_t base_seq = 0;       // 本批起点（history seq）
    std::vector<AttrDelta> deltas;     // 差分条目；快照时含全量当前值
    bool is_snapshot = false;          // true=快照模式（deltas 已填全量值）
    std::uint32_t epoch = 0;           // 权威纪元（P3 预留）
};

class DeltaBuilder {
public:
    // 队/会域成员判定（宿主注入：same_team / same_guild）
    using GroupGate = std::function<bool(std::uint64_t viewer,
                                         std::uint64_t entity,
                                         AttrDomain domain)>;

    // 构造 delta 帧。viewer 水位由调用方通过 set_acked 预设。
    // 快照路径：deltas 填全量当前值；两条路径都经域裁剪（§2.2）。
    [[nodiscard]] AttrSyncFrame build(std::uint64_t entity_id,
                                      std::uint64_t viewer_id,
                                      const EntityAttrs& attrs,
                                      const ViewerAttrState& viewer) const;

    // delta 属性数阈值（超过则直接快照）
    void set_delta_attr_threshold(std::uint32_t threshold) {
        delta_attr_threshold_ = threshold;
    }

    // kTeam/kGuild 可见性判定（未设置 = 该两域不可见）
    void set_group_gate(GroupGate gate) { group_gate_ = std::move(gate); }

private:
    // 快照负载组装：deltas 填全量当前值（域裁剪后），base_seq 推到 world_seq
    void fill_snapshot(AttrSyncFrame& frame, std::uint64_t viewer_id,
                       std::uint64_t entity_id, const EntityAttrs& attrs) const;

    // 域裁剪判定：该 viewer 是否可见该 attr
    [[nodiscard]] bool visible(std::uint64_t viewer_id, std::uint64_t entity_id,
                               std::uint32_t attr_id, const EntityAttrs& attrs) const;

    std::uint32_t delta_attr_threshold_ = 128;
    GroupGate group_gate_;
};

}  // namespace apollo::game::attrsync
