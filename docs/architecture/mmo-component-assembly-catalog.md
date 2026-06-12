---
title: MMO 组件装配目录
icon: puzzle-piece
order: 41
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - Component
  - Assembly
  - Catalog
---

# MMO 组件装配目录

这篇文档把 Apollo 的 Profile 收敛到组件装配层。

它不重新讲理想架构，而是回答：

`Tower Defense Compact、Standard MMO、Distributed MMO 分别应该装哪些组件，这些组件落在哪些模块或 App，哪些组件当前必须后置。`

## 一、阅读方式

这份目录按四类状态标记组件：

| 状态 | 含义 |
|------|------|
| `必选` | 该 Profile 的最小闭环必须启用 |
| `可选` | 按项目需要启用，不影响 Profile 基线 |
| `后置` | 当前不应先做，等主链稳定后再做 |
| `关闭` | 该 Profile 默认不启用，避免复杂度污染 |

组件不是进程。

同一个组件可以装进不同进程，但它的状态权威归属必须保持一致。

## 二、总装配矩阵

| 组件 | 推荐落点 | Compact | Standard MMO | Distributed MMO | 边界说明 |
|------|----------|---------|--------------|-----------------|----------|
| `ApplicationHost` | `modules/runtime` | 必选 | 必选 | 必选 | 所有 App 统一宿主 |
| `Runtime Ops` | `modules/runtime/ops` | 必选 | 必选 | 必选 | console、watcher、diagnostics |
| `Protocol Envelope` | `modules/net` / `modules/protocol` | 必选 | 必选 | 必选 | 客户端协议和内部信封分离 |
| `LoginApp` | `apps/login-app` | 可选 | 必选 | 必选 | 认证、风控、票据，不持有长期在线权威 |
| `GatewayApp` | `apps/gateway-app` | 必选 | 必选 | 可选 | 默认公网接入；Proxy-hosted 模式可弱化 |
| `CompactGameServer` | 未来 `apps/compact-game-app` 或 `apps/game-server` | 必选 | 关闭 | 关闭 | 固定地图、塔防、局内玩法 |
| `SceneInstance` | `modules/game/world` 或 compact domain | 必选 | 可选 | 可选 | Compact 的主运行单元 |
| `WaveRuntime` | `modules/game` domain | 必选 | 可选 | 可选 | 塔防波次驱动，不能变成 L9 能力 |
| `BattleRuntime` | `modules/game/battle` | 必选 | 必选 | 必选 | 规则结算，热路径不强制走远程调用 |
| `PlayerAnchor` | `modules/game/session` | 可选 | 必选 | 必选 | 玩家长期在线主状态 |
| `AnchorManager` | `modules/game/session` | 可选 | 必选 | 必选 | 创建、替换、销毁锚点 |
| `SessionLocator` | `modules/game/session` | 可选 | 必选 | 必选 | session/player 归属定位 |
| `WorldAssignment` | `modules/game/session` | 可选 | 必选 | 必选 | 当前 world / instance / zone 归属 |
| `WorldHost` | `modules/runtime` + `modules/game/world` | 关闭 | 必选 | 必选 | 普通 MMO 世界宿主 |
| `MapInstance` | `modules/game/world` | 可选 | 必选 | 必选 | 地图和副本运行实例 |
| `WorldSession` | `modules/game/world` | 可选 | 必选 | 必选 | 玩家在世界内的运行上下文 |
| `AOI / Interest` | `modules/game/world` | 必选 | 必选 | 必选 | Compact 内嵌，Standard 本地，Distributed 跨 partition |
| `ReplicationPipeline` | `modules/game/core` / `modules/game/world` | 必选 | 必选 | 必选 | 复制入口必须统一 |
| `Repository` | `modules/data` 或未来 `modules/platform` | 必选 | 必选 | 必选 | 业务不直接依赖裸 SQL/Redis |
| `PersistenceService` | `apps/base-app` 过渡或未来 `persistence-app` | 必选 | 必选 | 必选 | 持久化执行层，不是在线主状态 |
| `ScriptRuntime` | 未来 `modules/script` | 可选 | 可选 | 可选 | 承接业务钩子，不承接高频热路径 |
| `MatchCoordinator` | 未来 match domain | 可选 | 可选 | 可选 | 对局编排，不替代 MMO world 主链 |
| `Proxy` | `BaseApp` 增强组件 | 关闭 | 后置 | 必选 | Distributed 模式客户端代理能力 |
| `Witness` | L9 distributed extension | 关闭 | 后置 | 必选 | 客户端可见世界上下文 |
| `GhostReplica` | L9 distributed extension | 关闭 | 后置 | 必选 | 非权威实体投影 |
| `AuthorityTransfer` | L9 distributed extension | 关闭 | 后置 | 必选 | 跨 partition 权威迁移 |
| `SpacePartition` | L9 distributed extension | 关闭 | 后置 | 必选 | 连续大世界空间切片 |
| `AppMgr` | 控制面增强 | 关闭 | 后置 | 必选 | 负载、拓扑、迁移控制 |

