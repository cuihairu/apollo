#include "world_app/cell_app.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/core/log/log_manager.h"
#include "apollo/net/linktrace/span_emitter.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/script/script_runtime.hpp"
#include "apollo_contract.h"
#include <algorithm>
#include <fstream>
#include <optional>
#include <stdexcept>

#include "cell_app_detail.hpp"

namespace world_app {

namespace protocol = apollo::protocol;

namespace {

// 本 TU 沿用的日志出口别名（LAY-001 拆分面；单一定义见 cell_app.cpp）
apollo::core::log::Logger& worldLog() {
    return cell_detail::world_log();
}

} // namespace

std::uint64_t CellApp::resolve_scene_id(std::uint64_t space_id) const {
    // 单进程单 scene 一条目（P1-1 口径）；未知空间回落默认 scene——
    // 与 create 的旧「空间无关全落全局表」行为等价
    const auto route = spaceToScene_.find(space_id);
    return route != spaceToScene_.end() ? route->second : defaultSceneId_;
}

apollo::game::world::SceneDescriptor space_descriptor_for(const WorldAppConfig& config,
                                                          std::uint64_t space_id,
                                                          const std::string& name) {
    // 声明（--add-space）与分配（962）两面的共用装配；几何 = 全局字段。
    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 1;  // 唯一在册地图资产——静态资产面不虚构 id
    descriptor.map_name = name.empty() ? ("space-" + std::to_string(space_id)) : name;
    descriptor.width = config.spaceWidth;
    descriptor.height = config.spaceHeight;
    descriptor.grid_size = config.gridSize;
    descriptor.view_radius = config.viewRadius;
    return descriptor;
}

const std::string& declared_space_name(const WorldAppConfig& config,
                                       std::uint64_t space_id) {
    // 声明名回查（L100 多 Space 声明面后续接线）：已声明的 space_id 返回声明
    // 名；未声明/默认 Space 返回空，调用方走 space_descriptor_for 的
    // space-<id> 缺省回退。
    static const std::string kEmpty;
    for (const auto& decl : config.spaces) {
        if (decl.space_id == space_id) {
            return decl.name;
        }
    }
    return kEmpty;
}

std::uint64_t CellApp::add_space(std::uint64_t space_id,
                                 const apollo::game::world::SceneDescriptor& descriptor) {
    // L99：显式登记面——space_id（路由键域）与 scene_id（world 自增域）
    // 数值独立；本方法只建 scene + 登路由，不建 Instance（持续开放 Space
    // 可无临时 Instance，玩法侧经副本受理面自建）。
    const auto existing = spaceToScene_.find(space_id);
    if (existing != spaceToScene_.end()) {
        return existing->second;  // 幂等：重复登记不重建 scene
    }
    auto* scene = world_->create_scene(descriptor);
    if (scene == nullptr) {
        return 0;  // descriptor 无效（is_valid 拒绝）
    }
    spaceToScene_[space_id] = scene->scene_id();
    return scene->scene_id();
}

std::uint64_t CellApp::remove_space(std::uint64_t space_id) {
    // L108 首切片：撤销面——World 经 962 ready=0 通知本 cell 让渡一个 Space。
    // 返回被撤 scene id（0 = 未撤）：默认 Space 是进程骨架不参与回收；未知
    // 空间幂等；占用中（avatar_count > 0）拒绝并保持路由——不迁不销毁实体，
    // 重平衡/排空联动后续批。已空则先擦路由再销毁 scene（World::destroy_scene
    // 自身再拒未销毁的 Instance；顺序防 use-after-free，sanitizer 树实抓）。
    // 内核线程上下文（同 add_space）：路由表与 world 容器由游戏线程单写。
    if (space_id == defaultSpaceId_) {
        return 0;  // 默认 Space 不回收
    }
    const auto route = spaceToScene_.find(space_id);
    if (route == spaceToScene_.end()) {
        return 0;  // 幂等：无路由可撤
    }
    const std::uint64_t scene_id = route->second;
    const auto* scene = world_->find_scene(scene_id);
    if (scene == nullptr) {
        spaceToScene_.erase(route);  // 残留路由：清表即可
        return 0;
    }
    if (scene->avatar_count() != 0) {
        return 0;  // 占用中：拒绝，路由保持
    }
    spaceToScene_.erase(route);
    if (world_->destroy_scene(scene_id)) {
        return scene_id;
    }
    spaceToScene_[space_id] = scene_id;  // 销毁失败则恢复路由，保持同存
    return 0;
}

bool CellApp::request_space_revoke(std::uint64_t space_id) {
    // 受理点（L108 占用联动切片）：只入队不读内核状态（跨线程安全面同
    // submitIntent 纪律——世界状态只允许内核线程读写）；默认 Space 拒收
    //（remove_space 同口径），重复待撤幂等拒收。
    if (space_id == defaultSpaceId_) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        if (!pendingRevokesSet_.insert(space_id).second) {
            return false;  // 已在待撤表
        }
    }
    pendingRevokes_.fetch_add(1, std::memory_order_acq_rel);
    return true;
}

