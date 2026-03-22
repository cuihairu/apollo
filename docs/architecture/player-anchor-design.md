---
title: PlayerAnchor 设计稿
icon: user-group
order: 8
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - PlayerAnchor
  - BaseApp
  - 设计稿
---

# PlayerAnchor 设计稿

这篇文档接在 [WorldHost 设计稿](./world-host-design.md) 后面。

目标是把 Apollo 里长期缺失的“玩家锚点层”补出来，并明确：

`什么对象才真正对应 KBE/BaseApp 语义里的玩家长期归属。`

先说结论：

- `Session` 不是玩家锚点
- `AvatarEntity` 不是玩家锚点
- `WorldHost` 不是玩家锚点
- 真正的长期归属对象应该是 `PlayerAnchor`

## 一、为什么 Apollo 现在必须补这一层

从当前代码看：

- [login_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/login-app/include/login/login_server.hpp)
  - `LoginServer` 里有 `SessionManager`
  - 登录成功后直接分配 `gateway`
- [login_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/login-app/src/login_server.cpp)
  - 当前登录流程创建的是短期 `sessionId`
  - 会话过期和分配逻辑都在 login 进程本地
- [gateway_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/include/gateway/gateway_server.hpp)
  - `GatewayServer` 维护连接和消息转发
  - `MessageRouter` 负责把消息转给 `cell/base/chat`
- [gateway_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/src/gateway_server.cpp)
  - 当前网关只知道 session 和后端转发
  - 没有一个稳定对象表示“这个玩家当前归属于谁、在哪个 world、是否在线”
- [base_server.hpp](/C:/Users/cui/Workspaces/apollo/apps/base-app/include/base/base_server.hpp)
  - 当前 `base-app` 暴露的是 `DB_LOAD_REQUEST`、`DB_SAVE_REQUEST`、`DB_QUERY_REQUEST`
  - 说明它现在更像数据服务原型

也就是说，Apollo 当前有：

- 登录会话
- 网关连接
- 世界实体
- 数据存取

但还没有：

- 玩家长期身份在在线系统中的宿主
- 玩家当前 gateway 归属
- 玩家当前 world/map 归属
- 重连恢复时的稳定归并点

这就是 `PlayerAnchor` 要补的那一层。

## 二、`PlayerAnchor` 的定位

`PlayerAnchor` 是玩家在线期的长期主对象。

它应该承接的是：

- 玩家账号登录后的在线身份
- 玩家与公网 session 的绑定
- 玩家与 world/map 的绑定
- 玩家跨图、重连、暂离时的稳定状态
- 面向持久化层的脏数据收口

它不是：

- socket 连接对象
- 地图里的表现实体
- 数据库记录本身

更准确地说：

`PlayerAnchor = Apollo 中承接玩家在线主状态的宿主对象`

这就是后续 `BaseApp` 语义真正该承载的核心。

## 三、为什么不能让 `Session == Player == AvatarEntity`

很多早期原型都会把这三层揉在一起，但一旦开始支持切图、断线重连、多实例和跨服，这样写很快就会出问题。

### 1. `Session` 是公网连接期对象

`Session` 的生命周期通常跟着公网连接走。

它应该关注：

- gateway 分配
- 连接状态
- 心跳
- 客户端协议上下文

它不应该天然等同于玩家长期在线对象。

因为玩家断线后：

- `Session` 可以消失
- 但玩家在线上下文不一定要立刻销毁

### 2. `AvatarEntity` 是世界表现对象

世界里的实体更适合表达：

- 位置
- 朝向
- 属性快照
- AOI 可见关系
- 战斗表现

它通常挂在 `WorldSpace` / `MapInstance` 内。

一旦玩家切图、切副本、离开世界服：

- 原实体可能销毁
- 新实体可能重建

所以它也不适合承载长期在线归属。

### 3. `PlayerAnchor` 才是跨 session、跨地图的稳定层

玩家从登录到下线这段时间里，真正需要稳定存在的是：

- 账号在线状态
- 当前 gateway/session 定位
- 当前 world assignment
- 待保存状态
- 正在切图或重连中的过渡状态

这些都应该挂在 `PlayerAnchor`。

## 四、它和 KBE `BaseApp` 的关系

这里要把两个概念分开：

- `当前 Apollo apps/base-app 的实现`
- `未来 Apollo base-app 的目标语义`

### 当前实现

从代码看，当前 `apps/base-app` 还不是 KBE 意义上的 `BaseApp`。

它更偏：

- 数据加载
- 数据保存
- 数据查询

### 目标语义

未来更合理的目标是：

