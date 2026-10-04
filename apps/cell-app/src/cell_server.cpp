#include "cell/cell_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/game/world/map_instance_manager.hpp"
#include "apollo/game/world/world_session_manager.hpp"
#include "apollo/runtime/world_host.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdexcept>

namespace cell {

namespace protocol = apollo::protocol;

namespace {

class CellWorldService final : public apollo::runtime::IWorldService {
public:
    CellWorldService(
        EntityManager& entity_manager,
        apollo::game::world::MapInstanceManager& map_instance_manager,
        CellConfig config)
        : entity_manager_(entity_manager)
        , map_instance_manager_(map_instance_manager)
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
        entity_manager_.update(static_cast<float>(context.delta_seconds));
        map_instance_manager_.update(static_cast<float>(context.delta_seconds));
    }

    void shutdown() override {
        tick_count_ = 0;
    }

private:
    EntityManager& entity_manager_;
    apollo::game::world::MapInstanceManager& map_instance_manager_;
    CellConfig config_;
    std::uint64_t tick_count_ = 0;
};

} // namespace

//==============================================================================
// AOIManager 实现
//==============================================================================

AOIManager::AOIManager(const CellConfig& config)
    : config_(config) {
    gridWidthCount_ = static_cast<int>(std::ceil(config_.spaceWidth / config_.gridSize));
    gridHeightCount_ = static_cast<int>(std::ceil(config_.spaceHeight / config_.gridSize));

    grids_.resize(gridWidthCount_ * gridHeightCount_);
}

void AOIManager::enter(Entity* entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    int gridId = getGridId(entity->position());
    entityGridMap_[entity->id()] = gridId;
    grids_[gridId].entities.push_back(entity);

    // 通知进入视野
    updateView(entity);
}

void AOIManager::move(Entity* entity, const Position& newPos) {
    std::lock_guard<std::mutex> lock(mutex_);

    int oldGridId = getGridId(entity->position());
    int newGridId = getGridId(newPos);

    // 更新位置
    entity->setPosition(newPos);

    if (oldGridId != newGridId) {
        // 跨格子移动
        auto& oldGrid = grids_[oldGridId];
        oldGrid.entities.erase(
            std::remove(oldGrid.entities.begin(), oldGrid.entities.end(), entity),
            oldGrid.entities.end()
        );

        auto& newGrid = grids_[newGridId];
        newGrid.entities.push_back(entity);

        entityGridMap_[entity->id()] = newGridId;
    }

    // 更新视野
    updateView(entity);
}

void AOIManager::leave(Entity* entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entityGridMap_.find(entity->id());
    if (it != entityGridMap_.end()) {
        auto& grid = grids_[it->second];
        grid.entities.erase(
            std::remove(grid.entities.begin(), grid.entities.end(), entity),
            grid.entities.end()
        );
        entityGridMap_.erase(it);
    }

    // 清理视野
    viewMap_.erase(entity->id());

    // 通知其他实体
    for (auto& pair : viewMap_) {
        auto& viewers = pair.second;
        viewers.erase(
            std::remove(viewers.begin(), viewers.end(), entity),
            viewers.end()
        );
    }
}

std::vector<Entity*> AOIManager::getViewers(const Position& pos, float radius) {
    std::vector<Entity*> result;

    int centerX = getGridX(pos.x);
    int centerY = getGridY(pos.y);

    // 检查九宫格
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int gx = centerX + dx;
            int gy = centerY + dy;

            if (gx < 0 || gx >= gridWidthCount_ || gy < 0 || gy >= gridHeightCount_) {
                continue;
            }

            int gridId = gy * gridWidthCount_ + gx;
            const auto& grid = grids_[gridId];

            for (auto* entity : grid.entities) {
                if (entity->position().distanceTo(pos) <= radius) {
                    result.push_back(entity);
                }
            }
        }
    }

    return result;
}

std::vector<Entity*> AOIManager::getViewers(Entity* entity) {
    return getViewers(entity->position(), config_.viewRadius);
}

void AOIManager::updateView(Entity* entity) {
    auto viewers = getViewers(entity);

    // 检查新增的视野内实体
    for (auto* other : viewers) {
        auto& myViewers = viewMap_[entity->id()];
        if (std::find(myViewers.begin(), myViewers.end(), other) == myViewers.end()) {
            myViewers.push_back(other);
            entity->onEnterView(other);
        }

        // 双向视野
        auto& otherViewers = viewMap_[other->id()];
        if (std::find(otherViewers.begin(), otherViewers.end(), entity) == otherViewers.end()) {
            otherViewers.push_back(entity);
            other->onEnterView(entity);
        }
    }
}

