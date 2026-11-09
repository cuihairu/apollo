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

void CellApp::attachPlayerWorldSession(
    protocol::SessionID session_id,
    protocol::PlayerID player_id_raw,
    EntityID entity_id,
    const Position& position,
    std::uint64_t space_id) {
    ensureDefaultScene();

    // 强分型（P0-2）：玩家身份与实体身份在类型面上分离——旧代码 entity_id 直接
    // 当 player_id 传（find_by_player/attach 三参同号），编译器不可见。
    const auto player_id = apollo::game::core::PlayerId(player_id_raw);
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);

    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        session = worldSessionManager_->create_session(session_id, player_id);
    }

    // L99：会话落点随 space 路由解析（未知空间回落默认 scene）；实例按落点
    // scene 反查——无实例的空间绑 0（持续开放 Space 可无临时 Instance）。
    const bool known_space = spaceToScene_.find(space_id) != spaceToScene_.end();
    const auto effective_space = known_space ? space_id : defaultSpaceId_;
    const auto scene_id = resolve_scene_id(space_id);
    auto* scene = world_->find_scene(scene_id);
    auto* instance = scene != nullptr ? world_->find_instance_by_scene(scene_id) : nullptr;

    // create Avatar + scene.enter / instance.enter（P0-3 任务书 §27 API 路径：
    // Avatar 由 Scene 持有（进 scene 生/出 scene 死），玩法侧走 instance.enter）
    auto avatar = std::make_shared<apollo::game::cell::Avatar>(player_id, avatar_entity_id);
    if (scene != nullptr) {
        scene->enter(avatar, {position.x, position.y, position.z});
    }
    if (instance != nullptr) {
        instance->enter(player_id);
    }

    session->assign_world(worldId_);
    session->assign_map_instance(instance != nullptr ? instance->id() : 0);
    // P0-2：space 与 instance 分离——space 承载 id 独立（旧代码以 map instance
    // 号兼任 space 号）。P0-3 拆 SceneDescriptor 后由 scene 提供。L99：登记的是
    // 实际落点空间（未知空间回落默认，与 resolve_scene_id 同一口径）。
    session->assign_space(effective_space);
    session->bind_avatar(avatar_entity_id);
    session->set_route_version(session->route_version() + 1);
    session->set_state(apollo::game::world::WorldSessionState::Entering);
    // Entering → Active（resume 唯一合法前态；Closed/Leaving 会拒绝——接线异常
    // 的可观察面，不静默吞）
    if (!session->resume()) {
        worldLog().warning("world session resume rejected for session "
                    + std::to_string(session_id));
    }
}

void CellApp::detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id) {
    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        // 无会话 id 时按玩家身份反查（P0-2/P0-3：经 Scene 的 Avatar 容器由
        // 实体号定位玩家号，再按玩家查会话；不再把实体号当玩家号传）
        // L99：落点 scene 由实体实际所在反查（多空间后不再假设默认 scene）
        if (auto* scene = find_scene_by_entity(entity_id)) {
            const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
            for (const auto& player_id : scene->avatars()) {
                const auto avatar = scene->get_avatar(player_id);
                if (avatar && avatar->entity_id() == avatar_entity_id) {
                    session = worldSessionManager_->find_by_player(player_id);
                    break;
                }
            }
        }
    }

    if (!session) {
        return;
    }

    // 出 scene 死：Avatar 由 scene.leave 解除归属并出 AOI（对象随 shared_ptr
    // 释放——scene 是唯一持有者，player-object-model §3「出 scene 死」）
    // L99：落点 scene / 实例由实体实际所在反查（多空间后不再假设默认 scene）
    const auto player_id = session->player_id();
    auto* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        // 实体号缺失或已失效时按玩家号反查落点 scene
        for (apollo::game::world::Scene* candidate : world_->scenes()) {
            if (candidate != nullptr && candidate->get_avatar(player_id) != nullptr) {
                scene = candidate;
                break;
            }
        }
    }
    if (scene != nullptr) {
        scene->leave(player_id);
        if (auto* instance = world_->find_instance_by_scene(scene->scene_id())) {
            instance->leave(player_id);
        }
    }

    worldSessionManager_->suspend_session(session->session_id());
    worldSessionManager_->close_session(session->session_id());
    // close 两段式收尾（P0-4）：Leaving 可观察窗口后显式终结（Closed 终态、
    // 摘除双索引）；内核侧登出无延迟清理面，窗口内即 finalize。
    worldSessionManager_->finalize_session(session->session_id());
}

} // namespace world_app
