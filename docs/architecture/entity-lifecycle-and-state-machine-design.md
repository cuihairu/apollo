---
title: Entity Lifecycle 与 State Machine 设计
icon: shuffle
order: 24
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Entity
  - Lifecycle
  - StateMachine
---

# Entity Lifecycle 与 State Machine 设计

这篇文档解决的是 Apollo 整体框架继续往世界运行时推进时，一个必须先定下来的问题：

`实体从创建、上线、进入世界、迁移、离线到销毁，这条生命周期到底怎么统一。`

如果这层不清楚，后面会很容易出现：

- session、anchor、avatar 生命周期打架
- world 内实体状态和在线状态不同步
- 迁移、切图、重连时状态混乱
- BigWorld 增强层接进来后找不到统一状态边界

## 一、设计目标

这层设计要解决 6 个问题：

1. `Proxy / PlayerAnchor / AvatarEntity` 的生命周期边界如何统一。
2. 实体状态如何显式表达，而不是散在 bool 字段里。
3. 普通 MMO 和 BigWorld 模式下，哪些状态共用，哪些状态扩展。
4. 生命周期事件如何和 `Domain Event`、`RemoteEntityCall`、`Persistence` 协同。
5. 热路径状态变化如何保持简单。
6. 如何避免照搬 KBE 的重对象状态机，也避免退化成“没有状态机”。

## 二、参考来源

### 1. 参考 KBE 的对象生命周期经验

参考点：

- `Proxy / Base / Cell` 生命周期分层
- enter world / leave world / migration / witness / ghost 等显式边界

不照搬点：

- 不把所有状态和流程都塞进重实体对象内部

### 2. 参考状态机设计思路

参考点：

- 状态显式化
- 状态转换有约束
- 失败回滚边界清楚

### 3. 参考 ECS 兼容原则

参考点：

- 生命周期管理仍然可以是对象语义
- 但不要让局部高频系统被重状态机拖垮

## 三、为什么这样设计

Apollo 当前已经明确：

- `Proxy`
- `PlayerAnchor`
- `AvatarEntity`

不是同一个对象。

既然对象分层已经存在，生命周期就必须随之分层。

更合理的方式不是：

- 一个全局大状态机统治全部对象

而是：

- 每层对象有自己的主状态
- 对象间通过少量关键边界状态协同

## 四、优点

- 生命周期边界清楚
- 重连、切图、迁移更容易做对
- 更适合普通 MMO 和 BigWorld 共存
- 更适合和持久化、事件、调度协同

## 五、代价与风险

- 状态显式化后，模型数量会增加
- 需要维护转换规则
- 如果状态切得过细，会导致实现臃肿

## 六、为什么不选其他方案

### 不选“完全没有状态机”

因为生命周期复杂后，隐式状态一定会失控。

### 不选“单一全局 Player 状态机”

因为 `Proxy / Anchor / Avatar` 生命周期根本不同。

### 不选“照搬 KBE 全量状态模型”

因为 Apollo 还要兼容更轻的普通在线与普通 MMO 模式。

更合理的路线是：

- 分层对象
- 分层状态
- 关键边界对齐

## 七、推荐对象与状态分层

Apollo 建议至少拆成 3 层主对象状态。

### 1. `ProxyState`

语义：

- 连接和客户端附着状态

建议状态：

```text
Detached
Binding
Bound
Replacing
Disconnected
Closed
```

### 2. `AnchorState`

语义：

- 玩家长期在线归属状态

建议状态：

```text
Idle
Activating
Online
Transferring
Disconnected
Recovering
Closing
Closed
```

### 3. `AvatarState`

语义：

- 玩家在 world / cell 中的表现状态

建议状态：

```text
Created
EnteringWorld
Active
LeavingWorld
Migrating
Ghosting
Dormant
Destroyed
```

## 八、推荐关键状态转换

### `Proxy`

1. `Detached -> Binding`
2. `Binding -> Bound`
3. `Bound -> Replacing`
4. `Bound -> Disconnected`
5. `Disconnected -> Closed`

