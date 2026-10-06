#include "apollo/game/world/instance.hpp"

#include <algorithm>

namespace apollo::game::world {

bool Instance::is_transition_valid(State from, State to) {
    // 八态单向生命周期校验表（任务书 §9）：仅相邻推进合法。
    switch (from) {
    case State::Create:
        return to == State::Initialize;
    case State::Initialize:
        return to == State::Waiting;
    case State::Waiting:
        return to == State::Running;
    case State::Running:
        return to == State::Finishing;
    case State::Finishing:
        return to == State::Rewarding;
    case State::Rewarding:
        return to == State::Draining;
    case State::Draining:
        return to == State::Destroyed;
    case State::Destroyed:
        return false;  // 终态封死：Destroyed 后任何转换拒绝
    }
    return false;
}

std::string_view Instance::to_string(State state) {
    switch (state) {
    case State::Create:
        return "Create";
    case State::Initialize:
        return "Initialize";
    case State::Waiting:
        return "Waiting";
    case State::Running:
        return "Running";
    case State::Finishing:
        return "Finishing";
    case State::Rewarding:
        return "Rewarding";
    case State::Draining:
        return "Draining";
    case State::Destroyed:
        return "Destroyed";
    }
    return "Unknown";
}

Instance::Instance(InstanceId id, std::uint64_t scene_id, std::string name)
    : id_(id)
    , scene_id_(scene_id)
    , name_(std::move(name)) {
}

Instance::InstanceId Instance::id() const noexcept {
    return id_;
}

std::uint64_t Instance::scene_id() const noexcept {
    return scene_id_;
}

const std::string& Instance::name() const noexcept {
    return name_;
}

Instance::State Instance::state() const noexcept {
    return state_;
}

bool Instance::transition_to(State next) {
    if (!is_transition_valid(state_, next)) {
        return false;  // 非法转换：状态保持原样（调用方可见校验失败）
    }
    state_ = next;
    return true;
}

bool Instance::initialize() {
    return transition_to(State::Initialize);
}

bool Instance::ready() {
    return transition_to(State::Waiting);
}

bool Instance::start() {
    // 挂了 battle 的 instance：start 前置校验参战者收集完成（battle.begin 需非空；
    // 失败则 instance 状态不推进——八态与五段一致可观察）
    if (battle_ && !battle_->begin()) {
        return false;
    }
    return transition_to(State::Running);
}

bool Instance::finish() {
    if (!transition_to(State::Finishing)) {
        return false;
    }
    // battle 结算五段尾（Battling→Rewarding→Finished；奖励经 sink 单向落账）
    if (battle_ && battle_->phase() == apollo::game::battle::BattlePhase::Battling) {
        battle_->finish();
    }
    return true;
}

bool Instance::settle() {
    return transition_to(State::Rewarding);
}

bool Instance::drain() {
    return transition_to(State::Draining);
}

bool Instance::destroy() {
    return transition_to(State::Destroyed);
}

bool Instance::is_destroyed() const noexcept {
    return state_ == State::Destroyed;
}

bool Instance::enter(apollo::game::core::PlayerId player_id) {
    // 准入窗口：Waiting（预进）/ Running（进行中）两态放行
    if (state_ != State::Waiting && state_ != State::Running) {
        return false;
    }
    if (!player_id.is_valid()) {
        return false;
    }
    if (player_index_.count(player_id.value()) != 0) {
        return false;  // 重复进入拒绝
    }
    // battle 参战者收集同步（仅收集期 Created/Entering 转发；Battling 起进人
    // 不参战——后进者观战，骨架口径）
    if (battle_) {
        const auto phase = battle_->phase();
        if (phase == apollo::game::battle::BattlePhase::Created ||
            phase == apollo::game::battle::BattlePhase::Entering) {
            if (!battle_->enter_player(player_id.value())) {
                return false;  // 参战侧拒绝（满员等）则准入整体拒绝
            }
        }
    }
    player_index_.insert(player_id.value());
    players_.push_back(player_id);
    return true;
}

bool Instance::leave(apollo::game::core::PlayerId player_id) {
    if (player_index_.erase(player_id.value()) == 0) {
        return false;
    }
    auto it = std::find(players_.begin(), players_.end(), player_id);
    if (it != players_.end()) {
        players_.erase(it);
    }
    return true;
}

bool Instance::has_player(apollo::game::core::PlayerId player_id) const noexcept {
    return player_index_.count(player_id.value()) != 0;
}

std::size_t Instance::player_count() const noexcept {
    return players_.size();
}

const std::vector<apollo::game::core::PlayerId>& Instance::players() const noexcept {
    return players_;
}

void Instance::tick(double delta_seconds) noexcept {
    // 玩法节奏：Running 态才累加（等待/结算期不计玩法时长）
    if (state_ != State::Running) {
        return;
    }
    ++tick_count_;
    elapsed_seconds_ += delta_seconds;
    // battle tick 接入（P2-2）：Running 态驱动玩法负载；tick_index 用累加后
    // 的 tick_count_（严格递增，与 battle 复算口径一致）
    if (battle_ && battle_->phase() == apollo::game::battle::BattlePhase::Battling) {
        battle_->tick(static_cast<std::uint32_t>(tick_count_));
    }
}

std::uint64_t Instance::tick_count() const noexcept {
    return tick_count_;
}

double Instance::elapsed_seconds() const noexcept {
    return elapsed_seconds_;
}

bool Instance::attach_battle(std::unique_ptr<apollo::game::battle::BattleRuntime> battle) {
    // 挂接窗口 = 开局装载三态；Running 起拒绝；一 instance 至多一 battle
    if (state_ != State::Create && state_ != State::Initialize && state_ != State::Waiting) {
        return false;
    }
    if (!battle || battle_) {
        return false;
    }
    battle_ = std::move(battle);
    return true;
}

apollo::game::battle::BattleRuntime* Instance::battle() noexcept {
    return battle_.get();
}

const apollo::game::battle::BattleRuntime* Instance::battle() const noexcept {
    return battle_.get();
}

} // namespace apollo::game::world