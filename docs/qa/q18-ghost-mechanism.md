# Q18: 什么是 Ghost/Shadow 机制？

## 问题分析

本题考察对分布式游戏实体管理的理解：
- Ghost/Shadow 机制的设计原理
- KBEngine 的 Real/Ghost/Shadow 实现
- BigWorld 的实体同步机制
- 跨服务器的实体状态同步

---

## 一、Ghost/Shadow 机制概述

### 1.1 基本概念

```
┌─────────────────────────────────────────────────────────────┐
│              Ghost/Shadow 机制核心概念                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  问题：分布式环境下的实体管理                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  一个玩家实体需要存在于多个地方：                  │       │
│  │                                                   │       │
│  │  CellApp1 ──► 玩家 A 的 Real (权威)              │       │
│  │  CellApp2 ──► 玩家 A 的 Ghost (镜像)             │       │
│  │  BaseApp  ──► 玩家 A 的 Ghost (镜像)             │       │
│  │  客户端   ──► 玩家 A 的 Shadow (显示)            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  定义：                                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Real    → 权威实体，可执行逻辑                   │       │
│  │  Ghost   → 镜像实体，只读状态                     │       │
│  │  Shadow  → 客户端实体，用于显示                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  核心思想：                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 每个实体只有一个 Real 副本                    │       │
│  │  2. 可以有多个 Ghost 副本                         │       │
│  │  3. Real 负责权威逻辑计算                          │       │
│  │  4. Ghost 从 Real 同步状态                         │       │
│  │  5. Shadow 从 Ghost 同步显示                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 为什么需要 Ghost/Shadow

```
┌─────────────────────────────────────────────────────────────┐
│              Ghost/Shadow 的必要性                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景 1: 玩家跨 CellApp 交互                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  CellApp1              CellApp2                   │       │
│  │  ┌─────────┐          ┌─────────┐               │       │
│  │  │ 玩家 A   │ ───交互───→│ 玩家 B   │               │       │
│  │  │ (Real)  │          │ (Real)  │               │       │
│  │  └─────────┘          └─────────┘               │       │
│  │       │                    │                    │       │
│  │       └────Ghost────────────┘                    │       │
│  │                   │                              │       │
│  │              (互相看到对方)                       │       │
│  └─────────────────────────────────────────────────┘       │
│  → 玩家 A 在 CellApp1 需要知道玩家 B 的状态                   │
│  → 玩家 B 在 CellApp2 需要知道玩家 A 的状态                   │
│                                                             │
│  场景 2: BaseApp 需要访问实体数据                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  BaseApp                                       │       │
│  │  ┌─────────┐                                    │       │
│  │  │ 玩家 A   │ ←── Ghost 同步 ──→ CellApp      │       │
│  │  │ (Ghost) │                                    │       │
│  │  └─────────┘                                    │       │
│  │       │                                         │       │
│  │       ├── 查询玩家状态                            │       │
│  │       ├── 发送邮件                                │       │
│  │       ├── 更新任务进度                            │       │
│  │       └── 处理交易                                │       │
│  └─────────────────────────────────────────────────┘       │
│  → BaseApp 需要 CellApp 实体的状态副本                        │
│                                                             │
│  场景 3: 客户端需要显示其他玩家                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  客户端 A                                       │       │
│  │  ┌─────────────────────────────────────┐        │       │
│  │  │ 本地玩家: Shadow (可预测)           │        │       │
│  │  │ 其他玩家: Shadow (只显示)           │        │       │
│  │  │                                      │        │       │
│  │  │ ┌─────┐ ┌─────┐ ┌─────┐           │        │       │
│  │  │ │我   │ │玩家B│ │玩家C│  ← Shadow │        │       │
│  │  │ └─────┘ └─────┘ └─────┘           │        │       │
│  │  └─────────────────────────────────────┘        │       │
│  └─────────────────────────────────────────────────┘       │
│  → 客户端需要服务器实体的显示副本                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、KBEngine Real/Ghost/Shadow 实现

### 2.1 架构总览

