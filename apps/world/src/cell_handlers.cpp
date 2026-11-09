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

// P1-3：Scene 持有 AOI 后内核侧直接消费 SceneAoi（Vec3/查询面）
using apollo::game::world::SceneAoi;

namespace {

// 本 TU 沿用的日志出口别名（LAY-001 拆分面；单一定义见 cell_app.cpp）
apollo::core::log::Logger& worldLog() {
    return cell_detail::world_log();
}

// P2-1b：契约枚举 → Scene 实体类型串（core::Entity 以字符串承载类型语义）
const char* entity_type_name(protocol::EntityType type) {
    switch (type) {
        case protocol::EntityType::PLAYER:   return "PLAYER";
        case protocol::EntityType::NPC:      return "NPC";
        case protocol::EntityType::MONSTER:  return "MONSTER";
        case protocol::EntityType::PET:      return "PET";
        default:                             return "UNKNOWN";
    }
}

} // namespace

std::vector<uint8_t> CellApp::handleCellCreateEntity(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellCreateEntity>(bodyData);

    // P2-1b：实体集合归 Scene（EntityManager 删除）。玩家实体经 attach 路径
    // 入 avatars_（AOI 随 scene.enter）；非玩家实体 spawn_entity 入 entities_，
    // 随 Scene::tick 六阶段驱动 on_update。空间路由沿 spaceToScene_（跨境同款），
    // 未知空间落默认 scene；位置承载随 NPC AOI 玩法批（现读方为零，不设存储）。
    auto* scene = world_->find_scene(resolve_scene_id(msg.spaceId));

    if (scene) {
        if (msg.entityType == protocol::EntityType::PLAYER) {
            // P0-2：双 id 复用收敛。消息面（CellCreateEntity）尚无独立 player_id
            // 字段——这是契约缺陷，P2-3 修文补字段；P0 代码侧先把身份显式化：
            //   玩家身份 = 创建请求当下携带的唯一玩家号（现为 entity 号派生，
            //   修文后替换为 msg.playerId）；实体身份独立强分型；
            //   不再把 entity_id 兼任 session_id。
            const auto session_id = header.sessionId;
            const auto player_id = static_cast<protocol::PlayerID>(msg.entityId);
            // L99：落点空间随请求（旧代码忽略 msg.spaceId 一律落默认空间）
            attachPlayerWorldSession(session_id, player_id, msg.entityId,
                                     {msg.position.x, msg.position.y, msg.position.z},
                                     msg.spaceId);
        } else {
            // P2-1b：非玩家实体入 Scene 实体集合（随 Scene::tick 驱动）
            scene->spawn_entity(std::make_shared<apollo::game::core::Entity>(
                apollo::game::core::EntityId(msg.entityId),
                entity_type_name(msg.entityType)));
        }
    }

    // 返回确认（简化）
    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellApp::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellDestroyEntity>(bodyData);

    // P1-3：AOI 出列由 detach 路径 scene.leave 承担（Scene 持有 AOI）
    detachPlayerWorldSession(header.sessionId, msg.entityId);
    // P2-1b：非玩家实体自 Scene 实体集合出列（玩家实体在 avatars_，由 detach 收口）
    if (auto* scene = find_scene_by_entity(msg.entityId)) {
        scene->despawn_entity(apollo::game::core::EntityId(msg.entityId));
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellApp::handleCellEntityMove(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(bodyData);

    // P1-3 AOI 收敛：Scene 持有的 SceneAoi 随动；P2-1b 起实体必属 Scene
    //（含非玩家，见 handleCellCreateEntity），无场景归属即不存在
    if (auto* scene = find_scene_by_entity(msg.entityId)) {
        const auto entity_id = apollo::game::core::EntityId(msg.entityId);
        scene->aoi().move(entity_id,
                          SceneAoi::Vec3{msg.newPos.x, msg.newPos.y, msg.newPos.z});

        // 广播移动消息给视野内玩家（viewer set 驱动，见 broadcastToViewers）
        broadcastToViewers(msg.entityId, request);
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellApp::handleCellCrossBorder(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::CellCrossBorder>(bodyData);

    // P1-1 重写：走 scene_transfer 流程模块（prepare→begin→detach→attach→
    // complete，六差距收口），替换旧「字段搬运 + 瞬时 complete」路径——
    // 旧路径无准入、无对象投影、无回滚、目标不存在时静默同场搬运。
    const auto route = spaceToScene_.find(static_cast<std::uint64_t>(msg.toSpace));
    if (route == spaceToScene_.end()) {
        // 目标 space 无路由（差距 ⑥ 显式驳回面；跨 cell 转移随 P3 部署面）
        return {};
    }

    // 玩家身份定位：优先按会话；缺失时经 Scene 的 Avatar 容器由实体号
    // 反查（P0-2 强分型，不再把实体号当玩家号传）
    auto session = worldSessionManager_->find_session(header.sessionId);
    apollo::game::core::PlayerId player_id{};
    if (session) {
        player_id = session->player_id();
    } else {
        const auto caster_entity = apollo::game::core::EntityId(msg.entityId);
        for (apollo::game::world::Scene* scene : world_->scenes()) {
            if (scene == nullptr) {
                continue;
            }
            for (const auto& pid : scene->avatars()) {
                const auto avatar = scene->get_avatar(pid);
                if (avatar && avatar->entity_id() == caster_entity) {
                    player_id = pid;
                    break;
                }
            }
            if (player_id.is_valid()) {
                break;
            }
        }
    }
    if (!player_id.is_valid()) {
        return {};
    }

    apollo::game::world::SceneTransferRequest transfer;
    transfer.player_id = player_id;
    transfer.target_scene_id = route->second;
    // 落点：跨边界请求携带的位置（契约口径——坐标易失态不带走，以
    // 请求落点进场；服务端权威可改）
    transfer.landing = SceneAoi::Vec3{msg.position.x, msg.position.y, msg.position.z};

    const auto outcome =
        apollo::game::world::execute_scene_transfer(*world_, *worldSessionManager_, transfer);
    if (!outcome.ok()) {
        // 驳回/回滚面：单进程阶段仅可观察日志；错误下发随网关面（P1-6）
        worldLog().warning("scene transfer rejected: player="
                       + std::to_string(player_id.value()) + " target_scene="
                       + std::to_string(transfer.target_scene_id) + " result="
                       + std::string(apollo::game::world::to_string(outcome.result))
                       + " at_step=" + std::to_string(static_cast<int>(outcome.failed_at)));
    }

    return {};
}

std::vector<uint8_t> CellApp::handleCombatSkillCast(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CombatSkillCast>(bodyData);

    // 处理技能逻辑（P2-1b：在场判定 = 实体归属某 Scene）
    const bool caster_present = find_scene_by_entity(msg.casterId) != nullptr;
    const bool target_present = find_scene_by_entity(msg.targetId) != nullptr;

    if (caster_present && target_present) {
        // 简化伤害计算
        protocol::CombatDamage damageMsg;
        damageMsg.targetId = msg.targetId;
        damageMsg.damage = -10;  // 固定伤害
        damageMsg.sourceId = msg.casterId;

        auto damageData = protocol::MessageCodec::encode(damageMsg);

        // 广播伤害
        broadcastToViewers(msg.targetId, damageData);

        worldLog().info("Skill cast: " + std::to_string(msg.casterId) + " -> "
                    + std::to_string(msg.targetId) + " (skill: "
                    + std::to_string(msg.skillId) + ")");
    }

    return {};
}

void CellApp::broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message) {
    // P1-3：viewer set 驱动——接收集合 = 实体所在 Scene 的 SceneAoi 视野
    // 查询（AOI 归 scene、scene_id 隔离）。下发面：每 viewer 按其会话号重编
    // 帧头（body 原样）走 939 client_deliver 网关下行；无会话 viewer（NPC
    // 等非玩家实体）自然落过。
    apollo::game::world::Scene* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        return;
    }
    if (message.size() < sizeof(protocol::MessageHeader)) {
        return;  // 残帧不广播
    }
    const auto header = protocol::MessageCodec::parseHeader(message);
    const std::vector<uint8_t> body(message.begin() + sizeof(protocol::MessageHeader),
                                    message.end());
    const auto viewers = scene->aoi().viewers_of(apollo::game::core::EntityId(entity_id));
    std::size_t delivered = 0;
    for (const auto& viewer : viewers) {
        const auto viewer_session = session_of_entity(viewer.value(), scene);
        if (viewer_session == 0) {
            continue;
        }
        protocol::MessageHeader routed = header;
        routed.sessionId = viewer_session;
        auto frame = protocol::MessageCodec::encodeHeader(routed);
        frame.insert(frame.end(), body.begin(), body.end());
        deliverToClient(viewer_session, frame);
        ++delivered;
    }
    if (delivered != 0) {
        worldLog().info("broadcast: entity=" + std::to_string(entity_id) + " scene="
                    + std::to_string(scene->scene_id()) + " viewers="
                    + std::to_string(viewers.size()) + " delivered="
                    + std::to_string(delivered) + " bytes="
                    + std::to_string(message.size()));
    }
}

std::uint64_t CellApp::session_of_entity(EntityID entity_id,
                                         apollo::game::world::Scene* scene) const {
    if (scene == nullptr) {
        return 0;
    }
    const auto entity = apollo::game::core::EntityId(entity_id);
    for (const auto& player_id : scene->avatars()) {
        const auto avatar = scene->get_avatar(player_id);
        if (avatar && avatar->entity_id() == entity) {
            if (worldSessionManager_) {
                if (const auto session = worldSessionManager_->find_by_player(player_id)) {
                    return session->session_id();
                }
            }
            return 0;
        }
    }
    return 0;
}

apollo::game::world::Scene* CellApp::find_scene_by_entity(EntityID entity_id) {
    // 玩家实体 → Avatar 实体号反查；非玩家实体 → Scene 实体集合直查
    //（P0-2 强分型 + P2-1b 实体集合归 Scene；实体号唯一归属一个 scene）
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
    for (apollo::game::world::Scene* scene : world_->scenes()) {
        if (scene == nullptr) {
            continue;
        }
        if (scene->get_entity(avatar_entity_id) != nullptr) {
            return scene;
        }
        for (const auto& player_id : scene->avatars()) {
            const auto avatar = scene->get_avatar(player_id);
            if (avatar && avatar->entity_id() == avatar_entity_id) {
                return scene;
            }
        }
    }
    return nullptr;
}

} // namespace world_app
