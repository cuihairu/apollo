#include "apollo/game/attrsync/attr_sync_manager.hpp"

#include <utility>

namespace apollo::game::attrsync {

// ---- 实体注册 ----

void AttrSyncManager::register_entity(std::uint64_t entity_id) {
    entities_.try_emplace(entity_id, entity_id);
}

bool AttrSyncManager::has_entity(std::uint64_t entity_id) const {
    return entities_.count(entity_id) > 0;
}

// ---- 属性写路径 ----

void AttrSyncManager::set_attribute(std::uint64_t entity_id, std::uint32_t attr_id,
                                     std::int64_t value) {
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        return;  // 未注册实体静默忽略
    }
    if (attr_hook_) {  // §5.2 空载零开销：无钩子不记录
        record_pending(entity_id, attr_id, it->second.get(attr_id), value);
    }
    it->second.set(attr_id, value);
    if (change_sink_) {
        change_sink_(entity_id, attr_id, value);
    }
}

void AttrSyncManager::set_change_sink(ChangeSink sink) {
    change_sink_ = std::move(sink);
}

// ---- §5.1 属性钩子窗口 ----

void AttrSyncManager::set_attr_hook(AttrHook hook) {
    attr_hook_ = std::move(hook);
}

void AttrSyncManager::record_pending(std::uint64_t entity, std::uint32_t attr,
                                     std::int64_t old_value, std::int64_t new_value) {
    const std::uint64_t key = pending_key(entity, attr);
    auto pit = pending_index_.find(key);
    if (pit != pending_index_.end()) {
        pending_order_[pit->second].new_value = new_value;  // 保留 first-old
        return;
    }
    pending_index_.emplace(key, pending_order_.size());
    pending_order_.push_back(HookChange{entity, attr, old_value, new_value});
}

void AttrSyncManager::apply_attr_hook() {
    if (!attr_hook_ || pending_order_.empty()) {
        return;
    }
    std::vector<HookChange> changes;
    changes.swap(pending_order_);
    pending_index_.clear();
    attr_hook_(changes);
    for (const auto& ch : changes) {
        auto it = entities_.find(ch.entity);
        if (it == entities_.end()) {
            continue;
        }
        if (it->second.get(ch.attr_id) == ch.new_value) {
            continue;  // 未修订：白名单外/预算跳过/错误保持原值/nil
        }
        // 修订走正规写路径（values + dirty + history + sink）；不重入窗集。
        it->second.set(ch.attr_id, ch.new_value);
        if (change_sink_) {
            change_sink_(ch.entity, ch.attr_id, ch.new_value);
        }
    }
}

// ---- 域投影（§2.2）----

void AttrSyncManager::set_attr_domain(std::uint64_t entity_id, std::uint32_t attr_id,
                                      AttrDomain domain) {
    auto it = entities_.find(entity_id);
    if (it == entities_.end()) {
        return;  // 未注册实体静默忽略
    }
    it->second.set_domain(attr_id, domain);
}

void AttrSyncManager::set_group_gate(DeltaBuilder::GroupGate gate) {
    group_gate_ = std::move(gate);
}

std::int64_t AttrSyncManager::get_attribute(std::uint64_t entity_id, std::uint32_t attr_id) const {
    auto it = entities_.find(entity_id);
    return (it != entities_.end()) ? it->second.get(attr_id) : 0;
}

// ---- AOI 事件接线 ----

void AttrSyncManager::on_viewer_enter(std::uint64_t viewer, std::uint64_t entity_id) {
    auto eit = entities_.find(entity_id);
    if (eit == entities_.end()) {
        return;
    }
    viewers_[viewer].set_acked(entity_id, 0);
    viewers_[viewer].set_acked(entity_id, eit->second.history().world_seq());
    eit->second.clear_dirty();
}

std::vector<AttrSyncFrame> AttrSyncManager::on_viewer_enter_collect(std::uint64_t viewer,
                                                                      std::uint64_t entity_id) {
    std::vector<AttrSyncFrame> frames;
    auto eit = entities_.find(entity_id);
    if (eit == entities_.end()) {
        return frames;
    }

    // 建 ViewerState（重进视野 = 清空重置，per §6 延迟回收由上层决定；此处直接重建）
    viewers_[viewer].set_acked(entity_id, 0);

    // 快照帧（DeltaBuilder 对 acked=0 返回 is_snapshot=true；域裁剪 §2.2）
    DeltaBuilder builder;
    if (group_gate_) {
        builder.set_group_gate(group_gate_);
    }
    frames.push_back(builder.build(entity_id, viewer, eit->second, viewers_[viewer]));

    // 快照即推进：对方进度直接推到当前 seq（§3.3 快照语义）
    viewers_[viewer].set_acked(entity_id, eit->second.history().world_seq());

    // 清 dirty（快照已含全部当前值）
    eit->second.clear_dirty();

    return frames;
}

