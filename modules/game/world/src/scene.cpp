#include "apollo/game/world/scene.hpp"
#include "apollo/game/core/entity.hpp"

namespace apollo::game::world {

using apollo::game::core::EntityId;

Scene::Scene(std::string name)
    : name_(std::move(name)) {
}

void Scene::spawn_entity(EntityPtr entity) {
    if (entity && entity->get_id().is_valid()) {
        entities_[entity->get_id().value()] = entity;
        entity->on_spawn();
    }
}

void Scene::despawn_entity(apollo::game::core::EntityId id) {
    auto it = entities_.find(id.value());
    if (it != entities_.end()) {
        it->second->on_despawn();
        entities_.erase(it);
    }
}

EntityPtr Scene::get_entity(apollo::game::core::EntityId id) const {
    auto it = entities_.find(id.value());
    return it != entities_.end() ? it->second : nullptr;
}

void Scene::update(float delta_time) {
    for (auto& [id, entity] : entities_) {
        entity->on_update(delta_time);
    }
}

} // namespace apollo::game::world
