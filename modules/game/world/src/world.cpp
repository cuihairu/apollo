#include "apollo/game/world/world.hpp"

#include <algorithm>

namespace apollo::game::world {

Scene* World::create_scene(const SceneDescriptor& descriptor) {
    if (!descriptor.is_valid()) {
        return nullptr;  // 未绑定地图资产的 descriptor 拒绝建场
    }

    const auto scene_id = next_scene_id_++;
    auto scene = std::make_unique<Scene>(scene_id, descriptor.map_name);
    // AOI 网格随 descriptor 初始化（宽度/高度/网格/视野半径来自地图资产声明）
    scene->aoi() = SceneAoi(descriptor.width, descriptor.height, descriptor.grid_size,
                            descriptor.view_radius);

    auto* ptr = scene.get();
    scenes_[scene_id] = std::move(scene);
    scene_order_.push_back(ptr);
    return ptr;
}

Scene* World::find_scene(std::uint64_t scene_id) {
    const auto it = scenes_.find(scene_id);
    return it != scenes_.end() ? it->second.get() : nullptr;
}

void World::destroy_scene(std::uint64_t scene_id) {
    scenes_.erase(scene_id);
    // 从顺序表中摘除（按 id 定位指针）
    for (auto order_it = scene_order_.begin(); order_it != scene_order_.end(); ++order_it) {
        if (*order_it && (*order_it)->scene_id() == scene_id) {
            scene_order_.erase(order_it);
            break;
        }
    }
}

std::size_t World::scene_count() const noexcept {
    return scenes_.size();
}

const std::vector<Scene*>& World::scenes() const noexcept {
    return scene_order_;
}

InstancePtr World::create_instance(std::uint64_t scene_id, const std::string& name) {
    if (scenes_.count(scene_id) == 0) {
        return nullptr;  // 场景不存在
    }
    const auto instance_id = next_instance_id_++;
    auto instance = std::make_shared<Instance>(instance_id, scene_id, name);
    instances_[instance_id] = instance;
    return instance;
}

Instance* World::find_instance(Instance::InstanceId instance_id) {
    const auto it = instances_.find(instance_id);
    return it != instances_.end() ? it->second.get() : nullptr;
}

Instance* World::find_instance_by_scene(std::uint64_t scene_id) {
    for (const auto& [_, instance] : instances_) {
        if (instance && instance->scene_id() == scene_id) {
            return instance.get();
        }
    }
    return nullptr;
}

std::size_t World::instance_count() const noexcept {
    return instances_.size();
}

void World::tick(double delta_seconds) noexcept {
    for (auto* scene : scene_order_) {
        if (scene) {
            scene->tick(delta_seconds);
        }
    }
}

} // namespace apollo::game::world