```
┌─────────────────────────────────────────────────────────────┐
│           KBEngine Real/Ghost/Shadow 架构                     │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                    ┌──────────────┐                        │
│                    │   BaseApp    │                        │
│                    │              │                        │
│   ┌────────────────┤  Ghost Pool  ├────────────────┐       │
│   │                │              │                │       │
│   │                └───────┬──────┘                │       │
│   │                        │                       │       │
│   │         ┌──────────────┼──────────────┐       │       │
│   │         │              │              │       │       │
│   │    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐  │       │
│   │    │CellApp1 │    │CellApp2 │    │CellApp3 │  │       │
│   │    │         │    │         │    │         │  │       │
│   │    │Real Pool│    │Real Pool│    │Real Pool│  │       │
│   │    │         │    │         │    │         │  │       │
│   │    │Ghost    │    │Ghost    │    │Ghost    │  │       │
│   │    │Pool     │    │Pool     │    │Pool     │  │       │
│   │    └────┬────┘    └────┬────┘    └────┬────┘  │       │
│   │         │              │              │         │       │
│   │         └──────┬───────┴──────────────┘         │       │
│   │                │ Inter-CellApp Comm            │       │
│   │                │                               │       │
│   │         ┌──────▼──────┐                        │       │
│   │         │  CellAppMgr │                        │       │
│   │         └─────────────┘                        │       │
│   │                                                │       │
│   │    ┌─────────────────────────────────────┐     │       │
│   │    │            客户端                    │     │       │
│   │    │  ┌─────┐ ┌─────┐ ┌─────┐           │     │       │
│   │    │  │Shadow│ │Shadow│ │Shadow│  显示层 │     │       │
│   │    │  └─────┘ └─────┘ └─────┘           │     │       │
│   │    └─────────────────────────────────────┘     │       │
│   │                                                │       │
│   └────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 KBEngine 源码分析

```cpp
// KBEngine Entity 类型定义
// src/server/entitydef/entity_def.h

namespace KBEngine {

// Entity 实体类型
enum EntityType {
    ENTITY_TYPE_NULL = 0,
    ENTITY_TYPE_CLIENT = 1,      // 客户端实体
    ENTITY_TYPE_BASE = 2,        // BaseApp 实体
    ENTITY_TYPE_CELL = 3,        // CellApp 实体
};

// Entity 基类
class Entity {
public:
    enum EntityFlags {
        ENTITY_FLAG_NORMAL = 0,
        ENTITY_FLAG_DESTROYED = 1 << 0,    // 已销毁
        ENTITY_FLAG_IN_GRID = 1 << 1,      // 在网格中
        ENTITY_FLAG_HAS_GHOST = 1 << 2,    // 有 Ghost
    };

    // 实体 ID
    typedef uint32_t ID;

    // 构造函数
    Entity(ID id):
        id_(id),
        type_(ENTITY_TYPE_NULL),
        flags_(ENTITY_FLAG_NORMAL),
        pWitness_(nullptr)
    {}

    virtual ~Entity() {}

    // 获取实体 ID
    ID id() const { return id_; }

    // 是否是 Real
    bool isReal() const {
        // 在 CellApp 上且不是 Ghost 就是 Real
        return pWitness_ != nullptr;
    }

    // 是否是 Ghost
    bool isGhost() const {
        return (flags_ & ENTITY_FLAG_HAS_GHOST) != 0;
    }

protected:
    ID id_;                        // 实体 ID
    EntityType type_;              // 实体类型
    uint32_t flags_;               // 实体标志

    // Witness（观察者管理器）
    class Witness* pWitness_;
};

// CellApp 上的实体（可以是 Real 或 Ghost）
class CellApp : public Entity {
public:
    // 创建 Ghost
    bool createGhost(EntityID id, const MemoryStream& stream) {
        // 从数据流创建 Ghost 实体
        Entity* entity = EntityFactory::create(id, ENTITY_TYPE_CELL);
        if (!entity) return false;

        // 设置为 Ghost
        entity->flags_ |= ENTITY_FLAG_HAS_GHOST;

        // 反序列化初始状态
        entity->createFromStream(stream);

        // 添加到 Ghost 列表
        ghostEntities_[id] = entity;

        return true;
    }

