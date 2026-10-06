#pragma once

// BattleRuntime：战斗运行时（P2-2，improvement-plan 场景树 Scene └─ BattleRuntime
// └─ ECS 定位；battle-determinism 四约束的可测骨架）
//
// 职责边界：
// - 生命周期对应 P2 出口判据① create→enter→battle→reward→leave 五段，状态机
//   单向（Created→Entering→Battling→Rewarding→Finished），非法跃迁拒绝；
// - 判定域唯一（约束①）：tick() 是同步纯推进段——子流派生、事件产生、hash
//   折叠全在调用线程内完成，无异步结果进判定；
// - 迭代序（约束③）：模拟段（实体 on_update）不要求次序；结算段（事件排序、
//   奖励落账）按 entity_id/player_id 全序执行；
// - 随机数（约束④）：无全局游标，任意 (tick, stream_id) 独立派生子流；
// - reward 单向（player-object-model §3）：BattleRuntime 只经 IRewardSink 产出
//   奖励、不持有 Anchor、不读玩家态；Anchor 字段与真实落账接线随 P2-4。
//
// 复算回放：同四元组（world_seed + 同输入序列 + 同实体拓扑）必得同事件流与
// 同 hash 链——test_battle_runtime 以此为判定。

#include "apollo/base/rng.hpp"
#include "apollo/game/battle/battle_replay.hpp"
#include "apollo/game/battle/battle_system.hpp"

#include <cstdint>
#include <vector>

namespace apollo::game::battle {

// RNG 件落点（battle-determinism §8-P1）：Pcg32/StreamId/derive_substream 随
// modules/base（全游戏域共消费），battle 经 apollo::base 直接取用，不反向持有。

// 战斗阶段（对应出口判据①五段；leave 段终态记 Finished）
enum class BattlePhase : std::uint8_t {
    Created = 0,   // create：已建，未收玩家
    Entering = 1,  // enter：收集参战玩家
    Battling = 2,  // battle：tick 推进
    Rewarding = 3, // reward：结算落账（骨架期同步瞬时段，异步发奖队列随 P2-4）
    Finished = 4,  // leave：散场终态
};

// 奖励单向落账口：battle 只出不进。实现方（P2-4 起）接 PlayerAnchor 落账，
// 本骨架期测试以内存 sink 承接。
class IRewardSink {
public:
    virtual ~IRewardSink() = default;
    virtual void on_reward(std::uint64_t player_id, std::int64_t score) = 0;
};

// 事件种类（域内编号；语义映射随玩法批扩充，编号入 glossary 口径）
enum class EventKind : std::uint32_t {
    Miss = 0,   // 未命中（combat_roll 未过线）
    Hit = 1,    // 命中
    Reward = 2, // 奖励落账
};

// 单条玩家意图（输入序列元素；协议化随 P2-3 契约修订，骨架期 uint64 编码）
struct BattleInput {
    std::uint32_t tick = 0;     // 意图所属 tick
    std::uint64_t player_id = 0;
    std::uint32_t opcode = 0;   // 1=攻击（骨架期唯一判定语义）
};

class BattleRuntime {
public:
    BattleRuntime(std::uint64_t battle_id, std::uint64_t world_seed,
                  IRewardSink* reward_sink = nullptr);

    // —— 生命周期（单向，非法跃迁返回 false）——
    bool enter_player(std::uint64_t player_id); // Created/Entering → Entering
    bool begin();                               // Entering → Battling（须有参战者）
    bool tick(std::uint32_t tick_index,
              const std::vector<BattleInput>& inputs = {}); // Battling 推进一步
    bool finish();                              // Battling → Rewarding → Finished

    BattlePhase phase() const { return phase_; }
    std::uint64_t battle_id() const { return battle_id_; }
    std::uint64_t world_seed() const { return world_seed_; }
    std::int64_t last_tick() const { return last_tick_; } // -1=尚无 tick

    // hash 链当前值（每 tick 末滚动折叠；Finished 后即整场指纹，可逐 tick 对照）
    std::uint64_t hash_chain() const { return hash_chain_; }
    const std::vector<BattleEvent>& events() const { return events_; }

    // 确定性子流（约束④）：按域取用，不得跨域共享；纯函数派生，可任意时点重建
    apollo::base::Pcg32 substream(std::uint32_t tick, apollo::base::StreamId stream) const {
        return apollo::base::derive_substream(world_seed_, tick, stream);
    }

    // 参战实体容器（模拟段驱动面；实体拓扑属四元组外的场景供给，测试须同拓扑）
    BattleSystem& system() { return system_; }
    const BattleSystem& system() const { return system_; }

    // 参战玩家（enter 段收集；不排序，排序发生在结算段）
    const std::vector<std::uint64_t>& players() const { return players_; }

private:
    void record_event(std::uint32_t tick, std::uint64_t entity_id,
                      EventKind kind, std::int32_t value);
    void fold_tick_events(); // 结算段：规范序排序 + 折叠 hash 链（约束③）
    void settle_rewards();   // Rewarding 段：按 player_id 全序逐个落 sink

    std::uint64_t battle_id_;
    std::uint64_t world_seed_;
    IRewardSink* reward_sink_; // 非所有权观察指针（单向出口）
    BattlePhase phase_ = BattlePhase::Created;
    BattleSystem system_;
    std::vector<std::uint64_t> players_;
    std::vector<BattleEvent> pending_; // 本 tick 未结算事件
    std::vector<BattleEvent> events_;  // 全场事件流（tick 内规范序）
    std::uint64_t hash_chain_ = 0;
    std::int64_t last_tick_ = -1; // 严格递增判据（含 tick 0 不可重放）
};

} // namespace apollo::game::battle
