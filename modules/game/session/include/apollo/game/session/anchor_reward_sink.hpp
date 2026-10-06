#pragma once

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/session/player_anchor.hpp"

#include <cstdint>
#include <string_view>

namespace apollo::game::session {

// AnchorRewardSink：battle 结算奖励 → Anchor 长期态的真实落账（P2-4「奖励
// 单向落 Anchor」接线，player-object-model §3）。P2-2 骨架期 Instance 闭环
// 用的是测试态 MemoryRewardSink；本类是生产语义的第一实现——score 累计入
// Progress「exp」键并 mark_dirty（随 SaveQueue/write-behind 落档）。
//
// battle 只出不进（IRewardSink 单向口）：本 sink 不回写 battle 任何状态。
// 逐玩家落账：score 记到 owner 之外的 player_id 时忽略（骨架期 sink 与单
// Anchor 一一绑定；多玩家结算分账随 P2 battle 结算面扩展）。
class AnchorRewardSink : public apollo::game::battle::IRewardSink {
public:
    static constexpr std::string_view kExpKey = "exp";

    explicit AnchorRewardSink(PlayerAnchor& anchor)
        : anchor_(anchor) {
    }

    void on_reward(std::uint64_t player_id, std::int64_t score) override {
        if (player_id != anchor_.player_id() || score == 0) {
            return;
        }
        anchor_.add_progress(kExpKey, score);
    }

private:
    PlayerAnchor& anchor_;
};

} // namespace apollo::game::session
