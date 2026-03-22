---
title: MMO 代码任务对照表
icon: diagram-project
order: 45
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - Code Mapping
  - Task
---

# MMO 代码任务对照表

这篇文档把架构任务进一步映射到当前仓库的真实目录。

目标不是重新定义模块，而是回答：

`现在要开始写代码，应该优先改哪些目录，每个目录承担哪一段主链语义。`

## 一、当前仓库的真实落点

当前仓库里与 MMO 主链直接相关的目录主要是：

### 模块目录

- `modules/base`
- `modules/core`
- `modules/runtime`
- `modules/game`
- `modules/net`
- `modules/protocol`
- `modules/data`
- `modules/starter`

### 应用目录

- `apps/login-app`
- `apps/gateway-app`
- `apps/base-app`
- `apps/cell-app`
- `apps/game-server`

## 二、推荐语义映射

### `modules/base`

职责：

- 时间、内存、线程池、对象池、基础工具

适合承接：

- 与 MMO 语义无关的纯基础能力

不适合承接：

- 登录票据
- 玩家锚点
- 世界运行时

### `modules/core`

职责：

- 配置、日志、生命周期、事件、定时器

适合承接：

- 所有应用共用的核心运行语义

### `modules/runtime`

职责：

- `ApplicationHost`
- `ProcessHost`
- `WorldHost`
- host bootstrap

适合承接：

- 应用装配壳
- 统一宿主生命周期

### `modules/game`

建议在语义上拆成三块理解：

- `modules/game/core`
- `modules/game/session`
- `modules/game/world`

其中：

- `session` 承接 `PlayerAnchor` 主链
- `world` 承接 `WorldHost / WorldSession / AOI / Movement`
- `core` 承接实体基础抽象和公共游戏语义

### `modules/net` / `modules/protocol`

职责：

- 客户端协议
- 内部 envelope
- 会话协议
- transport 接入边界

适合承接：

- `GatewayApp`
- `LoginApp`
- `BaseApp`
- `WorldApp / CellApp`

共用的协议抽象

### `modules/data`

当前仓库已有这个目录，因此短期内不一定要立即新建 `modules/platform`。

更合理的做法是：

- 先把 `modules/data` 作为持久化与数据访问过渡层
- 等语义稳定后再考虑升级到更清晰的 `platform/data` 分层

### `modules/starter`

职责：

- 应用装配、启动入口、模块注册

适合承接：

- topology 级装配
- feature flag / profile 注入

## 三、应用目录映射

### `apps/login-app`

当前应承接：

- `Authenticator`
- `LoginRiskController`
- `EntryAllocator`
- `LoginTicketIssuer`

近期应删除或降级的职责：

- 长期 session 权威
- 本地长期在线状态管理

### `apps/gateway-app`

当前应承接：

- `ClientIngressServer`
- `ConnectionRegistry`
- `SessionAdmission`
- `PacketDispatcher`
- `RouteExecutor`
- `ClientEgress`

不应承接：

- `PlayerAnchor`
- `WorldAssignment`
- 长期在线裁决

### `apps/base-app`

当前应承接：

- `PlayerAnchor`
- `AnchorManager`
- `SessionLocator`
- `ReconnectPolicy`
- `RuntimeTargetAssignment`
- `PersistenceCoordinator`

不应再承接：

- “数据库服务”语义

### `apps/cell-app`

当前仓库阶段更适合按下面方式理解：

- 短期：`world runtime` 原型
- 中期：分化为 `apps/world-app` 与真正 distributed `apps/cell-app`

因此近期有两种可执行路线：

1. 保留 `apps/cell-app` 目录名，但文档和代码注释按 `world runtime prototype` 理解
2. 新增 `apps/world-app`，让当前实现逐步迁移过去

### `apps/game-server`

这个目录更适合作为：

- 集成测试壳
- 单进程调试壳
- demo / sandbox host

不建议把最终 MMO 主链长期收口在这里。

## 四、Standard MMO 任务落点

### 1. 运行时基础

目录：

- `modules/base`
- `modules/core`
- `modules/runtime`

### 2. 协议边界

目录：

- `modules/net`
- `modules/protocol`

### 3. 在线主状态

目录：

- `modules/game/session`
- `apps/base-app`

### 4. 世界运行时

目录：

- `modules/game/world`
- `apps/cell-app` 或未来 `apps/world-app`

### 5. 边缘接入层

目录：

- `apps/gateway-app`

### 6. 登录入口

目录：

- `apps/login-app`

### 7. 持久化收口

目录：

- `modules/data`
- `apps/base-app`

## 五、Distributed World 任务落点

### 1. Proxy 化 BaseApp

目录：

- `modules/game/session`
- `apps/base-app`

### 2. Cell 侧空间权威

目录：

- `modules/game/world`
- `apps/cell-app`

### 3. Witness / Ghost

目录：

- `modules/game/world`

### 4. Authority Transfer

目录：

- `modules/game/world`

### 5. 控制面

目录：

- `modules/runtime`
- 未来独立 `apps/baseappmgr`
- 未来独立 `apps/cellappmgr`

## 六、推荐第一批代码任务

如果现在开始真正写代码，建议第一批只做下面 6 项：

1. `apps/login-app`
   - 把 `GatewayAllocator` 收口成 `EntryAllocator`

2. `apps/base-app`
   - 固定 `PlayerAnchor + RuntimeTargetAssignment` 主链

3. `apps/gateway-app`
   - 确保 admission 和 route 都经由 `BaseApp`

4. `modules/game/session`
   - 补稳定的锚点、定位、重连边界

5. `modules/game/world`
   - 稳定 `WorldHost / WorldSession / AOI / Movement`

6. `apps/cell-app`
   - 明确当前是 `world runtime prototype`，避免继续假装它已经是最终 `CellApp`

## 七、哪些目录现在不要大改

- `modules/bigworld`
  - 兼容层不应先于主链落地

- `apps/game-server`
  - 先作为调试壳，不要让它吞掉主链语义

- 控制面类目录
  - 如 `appmgr` 一类能力应后置

## 八、结论

Apollo 现在最重要的不是再发明新目录，而是先把现有目录按正确语义使用起来。

最关键的 5 个真实落点是：

- `apps/login-app`
- `apps/gateway-app`
- `apps/base-app`
- `modules/game/session`
- `modules/game/world`

以及一个必须做出的近期决定：

- 当前 `apps/cell-app` 到底继续临时承接 `world runtime`
- 还是开始新增真正的 `apps/world-app`

这个决定会直接影响后续普通 MMO 与分布式世界的代码分化成本。

## 相关阅读

- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [Standard MMO 任务清单](./standard-mmo-task-checklist.md)
- [Distributed World 任务清单](./distributed-world-task-checklist.md)
- [进程语义重定义](./process-semantics-redefinition.md)