    // 更新 Ghost 状态
    void updateGhost(EntityID id, const MemoryStream& stream) {
        auto it = ghostEntities_.find(id);
        if (it != ghostEntities_.end()) {
            it->second->onRemoteUpdate(stream);
        }
    }

    // 销毁 Ghost
    void destroyGhost(EntityID id) {
        auto it = ghostEntities_.find(id);
        if (it != ghostEntities_.end()) {
            delete it->second;
            ghostEntities_.erase(it);
        }
    }

private:
    std::unordered_map<EntityID, Entity*> ghostEntities_;
};

} // namespace KBEngine
```

### 2.3 Ghost 同步机制

```cpp
// KBEngine Ghost 同步实现
// src/server/cellapp/cellapp_interface.hpp

namespace KBEngine {

class CellAppInterface {
public:
    // 向其他 CellApp 广播实体状态（用于 Ghost 更新）
    void broadcastEntityData(EntityID entityId, const MemoryStream& data) {
        // 1. 获取实体的 AOI 观察者
        auto viewers = getWitnesses(entityId);

        // 2. 按 CellApp 分组
        std::unordered_map<ComponentID, std::vector<EntityID>> byCellApp;
        for (EntityID viewer : viewers) {
            ComponentID cellAppId = getEntityCellApp(viewer);
            byCellApp[cellAppId].push_back(viewer);
        }

        // 3. 发送到每个 CellApp
        for (const auto& [cellAppId, entityIds] : byCellApp) {
            if (cellAppId == this->componentID()) {
                // 本地 CellApp，直接更新 Ghost
                for (EntityID entityId : entityIds) {
                    updateLocalGhost(entityId, data);
                }
            } else {
                // 远程 CellApp，网络发送
                sendToCellApp(cellAppId, entityId, data);
            }
        }
    }

private:
    void sendToCellApp(ComponentID cellAppId, EntityID entityId,
                      const MemoryStream& data) {
        Bundle* pBundle = CellAppInterface::createBundle(
            cellapp,::remoteOnUpdateData);

        (*pBundle) << entityId;
        pBundle->append(data);

        CellAppInterface::send(cellapp, pBundle);
    }

    void updateLocalGhost(EntityID ghostId, const MemoryStream& data) {
        Entity* ghost = findEntity(ghostId);
        if (ghost && ghost->isGhost()) {
            ghost->onRemoteUpdate(data);
        }
    }
};

} // namespace KBEngine
```

### 2.4 Witness 机制

```cpp
// KBEngine Witness 机制
// src/server/entitydef/witness.h

namespace KBEngine {

// Witness：管理实体的观察者
class Witness {
public:
    Witness(Entity* pEntity):
        pEntity_(pEntity),
        lastUpdateTime_(0)
    {}

    // 添加观察者
    void attach(EntityID viewerID) {
        if (viewers_.count(viewerID) == 0) {
            viewers_.insert(viewerID);

            // 发送实体创建消息
            sendCreateToViewer(viewerID);
        }
    }

    // 移除观察者
    void detach(EntityID viewerID) {
        if (viewers_.erase(viewerID) > 0) {
            // 发送实体销毁消息
            sendRemoveToViewer(viewerID);
        }
    }

    // 更新观察者状态
    void updateViewers(const MemoryStream& stream) {
        for (EntityID viewerID : viewers_) {
            sendUpdateToViewer(viewerID, stream);
        }
    }

    // 获取所有观察者
    const std::unordered_set<EntityID>& viewers() const {
        return viewers_;
    }

private:
    Entity* pEntity_;
    std::unordered_set<EntityID> viewers_;
    uint32_t lastUpdateTime_;

    void sendCreateToViewer(EntityID viewerID) {
        Bundle* pBundle = ClientInterface::createBundle(
            client,::onRemoteCreateEntity);

        (*pBundle) << pEntity_->id();
        pEntity_->addPositionToBundle(pBundle);
        pEntity_->addDirectionToBundle(pBundle);

        ClientInterface::send(viewerID, pBundle);
    }

