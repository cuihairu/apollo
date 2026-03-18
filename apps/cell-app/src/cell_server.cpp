#include "cell/cell_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <iostream>
#include <cmath>

namespace cell {

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
    , aoiManager_(std::make_unique<AOIManager>(config)) {
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

    // 启动游戏循环线程
    gameThread_ = std::thread(&CellServer::gameLoop, this);

    running_ = true;

    std::cout << "Cell server listening on " << config_.host << ":" << config_.port << std::endl;
    std::cout << "Space: " << config_.spaceName << " (" << config_.spaceWidth
              << "x" << config_.spaceHeight << ")" << std::endl;
}

void CellServer::stop() {
    running_ = false;
    server_.stop();
    server_.reset();

    if (gameThread_.joinable()) {
        gameThread_.join();
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
        entity->setPosition(msg.position);
        aoiManager_->enter(entity);
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
    entityManager_->destroyEntity(msg.entityId);

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellServer::handleCellEntityMove(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto msg = protocol::MessageCodec::decodeBody<protocol::CellEntityMove>(bodyData);

    auto* entity = entityManager_->getEntity(msg.entityId);
    if (entity) {
        aoiManager_->move(entity, msg.newPos);

        // 广播移动消息给视野内玩家
        broadcastToViewers(entity, request);
    }

    return {};  // 空响应表示成功
}

std::vector<uint8_t> CellServer::handleCellCrossBorder(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    // 处理跨边界逻辑

    // 简化：直接返回
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
    const int tickRateMs = config_.tickRateMs;

    while (running_) {
        auto startTime = std::chrono::steady_clock::now();

        // 更新所有实体
        float dt = tickRateMs / 1000.0f;
        entityManager_->update(dt);

        // 计算下一帧时间
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        int sleepMs = tickRateMs - static_cast<int>(elapsed);
        if (sleepMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
    }
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

} // namespace cell