### `PlayerAnchor`

1. `Idle -> Activating`
2. `Activating -> Online`
3. `Online -> Transferring`
4. `Online -> Disconnected`
5. `Disconnected -> Recovering`
6. `Recovering -> Online`
7. `Online -> Closing`
8. `Closing -> Closed`

### `AvatarEntity`

1. `Created -> EnteringWorld`
2. `EnteringWorld -> Active`
3. `Active -> LeavingWorld`
4. `Active -> Migrating`
5. `Migrating -> Active`
6. `Active -> Ghosting`
7. `LeavingWorld -> Destroyed`

## 九、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

重点状态：

- `Online`
- `Transferring`
- `EnteringWorld`
- `LeavingWorld`

一般不需要：

- `Ghosting`
- `Migrating` 的跨 partition 复杂状态

### BigWorld 模式

需要扩展：

- `Migrating`
- `Ghosting`
- authority 迁移窗口
- witness 相关状态

### 设计结论

Apollo 应该让普通 MMO 先只实现主链状态，

然后在 BigWorld 模式下扩展额外状态，而不是一开始全量打开所有状态。

## 十、推荐生命周期事件

为了和 `Domain Event` 对齐，建议生命周期关键边界统一发事件。

例如：

- `ProxyBound`
- `PlayerAnchorActivated`
- `PlayerEnteredWorld`
- `PlayerTransferStarted`
- `PlayerTransferCompleted`
- `PlayerDisconnected`
- `AvatarDestroyed`

### 为什么要发事件

因为很多旁路系统只需要知道：

- 某件事已经发生

并不需要直接进入对象内部。

## 十一、和 `RemoteEntityCall` 的关系

状态机不应直接变成远程调用本身，但很多状态切换需要借助远程调用完成。

例如：

- `Anchor -> Avatar` 创建世界会话
- `World -> Anchor` 回报进入完成
- `Anchor -> Proxy` 重绑连接

设计原则：

- 状态机负责状态边界
- `RemoteEntityCall` 负责跨宿主执行动作

## 十二、和 `Persistence` 的关系

并不是所有状态都需要持久化。

建议区分：

### 需要持久化的

- `Anchor` 长期状态
- world assignment
- reconnect 窗口关键信息

### 不需要长期持久化的

- 大多数 `ProxyState`
- world 实时临时状态
- 大部分 `Avatar` 的热路径运行态

## 十三、和 `World Tick / Scheduling` 的关系

状态机不应在任何线程、任何回调里随意切换。

更合理的方式是：

- 生命周期动作进入统一调度点
- 在宿主 tick 或任务执行边界完成状态推进

这样更容易保证：

- 顺序
- 回滚
- 诊断

## 十四、推荐对象模型

```text
LifecycleCoordinator
    ├── ProxyStateMachine
    ├── AnchorStateMachine
    ├── AvatarStateMachine
    ├── TransitionGuard
    └── LifecycleEventEmitter
```

### `TransitionGuard`

职责：

- 校验状态转换是否合法
- 校验前置条件

### `LifecycleEventEmitter`

职责：

- 在关键状态边界发事件

## 十五、对当前 Apollo 的直接含义

Apollo 下一步如果继续往代码实现推进，建议优先补：

- `PlayerAnchor` 的显式状态机
- `WorldSession / AvatarEntity` 的进入和离开状态
- `GatewaySession / Proxy` 的绑定和替换状态

而不是继续靠：

- 零散标志位
- 多处 if/else

## 十六、结论

Apollo 的对象模型如果已经明确分层，那么生命周期也必须分层。

更合理的方式是：

- `Proxy / Anchor / Avatar` 各自有主状态机
- 普通 MMO 先实现主链状态
- BigWorld 再扩展迁移、ghost、authority 相关状态

这样才能让在线主链和分布式世界主链都真正稳定下来。

## 相关阅读

- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [玩家在线主链设计](./player-online-flow.md)
- [Domain Event 与 Message Bus 设计](./domain-event-and-message-bus-design.md)
