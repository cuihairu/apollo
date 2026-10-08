#include "apollo/game/session/player_directory.hpp"

#include <algorithm>

namespace apollo::game::session {

void PlayerDirectory::set_event_sink(EventSink sink) {
    sink_ = std::move(sink);
}

void PlayerDirectory::emit(const Event& event) {
    if (sink_) {
        sink_(event);
    }
}

std::uint64_t PlayerDirectory::session_up(std::uint64_t player_id,
                                          const SessionBinding& binding,
                                          const WorldAssignment& assignment,
                                          std::uint32_t zone_id) {
    if (player_id == 0) {
        return 0;
    }

    // 顶号预裁（§3 新顶旧）：同账号新登录命中已有条目——Online/Suspended/
    // Leaving 皆然，旧条目踢除后写新条目。
    evict_for_relogin(player_id);

    Entry entry;
    entry.binding = binding;
    entry.assignment = assignment;
    entry.state = EntryState::Online;
    entry.anchor_epoch = next_epoch_++;
    entry.zone_id = zone_id;

    entries_[player_id] = entry;

    Event event;
    event.kind = Event::Kind::SessionUp;
    event.player_id = player_id;
    event.anchor_epoch = entry.anchor_epoch;
    event.zone_id = zone_id;
    event.binding = binding;
    event.assignment = assignment;
    emit(event);
    return entry.anchor_epoch;
}

bool PlayerDirectory::session_down(std::uint64_t player_id, std::uint32_t reason) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end()) {
        return false;
    }

    Event event;
    event.kind = Event::Kind::SessionDown;
    event.player_id = player_id;
    event.anchor_epoch = it->second.anchor_epoch;
    event.zone_id = it->second.zone_id;
    event.reason = reason;
    event.binding = it->second.binding;
    entries_.erase(it);
    emit(event);
    return true;
}

bool PlayerDirectory::mark_suspended(std::uint64_t player_id,
                                     std::uint64_t now_tick,
                                     std::uint64_t window_ticks) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end() || it->second.state != EntryState::Online) {
        return false;  // 未知玩家 / Leaving 不回窗口
    }
    it->second.state = EntryState::Suspended;
    it->second.deadline_tick = now_tick + window_ticks;
    return true;
}

std::size_t PlayerDirectory::mark_suspended_by_zone(std::uint32_t zone_id,
                                                    std::uint64_t now_tick,
                                                    std::uint64_t window_ticks) {
    if (zone_id == 0) {
        return 0;  // 未声明 Zone 不批量处置
    }
    std::size_t moved = 0;
    for (auto& [player_id, entry] : entries_) {
        (void)player_id;
        if (entry.zone_id == zone_id && entry.state == EntryState::Online) {
            entry.state = EntryState::Suspended;
            entry.deadline_tick = now_tick + window_ticks;
            ++moved;
        }
    }
    return moved;
}

std::size_t PlayerDirectory::mark_suspended_by_gateway(std::uint32_t gateway_id,
                                                       std::uint64_t now_tick,
                                                       std::uint64_t window_ticks) {
    if (gateway_id == 0) {
        return 0;  // 未声明 gateway 不批量处置
    }
    std::size_t moved = 0;
    for (auto& [player_id, entry] : entries_) {
        (void)player_id;
        if (entry.binding.gateway_id == gateway_id && entry.state == EntryState::Online) {
            entry.state = EntryState::Suspended;
            entry.deadline_tick = now_tick + window_ticks;
            ++moved;
        }
    }
    return moved;
}

bool PlayerDirectory::resume(std::uint64_t player_id,
                             std::uint64_t session_id,
                             std::uint64_t anchor_epoch) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end() || it->second.state != EntryState::Suspended) {
        return false;
    }
    // §3-③ 竞态窗口守卫：token 与 epoch 双锚——旧会话/旧 epoch 拒绝
    if (it->second.binding.session_id != session_id ||
        it->second.anchor_epoch != anchor_epoch) {
        return false;
    }
    it->second.state = EntryState::Online;  // §4 只改状态列，不动条目
    it->second.deadline_tick = 0;
    return true;
}

std::uint64_t PlayerDirectory::evict_for_relogin(std::uint64_t player_id,
                                                 std::uint32_t reason) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end()) {
        return 0;
    }

    const auto old_epoch = it->second.anchor_epoch;
    Event event;
    event.kind = Event::Kind::SessionKicked;
    event.player_id = player_id;
    event.anchor_epoch = old_epoch;
    event.zone_id = it->second.zone_id;
    event.reason = reason;
    event.binding = it->second.binding;
    entries_.erase(it);
    emit(event);
    return old_epoch;
}

