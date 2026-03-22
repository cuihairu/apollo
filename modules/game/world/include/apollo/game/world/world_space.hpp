#pragma once

#include "apollo/game/world/scene.hpp"

#include <memory>
#include <string>

namespace apollo::game::world {

class WorldSpace {
public:
    explicit WorldSpace(std::string name = "default_space");

    const std::string& name() const;

    void update(float delta_time);

    Scene& scene();
    const Scene& scene() const;

private:
    std::string name_;
    Scene scene_;
};

using WorldSpacePtr = std::shared_ptr<WorldSpace>;

} // namespace apollo::game::world
