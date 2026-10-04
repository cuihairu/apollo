#include "apollo/game/world/scene.hpp"

#include "apollo/game/core/entity.hpp"

#include <algorithm>
#include <string_view>

namespace apollo::game::world {

using apollo::game::core::EntityId;

std::string_view to_string(SceneTickPhase phase) {
    switch (phase) {
    case SceneTickPhase::Simulate:
        return "simulate";
    case SceneTickPhase::Recalc:
        return "recalc";
    case SceneTickPhase::AoiDecay:
        return "aoidecay";
    case SceneTickPhase::Collect:
        return "collect";
    case SceneTickPhase::BudgetFlush:
        return "budget+flush";
    case SceneTickPhase::PersistBatch:
        return "persist-batch";
    }
    return "unknown";
}

Scene::Scene(std::string name)
    : name_(std::move(name)) {
}

Scene::Scene(std::uint64_t scene_id, std::string name)
    : scene_id_(scene_id)
    , name_(std::move(name)) {
}

std::uint64_t Scene::scene_id() const noexcept {
    return scene_id_;
}

void Scene::spawn_entity(EntityPtr entity) {
    if (entity && entity->get_id().is_valid()) {
        entities_[entity->get_id().value()] = entity;
        entity->on_spawn();
    }
}

void Scene::despawn_entity(EntityId id) {
    auto it = entities_.find(id.value());
    if (it != entities_.end()) {
        it->second->on_despawn();
        entities_.erase(it);
    }
}

EntityPtr Scene::get_entity(EntityId id) const {
    auto it = entities_.find(id.value());
    return it != entities_.end() ? it->second : nullptr;
}

bool Scene::enter(const AvatarPtr& avatar, const SceneAoi::Vec3& position) {
    if (!avatar || !avatar->player_id().is_valid()) {
        return false;
    }
    const auto pid = avatar->player_id();
    if (avatars_.count(pid.value()) != 0) {
        return false;  // 重复进入拒绝
    }

    avatars_[pid.value()] = avatar;
    avatar_order_.push_back(pid);

    // 进 scene 生：挂 scene 归属 + 权威落点 + AOI 入列
    avatar->attach_scene(scene_id_);
    avatar->set_position({position.x, position.y, position.z});
    aoi_.enter(avatar->entity_id(), position);
    return true;
}

bool Scene::suspend_avatar(apollo::game::core::PlayerId player_id) {
    const auto avatar = get_avatar(player_id);
    if (!avatar || avatar->state() != AvatarState::Active) {
        return false;
    }
    avatar->suspend();
    return true;
}

bool Scene::resume_avatar(apollo::game::core::PlayerId player_id) {
    const auto avatar = get_avatar(player_id);
    if (!avatar || avatar->state() != AvatarState::Suspended) {
        return false;
    }
    avatar->resume();
    return true;
}

bool Scene::leave(apollo::game::core::PlayerId player_id) {
    auto it = avatars_.find(player_id.value());
    if (it == avatars_.end()) {
        return false;
    }

    // 出 scene 死：AOI 出列 + 归属解除（对象生命周期由持有方收尾）
    aoi_.leave(it->second->entity_id());
    it->second->detach_scene();
    avatars_.erase(it);

    auto order_it = std::find(avatar_order_.begin(), avatar_order_.end(), player_id);
    if (order_it != avatar_order_.end()) {
        avatar_order_.erase(order_it);
    }
    return true;
}

AvatarPtr Scene::get_avatar(apollo::game::core::PlayerId player_id) const {
    auto it = avatars_.find(player_id.value());
    return it != avatars_.end() ? it->second : nullptr;
}

bool Scene::has_avatar(apollo::game::core::PlayerId player_id) const noexcept {
    return avatars_.count(player_id.value()) != 0;
}

std::size_t Scene::avatar_count() const noexcept {
    return avatars_.size();
}

const std::vector<apollo::game::core::PlayerId>& Scene::avatars() const noexcept {
    return avatar_order_;
}

SceneAoi& Scene::aoi() noexcept {
    return aoi_;
}

const SceneAoi& Scene::aoi() const noexcept {
    return aoi_;
}

void Scene::update(float delta_time) {
    tick(static_cast<double>(delta_time));
}

void Scene::tick(double delta_seconds) noexcept {
    // 六阶段固定顺序（attribute-sync 口径）；P0-3 骨架：simulate 驱动实体，
    // 其余阶段计数占位（recalc/aoidecay/collect/budget+flush/persist-batch
    // 随 P1 持久化与属性管线接入）。
    ++tick_count_;
    run_phase(SceneTickPhase::Simulate);
    for (auto& [id, entity] : entities_) {
        (void)id;
        entity->on_update(static_cast<float>(delta_seconds));
    }
    run_phase(SceneTickPhase::Recalc);
    run_phase(SceneTickPhase::AoiDecay);
    run_phase(SceneTickPhase::Collect);
    run_phase(SceneTickPhase::BudgetFlush);
    run_phase(SceneTickPhase::PersistBatch);
}

void Scene::run_phase(SceneTickPhase phase) noexcept {
    ++phase_counts_[static_cast<std::size_t>(phase)];
}

std::uint64_t Scene::tick_count() const noexcept {
    return tick_count_;
}

std::uint64_t Scene::phase_count(SceneTickPhase phase) const noexcept {
    return phase_counts_[static_cast<std::size_t>(phase)];
}

} // namespace apollo::game::world