bool PlayerDirectory::moved(std::uint64_t player_id, const WorldAssignment& assignment) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end() ||
        (it->second.state != EntryState::Online && it->second.state != EntryState::Leaving)) {
        return false;
    }

    it->second.assignment = assignment;
    it->second.assignment.route_version =
        it->second.assignment.route_version != 0 ? it->second.assignment.route_version : 1;

    Event event;
    event.kind = Event::Kind::SessionMoved;
    event.player_id = player_id;
    event.anchor_epoch = it->second.anchor_epoch;
    event.zone_id = it->second.zone_id;
    event.binding = it->second.binding;
    event.assignment = it->second.assignment;
    emit(event);
    return true;
}

bool PlayerDirectory::begin_leave(std::uint64_t player_id) {
    const auto it = entries_.find(player_id);
    if (it == entries_.end() || it->second.state != EntryState::Online) {
        return false;
    }
    it->second.state = EntryState::Leaving;
    return true;
}

std::size_t PlayerDirectory::sweep(std::uint64_t now_tick) {
    std::vector<std::uint64_t> expired;
    for (const auto& [player_id, entry] : entries_) {
        if (entry.state == EntryState::Suspended && entry.deadline_tick != 0 &&
            now_tick >= entry.deadline_tick) {
            expired.push_back(player_id);
        }
    }

    for (const auto player_id : expired) {
        session_down(player_id, kReasonWindowExpired);
    }
    return expired.size();
}

bool PlayerDirectory::reconcile(const std::vector<std::uint64_t>& reported_online) const {
    auto local = online_ids();
    auto reported = reported_online;
    std::sort(local.begin(), local.end());
    std::sort(reported.begin(), reported.end());
    return local == reported;
}

std::size_t PlayerDirectory::snapshot_reset(
    const std::vector<std::uint64_t>& authoritative_online) {
    // 状态层快照重置（§2）：失配即全量对表——目录多出的条目静默删除、无
    // 事件（与事件层互不兜底）；上报有而目录无的条目留 P3 全量重报
    // （Zone 侧重报 SessionUp），目录侧单进程阶段无法凭空重建
    // binding/assignment，只保证不误删。
    std::size_t removed = 0;
    for (auto it = entries_.begin(); it != entries_.end();) {
        const auto found =
            std::find(authoritative_online.begin(), authoritative_online.end(), it->first);
        if (found == authoritative_online.end()) {
            it = entries_.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::size_t PlayerDirectory::intake_full_report(
    const std::vector<MirrorEntry>& sessions) {
    // §6 恢复相位 intake（restore-not-kick）：报告方现存会话重建目录。
    // 已有条目保 epoch 只刷绑定/定位列（事件面静默——epoch 未变、绑定列
    // 刷新属对账修复不是变更）；无条目走 session_up 全事件面（新 epoch）。
    std::size_t taken = 0;
    for (const auto& reported : sessions) {
        if (reported.player_id == 0) {
            continue;  // 零号玩家条目非法（wire 守卫之外的双保险）
        }
        const auto it = entries_.find(reported.player_id);
        if (it != entries_.end()) {
            Entry& entry = it->second;
            entry.binding.session_id = reported.session_id;
            entry.binding.gateway_id = reported.gateway_id;
            entry.assignment = reported.assignment;
            entry.zone_id = reported.zone_id;
            entry.state = EntryState::Online;
            entry.deadline_tick = 0;
            ++taken;
            continue;
        }
        SessionBinding binding;
        binding.session_id = reported.session_id;
        binding.gateway_id = reported.gateway_id;
        session_up(reported.player_id, binding, reported.assignment, reported.zone_id);
        ++taken;
    }
    return taken;
}

const PlayerDirectory::Entry* PlayerDirectory::find(std::uint64_t player_id) const {
    const auto it = entries_.find(player_id);
    return it != entries_.end() ? &it->second : nullptr;
}

std::vector<std::uint64_t> PlayerDirectory::online_ids() const {
    std::vector<std::uint64_t> ids;
    ids.reserve(entries_.size());
    for (const auto& [player_id, entry] : entries_) {
        if (entry.state == EntryState::Online) {
            ids.push_back(player_id);
        }
    }
    return ids;
}

std::vector<std::uint64_t> PlayerDirectory::suspended_ids() const {
    std::vector<std::uint64_t> ids;
    for (const auto& [player_id, entry] : entries_) {
        if (entry.state == EntryState::Suspended) {
            ids.push_back(player_id);
        }
    }
    return ids;
}

} // namespace apollo::game::session
