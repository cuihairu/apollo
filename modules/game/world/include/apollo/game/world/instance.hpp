#pragma once

#include "apollo/game/battle/battle_runtime.hpp"
#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/avatar.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace apollo::game::world {

class Scene;

// Instance（副本——契约 §1.1：「一次玩法开启的场景实例：生命周期随玩法起止」）。
//
// 八态生命周期（任务书 §9 / improvement-plan P0-3，逐态落字段 + 转换校验表）：
//
//   Create ──initialize──▶ Initialize ──ready──▶ Waiting ──start──▶ Running
//   Running ──finish──▶ Finishing ──settle──▶ Rewarding ──drain──▶ Draining
//   Draining ──destroy──▶ Destroyed（终态，不可逆）
//
// 校验表（is_transition_valid）：只有上表相邻推进合法；任何回退/跳段均拒绝
// （八态是单向生命周期，异常走 finish 而非回退）。Destroyed 后所有转换拒绝。
// Instance 是「Scene 的玩法生命周期载体」：Scene 是空间运行时（P0-3 拆分后），
// Instance 管玩法起止与在场玩家集合。
class Instance {
public:
    using InstanceId = std::uint64_t;  // P0-3 起由 Instance 自持（原 MapInstance::InstanceId）

    enum class State : std::uint8_t {
        Create = 0,
        Initialize,
        Waiting,
        Running,
        Finishing,
        Rewarding,
        Draining,
        Destroyed,
    };

    // 转换校验表：仅相邻推进合法（含 Destroyed 终态封死）
    static bool is_transition_valid(State from, State to);

    static std::string_view to_string(State state);

    Instance(InstanceId id, std::uint64_t scene_id, std::string name = {});

    [[nodiscard]] InstanceId id() const noexcept;
    [[nodiscard]] std::uint64_t scene_id() const noexcept;
    [[nodiscard]] const std::string& name() const noexcept;
    [[nodiscard]] State state() const noexcept;

    // 状态推进（走校验表；非法转换返回 false 且状态不变——可观察可校验）
    bool initialize();
    bool ready();
    bool start();
    bool finish();
    bool settle();
    bool drain();
    bool destroy();
    [[nodiscard]] bool is_destroyed() const noexcept;

    // ---- 玩家进出（instance.enter / instance.leave——任务书 §27 API 段）----
    // enter：仅 Waiting/Running 两态放行（等待可预进、运行中可进；其余态拒绝）。
    // 返回 false（含重复进入）不产生副作用。
    bool enter(apollo::game::core::PlayerId player_id);
    bool leave(apollo::game::core::PlayerId player_id);
    [[nodiscard]] bool has_player(apollo::game::core::PlayerId player_id) const noexcept;
    [[nodiscard]] std::size_t player_count() const noexcept;
    [[nodiscard]] const std::vector<apollo::game::core::PlayerId>& players() const noexcept;

    // ---- 玩法负载挂接（P2-2，任务书 §16「Scene └─ BattleRuntime」持有树）----
    // Instance 是玩法生命周期载体（八态），BattleRuntime 是其玩法负载（五段）；
    // 一 Instance 至多一 battle。挂接窗口 = Create/Initialize/Waiting（开局装载；
    // Running 起拒绝）。奖励经 battle 的 IRewardSink 单向落账，Instance 不经手。
    bool attach_battle(std::unique_ptr<apollo::game::battle::BattleRuntime> battle);
    [[nodiscard]] apollo::game::battle::BattleRuntime* battle() noexcept;
    [[nodiscard]] const apollo::game::battle::BattleRuntime* battle() const noexcept;

    // ---- tick 计数（玩法节奏观察点；Running 态才累加）----
    void tick(double delta_seconds) noexcept;
    [[nodiscard]] std::uint64_t tick_count() const noexcept;
    [[nodiscard]] double elapsed_seconds() const noexcept;

private:
    bool transition_to(State next);

    InstanceId id_ = 0;
    std::uint64_t scene_id_ = 0;
    std::string name_;
    State state_ = State::Create;
    std::vector<apollo::game::core::PlayerId> players_;  // 保序（进场顺序可观察）
    std::unordered_set<std::uint64_t> player_index_;
    std::unique_ptr<apollo::game::battle::BattleRuntime> battle_;
    std::uint64_t tick_count_ = 0;
    double elapsed_seconds_ = 0.0;
};

using InstancePtr = std::shared_ptr<Instance>;

} // namespace apollo::game::world