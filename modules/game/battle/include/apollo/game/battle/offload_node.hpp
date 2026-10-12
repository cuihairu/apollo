#pragma once

// InstanceOffloadNode：副本进程侧骨架（P3-1 批 2；term-contract §1.3 副本
// 进程行：单 scene 单开进程——本节点一次承载一个 instance）。
//
// 职责（§3 BattleInstance Manager 对应物，骨架口径）：
//   - spawn_request 受理：ack-on-accept 回执（ADR-017；骨架期恒受理——
//     满载/模板未注册拒绝面随 machined 控制面批）；
//   - BattleRuntime 驱动：输入按实例 tick 轴排队，tick() 推进；参战玩家按
//     输入首现懒入场（enter 段收集），首 tick 自动 begin；每 tick 出
//     battle_state_sync 投影（snapshot_seq 严格递增）；
//   - 结算：settle() → 五段收尾（finish）→ battle_result 落回（replay_hash =
//     hash 链尾，跨机回放校验）→ 自灭（finished()，宿主收割——teardown =
//     结算后自灭 §3）；
//   - 崩溃即作废（§7(a)）：abandon() 崩溃注入点——不出落回（生产死亡面 =
//     machined 死亡事件，宿主侧 notify_crash 消费）。
//
// 落回捕获：本类即 runtime 的 IRewardSink——settle_rewards 按 player_id 全序
// 逐个落本类（含 0 分），进 result 平行数组；Zone 侧真实落账在宿主
// （AnchorRewardSink），实例侧只出不进。
//
// 单线程纪律（同 discovery）：on_envelope/tick/settle 全部由宿主定时器/泵
// 线程驱动，无内锁；节拍（20-50ms 可配）由调用方注入，不依赖钟。

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/battle/offload_wire.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace apollo::game::battle::offload {

class InstanceOffloadNode : public apollo::game::battle::IRewardSink {
public:
    // uplink：上行传输（实例→Zone 方向）；world_seed：种子注入点（跨机同
    // binary 同确定域 §6；四元组复算的 seed 源随 baseappmgr 域接入定案）
    explicit InstanceOffloadNode(IOffloadTransport& uplink,
                                 std::uint64_t world_seed = 0);

    // —— 上行信封入口（宿主泵直呼）——
    // 受理 spawn_request（一次）与 battle_input；余族/非法跃迁 → false。
    bool on_envelope(const OffloadEnvelope& envelope);

    // —— 实例 tick 轴驱动（宿主定时器注入节拍）——
    // 消费到拍输入（拍前滞留、拍后迟到丢弃）；begin 后每拍出 state_sync。
    bool tick(std::uint64_t tick);

    // —— 结算收尾 → battle_result → 自灭 ——
    bool settle();

    // 崩溃注入点（§7(a)：不出落回；宿主经死亡面感知）
    void abandon();

    bool spawned() const { return spawned_; }
    bool finished() const { return finished_; }   // 已结算自灭（宿主收割）
    bool abandoned() const { return abandoned_; } // 已作废
    std::uint64_t instance_id() const { return instance_id_; }
    std::int32_t tick_interval_ms() const { return tick_interval_ms_; }
    std::int64_t last_tick() const { return last_tick_; }
    std::uint64_t snapshot_seq() const { return snapshot_seq_; }
    std::size_t pending_inputs() const { return inbox_.size(); }

    // IRewardSink：捕获结算奖励进落回平行数组（settle_rewards 全序调进）
    void on_reward(std::uint64_t player_id, std::int64_t score) override;

private:
    bool ensure_begun();

    IOffloadTransport& uplink_;
    std::uint64_t world_seed_;
    std::uint64_t instance_id_ = 0;
    std::int32_t tick_interval_ms_ = 0;
    bool spawned_ = false;
    bool begun_ = false;
    bool finished_ = false;
    bool abandoned_ = false;
    std::int64_t last_tick_ = -1;
    std::uint64_t snapshot_seq_ = 0;
    std::unique_ptr<apollo::game::battle::BattleRuntime> runtime_;
    std::vector<BattleInputMsg> inbox_; // 待推进输入（对齐实例 tick 轴）
    std::vector<std::int64_t> result_player_ids_;
    std::vector<std::int64_t> result_scores_;
};

} // namespace apollo::game::battle::offload
