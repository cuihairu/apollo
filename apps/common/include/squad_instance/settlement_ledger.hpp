#pragma once

// 结算幂等账本（P2-4「结算通过既有奖励/持久化接口幂等落账」）：
// battle 结算段（IRewardSink 单向口）的副本落点——逐玩家经 AnchorRewardSink
// 语义（exp 键 + mark_dirty 随 SaveQueue/write-behind 落档）入锚点长期态。
//
// 幂等口径：(instance, player) 一条账——applied 后重复 apply / 重试 / 恢复
// 重放一律拒绝，不双记；锚点暂不可得（离线/迁移中）则挂账不落，重试只补
// 未落条目；restore 供宿主重启后按持久化痕迹回填已落标记（防重放双记）。
//
// 凭据口径（真实奖励持久化）：配了 CredentialSink 时每条落账先经其持久提交
// 凭据再入锚点——未提交 = 挂账待补，绝不无凭据落账（重试同路径重提，凭据
// 提交须幂等）。凭据已提交而锚点未入账即崩溃的窗口内重放一律拒绝：至多
// 一次，不双记（与「已发未确认请求不跨代次自动重放」同口径）。

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

    // 凭据出口（真实奖励持久化）：落账前先经此持久提交 (player, score)
    // 凭据，返回 false = 未持久提交（挂账待补）——绝不无凭据落账。重试同
    // 路径重提，提交方须幂等。须在 begin 之后、结算流之前设置；未设置
    // （测试桩兼容）= 无凭据面，落账照旧。
    using CredentialSink = std::function<bool(std::uint64_t player_id,
                                              std::int64_t score)>;
    void set_credential_sink(CredentialSink sink);

    // 业务受理出口（P1 结算迁入；返回业务请求面）：锚点写权在 Base 时，
    // 落账不走本地锚点解析——分值经 958/959 受理 wire 入 Base 锚点长期态。
    // 返回 Ok = 已落（applied 收口）；其余 = 挂账待补（分值保留，retry 同
    // 路径重提）。设置后 resolver 不再消费（World 无锚点可解析）。
    using BusinessSink = std::function<ApplyError(std::uint64_t player_id,
                                                  std::int64_t score)>;
    void set_business_sink(BusinessSink sink);

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
    CredentialSink credential_sink_;
    BusinessSink business_sink_;
};

} // namespace squad_instance
