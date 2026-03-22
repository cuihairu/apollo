#include "apollo/game/world/world_space.hpp"

namespace apollo::game::world {

WorldSpace::WorldSpace(std::string name)
    : name_(std::move(name))
    , scene_(name_) {
}

const std::string& WorldSpace::name() const {
    return name_;
}

void WorldSpace::update(float delta_time) {
    scene_.update(delta_time);
}

Scene& WorldSpace::scene() {
    return scene_;
}

const Scene& WorldSpace::scene() const {
    return scene_;
}

} // namespace apollo::game::world
