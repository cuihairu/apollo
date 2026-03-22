---
title: Domain Event 与 Message Bus 设计
icon: arrows-turn-to-dots
order: 22
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - DomainEvent
  - MessageBus
  - Integration
---

# Domain Event 与 Message Bus 设计

这篇文档解决的是 Apollo 整体框架继续往业务协作层推进时，一个必须尽早收住的问题：

`不同业务域之间，到底应该如何协作。`

如果这层不清楚，后面很容易出现：

- world 直接调 social
- task 直接依赖 battle 内部对象
- activity 直接改 session 状态
- platform 回调直接深入世界宿主

最终就会把前面辛苦建立的分层和模块边界重新打穿。

## 一、设计目标

这层设计要解决 6 个问题：

1. 业务域之间如何解耦协作。
2. 哪些场景适合事件，哪些适合命令，哪些适合 request/reply。
3. 同进程和跨进程消息边界如何统一。
4. 如何避免业务域之间直接持有重依赖。
5. 如何支持审计、回放、观测和故障排查。
6. 如何和 `RemoteEntityCall`、`Platform Foundation`、`Bootstrap` 对齐。

## 二、参考来源

### 1. 参考领域事件思路

参考点：

- 领域状态变化以事件表达
- 不同业务域通过事件解耦

### 2. 参考消息总线思路

参考点：

- 命令、事件、查询分流
- 同进程和跨进程消息有统一抽象

### 3. 参考 KBE 的经验，但不照搬

参考点：

- 跨宿主、跨进程协作必须显式
- 不同组件之间不能靠隐式共享状态

不照搬点：

- 不把所有跨模块协作都收敛成重对象远程调用

## 三、为什么这样设计

Apollo 不是单一 world 逻辑服，而是：

- session
- world
- battle
- social
- task
- activity
- platform

共同组成的框架。

这些域之间既需要协作，又不能彼此强耦合。

更合理的方式应该是：

- 对象级权威协作走 `RemoteEntityCall`
- 业务域级解耦协作走 `Domain Event`
- 明确请求结果的操作走 `Command / RequestReply`

也就是说：

- 不能只有 RPC
- 也不能只有事件

## 四、优点

- 降低业务域强耦合
- 更利于扩展任务、活动、社交、排行等旁路能力
- 更利于观测、审计和重放
- 更适合 mono-repo 多模块架构

## 五、代价与风险

- 事件过多会带来追踪复杂度
- 如果边界不清，容易出现“万物皆事件”
- 引入消息总线会增加一层抽象成本

## 六、为什么不选其他方案

### 不选“业务域之间直接互调内部对象”

因为这会迅速打穿分层。

### 不选“所有东西都用远程调用”

因为很多业务协作本质是解耦通知，不是点对点同步调用。

### 不选“所有东西都用事件”

因为有些流程必须有明确响应和失败边界。

Apollo 更合理的路线是：

- 命令、事件、查询分流

## 七、推荐消息分类

建议 Apollo 至少统一成 4 类消息。

### 1. `DomainCommand`

用于：

- 明确要求某个域执行动作

例如：

- `ActivatePlayerAnchor`
- `CreateWorldSession`
- `GrantActivityReward`

### 2. `DomainEvent`

用于：

- 某个域宣布“某件事已经发生”

例如：

- `PlayerLoggedIn`
- `PlayerEnteredWorld`
- `BattleFinished`
- `TaskProgressChanged`

### 3. `DomainQuery`

用于：

- 读取某个域的视图或状态

例如：

- `GetLeaderboardPage`
- `QueryPlayerSocialProfile`

### 4. `IntegrationEvent`

用于：

- 跨 app、跨边界、跨子系统传播的重要事件

例如：

- `PaymentOrderConfirmed`
- `SeasonRolled`
- `WorldNodeOverloaded`

## 八、推荐对象模型

```text
MessageBus
    ├── CommandBus
    ├── EventBus
    ├── QueryDispatcher
    ├── HandlerRegistry
    ├── MiddlewareChain
    └── DeliveryAdapter
```

### `CommandBus`

职责：

- 分发命令
- 返回明确处理结果

### `EventBus`

职责：

- 发布领域事件
- 支持 1:N 订阅

### `QueryDispatcher`

职责：

- 读模型查询
- 不承接状态变更

### `HandlerRegistry`

职责：

- 注册 handler
- 绑定消息类型和模块

### `MiddlewareChain`

职责：

- tracing
- metrics
- retry
- idempotency
- auth / permission

### `DeliveryAdapter`

职责：

- 同进程分发
- 跨进程投递
- 与 internal envelope 对接

## 九、推荐边界规则

### 1. 对象级权威操作优先走 `RemoteEntityCall`

例如：

- `PlayerAnchor -> AvatarEntity`
- `AvatarEntity -> Proxy`

### 2. 域级解耦通知优先走 `DomainEvent`

例如：

- 玩家升级后，任务、活动、成就系统都可监听

### 3. 需要强结果语义的流程走 `Command`

例如：

- 发奖
- 创建房间
- 创建世界会话

### 4. 查询不应偷偷改状态

查询应该保持：

- 读模型语义

## 十、推荐典型流程

### 登录完成流程

1. `Login` 发出 `ActivatePlayerAnchor` 命令
2. `Session` 域成功后发布 `PlayerLoggedIn`
3. `Social`、`Activity`、`Task` 等域订阅事件做旁路处理

### 世界进入流程

1. `Session` 域发 `CreateWorldSession`
2. `World` 域完成后发布 `PlayerEnteredWorld`
3. `Task`、`Activity` 域监听进入世界事件

### 支付回调流程

1. `Platform` 域收到支付确认
2. 发布 `PaymentOrderConfirmed`
3. `Mail` 或 `Reward` 域处理发货

## 十一、同进程与跨进程统一策略

Apollo 不应该维护两套完全不同的消息模型。

更合理的方式是：

- 域内先走统一消息抽象
- 由 `DeliveryAdapter` 决定：
  - 本地直投
  - 跨进程 envelope 投递

这样可以保证：

- 业务 handler 不必关心底层在哪个 app

## 十二、和 Platform Foundation 的关系

`MessageBus` 本身不负责持久化业务结果。

但它可以配合平台层实现：

- outbox
- idempotency
- delayed retry

也就是说：

- 事件协作在消息层
- 可靠性增强在平台层

## 十三、和测试策略的关系

这层应至少对应下面几类测试：

- command handler unit test
- event bus contract test
- assembly test
- scenario test

否则消息总线很容易变成“看起来很优雅，但没人知道是否真工作”。

## 十四、对当前 Apollo 的直接含义

Apollo 当前已经在讨论：

- `RemoteEntityCall`
- `Platform Foundation`
- `Starter / Bootstrap`

下一步更合理的是：

1. 先定义 `Command / Event / Query` 三类消息抽象
2. 先在 session/world/platform 之间试点
3. 再逐步扩到 task/activity/social

## 十五、结论

Apollo 后续如果要把业务域协作真正做稳，必须明确：

- 什么走对象远程调用
- 什么走命令
- 什么走事件
- 什么走查询

只有这层清楚，模块化和分层设计才不会在业务增长后失效。

## 相关阅读

- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [Testing 与 Verification 策略](./testing-and-verification-strategy.md)
