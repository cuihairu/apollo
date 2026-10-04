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
    return transition_to(State::Running);
}

bool Instance::finish() {
    return transition_to(State::Finishing);
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
}

std::uint64_t Instance::tick_count() const noexcept {
    return tick_count_;
}

double Instance::elapsed_seconds() const noexcept {
    return elapsed_seconds_;
}

} // namespace apollo::game::world