void CellApp::cancel_space_revoke(std::uint64_t space_id) {
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        if (pendingRevokesSet_.erase(space_id) == 0) {
            return;
        }
    }
    pendingRevokes_.fetch_sub(1, std::memory_order_acq_rel);
}

void CellApp::drainPendingRevokes() {
    // tick 边界重试（L108 占用联动切片，内核线程单写者）：腾空即撤销成功
    // 出表，占用被拒留在表下一拍再试；路由已消失（他面撤过）直接出表。
    if (pendingRevokes_.load(std::memory_order_acquire) == 0) {
        return;
    }
    std::size_t done = 0;
    {
        std::lock_guard<std::mutex> lock(pendingRevokesMutex_);
        for (auto it = pendingRevokesSet_.begin(); it != pendingRevokesSet_.end();) {
            const auto space_id = *it;
            if (spaceToScene_.find(space_id) == spaceToScene_.end()) {
                it = pendingRevokesSet_.erase(it);  // 路由已消失：出表
                ++done;
                continue;
            }
            if (remove_space(space_id) != 0) {
                it = pendingRevokesSet_.erase(it);
                ++done;
                worldLog().info("space revoked (pending retry): space="
                                + std::to_string(space_id));
                continue;
            }
            ++it;  // 占用中：下一拍再试
        }
    }
    if (done != 0) {
        pendingRevokes_.fetch_sub(done, std::memory_order_acq_rel);
    }
}

void CellApp::ensureDefaultScene() {
    // P0-3：world.create_scene 路径（替代旧 ensureDefaultMapInstance 三层壳）。
    // 最小闭环：1 scene（descriptor 绑定地图资产）+ 1 instance（八态推进至 Running）。
    if (world_->find_scene(defaultSceneId_) != nullptr) {
        return;
    }

    apollo::game::world::SceneDescriptor descriptor;
    descriptor.map_id = 1;
    descriptor.map_name =
        config_.spaceName.empty() ? std::string("default-world") : config_.spaceName;
    descriptor.width = config_.spaceWidth;
    descriptor.height = config_.spaceHeight;
    descriptor.grid_size = config_.gridSize;
    descriptor.view_radius = config_.viewRadius;

    auto* scene = world_->create_scene(descriptor);
    if (scene == nullptr) {
        throw std::runtime_error("failed to create default scene (invalid descriptor)");
    }
    defaultSceneId_ = scene->scene_id();

    defaultInstance_ = world_->create_instance(defaultSceneId_, "default");
    // 八态推进至 Running（Create→Initialize→Waiting→Running；逐态校验）
    defaultInstance_->initialize();
    defaultInstance_->ready();
    defaultInstance_->start();

    // space → scene 路由登记（P1-1 换幕目标解析；多 scene 随 descriptor 扩充）
    spaceToScene_[defaultSpaceId_] = defaultSceneId_;
}

} // namespace world_app
