#pragma once

// InstanceOffloadHost：offload 需求方/Zone 侧骨架（P3-1 批 2；
// battle-instance-offload §4 拍板 (b) 按需 spawn 请求协议 + ADR-017
// ack-on-accept 相容注记：受理点回执 ≠ 就绪——就绪经目录 Advertise 可见）。
//
// 生命周期（单向，非法跃迁拒绝——纪律同 BattlePhase/Instance 八态）：
//   Requesting →(spawn_reply accepted=1)→ Accepted
//              →(notify_ready：目录 Advertise 消费钩子)→ Running
//              →(battle_state_sync 水位推进)→ Running …
//              →(battle_result)→ Settled（终态：teardown = 结算后自灭 §3）
//   Requesting →(spawn_reply accepted=0)→ Rejected（终态：满载/模板未注册）
//   Requesting/Accepted/Running →(notify_crash：machined 死亡事件消费钩子)
//                              → Voided（终态：§7(a) 崩溃即作废，落回不再受理）
//
// 落回（Result Dispatcher 单向，§3）：battle_result.player_ids/scores 逐玩家
// 经 IRewardSink 语义出账（Zone 侧生产实现 = AvatarRewardSink，P2-4）；battle
// 只出不进——本类不回写实例任何状态。
//
// 单线程纪律（同 discovery）：request/notify/on_envelope 全部由驱动方线程
// 泵，无内锁；传输底座经 IOffloadTransport 注入（M1 换底座不动本类）。

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/battle/offload_wire.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace apollo::game::battle::offload {

class InstanceOffloadHost {
public:
    enum class InstancePhase : std::uint8_t {
        Requesting = 0, // spawn_request 已发，未收受理回执
        Accepted = 1,   // 受理回执已收（ack-on-accept），实例拉起中
        Running = 2,    // 就绪后（notify_ready），输入可注入、投影/落回可收
        Settled = 3,    // 结算落回已收（终态：实例自灭，宿主收割条目）
        Rejected = 4,   // 受理拒绝（终态）
        Voided = 5,     // 崩溃作废（终态：§7(a)）
    };

    // downlink：下行传输（Zone→实例方向）；result_sink：落回出账口
    // （非所有权观察指针；生产 = Zone 侧 AvatarRewardSink）
    explicit InstanceOffloadHost(IOffloadTransport& downlink,
                                 apollo::game::battle::IRewardSink* result_sink = nullptr);

    // —— 按需 spawn 请求协议（§4(b)）——
    // 发起拉起；instance_id 重复或传输拒发 → false（不发不记）。
    bool request_spawn(const InstanceSpawnRequest& request);

    // —— 就绪/崩溃消费钩子（生产由目录面 / machined 死亡事件驱动）——
    bool notify_ready(std::uint64_t instance_id); // Accepted → Running（余皆拒）
    bool notify_crash(std::uint64_t instance_id); // 未终结 → Voided（终态拒）

    // —— 数据面（Running 才受理）——
    bool send_battle_input(const BattleInputMsg& input);

    // —— 下行信封入口（传输泵直呼）——
    // 只受理副本实例族（is_offload_msg）；载荷非法/跃迁非法 → false。
    bool on_envelope(const OffloadEnvelope& envelope);

    // 查询
    std::optional<InstancePhase> phase(std::uint64_t instance_id) const;
    std::uint64_t last_snapshot_seq(std::uint64_t instance_id) const; // 0=未收投影
    std::size_t tracked() const { return instances_.size(); }

private:
    struct InstanceState {
        InstancePhase phase = InstancePhase::Requesting;
        std::uint64_t snapshot_seq = 0; // 投影水位（严格递增，attribute-sync 同源）
    };

    IOffloadTransport& downlink_;
    apollo::game::battle::IRewardSink* result_sink_;
    std::unordered_map<std::uint64_t, InstanceState> instances_;
};

} // namespace apollo::game::battle::offload