int AOIManager::getGridX(float x) const {
    return static_cast<int>(std::floor(x / config_.gridSize));
}

int AOIManager::getGridY(float y) const {
    return static_cast<int>(std::floor(y / config_.gridSize));
}

int AOIManager::getGridId(const Position& pos) const {
    int gx = getGridX(pos.x);
    int gy = getGridY(pos.y);

    gx = std::max(0, std::min(gx, gridWidthCount_ - 1));
    gy = std::max(0, std::min(gy, gridHeightCount_ - 1));

    return gy * gridWidthCount_ + gx;
}

//==============================================================================
// EntityManager 实现
//==============================================================================

EntityManager::EntityManager(const CellConfig& config)
    : config_(config) {
}

Entity* EntityManager::createEntity(EntityID id, EntityType type) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unique_ptr<Entity> entity;
    switch (type) {
        case EntityType::PLAYER:
            entity = std::make_unique<PlayerEntity>(id);
            break;
        default:
            entity = std::make_unique<Entity>(id, type);
            break;
    }

    auto* ptr = entity.get();
    entities_[id] = std::move(entity);

    std::cout << "Created entity " << id << " (type: " << static_cast<int>(type) << ")" << std::endl;

    return ptr;
}

void EntityManager::destroyEntity(EntityID id) {
    std::lock_guard<std::mutex> lock(mutex_);
    entities_.erase(id);
    std::cout << "Destroyed entity " << id << std::endl;
}

Entity* EntityManager::getEntity(EntityID id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(id);
    if (it != entities_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<Entity*> EntityManager::getAllEntities() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Entity*> result;
    result.reserve(entities_.size());

    for (auto& pair : entities_) {
        result.push_back(pair.second.get());
    }

    return result;
}

void EntityManager::update(float dt) {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& pair : entities_) {
        pair.second->update(dt);
    }
}

//==============================================================================
// CellServer 实现
//==============================================================================

