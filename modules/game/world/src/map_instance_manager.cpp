#include "apollo/game/world/map_instance_manager.hpp"

namespace apollo::game::world {

MapInstancePtr MapInstanceManager::create_instance(MapInstance::InstanceId id, std::string map_name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto instance = std::make_shared<MapInstance>(id, std::move(map_name));
    instances_[id] = instance;
    return instance;
}

MapInstancePtr MapInstanceManager::find_instance(MapInstance::InstanceId id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto it = instances_.find(id);
    return it == instances_.end() ? nullptr : it->second;
}

void MapInstanceManager::destroy_instance(MapInstance::InstanceId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_.erase(id);
}

void MapInstanceManager::update(float delta_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [_, instance] : instances_) {
        if (instance) {
            instance->update(delta_time);
        }
    }
}

std::size_t MapInstanceManager::instance_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_.size();
}

} // namespace apollo::game::world
