#pragma once
//
// AttrSyncScheduler — 优先级与节流调度（attribute-sync §5）。
//
// 职责：
//   - 每 viewer token bucket：带宽预算，每拍重置（§5.1）
//   - 实体优先级：importance × 1000 + dirty_count（§5.2，distance/visible
//     待 scene 位置面接入，当前为 0 权重）
//   - min_interval 节流分级：INSTANT(0) / HIGH(1) / NORM(4) / LOW(20)（§5.3）
//   - 饥饿保护：推迟 ≥ max_defer_ticks 强制发送（§5.2）
//
// 调度流程（schedule）：
//   1. 过滤：跳过 min_interval 窗口内的实体（饥饿除外）
//   2. 排序：priority 降序（importance 为主，dirty 破平，entity_id 定序）
//   3. 贪心装包：逐实体 collect → 编码量 ≤ 余量则发出，余量耗尽停
//
// 单写者：归 scene 线程，无内锁。
//

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include "apollo/game/attrsync/delta_builder.hpp"

namespace apollo::game::attrsync {

class AttrSyncManager;

class AttrSyncScheduler {
public:
    explicit AttrSyncScheduler(AttrSyncManager& mgr);

    // ---- §5.1 带宽预算 ----
    // 每拍字节上限（encode_frame 产物之和）。默认 3276（32KB/s @ 10Hz）
    void set_bandwidth_budget(std::uint64_t bytes_per_tick) {
        bytes_per_tick_ = bytes_per_tick;
    }

    // ---- §5.2 实体优先级 ----
    // importance 0-255（契约字段 importance(class)）；默认 0
    void set_entity_importance(std::uint64_t entity_id, std::uint8_t importance) {
        importance_[entity_id] = importance;
    }

    // ---- §5.3 min_interval 节流 ----
    // tick 数；0=INSTANT（不节流）。默认 0
    void set_min_interval(std::uint64_t entity_id, std::uint32_t min_interval) {
        min_interval_[entity_id] = min_interval;
    }

    // ---- §5.2 饥饿保护 ----
    // 推迟超过此拍数强制发送（即使 min_interval 未满足）。默认 100
    void set_max_defer_ticks(std::uint64_t ticks) { max_defer_ticks_ = ticks; }

    // ---- 拍推进 ----
    // 递增当前拍号 + 重置每 viewer 预算
    void advance_tick();

    // ---- 调度 ----
    // 给定 viewer 与其可见实体集，返回本拍应下发的帧（按优先级排序，
    // 总编码量 ≤ 预算）。发送后更新 last_sent_tick。
    [[nodiscard]] std::vector<AttrSyncFrame> schedule(
        std::uint64_t viewer, const std::vector<std::uint64_t>& visible);

    // ---- 查询 ----
    [[nodiscard]] std::uint64_t current_tick() const noexcept { return current_tick_; }
    [[nodiscard]] std::uint64_t budget_remaining(std::uint64_t viewer) const;

private:
    // (viewer, entity) 展平键（与 AttrSyncManager::pair_key 同构）
    static std::uint64_t pair_key(std::uint64_t viewer, std::uint64_t entity_id) {
        return (viewer << 32) | (entity_id & 0xFFFFFFFFull);
    }

    // 优先级：importance 为主权重，dirty_count 破平
    static std::uint64_t priority(std::uint64_t importance, std::uint64_t dirty) {
        return importance * 1000 + dirty;
    }

    AttrSyncManager& mgr_;

    // 每 viewer 预算余量
    std::unordered_map<std::uint64_t, std::uint64_t> budget_;
    // (viewer, entity) → last_sent_tick（MAX = 从未发送）
    std::unordered_map<std::uint64_t, std::uint64_t> last_sent_tick_;

    std::unordered_map<std::uint64_t, std::uint8_t> importance_;
    std::unordered_map<std::uint64_t, std::uint32_t> min_interval_;

    std::uint64_t bytes_per_tick_ = 3276;
    std::uint64_t max_defer_ticks_ = 100;
    std::uint64_t current_tick_ = 0;
};

}  // namespace apollo::game::attrsync
