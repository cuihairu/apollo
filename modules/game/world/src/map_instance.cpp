#include "apollo/game/world/map_instance.hpp"

namespace apollo::game::world {

MapInstance::MapInstance(InstanceId id, std::string map_name)
    : id_(id)
    , map_name_(std::move(map_name))
    , world_space_(map_name_) {
}

MapInstance::InstanceId MapInstance::id() const {
    return id_;
}

const std::string& MapInstance::map_name() const {
    return map_name_;
}

WorldSpace& MapInstance::world_space() {
    return world_space_;
}

const WorldSpace& MapInstance::world_space() const {
    return world_space_;
}

void MapInstance::update(float delta_time) {
    world_space_.update(delta_time);
}

} // namespace apollo::game::world
