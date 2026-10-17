#pragma once

// 结算幂等账本（P2-4「结算通过既有奖励/持久化接口幂等落账」）：
// battle 结算段（IRewardSink 单向口）的副本落点——逐玩家经 AnchorRewardSink
// 语义（exp 键 + mark_dirty 随 SaveQueue/write-behind 落档）入锚点长期态。
//
// 幂等口径：(instance, player) 一条账——applied 后重复 apply / 重试 / 恢复
// 重放一律拒绝，不双记；锚点暂不可得（离线/迁移中）则挂账不落，重试只补
// 未落条目；restore 供宿主重启后按持久化痕迹回填已落标记（防重放双记）。

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/session/anchor_reward_sink.hpp"
#include "apollo/game/session/player_anchor.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace squad_instance {

// 锚点解析：player_id → PlayerAnchor*（null = 暂不可落）。宿主按自己的
// AnchorManager/PlayerStore 面装配；未装配 = 全部挂账待补。
using AnchorResolveFn = std::function<apollo::game::session::PlayerAnchor*(std::uint64_t)>;

class SettlementLedger : public apollo::game::battle::IRewardSink {
public:
    enum class ApplyError : std::uint32_t {
        Ok = 0,
        UnknownPlayer,     // 非结算名册成员
        AlreadyApplied,    // 已落账（幂等拒绝）
        AnchorUnavailable, // 锚点暂不可得（挂账，待重试）
    };

    struct Snapshot {
        std::size_t roster_size = 0;
        std::size_t applied = 0;
        std::size_t pending = 0;  // 曾尝试未落（挂账待补）
        bool complete = false;
    };

    // 开账：结算名册 = 受理快照成员（全序由调用面保证 = 名册顺序）。
    void begin(std::vector<std::uint64_t> roster, AnchorResolveFn resolver);

    // 单条落账：名册校验 → 锚点解析 → AnchorRewardSink 语义入账 → 标记已落。
    // applied 后重复调用返回 AlreadyApplied（幂等核心）。
    ApplyError apply(std::uint64_t player_id, std::int64_t score);

    // 补账：重试挂账条目（曾尝试落账但锚点未得），返回本次补账人数；
    // 已落与从未受理的条目不动。
    std::size_t retry();

    // 恢复面：宿主按持久化痕迹回填已落标记，后续重放一律幂等拒绝。
    void restore(const std::vector<std::uint64_t>& applied_players);

    [[nodiscard]] bool settled(std::uint64_t player_id) const noexcept;
    [[nodiscard]] Snapshot snapshot() const noexcept;

    // IRewardSink：battle 结算段逐玩家回调（奖励单向，不回写 battle）。
    void on_reward(std::uint64_t player_id, std::int64_t score) override;

private:
    struct Entry {
        std::int64_t score = 0;  // 挂账分值（锚点可得时落账）
        bool attempted = false;  // 曾受理（含挂账失败）
        bool applied = false;
    };
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::vector<std::uint64_t> roster_;
    AnchorResolveFn resolver_;
};

} // namespace squad_instance
