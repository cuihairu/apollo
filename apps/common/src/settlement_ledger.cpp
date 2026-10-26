#include "squad_instance/settlement_ledger.hpp"

namespace squad_instance {

void SettlementLedger::begin(std::vector<std::uint64_t> roster) {
    roster_ = std::move(roster);
    entries_.clear();
    for (const auto player : roster_) entries_.emplace(player, Entry{});
}

void SettlementLedger::set_credential_sink(CredentialSink sink) {
    credential_sink_ = std::move(sink);
}

void SettlementLedger::set_business_sink(BusinessSink sink) {
    business_sink_ = std::move(sink);
}

SettlementLedger::ApplyError SettlementLedger::apply(std::uint64_t player_id,
                                                     std::int64_t score) {
    auto it = entries_.find(player_id);
    if (it == entries_.end()) return ApplyError::UnknownPlayer;
    if (it->second.applied) return ApplyError::AlreadyApplied;
    it->second.attempted = true;
    // 凭据持久提交先于业务入账（真实奖励持久化）：未提交 = 挂账待补，
    // 绝不无凭据入账；重试同路径重提，提交方须幂等。
    if (credential_sink_ && !credential_sink_(player_id, score)) {
        it->second.score = score;
        return ApplyError::AvatarUnavailable;
    }
    // 业务受理出口（P1 结算迁入）：锚点写权在 Base——落账唯一路径 = 分值经
    // 958/959 受理 wire 走返回业务请求；本域不解析本地锚点（todo L84 跨界
    // 纪律）。Ok = 已受理收口；其余 = 挂账待补（分值保留，retry 同路径重
    // 提）。未装配业务面 = 无落账路径，同样挂账不落（分值保留待补账）。
    if (!business_sink_) {
        it->second.score = score;
        return ApplyError::AvatarUnavailable;
    }
    const auto rc = business_sink_(player_id, score);
    if (rc != ApplyError::Ok) {
        it->second.score = score;
        return rc;
    }
    it->second.applied = true;
    it->second.score = 0;
    return ApplyError::Ok;
}

std::size_t SettlementLedger::retry() {
    std::size_t credited = 0;
    for (auto& [player_id, entry] : entries_) {
        // 只补挂账（曾尝试未落）；从未受理的条目留给 battle 结算或显式落账。
        if (entry.applied || !entry.attempted) continue;
        if (apply(player_id, entry.score) == ApplyError::Ok) ++credited;
    }
    return credited;
}

void SettlementLedger::restore(const std::vector<std::uint64_t>& applied_players) {
    for (const auto player : applied_players) {
        auto it = entries_.find(player);
        if (it != entries_.end()) {
            it->second.applied = true;
            it->second.score = 0;
        }
    }
}

bool SettlementLedger::settled(std::uint64_t player_id) const noexcept {
    const auto it = entries_.find(player_id);
    return it != entries_.end() && it->second.applied;
}

SettlementLedger::Snapshot SettlementLedger::snapshot() const noexcept {
    Snapshot snap;
    snap.roster_size = roster_.size();
    for (const auto& [player_id, entry] : entries_) {
        if (entry.applied) {
            ++snap.applied;
        } else if (entry.attempted) {
            ++snap.pending;
        }
    }
    snap.complete = snap.roster_size > 0 && snap.applied == snap.roster_size;
    return snap;
}

void SettlementLedger::on_reward(std::uint64_t player_id, std::int64_t score) {
    apply(player_id, score);
}

} // namespace squad_instance