    void sendRemoveToViewer(EntityID viewerID) {
        Bundle* pBundle = ClientInterface::createBundle(
            client,::onRemoteRemoveEntity);

        (*pBundle) << pEntity_->id();

        ClientInterface::send(viewerID, pBundle);
    }

    void sendUpdateToViewer(EntityID viewerID, const MemoryStream& stream) {
        Bundle* pBundle = ClientInterface::createBundle(
            client,::onRemoteUpdateEntity);

        (*pBundle) << pEntity_->id();
        pBundle->append(stream);

        ClientInterface::send(viewerID, pBundle);
    }
};

} // namespace KBEngine
```

---

## 三、BigWorld Ghost 机制

### 3.1 BigWorld 架构

```
┌─────────────────────────────────────────────────────────────┐
│            BigWorld Real/Ghost/Shadow 架构                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                    ┌──────────────┐                        │
│                    │   BaseApp    │                        │
│                    │              │                        │
│   ┌────────────────┤  Base Entity ├────────────────┐       │
│   │                │   (Ghost)   │                │       │
│   │                └───────┬──────┘                │       │
│   │                        │                       │       │
│   │         ┌──────────────┼──────────────┐       │       │
│   │         │              │              │       │       │
│   │    ┌────▼────┐    ┌────▼────┐    ┌────▼────┐  │       │
│   │    │CellApp1 │    │CellApp2 │    │CellApp3 │  │       │
│   │    │         │    │         │    │         │  │       │
│   │    │Cell     │    │Cell     │    │Cell     │  │       │
│   │    │Entity   │    │Entity   │    │Entity   │  │       │
│   │    │(Real)   │    │(Real)   │    │(Real)   │  │       │
│   │    │         │    │         │    │         │  │       │
│   │    │Ghost    │    │Ghost    │    │Ghost    │  │       │
│   │    │Entity   │    │Entity   │    │Entity   │  │       │
│   │    └────┬────┘    └────┬────┘    └────┬────┘  │       │
│   │         │              │              │         │       │
│   │         └──────┬───────┴──────────────┘         │       │
│   │                │ Inter-CellApp Comm            │       │
│   │                                                │       │
│   │    ┌─────────────────────────────────────┐     │       │
│   │    │            客户端                    │     │       │
│   │    │  ┌─────┐ ┌─────┐ ┌─────┐           │     │       │
│   │    │  │Shadow│ │Shadow│ │Shadow│  显示层 │     │       │
│   │    │  │Entity│ │Entity│ │Entity│         │     │       │
│   │    │  └─────┘ └─────┘ └─────┘           │     │       │
│   │    └─────────────────────────────────────┘     │       │
│   │                                                │       │
│   └────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 BigWorld Ghost 同步流程

