#include "apollo/game/cell/avatar.hpp"

#include <algorithm>

namespace apollo::game::cell {

Avatar::Avatar(apollo::game::core::PlayerId player_id,
               apollo::game::core::EntityId entity_id,
               std::string name)
    : player_id_(player_id)
    , entity_id_(entity_id)
    , name_(std::move(name)) {
}

apollo::game::core::PlayerId Avatar::player_id() const noexcept {
    return player_id_;
}

apollo::game::core::EntityId Avatar::entity_id() const noexcept {
    return entity_id_;
}

const std::string& Avatar::name() const noexcept {
    return name_;
}

void Avatar::set_name(std::string name) noexcept {
    name_ = std::move(name);
}

std::uint64_t Avatar::scene_id() const noexcept {
    return scene_id_;
}

void Avatar::attach_scene(std::uint64_t scene_id) noexcept {
    scene_id_ = scene_id;
    state_ = AvatarState::Active;  // 进 scene 即生
}

void Avatar::detach_scene() noexcept {
    scene_id_ = 0;
    state_ = AvatarState::Leaving;
}

AvatarState Avatar::state() const noexcept {
    return state_;
}

void Avatar::suspend() noexcept {
    if (state_ == AvatarState::Active) {
        state_ = AvatarState::Suspended;
    }
}

void Avatar::resume() noexcept {
    if (state_ == AvatarState::Suspended) {
        state_ = AvatarState::Active;
    }
}

void Avatar::begin_leave() noexcept {
    if (state_ != AvatarState::Leaving) {
        state_ = AvatarState::Leaving;
    }
}

const Avatar::Position& Avatar::position() const noexcept {
    return position_;
}

void Avatar::set_position(const Position& position) noexcept {
    position_ = position;
}

bool Avatar::reconcile(std::uint64_t view_seq, const Position& target) noexcept {
    // 移动权威：序号单调门禁。乱序（小于/等于已应用）输入直接拒绝——
    // 防止客户端回滚包重放；server 权威位置的唯一写路径是 set_position/
    // reconcile（两者都记序）。
    if (state_ != AvatarState::Active) {
        return false;  // 挂起/出场景态不接收移动输入
    }
    if (view_seq <= last_reconciled_seq_) {
        return false;
    }
    last_reconciled_seq_ = view_seq;
    position_ = target;
    return true;
}

std::uint64_t Avatar::last_reconciled_seq() const noexcept {
    return last_reconciled_seq_;
}

int Avatar::hp() const noexcept {
    return hp_;
}

int Avatar::max_hp() const noexcept {
    return max_hp_;
}

void Avatar::set_hp(int hp, int max_hp) noexcept {
    max_hp_ = std::max(max_hp, 1);
    hp_ = std::clamp(hp, 0, max_hp_);
}

bool Avatar::apply_damage(int amount) noexcept {
    if (state_ == AvatarState::Leaving) {
        return false;  // 出场景态不再结算
    }
    hp_ = std::clamp(hp_ - std::max(amount, 0), 0, max_hp_);
    return hp_ <= 0;
}

void Avatar::add_viewer(apollo::game::core::EntityId viewer) {
    if (std::find(viewers_.begin(), viewers_.end(), viewer) == viewers_.end()) {
        viewers_.push_back(viewer);
    }
}

void Avatar::remove_viewer(apollo::game::core::EntityId viewer) {
    auto it = std::remove(viewers_.begin(), viewers_.end(), viewer);
    viewers_.erase(it, viewers_.end());
}

bool Avatar::is_viewer(apollo::game::core::EntityId viewer) const noexcept {
    return std::find(viewers_.begin(), viewers_.end(), viewer) != viewers_.end();
}

const std::vector<apollo::game::core::EntityId>& Avatar::viewers() const noexcept {
    return viewers_;
}

std::size_t Avatar::viewer_count() const noexcept {
    return viewers_.size();
}

void Avatar::broadcast() noexcept {
    ++broadcast_count_;
}

std::uint64_t Avatar::broadcast_count() const noexcept {
    return broadcast_count_;
}

void Avatar::project_from(const apollo::game::base::GameplaySnapshot& snapshot) noexcept {
    // player-object-model §3：换幕时长期属性从只读玩法快照单向拷贝（值投影，
    // 不持档案指针——todo L84 跨界纪律）。挂起保活窗口由会话/scene 侧驱动
    // （scene.cpp suspend），不再耦合档案宿主的运行态。
    home_zone_id_ = snapshot.home_zone_id;
    max_hp_ = std::max(snapshot.max_hp, 1);
    hp_ = std::clamp(snapshot.hp, 0, max_hp_);
    projected_ = true;
}

bool Avatar::has_projection() const noexcept {
    return projected_;
}

std::uint32_t Avatar::home_zone_id() const noexcept {
    return home_zone_id_;
}

} // namespace apollo::game::cell