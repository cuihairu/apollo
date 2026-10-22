---
title: AOI Broadcast and Message Deduplication
icon: broadcast
order: 5
category:
  - Architecture
  - AOI
tag:
  - AOI
  - Broadcast
  - Message Deduplication
---

# AOI Broadcast and Message Deduplication

## The Core Problem

When an entity moves, **all players in its view** must be notified of the entity's state change.

```
Scenario: a monster moves from point A to point B
      M moves →

Must notify:
- all players in the monster's view: the monster's new position
- also tell the players: which monsters entered/left the view
```

---

## 1. Broadcast Types

### 1. Enter broadcast
```cpp
// 玩家进入地图时 (when a player enters the map)
onPlayerEnter(Player* player) {
    // 1. 获取玩家视野内的所有实体 (1. get all entities in the player's view)
    auto entities = aoi.getEntitiesInPlayerView(player);

    // 2. 通知玩家：这些实体在你视野内 (2. notify the player: these entities are in your view)
    for (Entity* e : entities) {
        player->send(EntityAppearMsg(e));  // "怪物X出现在(x,y)" ("monster X appears at (x,y)")
    }

    // 3. 通知视野内玩家：有一个新玩家来了 (3. notify players in the view: a new player has arrived)
    for (Entity* e : entities) {
        if (e->isPlayer()) {
            e->send(PlayerEnterMsg(player));
        }
    }
}
```

### 2. Leave broadcast
```cpp
// 玩家离开时 (when a player leaves)
onPlayerLeave(Player* player) {
    auto viewers = aoi.getViewers(player);  // 谁能看到我 (who can see me)

    // 通知所有能看到我的玩家：我离开了 (notify all players who can see me: I'm leaving)
    for (Entity* viewer : viewers) {
        viewer->send(EntityLeaveMsg(player));
    }
}
```

### 3. Move broadcast (the most complex)
```cpp
// 玩家/怪物移动时 (when a player/monster moves)
onEntityMove(Entity* entity, Position oldPos, Position newPos) {
    // 1. 计算旧的视野和新的视野 (1. compute the old view and the new view)
    auto oldViewers = getEntityViewers(oldPos);
    auto newViewers = getEntityViewers(newPos);

    // 2. 找出差异 (2. find the differences)
    auto enterViewers = newViewers - oldViewers;  // 新看到的玩家 (players newly seeing it)
    auto leaveViewers = oldViewers - newViewers;  // 丢失视野的玩家 (players who lost sight of it)
    auto stayViewers   = oldViewers & newViewers;  // 一直看到的玩家 (players seeing it all along)

    // 3. 分发消息 (3. dispatch the messages)
    for (auto viewer : leaveViewers) {
        viewer->send(EntityLeaveMsg(entity));
    }

    for (auto viewer : enterViewers) {
        viewer->send(EntityAppearMsg(entity));
    }

    for (auto viewer : stayViewers) {
        viewer->send(EntityMoveMsg(entity, oldPos, newPos));  // 增量更新 (incremental update)
    }
}
```

---

## 2. Message Deduplication Strategies

### Problem: message storms

```
100 players moving simultaneously in one grid cell
→ each player notifies 99 other players
→ 100 * 99 = 9900 messages
→ network explosion!
```

### Option 1: Interest List

Each entity maintains a list of **who is watching me**:

```cpp
struct Entity {
    std::unordered_set<Entity*> viewers;  // 谁在看我 (who is watching me)
    std::unordered_set<Entity*> watchees; // 我在看谁 (whom I am watching)
};

void updateViewers(Entity* entity) {
    auto newViewers = aoi.getViewers(entity);  // O(9格) (O(9 cells))
    auto oldViewers = entity->viewers;

    // 新增的观察者 (newly added watchers)
    for (auto v : newViewers - oldViewers) {
        entity->viewers.insert(v);
        v->send(EntityAppearMsg(entity));
    }

    // 离开的观察者 (departing watchers)
    for (auto v : oldViewers - newViewers) {
        entity->viewers.erase(v);
        v->send(EntityLeaveMsg(entity));
    }
}
```

**Advantages**:
- computed only when the view changes; incremental updates
- avoids walking the whole 3×3 grid on every move

### Option 2: 3×3 grid cross-deduplication

```cpp
// 核心思想：每个格子维护自己的广播列表 (core idea: each grid cell maintains its own broadcast list)
class Grid {
    std::vector<Entity*> entities;

    // 本格子的实体需要广播给哪些格子？ (which cells do this cell's entities need to be broadcast to?)
    std::unordered_set<Grid*> broadcastTargets;  // 需要通知的格子 (cells to notify)
};

void broadcastMove(Entity* mover, Grid* oldGrid, Grid* newGrid) {
    // 1. 收集需要通知的格子（去重） (1. collect the cells to notify (deduplicated))
    std::unordered_set<Grid*> targetGrids;

    for (auto* grid : oldGrid->broadcastTargets) {
        targetGrids.insert(grid);
    }
    for (auto* grid : newGrid->broadcastTargets) {
        targetGrids.insert(grid);
    }

    // 2. 向每个目标格子广播一次 (2. broadcast once to each target cell)
    for (auto* grid : targetGrids) {
        for (auto* entity : grid->entities) {
            if (entity != mover) {
                entity->send(EntityMoveMsg(mover));
            }
        }
    }
}
```

### Option 3: Message Batching

