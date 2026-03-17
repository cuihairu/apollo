#include "apollo/game/core/entity.hpp"

namespace apollo::game::core {

void Entity::on_spawn() {
    // Initialize components
    for (auto& [name, component] : components_) {
        component->on_attach(this);
    }
}

void Entity::on_despawn() {
    // Cleanup components
    for (auto& [name, component] : components_) {
        component->on_detach();
    }
}

void Entity::on_update(float delta_time) {
    for (auto& [name, component] : components_) {
        component->on_update(delta_time);
    }
}

void Entity::add_component(ComponentPtr component) {
    if (component) {
        components_[component->get_type_name()] = component;
        component->on_attach(this);
    }
}

void Entity::remove_component(const std::string& component_type) {
    auto it = components_.find(component_type);
    if (it != components_.end()) {
        it->second->on_detach();
        components_.erase(it);
    }
}

} // namespace apollo::game::core