```
┌─────────────────────────────────────────────────────────────┐
│            BigWorld Ghost 同步流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Real Entity 更新:                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家 A 在 CellApp1 移动                        │       │
│  │  2. Real Entity 状态变化                           │       │
│  │  3. 检查 AOI 观察者                                │       │
│  │  4. 发现玩家 B 在 CellApp2                         │       │
│  └─────────────────────────────────────────────────┘       │
│                         │                                   │
│                         ▼                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  5. 发送 Ghost 更新到 CellApp2                    │       │
│  │  消息: {                                          │       │
│  │    entityType: "Player",                          │       │
│  │    entityId: 12345,                               │       │
│  │    position: {x: 100, y: 0, z: 200},            │       │
│  │    velocity: {x: 5, y: 0, z: 3}                  │       │
│  │  }                                                │       │
│  └─────────────────────────────────────────────────┘       │
│                         │                                   │
│                         ▼                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  6. CellApp2 更新本地 Ghost                      │       │
│  │  7. Ghost 更新玩家 A 的位置                       │       │
│  │  8. 检查玩家 B 的 AOI                             │       │
│  │  9. 通知玩家 B 的客户端                           │       │
│  └─────────────────────────────────────────────────┘       │
│                         │                                   │
│                         ▼                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  10. 客户端 B 接收更新                             │       │
│  │  11. 更新 Shadow Entity                          │       │
│  │  12. 平滑插值显示                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、Real/Ghost 转换

### 4.1 跨 CellApp 迁移

```
┌─────────────────────────────────────────────────────────────┐
│              Real/Ghost 跨 CellApp 迁移                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景：玩家从 CellApp1 移动到 CellApp2                        │
│                                                             │
│  步骤 1: 检测边界                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  CellApp1          CellApp2                       │       │
│  │  ┌─────┬────┐    ┌────┬─────┐                   │       │
│  │  │     │    │    │    │     │                   │       │
│  │  │     │ A  │───→│    │     │  ← A 跨越边界      │       │
│  │  │     │    │    │    │     │                   │       │
│  │  └─────┴────┘    └────┴─────┘                   │       │
│  │         │                │                       │       │
│  │    Real Entity        Ghost Entity              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  步骤 2: 开始迁移                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. CellApp1 检测 A 越出边界                      │       │
│  │  2. 向 CellAppMgr 请求迁移                        │       │
│  │  3. CellAppMgr 确定 CellApp2                     │       │
│  │  4. 通知 CellApp1 和 CellApp2                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  步骤 3: 迁移 Real                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. CellApp1 序列化 Real 状态                     │       │
│  │  2. 发送完整实体数据到 CellApp2                    │       │
│  │  3. CellApp2 创建新 Real                          │       │
│  │  4. CellApp1 将 Real 转为 Ghost                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  步骤 4: 更新观察者                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 通知客户端切换连接                             │       │
│  │  2. 更新所有相关 Ghost                            │       │
│  │  3. 重新计算 AOI                                  │       │
│  │  4. 旧 Real 在延迟后销毁                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 迁移代码示例

```cpp
// 跨 CellApp 实体迁移

class CellAppEntityMigration {
public:
    // 开始迁移
    void startMigration(EntityID entityId, CellAppID targetCellApp) {
        Entity* entity = getEntity(entityId);
        if (!entity || !entity->isReal()) {
            return;
        }

        // 1. 序列化实体状态
        MemoryStream entityData;
        entity->serializeTo(entityData);

        // 2. 发送到目标 CellApp
        sendMigrationData(targetCellApp, entityId, entityData);

        // 3. 转换为 Ghost
        entity->setGhost();
        migratingEntities_[entityId] = targetCellApp;

        // 4. 设置迁移超时
        scheduleMigrationTimeout(entityId, 5000); // 5秒
    }

    // 接收迁移数据
    void onMigrationData(EntityID entityId, const MemoryStream& data) {
        // 1. 创建新的 Real 实体
        Entity* newEntity = createEntityFromData(entityId, data);
        if (!newEntity) {
            // 迁移失败，通知源 CellApp
            sendMigrationFailed(entityId);
            return;
        }

        // 2. 确认为 Real
        newEntity->setReal();

        // 3. 通知源 CellApp 迁移成功
        sendMigrationSuccess(entityId);
    }

    // 迁移成功
    void onMigrationSuccess(EntityID entityId) {
        auto it = migratingEntities_.find(entityId);
        if (it != migratingEntities_.end()) {
            // 1. 清理迁移状态
            migratingEntities_.erase(it);

            // 2. 销毁旧的 Ghost
            Entity* oldGhost = getEntity(entityId);
            if (oldGhost) {
                destroyEntity(oldGhost);
            }
        }
    }

    // 迁移失败
    void onMigrationFailed(EntityID entityId) {
        auto it = migratingEntities_.find(entityId);
        if (it != migratingEntities_.end()) {
            // 1. 恢复为 Real
            Entity* entity = getEntity(entityId);
            if (entity) {
                entity->setReal();
            }

            migratingEntities_.erase(it);
        }
    }

private:
    std::unordered_map<EntityID, CellAppID> migratingEntities_;
};
```

---

## 五、Shadow Entity（客户端）

### 5.1 Shadow 与 Ghost 的关系

