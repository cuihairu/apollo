# Q31: KBEngine 如何高效广播？如何保证消息不重复？

## 问题分析

本题考察对 KBEngine **AOI 广播机制** 和 **消息去重策略** 的理解：
- AOI 的实现方式（三轴十字链表）
- ViewEntity 和 Witness 机制
- 消息广播的去重策略

---

## 一、KBEngine 的 AOI 实现

### 三轴十字链表

根据 [KBEngine 源码分析](https://www.cnblogs.com/coding-my-life/p/14256640.html)：

```
KBEngine 的 AOI 实现：三轴十字链表

传统十字链表（2D）：
     Y
     ↑
     │
  ───┼─── X
     │

KBEngine 三轴十字链表（3D）：
     Y
     ↑
     │
  ───┼─── X
   ╱
  ↓ Z

每个 Entity 维护三条链表：
- pXXX_：X 轴方向的邻居
- pYXX_：Y 轴方向的邻居
- pZXX_：Z 轴方向的邻居

查找附近的 Entity：
1. 遍历三条链表
2. 计算距离
3. 判断是否在视野范围内
```

### CoordinateSystem 核心

根据源码分析，`CoordinateSystem` 是 AOI 的核心：

```cpp
// coordinate_system.h 核心结构
class CoordinateSystem {
private:
    // 三轴链表节点
    struct CoordinateNode {
        Entity* pEntity;

        // 三条链表指针
        CoordinateNode* pXNext_;   // X 轴正向
        CoordinateNode* pXPrev_;   // X 轴负向
        CoordinateNode* pYNext_;   // Y 轴正向
        CoordinateNode* pYPrev_;   // Y 轴负向
        CoordinateNode* pZNext_;   // Z 轴正向
        CoordinateNode* pZPrev_;   // Z 轴负向

        float x, y, z;            // 3D 坐标
    };

    // 插入节点（Entity 进入世界）
    bool insert(CoordinateNode* pNode);

    // 更新节点位置
    void update(CoordinateNode* pNode);
};
```

---

## 二、ViewEntity 和 Witness 机制

### 视野管理架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      KBEngine 视野管理                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Entity (观察者)                                                │
│  │                                                              │
│  │  ViewEntity (被观察者列表)                                   │
│  │  │  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │  │  Witness1 │  Witness2 │  Witness3 │  (能看到我的)       │
│  │  │  (玩家A)  │  (玩家B)  │  (NPC)    │                   │
│  │  │  ┌──────┐  │  ┌──────┐  │  ┌──────┐                   │
│  │  │  │ViewEntities│  │ViewEntities│  │ViewEntities│         │
│  │  │  └──────┘  │  └──────┘  │  └──────┘                   │
│  │  └──────────┘  └──────────┘  └──────────┘               │
│  │                                                              │
│  │  当 Entity 状态变化时：                                     │
│  │  1. 遍历 ViewEntity 中的所有 Witness                       │
│  │  2. 向每个 Witness 对应的客户端发送消息                      │
│  │  3. 自动去重（同一个客户端只发一次）                         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Witness 生命周期

根据 [KBEngine 技术概览](https://imgamer.gitbooks.io/kbengine-overview/content/content/6_1ServerComponents.html)：

```cpp
// ViewTrigger 处理视野进入/离开事件
class ViewTrigger : public RangeTrigger {
public:
    // Entity 进入视野
    virtual void onEnter(CoordinateNode* pNode) {
        Entity* pEntity = pNode->getEntity();

        // 通知观察者
        if (pWitness_ != nullptr) {
            pWitness_->onEnterWitness(pEntity);
        }
    }

    // Entity 离开视野
    virtual void onLeave(CoordinateNode* pNode) {
        Entity* pEntity = pNode->getEntity();

        // 通知观察者
        if (pWitness_ != nullptr) {
            pWitness_->onLeaveWitness(pEntity);
        }
    }

private:
    Witness* pWitness_;  // 关联的 Witness
};
```

### onEnterWitness / onLeaveWitness

```python
# Python 脚本中定义的回调

class Account(KBEngine.Entity):
    def onEnterWitness(self, entity):
        """
        当 Entity 进入我的视野时触发
        entity: 进入视野的 Entity
        """
        if entity.isPlayer():
            self.addWitness(entity)  # 添加到 ViewEntity

    def onLeaveWitness(self, entity):
        """
        当 Entity 离开我的视野时触发
        """
        if entity.isPlayer():
            self.delWitness(entity)  # 从 ViewEntity 移除
```

---

## 三、广播去重机制

### 去重策略 1：ViewEntity 自动去重

```
ViewEntity 的本质：一个客户端对应一个 Witness

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Server CellApp                                            │
│                                                             │
│  Entity_Monster                                            │
│  │                                                          │
│  │  ViewEntity (能看到我的客户端列表)                        │
│  │  ┌─────────────────────────────────────────────┐         │
│  │  │ Witness_1 → Client_A (ID=1001)            │         │
│  │  │ Witness_2 → Client_B (ID=1002)            │         │
│  │  │ Witness_3 → Client_C (ID=1003)            │         │
│  │  └─────────────────────────────────────────────┘         │
│                                                             │
│  广播时：                                                    │
│  for witness in ViewEntity:                                 │
│      witness.sendToClient(message)  ← 每个客户端只发一次      │
│                                                             │
└─────────────────────────────────────────────────────────────┘

关键：同一个客户端只会被添加一次到 ViewEntity
所以自动去重！
```

### 去重策略 2：坐标系层级

```
三轴十字链表的层级关系：

Z 轴分层 (简化为 2D 俯视图)：

     Z (前)
     ↑
     │
  ┌───┼───┐
  │ 1 │ 2 │
  ├───┼───┤
  │ 3 │ 4 │
  └───┴───┘
     │
     └───→ X (右)

Entity A 在格子 (1, 1) 附近的 Entity：
- 遍历 X 轴链表：找到 X 方向的邻居
- 遍历 Y 轴链表：找到 Y 方向的邻居
- 遍历 Z 轴链表：找到 Z 方向的邻居
- 取并集后去重（通过 EntityID）

去重方式：
for entity in x_axis + y_axis + z_axis:
    if entity.id not in seen:
        seen.add(entity.id)
        process(entity)
```

### 去重策略 3：帧标记

```cpp
// entity.h 中的去重标记
class Entity {
protected:
    // 每帧递增的 ID
    static uint32_t g_viewEntityCacheID;

    // 每个 Entity 记录最后一次处理的帧 ID
    uint32_t viewEntityCacheID_;

public:
    // 检查是否已处理
    bool isViewEntityCached(uint32_t cacheID) const {
        return viewEntityCacheID_ == cacheID;
    }

    // 标记为已处理
    void markViewEntityCached(uint32_t cacheID) {
        viewEntityCacheID_ = cacheID;
    }
};

// 使用示例
uint32_t cacheID = ++g_viewEntityCacheID;

for (auto* entity : nearbyEntities) {
    if (!entity->isViewEntityCached(cacheID)) {
        entity->markViewEntityCached(cacheID);

        // 只处理未缓存的 Entity
        addToViewEntity(entity);
    }
}
```

---

## 四、高效广播策略

### 策略 1：只广播视野内的变化

```
高效广播的核心原则：只发送必要的数据

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Entity_Player 的状态变化：                                   │
│  │                                                          │
│  │  位置变化 (每帧)                                         │
│  │  → 只广播给视野内的玩家                                  │
│  │                                                          │
│  │  血量变化 (受伤时)                                       │
│  │  → 只广播给视野内的玩家                                  │
│  │                                                          │
│  │  背包变化 (获得物品)                                     │
│  │  → 不广播（不是视野内关注的数据）                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 策略 2：增量广播

```cpp
// 每个Entity维护脏标记
class Entity {
private:
    // 属性脏标记
    uint32_t dirtyFlags_;

    // 各属性对应位
    enum DirtyFlags {
        DIRTY_POSITION = 1 << 0,
        DIRTY_ROTATION = 1 << 1,
        DIRTY_HP = 1 << 2,
        DIRTY_MP = 1 << 3,
        // ...
    };

public:
    void setPosition(float x, float y, float z) {
        x_ = x; y_ = y; z_ = z;
        dirtyFlags_ |= DIRTY_POSITION;  // 标记脏
    }

    // 广播时只发送变化的属性
    void broadcastToWitnesses() {
        if (dirtyFlags_ == 0) return;  // 无变化，不广播

        MemoryStream stream;
        if (dirtyFlags_ & DIRTY_POSITION) {
            stream.pack(x_); stream.pack(y_); stream.pack(z_);
        }
        if (dirtyFlags_ & DIRTY_HP) {
            stream.pack(hp_);
        }
        // ...

        // 发送给所有 Witness
        for (auto* witness : viewEntities_) {
            witness->send(stream);
        }

        dirtyFlags_ = 0;  // 清除脏标记
    }
};
```

### 策略 3：频率控制

```cpp
// 不同属性使用不同的广播频率

class BroadcastController {
public:
    // 高频（每帧）：位置
    static constexpr int POSITION_RATE = 1;  // 每帧
    // 中频（每 5 帧）：朝向、速度
    static constexpr int ROTATION_RATE = 5;
    // 低频（每 10 帧）：血量、状态
    static constexpr int HP_RATE = 10;

    void update() {
        frameCount_++;

        if (frameCount_ % POSITION_RATE == 0) {
            broadcastPosition();
        }
        if (frameCount_ % ROTATION_RATE == 0) {
            broadcastRotation();
        }
        if (frameCount_ % HP_RATE == 0) {
            broadcastHP();
        }
    }
};
```

---

## 五、完整的广播流程

### Entity 移动时的广播

```
玩家 A 移动时的完整流程：

1. 客户端发送移动请求
   Client → Server: "Move to (100, 0, 100)"

2. 服务端更新位置
   Entity_Player: position_ = (100, 0, 100)
   markDirty(DIRTY_POSITION)

3. AOI 检测（CoordinateSystem）
   - 更新三轴十字链表位置
   - 检测是否有新的 Entity 进入/离开视野

4. 视野变化处理（ViewTrigger）
   for newEntity in enteredEntities:
       onEnterWitness(newEntity)
           → 添加到 ViewEntity
           → 发送 "EntityEnter" 消息给客户端

   for oldEntity in leftEntities:
       onLeaveWitness(oldEntity)
           → 从 ViewEntity 移除
           → 发送 "EntityLeave" 消息给客户端

5. 状态广播（只给 ViewEntity 中的 Witness）
   for witness in ViewEntity:
       if (dirtyFlags_ != 0):
           witness.sendChanges(dirtyFlags_)

   dirtyFlags_ = 0

关键：只给 ViewEntity 中的 Witness 广播，自动避免重复
```

### 多人广播去重图解

```
场景：玩家 A、B、C 互相在视野内

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   Entity_A          Entity_B          Entity_C              │
│      │                │                │                    │
│      │                │                │                    │
│      └────────────────┼────────────────┘                    │
│                       │                                     │
│                  "Move to (100, 0, 100)"                    │
│                       │                                     │
│                       ▼                                     │
│              Server 处理移动                                 │
│                       │                                     │
│              ┌─────────────────────┐                        │
│              │  AOI 检测          │                        │
│              │  位置更新          │                        │
│              │  脏标记设置        │                        │
│              └─────────────────────┘                        │
│                       │                                     │
│        ┌──────────────┼──────────────┐                      │
│        ▼              ▼              ▼                      │
│  ViewEntity_A    ViewEntity_B    ViewEntity_C               │
│  ├─ Witness_B,C  ├─ Witness_A,C  ├─ Witness_A,B            │
│        │              │              │                      │
│        ▼              ▼              ▼                      │
│    Client_B      Client_A      Client_C                     │
│    (收到A)        (收到B)        (收到A,B)                   │
│                                                             │
│  每个客户端只收到一次消息！                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、性能优化技巧

### 1. 视野半径控制

```python
# 不同类型的 Entity 使用不同的视野半径

class NPC:
    def __init__(self):
        self.view_radius = 50  # NPC 视野小

class Player:
    def __init__(self):
        self.view_radius = 100  # 玩家视野大

class Monster:
    def __init__(self):
        self.view_radius = 30  # 怪物视野最小
```

### 2. 分层广播

```
根据消息优先级分层：

┌─────────────────────────────────────────────────────────────┐
│  高优先级通道（可靠，立即）                                   │
│  ├── 战斗伤害                                               │
│  ├── 血量变化                                               │
│  └── 死亡事件                                               │
├─────────────────────────────────────────────────────────────┤
│  中优先级通道（可靠，延迟）                                   │
│  ├── 聊天消息                                               │
│  ├── 技能释放                                               │
│  └── 状态变化                                               │
├─────────────────────────────────────────────────────────────┤
│  低优先级通道（可丢，批量）                                   │
│  ├── 位置更新                                               │
│  ├── 动画状态                                               │
│  └── 装饰变化                                               │
└─────────────────────────────────────────────────────────────┘
```

### 3. 批量打包

```cpp
// 将多个小消息打包成一个包发送

class MessageBatcher {
private:
    std::vector<Message> pending_;
    size_t batchSize_;

public:
    void addMessage(const Message& msg) {
        pending_.push_back(msg);

        if (pending_.size() >= batchSize_) {
            flush();
        }
    }

    void flush() {
        if (pending_.empty()) return;

        BatchMessage batch;
        batch.messages = pending_;

        for (auto* witness : viewEntities_) {
            witness->send(batch);
        }

        pending_.clear();
    }
};
```

---

## 七、常见问题

### Q1: 如果两个 Entity 同时移动，会重复广播吗？

```
不会！

每个 Entity 独立维护自己的 ViewEntity。
Entity A 的 ViewEntity 只包含能看到 A 的客户端。
Entity B 的 ViewEntity 只包含能看到 B 的客户端。

广播时：
- Entity A 只向 ViewEntity_A 中的 Witness 发送
- Entity B 只向 ViewEntity_B 中的 Witness 发送

没有交集，自然没有重复。
```

### Q2: 客户端如何知道哪些 Entity 在视野内？

```
服务端主动通知！

1. Entity 进入视野
   Server → Client: "onEnterWitness(entityID, entityType, position)"
   → 客户端创建 Entity 显示

2. Entity 离开视野
   Server → Client: "onLeaveWitness(entityID)"
   → 客户端销毁 Entity

3. Entity 状态更新
   Server → Client: "onUpdatePosition(entityID, x, y, z)"
   → 客户端更新位置
```

### Q3: 如何避免视野边界频繁进出？

```
使用滞后边界：

实际视野半径: 100m
添加边界:    110m
移除边界:    90m

Entity 距离 105m → 还在视野内
Entity 距离 95m  → 还在视野内
Entity 距离 89m  → 离开视野

避免在 100m 边界上频繁进出/退出
```

---

## 八、参考资料

- [游戏服务器 AOI 的实现 - coding my life](https://www.cnblogs.com/coding-my-life/p/14256640.html)
- [网络游戏同步技术二：状态同步的优化与实现](https://zhuanlan.zhihu.com/p/697158275)
- [KBEngine 技术概览 - 服务器组件](https://imgamer.gitbooks.io/kbengine-overview/content/content/6_1ServerComponents.html)
- [KBEngine GitHub - view_trigger.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/view_trigger.h)
- [KBEngine GitHub - coordinate_system.h](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/cellapp/coordinate_system.h)
