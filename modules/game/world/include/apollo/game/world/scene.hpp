#pragma once

#include "apollo/game/core/entity.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace apollo::game::world {

using EntityPtr = std::shared_ptr<apollo::game::core::Entity>;

class Scene {
public:
    explicit Scene(std::string name = "DefaultScene");

    void spawn_entity(EntityPtr entity);
    void despawn_entity(apollo::game::core::EntityId id);
    EntityPtr get_entity(apollo::game::core::EntityId id) const;

    void update(float delta_time);

    const std::string& get_name() const { return name_; }
    size_t get_entity_count() const { return entities_.size(); }

private:
    std::string name_;
    std::unordered_map<uint64_t, EntityPtr> entities_;
};

} // namespace apollo::game::world