void AttrSyncManager::on_viewer_leave(std::uint64_t viewer, std::uint64_t entity_id) {
    auto vit = viewers_.find(viewer);
    if (vit != viewers_.end()) {
        vit->second.remove(entity_id);
    }
}

bool AttrSyncManager::has_viewer(std::uint64_t viewer, std::uint64_t entity_id) const {
    auto vit = viewers_.find(viewer);
    if (vit == viewers_.end()) {
        return false;
    }
    // acked_ 里存在该实体即视为在视野（set_acked(0) 也算登记）
    return vit->second.tracked(entity_id);
}

// ---- Broadcast tick 收集 ----

std::vector<AttrSyncFrame> AttrSyncManager::collect(std::uint64_t viewer, std::uint64_t entity_id) {
    std::vector<AttrSyncFrame> frames;
    auto eit = entities_.find(entity_id);
    auto vit = viewers_.find(viewer);
    if (eit == entities_.end() || vit == viewers_.end()) {
        return frames;
    }
    if (!vit->second.tracked(entity_id)) {
        return frames;
    }

    DeltaBuilder builder;
    if (group_gate_) {
        builder.set_group_gate(group_gate_);
    }
    auto key = pair_key(viewer, entity_id);

    // ACK 超时降级（§3.3）：静默 >= 阈值 → 每拍快照重置，直到 ACK 到达解除
    auto sit = silent_ticks_.find(key);
    if (sit != silent_ticks_.end() && sit->second >= ack_timeout_ticks_) {
        // 水位归零 → build 走真快照路径（全量当前值 + 域裁剪 + base_seq=world_seq）
        vit->second.set_acked(entity_id, 0);
        frames.push_back(builder.build(entity_id, viewer, eit->second, vit->second));
        // 快照即推进（对方进度推到当前 seq）；静默计数保留——
        // 降级态持续，ACK 到达才解除（自愈路径 §3.3）
        vit->second.set_acked(entity_id, eit->second.history().world_seq());
        eit->second.clear_dirty();
        return frames;
    }

    frames.push_back(builder.build(entity_id, viewer, eit->second, vit->second));

    // delta 已组包：推进水位（发送成功≠确认，但本层以"已组包"推进；
    // 丢包重发由上层可靠通道处理，快照切换是兜底）
    if (!frames.back().is_snapshot && !frames.back().deltas.empty()) {
        vit->second.set_acked(entity_id, frames.back().base_seq + frames.back().deltas.size());
        eit->second.clear_dirty();
    }

    // 静默计数 +1（无论是否有变更，collect 即一拍）
    silent_ticks_[key]++;

    return frames;
}

// ---- 只读收集（调度器预检用）----

std::vector<AttrSyncFrame> AttrSyncManager::peek(std::uint64_t viewer, std::uint64_t entity_id) {
    std::vector<AttrSyncFrame> frames;
    auto eit = entities_.find(entity_id);
    auto vit = viewers_.find(viewer);
    if (eit == entities_.end() || vit == viewers_.end()) {
        return frames;
    }
    if (!vit->second.tracked(entity_id)) {
        return frames;
    }
    DeltaBuilder builder;
    if (group_gate_) {
        builder.set_group_gate(group_gate_);
    }
    frames.push_back(builder.build(entity_id, viewer, eit->second, vit->second));
    return frames;
}

// ---- ACK 超时降级 ----

bool AttrSyncManager::is_degraded(std::uint64_t viewer, std::uint64_t entity_id) const {
    auto it = silent_ticks_.find(pair_key(viewer, entity_id));
    return it != silent_ticks_.end() && it->second >= ack_timeout_ticks_;
}

// ---- ACK ----

void AttrSyncManager::ack(std::uint64_t viewer, std::uint64_t entity_id, std::uint64_t seq) {
    auto vit = viewers_.find(viewer);
    if (vit != viewers_.end()) {
        vit->second.set_acked(entity_id, seq);
    }
    // ACK 到达 → 静默计数清零
    silent_ticks_[pair_key(viewer, entity_id)] = 0;
}

// ---- 内部访问 ----

const EntityAttrs& AttrSyncManager::entity_attrs(std::uint64_t entity_id) const {
    return entities_.at(entity_id);
}

}  // namespace apollo::game::attrsync
