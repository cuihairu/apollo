# AOI广播与消息去重

## 目录
- [广播类型](#广播类型)
- [消息去重策略](#消息去重策略)
- [高性能优化技巧](#高性能优化技巧)
- [消息格式优化](#消息格式优化)

---

## 核心问题

当实体移动时，需要通知**视野内所有玩家**这个实体的状态变化。

```
场景：怪物从A点移动到B点
      👻 移动 →

需要通知：
- 怪物视野内所有玩家：怪物的新位置
- 同时需要告诉玩家：哪些怪物进入/离开了视野
```

---

## 一、广播类型

### 1. 进入广播
```cpp
// 玩家进入地图时
onPlayerEnter(Player* player) {
    // 1. 获取玩家视野内的所有实体
    auto entities = aoi.getEntitiesInPlayerView(player);

    // 2. 通知玩家：这些实体在你视野内
    for (Entity* e : entities) {
        player->send(EntityAppearMsg(e));  // "怪物X出现在(x,y)"
    }

    // 3. 通知视野内玩家：有一个新玩家来了
    for (Entity* e : entities) {
        if (e->isPlayer()) {
            e->send(PlayerEnterMsg(player));
        }
    }
}
```

### 2. 离开广播
```cpp
// 玩家离开时
onPlayerLeave(Player* player) {
    auto viewers = aoi.getViewers(player);  // 谁能看到我

    // 通知所有能看到我的玩家：我离开了
    for (Entity* viewer : viewers) {
        viewer->send(EntityLeaveMsg(player));
    }
}
```

### 3. 移动广播（最复杂）
```cpp
// 玩家/怪物移动时
onEntityMove(Entity* entity, Position oldPos, Position newPos) {
    // 1. 计算旧的视野和新的视野
    auto oldViewers = getEntityViewers(oldPos);
    auto newViewers = getEntityViewers(newPos);

    // 2. 找出差异
    auto enterViewers = newViewers - oldViewers;  // 新看到的玩家
    auto leaveViewers = oldViewers - newViewers;  // 丢失视野的玩家
    auto stayViewers   = oldViewers & newViewers;  // 一直看到的玩家

    // 3. 分发消息
    for (auto viewer : leaveViewers) {
        viewer->send(EntityLeaveMsg(entity));
    }

    for (auto viewer : enterViewers) {
        viewer->send(EntityAppearMsg(entity));
    }

    for (auto viewer : stayViewers) {
        viewer->send(EntityMoveMsg(entity, oldPos, newPos));  // 增量更新
    }
}
```

---

## 二、消息去重策略

### 问题：消息风暴

```
一个格子里100个玩家同时移动
→ 每个玩家通知99个其他玩家
→ 100 * 99 = 9900 条消息
→ 网络爆炸！
```

### 方案1: 感兴趣列表（Interest List）

每个实体维护一个**谁在看我**的列表：

```cpp
struct Entity {
    std::unordered_set<Entity*> viewers;  // 谁在看我
    std::unordered_set<Entity*> watchees; // 我在看谁
};

void updateViewers(Entity* entity) {
    auto newViewers = aoi.getViewers(entity);  // O(9格)
    auto oldViewers = entity->viewers;

    // 新增的观察者
    for (auto v : newViewers - oldViewers) {
        entity->viewers.insert(v);
        v->send(EntityAppearMsg(entity));
    }

    // 离开的观察者
    for (auto v : oldViewers - newViewers) {
        entity->viewers.erase(v);
        v->send(EntityLeaveMsg(entity));
    }
}
```

**优势**：
- 只在视野变化时计算，增量更新
- 避免每次移动都遍历九宫格

### 方案2: 九宫格交叉去重

```cpp
// 核心思想：每个格子维护自己的广播列表
class Grid {
    std::vector<Entity*> entities;

    // 本格子的实体需要广播给哪些格子？
    std::unordered_set<Grid*> broadcastTargets;  // 需要通知的格子
};

void broadcastMove(Entity* mover, Grid* oldGrid, Grid* newGrid) {
    // 1. 收集需要通知的格子（去重）
    std::unordered_set<Grid*> targetGrids;

    for (auto* grid : oldGrid->broadcastTargets) {
        targetGrids.insert(grid);
    }
    for (auto* grid : newGrid->broadcastTargets) {
        targetGrids.insert(grid);
    }

    // 2. 向每个目标格子广播一次
    for (auto* grid : targetGrids) {
        for (auto* entity : grid->entities) {
            if (entity != mover) {
                entity->send(EntityMoveMsg(mover));
            }
        }
    }
}
```

### 方案3: 消息聚合（Message Batching）

```cpp
// 批量发送，减少网络包数量
class MessageBatcher {
    struct PendingMessage {
        std::vector<EntityMoveData> moves;
        std::vector<EntityAppearData> appears;
        std::vector<EntityLeaveData> leaves;
    };

    std::unordered_map<Player*, PendingMessage> pending;

    void addMove(Player* receiver, Entity* mover, Pos from, Pos to) {
        pending[receiver].moves.push_back({mover->id, from, to});
    }

    void flush(int maxDelayMs) {
        // 定时批量发送
        for (auto& [player, msg] : pending) {
            if (!msg.moves.empty()) {
                player->send(BatchMoveMsg(msg.moves));  // 一次包发多个移动
            }
        }
        pending.clear();
    }
};

// 使用：每50ms批量发送一次
MessageBatcher batcher;
timer.every(50ms, [&]() { batcher.flush(); });
```

---

## 三、完整广播实现

```cpp
class AOIBroadcaster {
public:
    // 实体移动时的广播
    void onMove(Entity* entity, Position from, Position to) {
        // 1. 计算视野变化
        ViewChange change = calculateViewChange(from, to);

        // 2. 处理离开视野的玩家
        for (auto* leaver : change.leavers) {
            entity->removeWatcher(leaver);
            leaver->send(EntityLeave{entity->id});
        }

        // 3. 处理进入视野的玩家
        for (auto* enterer : change.enterers) {
            entity->addWatcher(enterer);
            enterer->send(EntityAppear{entity->id, to});
        }

        // 4. 处理一直在视野内的玩家（发送移动消息）
        if (!change.stayers.empty()) {
            // 去重：同一帧内只发送一次
            if (!entity->hasMoveFlagThisFrame()) {
                for (auto* stayer : change.stayers) {
                    stayer->send(EntityMove{entity->id, from, to});
                }
                entity->setMoveFlag();
            }
        }
    }

    // 定时清理标记（每帧清理）
    void clearFlags() {
        for (auto* entity : allEntities) {
            entity->clearMoveFlag();
        }
    }

private:
    struct ViewChange {
        std::vector<Entity*> leavers;  // 离开视野的
        std::vector<Entity*> enterers; // 进入视野的
        std::vector<Entity*> stayers;  // 一直在的
    };

    ViewChange calculateViewChange(Position from, Position to) {
        ViewChange result;

        auto oldViewers = getViewers(from);  // 旧位置能看到谁
        auto newViewers = getViewers(to);    // 新位置能看到谁

        // 计算差集（去重关键）
        for (auto* v : oldViewers) {
            if (newViewers.count(v) == 0) {
                result.leavers.push_back(v);
            } else {
                result.stayers.push_back(v);
            }
        }

        for (auto* v : newViewers) {
            if (oldViewers.count(v) == 0) {
                result.enterers.push_back(v);
            }
        }

        return result;
    }
};
```

---

## 四、高性能优化技巧

### 1. 对象池复用消息对象
```cpp
// 避免频繁分配消息对象
class MessagePool {
    std::vector<EntityMoveMsg*> freeList;

    EntityMoveMsg* acquire() {
        if (freeList.empty()) {
            return new EntityMoveMsg();
        }
        auto* msg = freeList.back();
        freeList.pop_back();
        return msg;
    }

    void release(EntityMoveMsg* msg) {
        msg->reset();
        freeList.push_back(msg);
    }
};
```

### 2. 区域广播（Zone Broadcast）
```cpp
// 将地图划分为区域，区域内统一广播
class BroadcastZone {
    std::vector<Entity*> entities;

    void broadcast(const IMessage& msg) {
        for (auto* e : entities) {
            e->send(msg);
        }
    }
};

// 使用：相同区域的消息合并
zone.broadcast(BatchMoveMsg{allMovesInThisZone});
```

### 3. 层次化AOI（多级九宫格）
```cpp
// 远距离用大格子，近距离用小格子
class HierarchicalAOI {
    NineGrid coarseGrid;  // 1000m × 1000m 粗粒度
    NineGrid fineGrid;    // 100m × 100m  细粒度

    void broadcastMove(Entity* e, Pos from, Pos to) {
        // 远距离玩家：只通知大格子
        coarseGrid.broadcastMove(e, from, to);

        // 近距离玩家：通知小格子
        fineGrid.broadcastMove(e, from, to);
    }
};
```

---

## 五、消息格式优化

### 压缩消息结构
```cpp
// 优化前：每个移动50字节
struct MoveMsg_v1 {
    uint64_t entityId;   // 8字节
    float fromX, fromY;  // 8字节
    float toX, toY;      // 8字节
    uint32_t timestamp;  // 4字节
    // ... 共50字节
};

// 优化后：每个移动15字节（打包发送）
struct BatchMoveMsg {
    uint16_t count;      // 数量
    struct {
        uint32_t entityId : 24;  // 3字节（最多1600万实体）
        int16_t deltaX : 11;     // 2字节（-1024~1023，相对坐标）
        int16_t deltaY : 11;     // 2字节
    } moves[100];  // 一次发送100个移动

    // 100个移动 = 2 + 100*7 = 702字节
    // 平均每个 = 7字节
};
```

---

## 总结

| 问题 | 解决方案 |
|------|---------|
| **广播给谁** | 维护观察者列表，增量更新 |
| **消息去重** | 位标记法 + 感兴趣列表 |
| **性能优化** | 批量发送、对象池、消息压缩 |
| **消息风暴** | 区域广播 + 层次化AOI |

核心思想：**只通知变化，避免重复，批量发送**
