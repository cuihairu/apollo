#pragma once

#include "apollo/game/world/world_space.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace apollo::game::world {

class MapInstance {
public:
    using InstanceId = std::uint64_t;

    MapInstance(InstanceId id, std::string map_name);

    InstanceId id() const;
    const std::string& map_name() const;

    WorldSpace& world_space();
    const WorldSpace& world_space() const;

    void update(float delta_time);

private:
    InstanceId id_ = 0;
    std::string map_name_;
    WorldSpace world_space_;
};

using MapInstancePtr = std::shared_ptr<MapInstance>;

} // namespace apollo::game::world
