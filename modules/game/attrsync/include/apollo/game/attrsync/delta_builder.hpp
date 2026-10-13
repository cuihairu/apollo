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

#include <cstdint>
#include <vector>

#include "apollo/game/attrsync/attr_delta.hpp"
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
    // 构造 delta 帧。viewer 水位由调用方通过 set_acked 预设。
    // 快照路径：deltas 填全量当前值（无域裁剪，域投影留契约批）。
    [[nodiscard]] AttrSyncFrame build(std::uint64_t entity_id,
                                      const EntityAttrs& attrs,
                                      const ViewerAttrState& viewer) const;

    // delta 属性数阈值（超过则直接快照）
    void set_delta_attr_threshold(std::uint32_t threshold) {
        delta_attr_threshold_ = threshold;
    }

private:
    // 快照负载组装：deltas 填全量当前值，base_seq 推到 world_seq
    void fill_snapshot(AttrSyncFrame& frame, const EntityAttrs& attrs) const;

    std::uint32_t delta_attr_threshold_ = 128;
};

}  // namespace apollo::game::attrsync
