#include "apollo/game/battle/battle_runtime.hpp"

#include <algorithm>

namespace apollo::game::battle {

// 骨架期判定常量：combat_roll 过线阈值与伤害基数（玩法数值随玩法批调，
// 语义不变——判定只依赖 (world_seed, tick, CombatRoll) 子流，约束④）
namespace {
constexpr double kCombatRollThreshold = 0.5;
constexpr std::int32_t kHitDamageBase = 10;
constexpr float kTickDelta = 0.1f; // 10Hz 口径（clock-and-time §3）
} // namespace

BattleRuntime::BattleRuntime(std::uint64_t battle_id, std::uint64_t world_seed,
                             IRewardSink* reward_sink)
    : battle_id_(battle_id), world_seed_(world_seed), reward_sink_(reward_sink) {}

bool BattleRuntime::enter_player(std::uint64_t player_id) {
    if (phase_ != BattlePhase::Created && phase_ != BattlePhase::Entering) {
        return false;
    }
    if (player_id == 0) {
        return false;
    }
    const bool dup =
        std::find(players_.begin(), players_.end(), player_id) != players_.end();
    if (dup) {
        return false;
    }
    players_.push_back(player_id);
    phase_ = BattlePhase::Entering;
    return true;
}

bool BattleRuntime::begin() {
    if (phase_ != BattlePhase::Entering || players_.empty()) {
        return false;
    }
    phase_ = BattlePhase::Battling;
    last_tick_ = -1;
    return true;
}

bool BattleRuntime::tick(std::uint32_t tick_index,
                         const std::vector<BattleInput>& inputs) {
    if (phase_ != BattlePhase::Battling) {
        return false;
    }
    // 复算重演口径：tick 严格递增（含 tick 0 不可重放），跳拍/回退即失配
    // （battle-determinism §3 逐 tick 对照前提；丢帧追赶由上层场景 tick
    // 负责，不在判定域内补拍）
    if (static_cast<std::int64_t>(tick_index) <= last_tick_) {
        return false;
    }
    last_tick_ = tick_index;

    // —— 模拟段（约束③：不要求次序）——
    system_.update(kTickDelta);

    // —— 判定段（约束①：同步；约束④：子流派生）——
    // 输入先按 (player_id, opcode) 排序：同 tick 输入集的到达次序不影响结果。
    auto ordered = inputs;
    std::sort(ordered.begin(), ordered.end(),
              [](const BattleInput& a, const BattleInput& b) {
                  if (a.player_id != b.player_id) return a.player_id < b.player_id;
                  return a.opcode < b.opcode;
              });
    for (const auto& in : ordered) {
        if (in.tick != tick_index) {
            continue; // 意图错拍：留待其所属 tick（不进本 tick 判定域）
        }
        if (in.opcode != 1) {
            continue; // 骨架期仅攻击语义
        }
        auto rng = substream(tick_index, apollo::base::StreamId::CombatRoll);
        const double roll = rng.next_double();
        if (roll < kCombatRollThreshold) {
            record_event(tick_index, in.player_id, EventKind::Hit,
                         kHitDamageBase + static_cast<std::int32_t>(rng.next_uint32() % 90));
        } else {
            record_event(tick_index, in.player_id, EventKind::Miss, 0);
        }
    }
    fold_tick_events();
    return true;
}

bool BattleRuntime::finish() {
    if (phase_ != BattlePhase::Battling) {
        return false;
    }
    phase_ = BattlePhase::Rewarding;
    settle_rewards();
    fold_tick_events();
    phase_ = BattlePhase::Finished;
    return true;
}

void BattleRuntime::record_event(std::uint32_t tick, std::uint64_t entity_id,
                                 EventKind kind, std::int32_t value) {
    BattleEvent e;
    e.tick = tick;
    e.entity_id = entity_id;
    e.kind = static_cast<std::uint32_t>(kind);
    e.value = value;
    pending_.push_back(e);
}

void BattleRuntime::fold_tick_events() {
    // 结算段（约束③）：本 tick 事件按规范序（entity_id/kind/value）排序后
    // 一次性折叠进链——同 tick 事件全集的 hash 与产生次序无关。
    std::sort(pending_.begin(), pending_.end(), battle_event_canonical_less);
    for (const auto& e : pending_) {
        hash_chain_ = hash_chain_step(hash_chain_, e);
    }
    events_.insert(events_.end(), pending_.begin(), pending_.end());
    pending_.clear();
}

void BattleRuntime::settle_rewards() {
    // Rewarding 段（约束③同型纪律）：按 player_id 全序落账；奖励值=该玩家
    // 命中伤害合计（骨架口径，玩法数值随玩法批）。
    auto ordered = players_;
    std::sort(ordered.begin(), ordered.end());
    const std::uint32_t settle_tick =
        last_tick_ < 0 ? 0 : static_cast<std::uint32_t>(last_tick_);
    for (const auto pid : ordered) {
        std::int64_t score = 0;
        for (const auto& e : events_) {
            if (e.entity_id == pid && e.kind == static_cast<std::uint32_t>(EventKind::Hit)) {
                score += e.value;
            }
        }
        record_event(settle_tick, pid, EventKind::Reward, static_cast<std::int32_t>(score));
        if (reward_sink_) {
            reward_sink_->on_reward(pid, score);
        }
    }
}

} // namespace apollo::game::battle
