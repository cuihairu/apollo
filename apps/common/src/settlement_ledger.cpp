#include "squad_instance/settlement_ledger.hpp"

namespace squad_instance {

void SettlementLedger::begin(std::vector<std::uint64_t> roster, AnchorResolveFn resolver) {
    roster_ = std::move(roster);
    resolver_ = std::move(resolver);
    entries_.clear();
    for (const auto player : roster_) entries_.emplace(player, Entry{});
}

void SettlementLedger::set_credential_sink(CredentialSink sink) {
    credential_sink_ = std::move(sink);
}

SettlementLedger::ApplyError SettlementLedger::apply(std::uint64_t player_id,
                                                     std::int64_t score) {
    auto it = entries_.find(player_id);
    if (it == entries_.end()) return ApplyError::UnknownPlayer;
    if (it->second.applied) return ApplyError::AlreadyApplied;
    it->second.attempted = true;
    // 凭据持久提交先于锚点入账（真实奖励持久化）：未提交 = 挂账待补，
    // 绝不无凭据落账；重试同路径重提，提交方须幂等。
    if (credential_sink_ && !credential_sink_(player_id, score)) {
        it->second.score = score;
        return ApplyError::AnchorUnavailable;
    }
    // 锚点缺失挂账不落：分值保留，待重试/补账收口。
    auto* anchor = resolver_ ? resolver_(player_id) : nullptr;
    // 冻结锚点同挂账（P3-6 迟到输入/迟到结算拒绝）：Transferring = 写权已
    // 交新进程——本域不得再落账（否则目标侧重算即双记）；分值保留，待
    // 权威侧裁决（跨域结算重定向属后续跨服条目）。
    if (anchor && anchor->state() ==
                      apollo::game::session::AnchorState::Transferring) {
        it->second.score = score;
        return ApplyError::AnchorUnavailable;
    }
    if (!anchor) {
        it->second.score = score;
        return ApplyError::AnchorUnavailable;
    }
    // 既有奖励接口：AnchorRewardSink 语义（exp 键 + mark_dirty 随
    // SaveQueue 落档；零分值跳过入账——无档案脏位）。落账后即收口。
    apollo::game::session::AnchorRewardSink sink(*anchor);
    sink.on_reward(player_id, score);
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
