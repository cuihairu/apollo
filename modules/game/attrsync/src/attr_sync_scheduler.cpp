#include "apollo/game/attrsync/attr_sync_scheduler.hpp"

#include <algorithm>

#include "apollo/game/attrsync/attr_sync_manager.hpp"
#include "apollo/game/attrsync/frame_codec.hpp"

namespace apollo::game::attrsync {

AttrSyncScheduler::AttrSyncScheduler(AttrSyncManager& mgr) : mgr_(mgr) {}

void AttrSyncScheduler::advance_tick() {
    ++current_tick_;
    // 每拍重置预算（§5.1：每广播 tick 从预算领取字节数）
    for (auto& [viewer, remaining] : budget_) {
        remaining = bytes_per_tick_;
    }
}

std::vector<AttrSyncFrame> AttrSyncScheduler::schedule(
    std::uint64_t viewer, const std::vector<std::uint64_t>& visible) {
    std::vector<AttrSyncFrame> out;

    // 预算余量（首次调度该 viewer 时初始化为满额）
    auto budget_it = budget_.find(viewer);
    if (budget_it == budget_.end()) {
        budget_it = budget_.emplace(viewer, bytes_per_tick_).first;
    }
    std::uint64_t remaining = budget_it->second;

    // 1. 过滤 + 收集候选（含优先级信号）
    struct Candidate {
        std::uint64_t entity_id;
        std::uint64_t prio;
        bool starving;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(visible.size());

    for (const auto entity_id : visible) {
        const auto key = pair_key(viewer, entity_id);
        const auto last_it = last_sent_tick_.find(key);
        const bool never_sent = last_it == last_sent_tick_.end();
        const std::uint64_t last =
            never_sent ? std::numeric_limits<std::uint64_t>::max() : last_it->second;

        // 饥饿判定：推迟拍数（从未发送视为 0，不触发饥饿）
        const std::uint64_t deferred =
            never_sent ? 0
                       : (current_tick_ >= last ? current_tick_ - last : 0);
        const bool starving = deferred >= max_defer_ticks_;

        // min_interval 窗口（§5.3）：INSTANT(0) 不节流；从未发送放行
        const auto itv_it = min_interval_.find(entity_id);
        const std::uint32_t min_itv =
            itv_it != min_interval_.end() ? itv_it->second : 0;
        const bool interval_ok =
            never_sent || min_itv == 0 ||
            (current_tick_ >= last && current_tick_ - last >= min_itv);

        if (!interval_ok && !starving) {
            continue;  // 节流窗口内且未饥饿 → 推迟
        }

        // 优先级信号：importance + dirty_count（§5.2）
        const auto imp_it = importance_.find(entity_id);
        const std::uint64_t importance =
            imp_it != importance_.end() ? imp_it->second : 0;
        std::uint64_t dirty = 0;
        if (mgr_.has_entity(entity_id)) {
            dirty = mgr_.entity_attrs(entity_id).dirty_count();
        }
        candidates.push_back(
            {entity_id, priority(importance, dirty), starving});
    }

    // 2. 排序：priority 降序，entity_id 升序定序
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.prio != b.prio) {
                      return a.prio > b.prio;
                  }
                  return a.entity_id < b.entity_id;
              });

    // 3. 贪心装包
    for (const auto& cand : candidates) {
        if (remaining == 0) {
            break;
        }
        // 只读预检（不推进 acked，避免推迟帧数据丢失）
        auto peeked = mgr_.peek(viewer, cand.entity_id);
        if (peeked.empty() || (peeked.size() == 1 &&
                               peeked[0].deltas.empty() && !peeked[0].is_snapshot)) {
            continue;  // 无变更或空 delta
        }
        // 编码量校验（§5.1：预算耗尽 → 队列保留，不丢真相）
        std::uint64_t encoded = 0;
        for (const auto& f : peeked) {
            encoded += encode_frame(f).size();
        }
        if (encoded > remaining) {
            continue;  // 超预算 → 推迟到下拍
        }
        // 正式收集（推进 acked + 清 dirty）
        auto frames = mgr_.collect(viewer, cand.entity_id);
        remaining -= encoded;
        last_sent_tick_[pair_key(viewer, cand.entity_id)] = current_tick_;
        out.insert(out.end(), std::make_move_iterator(frames.begin()),
                   std::make_move_iterator(frames.end()));
    }

    budget_it->second = remaining;
    return out;
}

std::uint64_t AttrSyncScheduler::budget_remaining(std::uint64_t viewer) const {
    auto it = budget_.find(viewer);
    return it != budget_.end() ? it->second : bytes_per_tick_;
}

}  // namespace apollo::game::attrsync