## 三、Tower Defense Compact 装配包

### 必选组件

- `ApplicationHost`
- `Runtime Ops`
- `Protocol Envelope`
- `GatewayApp`
- `CompactGameServer`
- `SceneInstance`
- `WaveRuntime`
- `BattleRuntime`
- `AOI / Visibility`
- `SceneSnapshot`
- `Repository`
- `PersistenceService`

### 可选组件

- `LoginApp`
- 轻量 `PlayerAnchor`
- `MatchCoordinator`
- `ScriptRuntime`
- `Chat / Social`
- `Leaderboard`

### 默认关闭

- `Proxy`
- `Witness`
- `GhostReplica`
- `AuthorityTransfer`
- `SpacePartition`
- `CellAppMgr`

### 最小验收链路

```text
Client
  -> GatewayApp
  -> CompactGameServer
  -> enter SceneInstance
  -> spawn wave
  -> build tower / cast skill
  -> battle tick
  -> state replication
  -> snapshot / reconnect
  -> persistence
```

验收重点：

- 每个 `SceneInstance` 有独立 tick 和快照。
- 波次、建造、技能、怪物路径在同一局内闭环。
- `GatewayApp` 不保存完整局内权威。
- 需要断线恢复时，恢复入口由 `SceneSnapshot` 和轻量在线状态共同完成。

## 四、Standard MMO 装配包

### 必选组件

- `ApplicationHost`
- `Runtime Ops`
- `Protocol Envelope`
- `LoginApp`
- `GatewayApp`
- `PlayerAnchor`
- `AnchorManager`
- `SessionLocator`
- `WorldAssignment`
- `WorldHost`
- `MapInstance`
- `WorldSession`
- `AOI / Interest`
- `ReplicationPipeline`
- `Repository`
- `PersistenceService`

### 可选组件

- `BattleRuntime`
- `ScriptRuntime`
- `Chat / Social`
- `Task / Activity`
- `MatchCoordinator`
- `Leaderboard`

### 默认关闭

- `Proxy`
- `Witness`
- `GhostReplica`
- `AuthorityTransfer`
- `SpacePartition`
- `CellAppMgr`

### 最小验收链路

```text
Client
  -> LoginApp
  -> GatewayApp
  -> BaseApp(PlayerAnchor)
  -> WorldApp(WorldHost)
  -> enter MapInstance
  -> AOI / movement / replication
  -> reconnect
  -> persistence
```

验收重点：

- `BaseApp` 是唯一在线主状态中心。
- `GatewayApp` 只持有连接态和短期路由快照。
- `WorldApp` 只持有世界运行态，不持有玩家长期归属真相。
- `PersistenceService` 是支撑层，不反向成为 `BaseApp` 的语义中心。

## 五、Distributed MMO 装配包

### 前置条件

必须先满足 `Standard MMO` 的最小验收链路。

如果下面能力还不稳定，不进入 Distributed MMO：

- 登录票据
- 玩家锚点
- 世界进入
- 切图 / 切实例
- 断线重连
- 持久化边界
- 复制管线
- 运行时观测

### 追加组件

- `Proxy`
- `ClientAttachment`
- `Witness`
- `GhostReplica`
- `AuthorityTransfer`
- `SpacePartition`
- `CellRuntime`
- `AppMgr`
- `TopologyRegistry`
- `MigrationControl`

### 最小验收链路

```text
Client
  -> LoginApp
  -> GatewayApp or BaseApp Proxy
  -> BaseApp(PlayerAnchor + optional Proxy)
  -> CellApp / Partition
  -> Witness visible set
  -> Ghost replication
  -> authority transfer
  -> persistence
```

验收重点：

- 客户端附着点和空间权威分离。
- real entity 和 ghost replica 关系明确。
- authority transfer 有 prepare、commit、rollback。
- `AppMgr` 是控制面，不承接业务主状态。

