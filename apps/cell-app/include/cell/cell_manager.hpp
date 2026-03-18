#pragma once

#include "cell/config.hpp"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace cell {

using EntityID = uint64_t;
using SpaceID = uint32_t;

// 实体类型
enum class EntityType : uint8_t {
    UNKNOWN = 0,
    PLAYER = 1,
    NPC = 2,
    MONSTER = 3,
    PET = 4,
};

// 位置
struct Position {
    float x, y, z;
    Position() : x(0), y(0), z(0) {}
    Position(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    float distanceTo(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

// 实体基类
class Entity {
public:
    Entity(EntityID id, EntityType type)
        : id_(id), type_(type) {}

    virtual ~Entity() = default;

    EntityID id() const { return id_; }
    EntityType type() const { return type_; }

    const Position& position() const { return position_; }
    void setPosition(const Position& pos) { position_ = pos; }

    // 每帧更新
    virtual void update(float dt) {}

    // 进入视野回调
    virtual void onEnterView(Entity* other) {}

    // 离开视野回调
    virtual void onLeaveView(Entity* other) {}

protected:
    EntityID id_;
    EntityType type_;
    Position position_;
};

// 玩家实体
class PlayerEntity : public Entity {
public:
    explicit PlayerEntity(EntityID id)
        : Entity(id, EntityType::PLAYER) {}

    std::string name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    int hp() const { return hp_; }
    void setHp(int hp) { hp_ = hp; }

    int level() const { return level_; }
    void setLevel(int level) { level_ = level; }

    void update(float dt) override {
        // 处理玩家逻辑
    }

private:
    std::string name_;
    int hp_ = 100;
    int maxHp_ = 100;
    int level_ = 1;
    int64_t exp_ = 0;
};

// AOI 管理器
class AOIManager {
public:
    explicit AOIManager(const CellConfig& config);

    // 实体进入
    void enter(Entity* entity);

    // 实体移动
    void move(Entity* entity, const Position& newPos);

    // 实体离开
    void leave(Entity* entity);

    // 获取视野内的实体
    std::vector<Entity*> getViewers(const Position& pos, float radius);
    std::vector<Entity*> getViewers(Entity* entity);

    // 更新视野
    void updateView(Entity* entity);

private:
    struct Grid {
        std::vector<Entity*> entities;
    };

    int getGridX(float x) const;
    int getGridY(float y) const;
    int getGridId(const Position& pos) const;

    CellConfig config_;
    std::vector<Grid> grids_;
    int gridWidthCount_ = 0;
    int gridHeightCount_ = 0;

    std::unordered_map<EntityID, size_t> entityGridMap_;
    std::unordered_map<EntityID, std::vector<Entity*>> viewMap_;
    std::mutex mutex_;
};

// 实体管理器
class EntityManager {
public:
    explicit EntityManager(const CellConfig& config);

    // 创建实体
    Entity* createEntity(EntityID id, EntityType type);

    // 销毁实体
    void destroyEntity(EntityID id);

    // 获取实体
    Entity* getEntity(EntityID id);

    // 获取所有实体
    std::vector<Entity*> getAllEntities();

    // 更新所有实体
    void update(float dt);

private:
    CellConfig config_;
    std::unordered_map<EntityID, std::unique_ptr<Entity>> entities_;
    std::atomic<EntityID> nextEntityId_{1};
    std::mutex mutex_;
};

} // namespace cell