- `base-app` 继续保留名字
- 但其内部主语义从“数据服务原型”升级为“玩家锚点宿主”

也就是说，未来 `base-app` 应该主要持有：

- `PlayerAnchor`
- `AnchorManager`
- `SessionLocator`
- `WorldAssignment`

数据服务仍然可以留在里面，但不再是唯一中心。

## 五、`PlayerAnchor` 和主要进程的关系

建议形成下面这条主链：

```text
LoginApp
    -> 认证成功
    -> 请求 AnchorManager 激活玩家锚点

BaseApp(PlayerAnchor Host)
    -> 创建/恢复 PlayerAnchor
    -> 分配 Gateway / Session / World

GatewayApp
    -> 挂公网连接
    -> 把 session 绑定到 PlayerAnchor

WorldHost
    -> 承接玩家进入世界
    -> 创建或绑定 AvatarEntity
```

### 1. `LoginApp`

`LoginApp` 负责：

- 账号认证
- 基础风控
- 生成登录结果

它不应该长期持有玩家在线主状态。

认证成功后，更合理的方向是：

- 把 `playerId` 交给 `AnchorManager`
- 由锚点层决定是否恢复旧状态、挤号、分配 gateway、分配 world

### 2. `GatewayApp`

`Gateway` 负责：

- 客户端连接
- 心跳
- 协议编解码
- 上下行消息转发

它和 `PlayerAnchor` 的关系应该是：

- `GatewaySession` 归属于某个 `PlayerAnchor`
- `PlayerAnchor` 记录当前连接在哪个 `gateway`、哪个 `session`

这样断线重连时，真正恢复的是：

- 玩家锚点状态

而不是让 `Gateway` 自己决定玩家一切归属。

### 3. `WorldHost`

`WorldHost` 负责：

- 地图实例
- 世界 tick
- world session
- 实体运行

`PlayerAnchor` 和它的关系应该是：

- `PlayerAnchor` 保存当前 world assignment
- `WorldHost` 接收进入/离开请求
- 世界里的表现实体由 world 创建
- world 内实体销毁不等于玩家锚点销毁

### 4. 数据层

`PlayerAnchor` 不应该等于数据库行。

但它应该是：

- 脏数据收口点
- 持久化调度入口

例如：

- 背包修改
- 角色属性修改
- 登录态更新时间

都先记到锚点，再由保存队列异步落库。

这里要单独强调：

- `PlayerAnchor` 是在线主状态对象
- `DBMgr / PersistenceService` 是持久化执行层
- 两者协作，但不是同一个概念

## 六、推荐对象关系

建议先形成下面这组对象：

```text
AnchorManager
    ├── PlayerAnchor
    ├── SessionLocator
    ├── WorldAssignment
    └── SaveCoordinator
```

### `AnchorManager`

职责：

- 创建锚点
- 恢复锚点
- 销毁锚点
- 防止同一玩家重复激活
- 提供按 `playerId` 查询

### `PlayerAnchor`

职责：

- 持有玩家在线主状态
- 记录 session/gateway/world 归属
- 记录当前生命周期状态
- 管理 dirty flag
- 协调进入 world、离开 world、断线、重连

### `SessionLocator`

职责：

- 根据 `playerId` 找当前 session
- 根据 `sessionId` 找当前玩家
- 给 gateway/world 提供统一定位服务

### `WorldAssignment`

职责：

- 记录玩家当前应该去哪个 world
- 记录 map/instance/space
- 管理切图过程中的旧归属与新归属切换

## 七、推荐生命周期

建议把 `PlayerAnchor` 的生命周期收成显式状态机。

至少应该有：

- `Loading`
- `Online`
- `Transferring`
- `Disconnected`
- `Saving`
- `Offline`

### 1. 登录激活

流程建议：

1. `LoginApp` 完成认证
2. `AnchorManager` 按 `playerId` 查找锚点
3. 如果不存在则创建，如果存在则恢复或挤掉旧连接
4. 锚点完成数据加载
5. 分配 `gateway`
6. 分配 `world`
7. 状态进入 `Online`

### 2. 进入世界

流程建议：

1. `PlayerAnchor` 持有目标 `WorldAssignment`
2. 向目标 `WorldHost` 发起 enter 请求
3. `WorldHost` 创建或恢复世界内表现实体
4. world 返回进入成功
5. 锚点记录当前 `worldId/mapId/instanceId`

### 3. 切图

流程建议：

1. `PlayerAnchor` 状态切到 `Transferring`
2. 旧 world 执行离开
3. 更新 `WorldAssignment`
4. 新 world 执行进入
5. 成功后切回 `Online`

### 4. 断线重连

流程建议：

