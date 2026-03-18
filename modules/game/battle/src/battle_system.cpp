#include "apollo/game/battle/battle_system.hpp"

namespace apollo::game::battle {

BattleSystem::BattleSystem() = default;

void BattleSystem::update(float delta_time) {
    for (auto& entity : entities_) {
        if (entity) {
            entity->on_update(delta_time);
        }
    }
}

void BattleSystem::add_entity(std::shared_ptr<apollo::game::core::Entity> entity) {
    if (entity) {
        entities_.push_back(entity);
    }
}

void BattleSystem::remove_entity(apollo::game::core::EntityId id) {
    entities_.erase(
        std::remove_if(entities_.begin(), entities_.end(),
            [id](const auto& e) { return e && e->get_id() == id; }),
        entities_.end()
    );
}

} // namespace apollo::game::battle