CellServer::CellServer(const CellConfig& config)
    : config_(config)
    , entityManager_(std::make_unique<EntityManager>(config))
    , aoiManager_(std::make_unique<AOIManager>(config))
    , mapInstanceManager_(std::make_shared<apollo::game::world::MapInstanceManager>())
    , worldSessionManager_(std::make_shared<apollo::game::world::WorldSessionManager>())
    , worldHost_(std::make_shared<apollo::runtime::WorldHost>(
          static_cast<std::uint32_t>(1000 / std::max(config.tickRateMs, 1)))) {
    worldHost_->add_world_service(
        std::make_shared<CellWorldService>(*entityManager_, *mapInstanceManager_, config_));
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
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR_MESSAGE);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    server_->start();

    ensureDefaultMapInstance();

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

    // 创建实体
    auto* entity = entityManager_->createEntity(
        msg.entityId,
        static_cast<EntityType>(msg.entityType)
    );

    if (entity) {
        entity->setPosition(Position{msg.position.x, msg.position.y, msg.position.z});
        aoiManager_->enter(entity);

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
        }
    }

    // 返回确认（简化）
    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellServer::handleCellDestroyEntity(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellDestroyEntity>(bodyData);

    auto* entity = entityManager_->getEntity(msg.entityId);
    if (entity) {
        aoiManager_->leave(entity);
    }
    detachPlayerWorldSession(header.sessionId, msg.entityId);
    entityManager_->destroyEntity(msg.entityId);

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellServer::handleCellEntityMove(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(bodyData);

    auto* entity = entityManager_->getEntity(msg.entityId);
    if (entity) {
        aoiManager_->move(entity, Position{msg.newPos.x, msg.newPos.y, msg.newPos.z});

        // 广播移动消息给视野内玩家
        broadcastToViewers(entity, request);
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellServer::handleCellCrossBorder(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());
    auto msg = protocol::MessageCodec::decodeBody<protocol::CellCrossBorder>(bodyData);

    // P0-2：类型显式——先经 Avatar 容器由实体号反查玩家号，再按玩家查会话
    // （旧代码直接把实体号当玩家号传 find_by_player，编译器不可见）
    auto session = worldSessionManager_->find_session(header.sessionId);
    if (!session) {
        const auto caster_entity = apollo::game::core::EntityId(msg.entityId);
        for (const auto& [player_id, avatar] : avatars_) {
            if (avatar->entity_id() == caster_entity) {
                session = worldSessionManager_->find_by_player(player_id);
                break;
            }
        }
    }
    if (session) {
        worldSessionManager_->transfer_session(
            session->session_id(),
            worldId_,
            defaultMapInstanceId_,
            defaultSpaceId_,
            false);
        worldSessionManager_->complete_transfer(session->session_id());
    }

    return {};
}

std::vector<uint8_t> CellServer::handleCombatSkillCast(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CombatSkillCast>(bodyData);

    // 处理技能逻辑
    auto* caster = entityManager_->getEntity(msg.casterId);
    auto* target = entityManager_->getEntity(msg.targetId);

    if (caster && target) {
        // 简化伤害计算
        protocol::CombatDamage damageMsg;
        damageMsg.targetId = msg.targetId;
        damageMsg.damage = -10;  // 固定伤害
        damageMsg.sourceId = msg.casterId;

        auto damageData = protocol::MessageCodec::encode(damageMsg);

        // 广播伤害
        broadcastToViewers(target, damageData);

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

void CellServer::broadcastToViewers(Entity* entity, const std::vector<uint8_t>& message) {
    auto viewers = aoiManager_->getViewers(entity);

    // 简化处理：实际应该通过 Gateway 发送
    for (auto* viewer : viewers) {
        if (viewer->type() == EntityType::PLAYER) {
            // 发送给玩家
            (void)viewer;  // 避免未使用警告
            // 实际实现需要查找玩家对应的 Gateway 连接
        }
    }
}

void CellServer::ensureDefaultMapInstance() {
    if (mapInstanceManager_->find_instance(defaultMapInstanceId_)) {
        return;
    }

    mapInstanceManager_->create_instance(
        defaultMapInstanceId_,
        config_.spaceName.empty() ? std::string("default-world") : config_.spaceName);
}

void CellServer::attachPlayerWorldSession(
    protocol::SessionID session_id,
    protocol::PlayerID player_id_raw,
    EntityID entity_id,
    const Position& position) {
    ensureDefaultMapInstance();

    // 强分型（P0-2）：玩家身份与实体身份在类型面上分离——旧代码 entity_id 直接
    // 当 player_id 传（find_by_player/attach 三参同号），编译器不可见。
    const auto player_id = apollo::game::core::PlayerId(player_id_raw);
    const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);

    auto session = worldSessionManager_->find_session(session_id);
    if (!session) {
        session = worldSessionManager_->create_session(session_id, player_id);
    }

    // create Avatar by scene（P0-2 最小闭环：「进 scene 生」。
    // P0-3 起由 instance.enter 接管创建与 scene 挂载。）
    auto avatar = avatars_[player_id];
    if (!avatar) {
        avatar = std::make_shared<apollo::game::world::Avatar>(player_id, avatar_entity_id);
        avatars_[player_id] = avatar;
    }
    avatar->set_position({position.x, position.y, position.z});
    avatar->attach_scene(defaultMapInstanceId_);

    session->assign_world(worldId_);
    session->assign_map_instance(defaultMapInstanceId_);
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
        // 无会话 id 时按玩家身份反查（P0-2：类型显式——先经 Avatar 容器由
        // 实体号定位玩家号，再按玩家查会话；不再把实体号当玩家号传）
        const auto avatar_entity_id = apollo::game::core::EntityId(entity_id);
        for (const auto& [player_id, avatar] : avatars_) {
            if (avatar->entity_id() == avatar_entity_id) {
                session = worldSessionManager_->find_by_player(player_id);
                break;
            }
        }
    }

    if (!session) {
        return;
    }

    // 出 scene 死：Avatar 销毁（P0-3 迁入 scene 后由 instance.leave 接管）
    const auto player_id = session->player_id();
    auto avatar_it = avatars_.find(player_id);
    if (avatar_it != avatars_.end()) {
        avatar_it->second->begin_leave();
        avatars_.erase(avatar_it);
    }

    worldSessionManager_->suspend_session(session->session_id());
    worldSessionManager_->close_session(session->session_id());
}

} // namespace cell