```
┌─────────────────────────────────────────────────────────────┐
│           Shadow Entity 与 Ghost 的关系                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  服务器端:                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  CellApp                                        │       │
│  │  ┌─────────────┐        ┌─────────────┐        │       │
│  │  │  Real       │        │  Ghost      │        │       │
│  │  │  Entity     │───────→│  Entity     │        │       │
│  │  │             │ 同步   │             │        │       │
│  │  └─────────────┘        └─────────────┘        │       │
│  │         │                       │               │       │
│  │         │                       │               │       │
│  │    ┌────▼───────────────────────▼────┐         │       │
│  │    │        BaseApp                 │         │       │
│  │    │  ┌─────────────────────────┐  │         │       │
│  │    │  │  Ghost Entity           │  │         │       │
│  │    │  └─────────────────────────┘  │         │       │
│  │    │            │                  │         │       │
│  │    └────────────┼──────────────────┘         │       │
│  │                 │                            │       │
│  └─────────────────┼────────────────────────────┘       │
│                    │ 网络同步                              │
│                    ▼                                       │
│  客户端:                                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  ┌─────────────────────────────────────┐        │       │
│  │  │        Shadow Entity               │        │       │
│  │  │                                     │        │       │
│  │  │  ┌─────────────────────────────┐    │        │       │
│  │  │  │  Visual Component          │    │        │       │
│  │  │  │  - 3D 模型                  │    │        │       │
│  │  │  │  - 动画                     │    │        │       │
│  │  │  │  - 特效                     │    │        │       │
│  │  │  └─────────────────────────────┘    │        │       │
│  │  │                                     │        │       │
│  │  │  ┌─────────────────────────────┐    │        │       │
│  │  │  │  Predictive Component      │    │        │       │
│  │  │  │  - 客户端预测 (仅本地)      │    │        │       │
│  │  │  └─────────────────────────────┘    │        │       │
│  │  │                                     │        │       │
│  │  │  ┌─────────────────────────────┐    │        │       │
│  │  │  │  Smoothing Component       │    │        │       │
│  │  │  │  - 插值平滑                  │    │        │       │
│  │  │  │  - 外推预测                  │    │        │       │
│  │  │  └─────────────────────────────┘    │        │       │
│  │  └─────────────────────────────────────┘        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 Shadow 更新流程

```cpp
// 客户端 Shadow Entity 更新

class ShadowEntity {
public:
    // 从服务器更新创建/更新 Shadow
    void onServerUpdate(const EntityUpdate& update) {
        // 1. 更新服务器位置
        serverStates_.push_back({
            update.position,
            update.direction,
            update.velocity,
            update.timestamp
        });

        // 2. 限制历史数量
        if (serverStates_.size() > MAX_SERVER_STATES) {
            serverStates_.erase(serverStates_.begin());
        }

        // 3. 如果是本地玩家，重置预测
        if (isLocalPlayer_) {
            resetPrediction();
        }
    }

    // 每帧更新
    void update(float deltaTime) {
        if (isLocalPlayer_) {
            // 本地玩家：使用预测位置
            updatePrediction(deltaTime);
        } else {
            // 其他玩家：插值服务器位置
            updateInterpolation(deltaTime);
        }
    }

private:
    void updateInterpolation(float deltaTime) {
        if (serverStates_.size() < 2) {
            return;
        }

        // 找到当前时间的插值点
        uint32_t renderTime = getCurrentTime() - INTERPOLATION_DELAY;

        auto nextIt = std::upper_bound(serverStates_.begin(),
            serverStates_.end(), renderTime,
            [](uint32_t t, const ServerState& s) {
                return t < s.timestamp;
            });

        if (nextIt == serverStates_.begin() ||
            nextIt == serverStates_.end()) {
            return;
        }

        auto prevIt = std::prev(nextIt);

        // 计算插值系数
        float t = (renderTime - prevIt->timestamp) /
                  float(nextIt->timestamp - prevIt->timestamp);

        // 插值位置
        displayPosition_ = lerp(prevIt->position, nextIt->position, t);
        displayDirection_ = slerp(prevIt->direction, nextIt->direction, t);
    }