1. `Gateway` 检测断开
2. 锚点状态切到 `Disconnected`
3. 短时间内不立即销毁 world 归属
4. 客户端重连后重新绑定新 `session`
5. 锚点恢复到 `Online`

### 5. 下线

流程建议：

1. 锚点通知 world 执行离开
2. 锚点触发保存
3. 清理 session/gateway/world 绑定
4. 状态切到 `Offline`

## 八、推荐接口草案

下面只是方向，不是最终代码。

```cpp
namespace apollo::game::session {

enum class AnchorState {
    Loading,
    Online,
    Transferring,
    Disconnected,
    Saving,
    Offline
};

struct SessionBinding {
    uint64_t session_id = 0;
    uint32_t gateway_id = 0;
    std::string gateway_addr;
    int64_t bind_time_ms = 0;
};

struct WorldAssignment {
    uint32_t world_id = 0;
    uint64_t map_id = 0;
    uint64_t instance_id = 0;
    uint64_t space_id = 0;
};

class PlayerAnchor {
public:
    uint64_t player_id() const noexcept;
    AnchorState state() const noexcept;

    void bind_session(const SessionBinding& binding);
    void unbind_session(uint64_t session_id);

    void assign_world(const WorldAssignment& assignment);
    const WorldAssignment& world_assignment() const noexcept;

    void mark_dirty(std::string_view reason);
    bool needs_save() const noexcept;
};

class AnchorManager {
public:
    std::shared_ptr<PlayerAnchor> activate(uint64_t player_id);
    std::shared_ptr<PlayerAnchor> find(uint64_t player_id) const;
    void deactivate(uint64_t player_id);
};

} // namespace apollo::game::session
```

## 九、推荐目录落点

建议新增：

- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`
- `modules/game/session/include/apollo/game/session/session_locator.hpp`
- `modules/game/session/include/apollo/game/session/world_assignment.hpp`
- `modules/game/session/src/player_anchor.cpp`
- `modules/game/session/src/anchor_manager.cpp`

### 为什么放 `modules/game/session`

因为这一层已经不是：

- `runtime` 的纯宿主语义

也不是：

- `world` 的场景运行语义

它属于典型的游戏在线会话域。

## 十、和当前 `base-app` 的改造关系

建议按两步走。

### 第一步：先补语义，不急着改 app 名字

短期内：

- 保留 `apps/base-app`
- 在内部新增 `AnchorManager`
- 把登录后的玩家归属、重连恢复、world 分配先收进去

这样做的好处是：

- 不打断当前目录结构
- 可以逐步从数据服务原型演进

### 第二步：再把数据服务降为锚点层附属能力

等 `PlayerAnchor` 站稳之后，再把 `base-app` 内部重构成：

- 锚点管理为主
- 数据加载/保存为配套服务

也就是说，未来 `DB` 能力是：

- `PlayerAnchor Host` 的支撑模块

而不是整个 `base-app` 的唯一主体。

## 十一、当前阶段不该做什么

为了控制复杂度，这一阶段先不要把 `PlayerAnchor` 写成万能对象。

### 不要直接塞业务系统细节

例如：

- 战斗逻辑
- 背包具体规则
- 任务系统具体流程

这些应该挂业务组件，不要全部塞进 anchor。

### 不要直接承载 world 实体全部状态

锚点记录的是长期归属和关键在线状态，不是整个 `AvatarEntity` 的镜像。

### 不要一开始就做跨区全局分布式

当前最优先的是：

- 单区服内的稳定玩家锚点

先把这层收住，再谈更大范围的全局目录。

## 十二、落地顺序建议

`PlayerAnchor` 最合适的落地顺序是：

1. 先完成 `WorldHost`
2. 再新增 `PlayerAnchor / AnchorManager`
3. 再让 `LoginApp` 登录后接入锚点激活
4. 再让 `Gateway` 接入 session 绑定
5. 再让 `WorldHost` 接入 world assignment

这样改动面是可控的。

如果反过来先在 `Login` 或 `Gateway` 里堆在线状态，后面还是要拆。

## 十三、结论

Apollo 当前最缺的不是更多进程名，而是玩家在线主状态的稳定宿主。

`PlayerAnchor` 的意义就在这里：

- 它把登录态、连接态、world 归属和持久化收口到一个稳定对象
- 它才是 Apollo 未来 `BaseApp` 语义的真正核心
- 它能把当前 login、gateway、world 三段割裂状态串成一条完整主链

只要这层不补，Apollo 的在线系统就会一直停留在“有登录、有网关、有场景，但没有真正玩家宿主”的阶段。

## 相关阅读

- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [WorldHost 设计稿](./world-host-design.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
