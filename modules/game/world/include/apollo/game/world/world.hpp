#pragma once

#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_descriptor.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::game::world {

// World（Zone 内世界容器——object-model §2.1 目标态）：场景集合的单一权威。
//
// 任务书 §27 目标 API 的落点（P0-3 起为真实代码路径）：
//   world.create_scene(descriptor)          → 建运行时 Scene（+ AOI 网格）
//   scene.create_instance(...)              → Instance（玩法八态载体，见 instance.hpp）
//   instance.enter(player_id) / leave(...)  → 玩家进出
//
// World 不做持久化（世界配置属 Definition 层）；生命周期随 Zone 进程启停。
class World {
public:
    // 创建运行时场景：descriptor 校验失败（未绑定资产）返回 nullptr。
    // scene_id 自增分配（进程内单调，0 保留为无效哨兵）。
    Scene* create_scene(const SceneDescriptor& descriptor);
    Scene* find_scene(std::uint64_t scene_id);
    void destroy_scene(std::uint64_t scene_id);
    [[nodiscard]] std::size_t scene_count() const noexcept;
    [[nodiscard]] const std::vector<Scene*>& scenes() const noexcept;

    // 场景内建玩法实例（scene.create_instance 语义的宿主侧入口：
    // 校验场景存在 + 实例 id 进程内唯一）
    InstancePtr create_instance(std::uint64_t scene_id, const std::string& name = {});
    Instance* find_instance(Instance::InstanceId instance_id);
    [[nodiscard]] std::size_t instance_count() const noexcept;

    // 全场 tick（WorldHost 服务侧调用；逐 scene 六阶段）
    void tick(double delta_seconds) noexcept;

private:
    std::uint64_t next_scene_id_ = 1;
    Instance::InstanceId next_instance_id_ = 1;
    std::unordered_map<std::uint64_t, std::unique_ptr<Scene>> scenes_;
    std::vector<Scene*> scene_order_;  // 创建顺序可观察
    std::unordered_map<Instance::InstanceId, InstancePtr> instances_;
};

} // namespace apollo::game::world