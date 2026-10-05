#include "cell/cell_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/instance.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_transfer.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdexcept>

namespace cell {

namespace protocol = apollo::protocol;

// P1-3：Scene 持有 AOI 后 cell 侧直接消费 SceneAoi（Vec3/查询面）
using apollo::game::world::SceneAoi;

namespace {

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

class CellWorldService final : public apollo::runtime::IWorldService {
public:
    CellWorldService(
        apollo::game::world::World& world,
        CellConfig config)
        : world_(world)
        , config_(std::move(config)) {
    }

    std::string_view service_name() const override {
        return "CellWorldService";
    }

    bool initialize() override {
        tick_count_ = 0;
        return true;
    }

    void tick(const apollo::runtime::WorldTickContext& context) override {
        tick_count_ = context.tick_index;
        // P0-3：世界 tick = 逐 scene 六阶段（simulate/recalc/aoidecay/
        // collect/budget+flush/persist-batch）
        world_.tick(static_cast<double>(context.delta_seconds));
    }

    void shutdown() override {
        tick_count_ = 0;
    }

private:
    apollo::game::world::World& world_;
    CellConfig config_;
    std::uint64_t tick_count_ = 0;
};

} // namespace

//==============================================================================
//==============================================================================
// CellServer 实现
//==============================================================================

CellServer::CellServer(const CellConfig& config)
    : config_(config)
    , world_(std::make_unique<apollo::game::world::World>())
    , worldSessionManager_(std::make_shared<apollo::game::world::WorldSessionManager>())
    , worldHost_(std::make_shared<apollo::runtime::WorldHost>(
          static_cast<std::uint32_t>(1000 / std::max(config.tickRateMs, 1)))) {
    worldHost_->add_world_service(
        std::make_shared<CellWorldService>(*world_, config_));
}

CellServer::~CellServer() {
    stop();
}

void CellServer::start() {
    if (running_) return;

    // 创建 RPC 服务器
    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(config_.host, config_.port)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        auto header = protocol::MessageCodec::parseHeader(data);
        auto msgType = static_cast<protocol::MessageType>(header.type);

        switch (msgType) {
            case protocol::MessageType::CELL_CREATE_ENTITY:
                return handleCellCreateEntity(data);

            case protocol::MessageType::CELL_DESTROY_ENTITY:
                return handleCellDestroyEntity(data);

            case protocol::MessageType::CELL_ENTITY_MOVE:
                return handleCellEntityMove(data);

            case protocol::MessageType::CELL_CROSS_BORDER:
                return handleCellCrossBorder(data);

            case protocol::MessageType::COMBAT_SKILL_CAST:
                return handleCombatSkillCast(data);

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    server_->start();

    ensureDefaultScene();

    if (!worldHost_->start()) {
        server_->stop();
        server_.reset();
        throw std::runtime_error("failed to start WorldHost");
    }

    running_ = true;

    // 启动游戏循环线程
    gameThread_ = std::thread(&CellServer::gameLoop, this);

    std::cout << "Cell server listening on " << config_.host << ":" << config_.port << std::endl;
    std::cout << "Space: " << config_.spaceName << " (" << config_.spaceWidth
              << "x" << config_.spaceHeight << ")" << std::endl;
}

void CellServer::stop() {
    running_ = false;

    if (gameThread_.joinable()) {
        gameThread_.join();
    }

    if (worldHost_) {
        worldHost_->stop();
    }

    if (server_) {
        server_->stop();
        server_.reset();
    }
}

std::vector<uint8_t> CellServer::handleCellCreateEntity(const std::vector<uint8_t>& request) {
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
            attachPlayerWorldSession(session_id, player_id, msg.entityId,
                                     {msg.position.x, msg.position.y, msg.position.z});
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

std::vector<uint8_t> CellServer::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellServer::handleCellEntityMove(const std::vector<uint8_t>& request) {
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

std::vector<uint8_t> CellServer::handleCellCrossBorder(const std::vector<uint8_t>& request) {
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
        std::cout << "scene transfer rejected: player=" << player_id.value()
                  << " target_scene=" << transfer.target_scene_id << " result="
                  << apollo::game::world::to_string(outcome.result)
                  << " at_step=" << static_cast<int>(outcome.failed_at) << std::endl;
    }

    return {};
}

std::vector<uint8_t> CellServer::handleCombatSkillCast(const std::vector<uint8_t>& request) {
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

        std::cout << "Skill cast: " << msg.casterId << " -> " << msg.targetId
                  << " (skill: " << msg.skillId << ")" << std::endl;
    }

    return {};
}

std::vector<uint8_t> CellServer::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

void CellServer::gameLoop() {
    while (running_) {
        auto startTime = std::chrono::steady_clock::now();

        worldHost_->tick();

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        const auto interval = tickInterval();
        const auto sleepMs = static_cast<int>(interval.count() - elapsed);
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }
}

std::chrono::milliseconds CellServer::tickInterval() const {
    return std::chrono::milliseconds(std::max(config_.tickRateMs, 1));
}

void CellServer::broadcastToViewers(EntityID entity_id, const std::vector<uint8_t>& message) {
    // P1-3：viewer set 驱动（旧 aoiManager_->getViewers 九宫格查询退役）——
    // 接收集合 = 实体所在 Scene 的 SceneAoi 视野查询（AOI 归 scene、scene_id
    // 隔离）；网关下发面随 P3-2（当前阶段下发 = 可观察日志）
    (void)message;
    apollo::game::world::Scene* scene = find_scene_by_entity(entity_id);
    if (scene == nullptr) {
        return;
    }
    const auto viewers = scene->aoi().viewers_of(apollo::game::core::EntityId(entity_id));
    if (!viewers.empty()) {
        std::cout << "broadcast: entity=" << entity_id << " scene=" << scene->scene_id()
                  << " viewers=" << viewers.size() << " bytes=" << message.size() << std::endl;
    }
}

std::uint64_t CellServer::resolve_scene_id(std::uint64_t space_id) const {
    // 单进程单 scene 一条目（P1-1 口径）；未知空间回落默认 scene——
    // 与 create 的旧「空间无关全落全局表」行为等价
    const auto route = spaceToScene_.find(space_id);
    return route != spaceToScene_.end() ? route->second : defaultSceneId_;
}

apollo::game::world::Scene* CellServer::find_scene_by_entity(EntityID entity_id) {
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

void CellServer::ensureDefaultScene() {
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

void CellServer::attachPlayerWorldSession(
    protocol::SessionID session_id,
    protocol::PlayerID player_id_raw,
    EntityID entity_id,
    const Position& position) {
    ensureDefaultScene();

    // 强分型（P0-2）：玩家身份与实体身份在类型面上分离——旧代码 entity_id 直接
    // 当 player_id 传（find_by_player/attach 三参同号），编译器不可见。
    const auto player_id = apollo::game::core::PlayerId(player_id_raw);
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);

    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        session = worldSessionManager_->create_session(session_id, player_id);
    }

    auto* scene = world_->find_scene(defaultSceneId_);

    // create Avatar + scene.enter / instance.enter（P0-3 任务书 §27 API 路径：
    // Avatar 由 Scene 持有（进 scene 生/出 scene 死），玩法侧走 instance.enter）
    auto avatar = std::make_shared<apollo::game::world::Avatar>(player_id, avatar_entity_id);
    if (scene != nullptr) {
        scene->enter(avatar, {position.x, position.y, position.z});
    }
    if (defaultInstance_) {
        defaultInstance_->enter(player_id);
    }

    session->assign_world(worldId_);
    session->assign_map_instance(defaultInstance_ ? defaultInstance_->id() : 0);
    // P0-2：space 与 instance 分离——space 承载 id 独立（旧代码以 map instance
    // 号兼任 space 号）。P0-3 拆 SceneDescriptor 后由 scene 提供。
    session->assign_space(defaultSpaceId_);
    session->bind_avatar(avatar_entity_id);
    session->set_route_version(session->route_version() + 1);
    session->set_state(apollo::game::world::WorldSessionState::Entering);
    session->resume();
}

void CellServer::detachPlayerWorldSession(protocol::SessionID session_id, EntityID entity_id) {
    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        // 无会话 id 时按玩家身份反查（P0-2/P0-3：经 Scene 的 Avatar 容器由
        // 实体号定位玩家号，再按玩家查会话；不再把实体号当玩家号传）
        const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
        auto* scene = world_->find_scene(defaultSceneId_);
        if (scene != nullptr) {
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
    const auto player_id = session->player_id();
    auto* scene = world_->find_scene(defaultSceneId_);
    if (scene != nullptr) {
        scene->leave(player_id);
    }
    if (defaultInstance_) {
        defaultInstance_->leave(player_id);
    }

    worldSessionManager_->suspend_session(session->session_id());
    worldSessionManager_->close_session(session->session_id());
    // close 两段式收尾（P0-4）：Leaving 可观察窗口后显式终结（Closed 终态、
    // 摘除双索引）；cell 侧登出无延迟清理面，窗口内即 finalize。
    worldSessionManager_->finalize_session(session->session_id());
}

} // namespace cell
