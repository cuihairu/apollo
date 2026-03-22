#pragma once

#include "apollo/game/world/map_instance.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace apollo::game::world {

class MapInstanceManager {
public:
    MapInstancePtr create_instance(MapInstance::InstanceId id, std::string map_name);
    MapInstancePtr find_instance(MapInstance::InstanceId id) const;
    void destroy_instance(MapInstance::InstanceId id);
    void update(float delta_time);
    [[nodiscard]] std::size_t instance_count() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<MapInstance::InstanceId, MapInstancePtr> instances_;
};

} // namespace apollo::game::world