```cpp
// 批量发送，减少网络包数量 (send in batches to reduce the network packet count)
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
        // 定时批量发送 (batch-send on a timer)
        for (auto& [player, msg] : pending) {
            if (!msg.moves.empty()) {
                player->send(BatchMoveMsg(msg.moves));  // 一次包发多个移动 (multiple moves in one packet)
            }
        }
        pending.clear();
    }
};

// 使用：每50ms批量发送一次 (usage: batch-send once every 50ms)
MessageBatcher batcher;
timer.every(50ms, [&]() { batcher.flush(); });
```

---

## 3. Complete Broadcast Implementation

```cpp
class AOIBroadcaster {
public:
    // 实体移动时的广播 (broadcast when an entity moves)
    void onMove(Entity* entity, Position from, Position to) {
        // 1. 计算视野变化 (1. compute the view change)
        ViewChange change = calculateViewChange(from, to);

        // 2. 处理离开视野的玩家 (2. handle players leaving the view)
        for (auto* leaver : change.leavers) {
            entity->removeWatcher(leaver);
            leaver->send(EntityLeave{entity->id});
        }

        // 3. 处理进入视野的玩家 (3. handle players entering the view)
        for (auto* enterer : change.enterers) {
            entity->addWatcher(enterer);
            enterer->send(EntityAppear{entity->id, to});
        }

        // 4. 处理一直在视野内的玩家（发送移动消息） (4. handle players who have been in view all along (send move messages))
        if (!change.stayers.empty()) {
            // 去重：同一帧内只发送一次 (dedup: send only once within the same frame)
            if (!entity->hasMoveFlagThisFrame()) {
                for (auto* stayer : change.stayers) {
                    stayer->send(EntityMove{entity->id, from, to});
                }
                entity->setMoveFlag();
            }
        }
    }

    // 定时清理标记（每帧清理） (periodically clear the flags (cleared every frame))
    void clearFlags() {
        for (auto* entity : allEntities) {
            entity->clearMoveFlag();
        }
    }

private:
    struct ViewChange {
        std::vector<Entity*> leavers;  // 离开视野的 (those leaving the view)
        std::vector<Entity*> enterers; // 进入视野的 (those entering the view)
        std::vector<Entity*> stayers;  // 一直在的 (those staying in view)
    };

    ViewChange calculateViewChange(Position from, Position to) {
        ViewChange result;

        auto oldViewers = getViewers(from);  // 旧位置能看到谁 (whom the old position can see)
        auto newViewers = getViewers(to);    // 新位置能看到谁 (whom the new position can see)

        // 计算差集（去重关键） (compute the set differences (the key to dedup))
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

## 4. High-Performance Optimization Techniques

### 1. Object pool to reuse message objects
```cpp
// 避免频繁分配消息对象 (avoid frequently allocating message objects)
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

### 2. Area Broadcast
```cpp
// 将地图划分为区域，区域内统一广播 (divide the map into areas and broadcast uniformly within an area)
class BroadcastArea {
    std::vector<Entity*> entities;

    void broadcast(const IMessage& msg) {
        for (auto* e : entities) {
            e->send(msg);
        }
    }
};

// 使用：相同区域的消息合并 (usage: merge messages for the same area)
area.broadcast(BatchMoveMsg{allMovesInThisArea});
```

### 3. Hierarchical AOI (multi-level 3×3 grids)
```cpp
// 远距离用大格子，近距离用小格子 (large cells for long range, small cells for short range)
class HierarchicalAOI {
    NineGrid coarseGrid;  // 1000m × 1000m 粗粒度 (coarse granularity)
    NineGrid fineGrid;    // 100m × 100m  细粒度 (fine granularity)

    void broadcastMove(Entity* e, Pos from, Pos to) {
        // 远距离玩家：只通知大格子 (long-range players: notify only the large cells)
        coarseGrid.broadcastMove(e, from, to);

        // 近距离玩家：通知小格子 (nearby players: notify the small cells)
        fineGrid.broadcastMove(e, from, to);
    }
};
```

---

## 5. Message Format Optimization

### Compressed message structure
```cpp
// 优化前：每个移动50字节 (before optimization: 50 bytes per move)
struct MoveMsg_v1 {
    uint64_t entityId;   // 8字节 (8 bytes)
    float fromX, fromY;  // 8字节 (8 bytes)
    float toX, toY;      // 8字节 (8 bytes)
    uint32_t timestamp;  // 4字节 (4 bytes)
    // ... 共50字节 (... 50 bytes in total)
};

// 优化后：每个移动15字节（打包发送） (after optimization: 15 bytes per move (sent packed))
struct BatchMoveMsg {
    uint16_t count;      // 数量 (count)
    struct {
        uint32_t entityId : 24;  // 3字节（最多1600万实体） (3 bytes (up to 16 million entities))
        int16_t deltaX : 11;     // 2字节（-1024~1023，相对坐标） (2 bytes (-1024~1023, relative coordinates))
        int16_t deltaY : 11;     // 2字节 (2 bytes)
    } moves[100];  // 一次发送100个移动 (send 100 moves at once)

    // 100个移动 = 2 + 100*7 = 702字节 (100 moves = 2 + 100*7 = 702 bytes)
    // 平均每个 = 7字节 (7 bytes each on average)
};
```

---

## Summary

| Problem | Solution |
|------|---------|
| **Whom to broadcast to** | maintain watcher lists, update incrementally |
| **Message deduplication** | bit-flag marking + interest lists |
| **Performance optimization** | batch sending, object pools, message compression |
| **Message storms** | area broadcast + hierarchical AOI |

Core idea: **notify only of changes, avoid duplication, send in batches**