    void updatePrediction(float deltaTime) {
        // 本地玩家使用客户端预测
        predictedPosition_ += clientInput_.velocity * deltaTime;

        // 服务器确认后校正
        if (!serverStates_.empty()) {
            ServerState& latest = serverStates_.back();
            Vector3 error = latest.position - predictedPosition_;

            // 平滑校正
            if (length(error) > MAX_CORRECTION_DISTANCE) {
                predictedPosition_ = latest.position;
            } else {
                predictedPosition_ += error * CORRECTION_SPEED;
            }
        }

        displayPosition_ = predictedPosition_;
    }

    struct ServerState {
        Vector3 position;
        Quaternion direction;
        Vector3 velocity;
        uint32_t timestamp;
    };

    std::vector<ServerState> serverStates_;
    Vector3 displayPosition_;
    Quaternion displayDirection_;
    Vector3 predictedPosition_;
    bool isLocalPlayer_;

    ClientInput clientInput_;

    static constexpr uint32_t INTERPOLATION_DELAY = 100; // ms
    static constexpr size_t MAX_SERVER_STATES = 60;
    static constexpr float MAX_CORRECTION_DISTANCE = 5.0f;
    static constexpr float CORRECTION_SPEED = 0.2f;
};
```

---

## 六、最佳实践

### 6.1 Ghost/Shadow 设计原则

```
┌─────────────────────────────────────────────────────────────┐
│              Ghost/Shadow 设计原则                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 权威唯一性                                              │
│     ├── 每个实体只有一个 Real                               │
│     ├── Real 负责所有权威计算                               │
│     ├── Ghost 只读、不执行逻辑                               │
│     └── Shadow 仅用于显示                                   │
│                                                             │
│  2. 同步优化                                                │
│     ├── 使用 AOI 过滤同步对象                                │
│     ├── 设置同步优先级                                      │
│     ├── 使用增量同步                                         │
│     └── 压缩同步数据                                         │
│                                                             │
│  3. 容错处理                                                │
│     ├── 处理同步丢失                                         │
│     ├── 处理乱序消息                                         │
│     ├── 定期全量同步                                         │
│     └── 超时重连机制                                         │
│                                                             │
│  4. 平滑处理                                                │
│     ├── 客户端插值显示                                       │
│     ├── 预测本地玩家                                         │
│     ├── 外推减少延迟感                                       │
│     └── 校正平滑过渡                                         │
│                                                             │
│  5. 性能考虑                                                │
│     ├── 限制 Ghost 数量                                      │
│     ├── 批量处理更新                                         │
│     ├── 使用对象池                                           │
│     └── 避免不必要的拷贝                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 常见问题与解决方案

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| **实体卡顿** | 同步频率低 | 增加同步频率 + 客户端插值 |
| **位置跳变** | 丢包后校正 | 使用平滑校正 + 历史插值 |
| **Ghost 不同步** | 网络分区 | 心跳检测 + 重连机制 |
| **迁移卡住** | 目标不可达 | 超时回滚 + 重新路由 |
| **Shadow 抖动** | 同步不稳定 | 增加插值延迟 + 平滑因子 |

---

## 七、总结

### Real/Ghost/Shadow 机制总结

| 类型 | 位置 | 职责 | 可写? | 可执行逻辑? |
|------|------|------|-------|-------------|
| **Real** | CellApp | 权威实体 | 是 | 是 |
| **Ghost** | CellApp/BaseApp | 镜像实体 | 否 | 否 |
| **Shadow** | 客户端 | 显示实体 | 否 | 仅预测 |

### 同步路径

```
Real Entity (CellApp)
    │
    ├─→ Ghost Entity (其他 CellApp) ──→ Shadow (客户端)
    │
    └─→ Ghost Entity (BaseApp) ──→ 数据操作
```

---

## 参考资料

- [KBEngine GitHub - Entity 定义](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/entity_def.h)
- [KBEngine GitHub - Witness 机制](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/witness.h)
- [BigWhite Wiki - Entity System](https://wiki.bigworldtech.com/)