## 六、当前代码落点

| 能力 | 当前推荐目录 | 近期动作 |
|------|--------------|----------|
| 应用宿主 | `modules/runtime` | 继续收口 `ApplicationHost / WorldHost` |
| 运行时治理 | `modules/runtime/ops` | 补 watcher、console、diagnostics |
| 在线主状态 | `modules/game/session` | 稳定 `PlayerAnchor / AnchorManager / SessionLocator` |
| 世界运行时 | `modules/game/world` | 稳定 `MapInstance / WorldSession / AOI` |
| 登录入口 | `apps/login-app` | 收口为认证、风控、票据、入口分配 |
| 公网接入 | `apps/gateway-app` | 收口为连接、admission、dispatch、egress |
| 玩家锚点宿主 | `apps/base-app` | 从数据服务原型转向 `PlayerAnchor Host` |
| 世界宿主壳 | `apps/cell-app` | 过渡期按 `WorldHost` 装配壳推进 |
| 持久化 | `modules/data` / 未来 `persistence-app` | 先抽象 Repository，再决定是否独立进程 |
| Compact 壳 | `apps/game-server` 或未来 `apps/compact-game-app` | 可作为塔防/固定地图最小闭环候选 |
| 大世界增强 | `modules/bigworld` / 未来 L9 目录 | 后置，不污染 Standard MMO |

## 七、装配优先级

### R0：公共运行底座

先做：

- `ApplicationHost`
- 配置和 profile
- 日志初始化
- shutdown hook
- watcher / diagnostics
- protocol envelope

### S1：Standard MMO 主链

先做：

- `LoginApp`
- `GatewayApp`
- `PlayerAnchor`
- `WorldAssignment`
- `WorldHost`
- `WorldSession`
- `Repository`

### C1：Compact 最小闭环

如果项目目标是塔防或固定地图，可以与 S1 并行推进：

- `CompactGameServer`
- `SceneInstance`
- `WaveRuntime`
- `BattleRuntime`
- `SceneSnapshot`

### D1：Distributed MMO 增强

只在 S1 稳定后做：

- `Proxy`
- `Witness`
- `GhostReplica`
- `AuthorityTransfer`
- `SpacePartition`
- `AppMgr`

## 八、验收矩阵

| Profile | 第一验收目标 | 不通过的典型表现 |
|---------|--------------|------------------|
| Compact | 玩家进入固定场景并完成一轮波次同步 | 场景状态只能存在 gateway；断线恢复找不到局内快照 |
| Standard MMO | 玩家登录后经 Gateway 进入 World，并可重连恢复 | BaseApp 仍只是 DB 服务；WorldApp 持有长期玩家归属 |
| Distributed MMO | 玩家跨 partition 迁移不丢权威和可见集 | Ghost / Witness 与真实实体关系不清；AppMgr 侵入业务状态 |

## 九、设计边界

组件装配必须遵守下面边界：

- `PlayerAnchor` 不依赖具体 `WorldApp` 实现。
- `WorldSession` 不等于公网连接。
- `Repository` 不知道玩家在线路由。
- `GatewayApp` 不直接修改玩家长期归属。
- `CompactGameServer` 可以复用 `AOI / Battle / Replication`，但不启用 L9。
- `Distributed MMO` 可以替换客户端接入形态，但不能绕过在线主状态。

## 十、结论

Apollo 后续不应再按“模块名补齐”推进，而应按 Profile 装配包推进。

近期最重要的两件事是：

- 把 `Standard MMO` 的 `Login -> Gateway -> BaseApp(PlayerAnchor) -> WorldHost` 主链做稳。
- 把 `Tower Defense Compact` 的 `Gateway -> CompactGameServer -> SceneInstance` 最小闭环做清楚。

`Witness / Ghost / AuthorityTransfer / AppMgr` 全部后置，避免在基础在线主链未稳定前引入分布式世界复杂度。

## 相关阅读

- [MMO Topology 范围与组合设计](./mmo-topology-scope-and-composition-design.md)
- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [Standard MMO 任务清单](./standard-mmo-task-checklist.md)
- [Distributed World 任务清单](./distributed-world-task-checklist.md)
- [MMO 代码任务对照表](./mmo-code-task-mapping.md)
- [Compact GameServer 设计方案](/30-Compact_GameServer_Design.md)
