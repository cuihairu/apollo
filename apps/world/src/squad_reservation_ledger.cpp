// 小队容量预留账本实现（D11 批②）：语义/线程口径见头注。
#include "world_app/squad_reservation_ledger.hpp"

namespace world_app {

SquadReservationLedger::SquadReservationLedger(const SquadCapacityView& capacity_view,
                                               std::uint64_t grant_window_ms)
    : view_(&capacity_view), grant_window_ms_(grant_window_ms) {}

SquadCapacityGrant SquadReservationLedger::request(const SquadCapacityRequest& req,
                                                   std::uint64_t now_ms) {
    SquadCapacityGrant reply;
    reply.request_id = req.request_id;

    if (!view_->instance_known(req.instance_id)) {
        reply.code = 2;  // 实例未知
        return reply;
    }
    if (view_->instance_draining(req.instance_id)) {
        reply.code = 3;  // 排空中
        return reply;
    }

    const std::int32_t need = req.slots;
    if (need <= 0) {
        reply.code = 1;  // 无容量（非法申请不记账）
        return reply;
    }

    // 有效余量 = 视图原值 − 本账 hold（hold 是本账本自铸的占用，视图不知）。
    const std::int32_t held = held_slots(req.cell_id);
    const std::int32_t free = view_->free_slots(req.cell_id) - held;
    if (need > free) {
        reply.code = 1;  // 无容量
        return reply;
    }

    Entry entry;
    entry.view.reservation_id = "sq-" + std::to_string(next_id_++);
    entry.view.party_id = req.party_id;
    entry.view.instance_id = req.instance_id;
    entry.view.cell_id = req.cell_id;
    entry.view.roster_hash = req.roster_hash;
    entry.view.slots = need;
    entry.view.held = need;
    entry.view.expires_at = now_ms + grant_window_ms_;
    reservations_[entry.view.reservation_id] = entry;
    holds_[req.cell_id] = held + need;

    reply.granted = true;
    reply.code = 0;  // 已预留
    reply.reservation_id = entry.view.reservation_id;
    reply.expires_at = entry.view.expires_at;
    return reply;
}

bool SquadReservationLedger::validate(const std::string& reservation_id,
                                      std::uint64_t now_ms) const {
    const auto it = reservations_.find(reservation_id);
    if (it == reservations_.end()) {
        return false;
    }
    return now_ms < it->second.view.expires_at;
}

bool SquadReservationLedger::consume(const std::string& reservation_id,
                                     std::uint64_t now_ms) {
    auto it = reservations_.find(reservation_id);
    if (it == reservations_.end()) {
        return false;
    }
    if (now_ms >= it->second.view.expires_at) {
        // 过期触达即回收（签发窗已过的凭证消费不成立）。
        holds_[it->second.view.cell_id] -= it->second.view.held;
        reservations_.erase(it);
        return false;
    }
    Entry& entry = it->second;
    if (entry.view.held <= 0) {
        return false;  // 已销账（防御：正常路径 hold 清零即 erase）
    }
    entry.view.held -= 1;
    holds_[entry.view.cell_id] -= 1;
    if (entry.view.held == 0) {
        reservations_.erase(it);
    }
    return true;
}

std::size_t SquadReservationLedger::sweep_expired(std::uint64_t now_ms) {
    std::vector<std::string> expired;
    for (const auto& pair : reservations_) {
        if (now_ms >= pair.second.view.expires_at) {
            expired.push_back(pair.first);
        }
    }
    for (const auto& id : expired) {
        const auto it = reservations_.find(id);
        holds_[it->second.view.cell_id] -= it->second.view.held;
        reservations_.erase(it);
    }
    return expired.size();
}

std::int32_t SquadReservationLedger::held_slots(std::uint64_t cell_id) const {
    const auto it = holds_.find(cell_id);
    return it == holds_.end() ? 0 : it->second;
}

bool SquadReservationLedger::find(const std::string& reservation_id,
                                  Reservation& out) const {
    const auto it = reservations_.find(reservation_id);
    if (it == reservations_.end()) {
        return false;
    }
    out = it->second.view;
    return true;
}

} // namespace world_app
