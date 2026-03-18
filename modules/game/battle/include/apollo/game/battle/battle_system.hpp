#pragma once

#include "apollo/game/core/entity.hpp"
#include <vector>
#include <memory>

namespace apollo::game::battle {

using EntityPtr = std::shared_ptr<apollo::game::core::Entity>;

class BattleSystem {
public:
    BattleSystem();

    void update(float delta_time);

    void add_entity(EntityPtr entity);
    void remove_entity(apollo::game::core::EntityId id);

    size_t get_entity_count() const { return entities_.size(); }

private:
    std::vector<EntityPtr> entities_;
};

} // namespace apollo::game::battle
