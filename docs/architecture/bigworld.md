---
title: BigWorld 架构深度解析
icon: server
order: 2
category:
  - 架构
  - BigWorld
tag:
  - BigWorld
  - 架构
  - CellApp
  - BaseApp
---

# BigWorld 架构深度解析

## 目录
- [什么是 BigWorld](#什么是-bigworld)
- [BigWorld 核心概念](#bigworld-核心概念)
- [BigWorld vs 传统架构对比](#bigworld-vs-传统架构对比)
- [BigWorld 核心组件](#bigworld-核心组件)
- [BigWorld 的特点](#bigworld-的特点)
- [适用场景](#适用场景)
- [BigWorld 在 Apollo 中的兼容层](#bigworld-在-apollo-中的兼容层)

---

## 一、什么是 BigWorld？

**BigWorld** 是一个专为 MMO 设计的服务器引擎技术，最著名的应用是 Wargaming 的《坦克世界》、《战舰世界》。

```
与传统游戏服务器对比:

传统MMO架构:           BigWorld架构:
┌─────────┐            ┌─────────────────────────┐
│  Gate   │            │      CellApp (单元格)   │
├─────────┤            ├─────────────────────────┤
│  Logic  │            │      BaseApp (数据库)   │
├─────────┤            ├─────────────────────────┤
│  DB     │            │     LoginApp (登录)     │
└─────────┘            │     ChatApp (聊天)      │
                        │      ... (专用服务)      │
                        └─────────────────────────┘
```

---

## 二、BigWorld 核心概念

### 1. 空间分割架构 (CellApps)

BigWorld 将游戏世界划分为**空间单元**，每个单元由独立的 **CellApp** 进程管理。

```
世界地图按区域划分:
┌────────┬────────┬────────┬────────┐
│CellApp0│CellApp1│CellApp2│CellApp3│
│  区域A  │  区域B  │  区域C  │  区域D  │
├────────┼────────┼────────┼────────┤
│CellApp4│CellApp5│CellApp6│CellApp7│
│  区域E  │  区域F  │  区域G  │  区域H  │
└────────┴────────┴────────┴────────┘

玩家跨区域移动时:
Zone A ──(无缝迁移)──> Zone B
```

**特点：**
- 每个区域独立进程，**故障隔离**
- 负载均衡：热点区域可独立扩展
- **无缝世界**：玩家感觉不到边界

### 2. 实体-组件分离 (Entity + Real/Shadow)

BigWorld 引入了 **Real** 和 **Shadow** 实体的概念：

```cpp
// Real Entity - 真实的、有状态的服务器端实体
class RealEntity {
    Position position;
    int health;
    std::vector<Spell> activeSpells;
    void update(float dt);  // 每帧更新逻辑
};

// Shadow Entity - 影子实体，只做状态同步
class ShadowEntity {
    EntityID id;
    Position position;     // 只同步位置
    int health;            // 只同步血量
    // 没有逻辑，只是数据容器
};
```

**设计理念：**
```
在本地区域的玩家 → Real Entity (有逻辑)
在远处区域看到的玩家 → Shadow Entity (只同步)
```

### 3. 唯一ID系统

BigWorld 使用 **64位唯一ID** 标识所有实体：

```cpp
struct EntityID {
    uint64_t id;

    // 格式: [类型(8位) | 服务器ID(16位) | 实体索引(40位)]
    uint8_t  type;      // Entity/Player/Monster等
    uint16_t serverId;  // 来源服务器
    uint64_t index;     // 本地索引
};
```

---

## 三、BigWorld vs 传统架构对比

### 对比表

| 特性 | 传统MMO架构 | BigWorld架构 |
|------|------------|--------------|
| **世界划分** | 单一世界/分线 | 空间分割 CellApps |
| **负载均衡** | 玩家数量分线 | 按区域动态迁移 |
| **扩展方式** | 垂直扩展 | 水平扩展 |
| **故障影响** | 整个世界/分线 | 只影响单个区域 |
| **无缝切换** | 需要切换场景 | 真正的无缝世界 |
| **实体管理** | 集中式 | 分布式 + Ghost模式 |
| **位置同步** | AOI九宫格 | CellApps间协商 |
| **数据持久化** | 定时保存 | BaseApp 专用服务 |

---

## 四、BigWorld 核心组件

### 1. CellApp - 游戏逻辑服务器

```cpp
class CellApp {
    // 管理一个空间区域
    Rect bounds;
    std::vector<RealEntity*> entities;

    // 跨区域通信
    void onEntityEnter(Entity* e, CellApp* from);
    void onEntityLeave(Entity* e, CellApp* to);

    // 与相邻 CellApp 同步边界实体
    void syncGhosts();
};

// 关键：区域交接
void CellApp::handoverEntity(Entity* e, Position newPos) {
    if (!myBounds.contains(newPos)) {
        CellApp* neighbor = findNeighbor(newPos);
        neighbor->receiveEntity(e, this);  // 迁移实体
        entities.erase(e->id);
    }
}
```

### 2. BaseApp - 数据库服务器

```cpp
class BaseApp {
    // 专门处理数据库操作
    DatabaseConnection db;

    // 异步加载数据
    Future<PlayerData> loadPlayer(uint64_t playerId);

    // 定时保存
    void savePlayer(PlayerData& data);

    // 不处理游戏逻辑，只做数据CRUD
};
```

**设计理念：数据库IO与应用逻辑分离**

### 3. LoginApp - 登录服务器

```cpp
class LoginApp {
    // 处理认证
    AuthManager auth;

    // 分配玩家到合适的 CellApp
    CellApp* assignCellApp(Player* p);

    // 创建 Proxy
    Proxy* createProxy(Player* p);
};
```

### 4. Proxy - 玩家连接代理

```cpp
class Proxy {
    // 代表玩家与服务器通信
    NetworkConnection connection;
    EntityID controlledEntity;  // 控制的实体ID

    // 转发客户端消息到 CellApp
    void forwardToCell(ClientMessage& msg);

    // 从 CellApp 接收更新
    void sendToClient(EntityUpdate& update);
};
```

---

## 五、BigWorld 的特点

### ✅ 优点

#### 1. 真正的无缝世界
```
玩家从地图一端走到另一端:
传统: ┌────┐ ┌────┐ ┌────┐  需要加载/切换场景
      │地图1│→│地图2│→│地图3│
      └────┘ └────┘ └────┘

BigWorld: ┌─────────────────────────┐
           │   单一连续世界          │  玩家感觉不到边界
           │                         │
           └─────────────────────────┘
```

#### 2. 水平扩展能力强
```
热点区域(如主城)压力大:
传统: 整个分线都卡
BigWorld: 只给主城区域加 CellApp
          ┌───────────────┐
          │主城:3个CellApp│  独立扩展
          └───────────────┘
```

#### 3. 故障隔离
```
一个 CellApp 崩溃:
影响范围: 只有一个区域的玩家掉线
其他区域: 正常运行
```

#### 4. 负载均衡
```
玩家自动迁移到低负载 CellApp:
CellApp A (1000人) ──→── CellApp B (300人)
```

### ❌ 缺点

#### 1. 复杂度高
```cpp
// 跨边界实体同步逻辑复杂
void updateEntityAtBorder(Entity* e) {
    // 1. 检查是否在边界
    // 2. 通知相邻 CellApp
    // 3. 创建 Ghost 实体
    // 4. 同步状态
    // 5. 处理边界碰撞
    // ... 100+ 行代码
}
```

#### 2. 一致性问题
```
问题: 玩家在边界附近
       CellApp A 认为在左边
       CellApp B 认为在右边

需要: 复杂的协商机制
```

#### 3. 调试困难
```
一个 bug 涉及多个 CellApp:
- 无法单步调试
- 日志分散在多个进程
- 时序问题难以重现
```

#### 4. 边界处理特殊
```
边界附近的技能/碰撞需要特殊处理:
- 技能跨越边界
- 子弹跨越边界
- NPC巡逻跨越边界
```

---

## 六、CellApp 水平扩展详解

### 核心思想：按位置（空间）拆分

BigWorld 的水平扩展**核心就是按位置（空间分割）**。

### 传统 vs BigWorld 扩展方式对比

#### 传统MMO：垂直扩展 + 分线复制

```
┌─────────────────────────────────────────┐
│          游戏世界 (完整副本)             │
│                                          │
│  🏰城堡──────────────🌲森林             │
│                          🏠村庄          │
│                                          │
└─────────────────────────────────────────┘
         ↓ 复制整个世界
┌─────────────────────────────────────────┐
│          分线1 (完整副本)                 │
│  🏰城堡──────────────🌲森林             │
│                          🏠村庄          │
└─────────────────────────────────────────┘
┌─────────────────────────────────────────┐
│          分线2 (完整副本)                 │
│  🏰城堡──────────────🌲森林             │
│                          🏠村庄          │
└─────────────────────────────────────────┘

扩展方式 = 加更多服务器，每个服务器跑完整世界
问题 = 每个分线是独立的，玩家无法跨分线交互
```

#### BigWorld：空间水平切割

```
┌─────────────────────────────────────────────────────┐
│                  单一游戏世界                        │
│                                                     │
│   ┌─────────┬─────────┬─────────┬─────────┐        │
│   │CellApp0 │CellApp1 │CellApp2 │CellApp3 │        │
│   │ 雪山区域 │ 城堡区域│ 森林区域│ 村庄区域│        │
│   │  500人  │ 2000人  │  800人  │ 1200人  │        │
│   ├─────────┼─────────┼─────────┼─────────┤        │
│   │CellApp4 │CellApp5 │CellApp6 │CellApp7 │        │
│   │ 荒原区域│ 主城区域│ 沙漠区域│ 海域区域│        │
│   │  300人  │ 5000人⭐│  600人  │  400人  │        │
│   └─────────┴─────────┴─────────┴─────────┘        │
│                                                     │
└─────────────────────────────────────────────────────┘

⭐ 主城人多 = 可以单独为主城加更多服务器资源
```

### 动态负载均衡

```
主城区域玩家过多:
┌────────────────────────────┐
│   CellApp5 (主城)          │
│   💥 5000玩家 = 拥挤      │
└────────────────────────────┘
          ↓ 自动分割
┌───────────────┬───────────────┐
│ CellApp5a     │ CellApp5b     │
│ 西半主城 2500 │ 东半主城 2500 │
└───────────────┴───────────────┘
```

**关键**：分割对玩家**透明**！玩家感觉不到自己被分配到了不同的 CellApp。

---

## 七、跨边界处理

### 1. 玩家跨 CellApp 移动

```
玩家从 CellApp0 移动到 CellApp1:

CellApp0                  CellApp1
   │                          │
   │ 1. 玩家接近边界          │
   ├──────────────────────────>│
   │ 2. 创建 Shadow 实体       │
   │                          │
   │ 3. 转移 Real 实体控制权  │
   │<──────────────────────────┤
   │ 4. 删除 Shadow 实体       │
   ▼                          ▼
完成迁移，玩家感觉无感
```

### 2. 边界交互

```
两个玩家在边界附近开战:

┌──────────────┬──────────────┐
│  CellApp0    │  CellApp1    │
│              │              │
│   🧙‍♂️法师 ←───→🏹射手       │  跨边界攻击
│              │              │
└──────────────┴──────────────┘

处理方式:
1. 射手在 CellApp1 创建 法师的 Shadow
2. 法师在 CellApp0 创建 射手的 Shadow
3. 伤害计算在各自 CellApp
4. 通过消息同步结果
```

---

## 八、适用场景

### BigWorld 适合的场景 ✅

| 场景 | 原因 |
|------|------|
| **大型开放世界MMO** | 无缝世界体验 |
| **车辆/战车游戏** | 载具跨区域移动 |
| **高并发热点** | 主城可独立扩展 |
| **长期运营** | 模块化便于维护 |

### BigWorld 不适合的场景 ❌

| 场景 | 原因 |
|------|------|
| **副本/闯关** | 不需要复杂的空间分割 |
| **回合制卡牌** | 无需位置同步 |
| **小规模游戏** | 架构过于复杂 |
| **强一致性要求** | 分布式一致性难保证 |

---

## 九、BigWorld 在 Apollo 中的兼容层

```cpp
// modules/bigworld/include/apollo/bigworld/runtime.hpp

namespace BigWorld {  // 全局命名空间兼容

    // 实体ID兼容
    using EntityID = uint64_t;
    using SpaceID = uint32_t;

    // 实体接口
    struct IEntity {
        virtual SpaceID spaceID() const = 0;
        virtual Position position() const = 0;
        virtual void teleport(Position pos) = 0;
        virtual void destroy() = 0;
    };

    // 回调接口
    struct ICallback {
        virtual void onEntityEnter(IEntity*) {}
        virtual void onEntityLeave(IEntity*) {}
        virtual void onSpaceGeometryChanged(SpaceID) {}
    };

    // 运行时接口
    struct IRuntime {
        virtual IEntity* createEntity(SpaceID, const char* type) = 0;
        virtual void destroyEntity(EntityID) = 0;
        virtual void registerCallback(ICallback*) = 0;
    };

    // 全局访问点 (BigWorld 风格)
    IRuntime* Runtime();
    IEntity* getEntity(EntityID id);
}
```

---

## 十、总结

```
传统MMO架构          vs          BigWorld架构
    │                           │
    ├─ 单一世界                ├─ 空间分割
    ├─ 垂直扩展                ├─ 水平扩展
    ├─ 简单直观                ├─ 复杂但强大
    ├─ 全局故障                ├─ 故障隔离
    ├─ 场景切换                ├─ 无缝世界
    └─ 适合副本/小规模         └─ 适合大世界MMO
```

**BigWorld 的本质**：用**空间换时间**，通过**水平扩展**和**区域隔离**，实现超大规模无缝世界的MMO服务器。

在 Apollo 中，BigWorld 模块作为**兼容层**，允许现有 BigWorld 代码平滑迁移到新的模块化架构。
