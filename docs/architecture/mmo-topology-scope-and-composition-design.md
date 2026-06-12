---
title: MMO Topology 范围与组合设计
icon: layer-group
order: 37
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - Topology
  - Profile
  - Composition
---

# MMO Topology 范围与组合设计

这篇文档是 Profile、进程语义和实施清单之间的收敛层。

它回答一个落地问题：

`一个项目到底应该启用哪些进程、组件和能力，哪些能力必须关闭，什么时候才允许升级到更复杂的 topology。`

如果只看进程名，很容易继续混淆：

- `Gateway` 到底是公网接入，还是在线主状态中心
- `BaseApp` 到底是玩家锚点，还是数据库服务
- `CellApp` 到底是普通世界服，还是分布式空间权威节点
- `CompactGameServer` 到底是 demo，还是固定地图玩法的一等 Profile

这篇文档把这些组合边界固定下来。

## 一、收敛结论

Apollo 近期只保留三条主 Profile：

| Profile | 目标形态 | 默认状态 |
|---------|----------|----------|
| `Tower Defense Compact` | 塔防、固定地图、合作防守、轻量局内玩法 | 当前优先 |
| `Standard MMO` / `Lightweight MMO` | 分线、地图实例、副本、轻社交、常规运营 | 当前优先 |
| `Distributed MMO` / `BigWorld Extension` | 连续大世界、空间切片、跨节点权威迁移 | 后续增强 |

关键原则：

- `Tower Defense Compact` 不是临时 demo，而是固定地图玩法的一等装配方式。
- `Standard MMO` 是 Apollo 的第一条完整在线主链。
- `Distributed MMO` 必须建立在 `Standard MMO` 稳定之后。
- 不通过“进程名”判断架构是否正确，而通过“状态权威归属”判断。

## 二、五个概念边界

### 1. `Profile`

Profile 是面向项目类型的推荐装配。

例如：

- 塔防项目选 `Tower Defense Compact`
- 分线 MMO 选 `Standard MMO`
- 连续大世界项目在后期叠加 `Distributed MMO`

### 2. `Topology`

Topology 是进程和消息流组合。

它回答：

- 客户端连谁
- 登录票据由谁发
- 在线主状态由谁持有
- 世界运行态由谁持有
- 持久化由谁执行

### 3. `App`

App 是最终可执行进程。

例如：

- `login-app`
- `gateway-app`
- `base-app`
- `world-app` 或当前过渡期的 `cell-app`
- `compact-game-app`
- `persistence-app`

### 4. `Component`

Component 是可装配能力，不一定等于一个进程。

例如：

- `PlayerAnchor`
- `WorldHost`
- `SceneInstance`
- `WaveRuntime`
- `Repository`
- `Witness`

### 5. `Capability`

Capability 是更细粒度的能力开关。

例如：

- `client_ingress`
- `session_admission`
- `world_assignment`
- `scene_snapshot`
- `authority_transfer`

同一个能力可以装进不同 App，但状态权威不能因此变得模糊。

## 三、Profile 选择表

| 项目特征 | 推荐 Profile | 主运行单元 | 默认启用 | 默认关闭 |
|----------|--------------|------------|----------|----------|
| 固定地图、波次、防守、建造、单局状态 | `Tower Defense Compact` | `SceneInstance` | `Gateway`、`CompactGameServer`、`WaveRuntime`、`BattleRuntime`、`Persistence` | `Witness`、`Ghost`、`AuthorityTransfer`、`CellAppMgr` |
| 分线世界、地图切换、副本、轻社交 | `Standard MMO` | `WorldHost / MapInstance` | `LoginApp`、`GatewayApp`、`BaseApp(PlayerAnchor)`、`WorldApp`、`Persistence` | `Proxy`、`Partition`、`Witness`、`Ghost` |
| 房间或匹配局为主 | `Match / Room` 组合 | `MatchInstance` | `Coordinator`、`Gateway`、`MatchServer`、`Persistence` | 完整 `WorldHost`、`CellApp`、`AuthorityTransfer` |
| 连续大世界、热点区域空间切片 | `Distributed MMO` | `Cell / Partition` | `BaseApp`、`CellApp`、`Witness`、`Ghost`、`AuthorityTransfer`、`AppMgr` | `CompactGameServer` 作为主世界 |

说明：

- `Match / Room` 不是当前文档主线，但 Apollo 的术语和组件需要给它留口。
- `Distributed MMO` 不是从零独立启动的一套架构，而是在 `Standard MMO` 上叠加 L9 能力。

## 四、三条推荐主链

### 1. Tower Defense Compact

```text
Client
  -> GatewayApp
  -> CompactGameServer
       -> SceneInstance
       -> WaveRuntime
       -> BattleRuntime
       -> AOI / Visibility
       -> SceneSnapshot
  -> PersistenceService
```

适合：

- 固定地图
- 多人合作防守
- 小规模局内实时同步
- 对运维成本敏感的项目

注意：

- `LoginApp` 可以作为账号入口存在，但不是局内实时主链的必选中心。
- 如果有长期成长、跨局恢复和复杂重连，可以引入轻量 `PlayerAnchor`，但不能让 `GatewayApp` 承接长期在线权威。

### 2. Standard MMO

```text
Client
  -> LoginApp
  -> GatewayApp
  -> BaseApp(PlayerAnchor Host)
  -> WorldApp(WorldHost)
  -> PersistenceService
```

适合：

- 分线 MMO
- 固定地图 MMO
- 副本型 RPG
- 有主城、野外、活动地图的轻量 MMO

注意：

- `BaseApp` 是玩家在线主状态中心，不是 `DBMgr`。
- `WorldApp` 是普通世界运行时，不等同于 BigWorld 完整 `CellApp`。
- `GatewayApp` 只保存连接态和短期路由快照。

### 3. Distributed MMO

```text
Client
  -> LoginApp
  -> GatewayApp or BaseApp Proxy
  -> BaseApp(PlayerAnchor + optional Proxy)
  -> CellApp / Space Partition
  -> AppMgr
  -> PersistenceService
```

适合：

- 连续大世界
- 大地图热点切片
- 跨节点实体权威迁移
- 客户端视野需要跨 partition 连续

注意：

- 是否保留独立 `GatewayApp` 是装配选择，不是是否 BigWorld 的判断标准。
- 真正的判断标准是是否启用了 `Partition / Witness / Ghost / AuthorityTransfer`。

## 五、状态权威分配

| 状态类型 | Compact | Standard MMO | Distributed MMO |
|----------|---------|--------------|-----------------|
| 公网连接态 | `GatewayApp` | `GatewayApp` | `GatewayApp` 或 `BaseApp Proxy` |
| 登录票据 | `LoginApp` 可选 | `LoginApp` | `LoginApp` |
| 玩家长期在线主状态 | 轻量 `PlayerAnchor` 可选 | `BaseApp(PlayerAnchor)` | `BaseApp(PlayerAnchor)` |
| 局内 / 世界运行态 | `CompactGameServer / SceneInstance` | `WorldApp / MapInstance` | `CellApp / Partition` |
| 拓扑归属 | `SceneAssignment` 或轻量 `PlayerAnchor` | `WorldAssignment` | `WorldAssignment + AppMgr` |
| 持久化执行 | `PersistenceService` | `PersistenceService` | `PersistenceService` |
| 分布式空间控制面 | 关闭 | 关闭 | `AppMgr` |

硬约束：

- `GatewayApp` 不能成为玩家长期在线主状态中心。
- `WorldApp` 不能成为玩家跨图归属权威。
- `BaseApp` 不能退化成数据库服务名字壳。
- `AppMgr` 不能先于基础 world 主链成为必选组件。

## 六、启用与关闭规则

### 1. 没有连续大世界，就关闭 L9

只要项目不需要连续空间切片，就默认关闭：

- `Witness`
- `Ghost`
- `AuthorityTransfer`
- `SpacePartition`
- `CellAppMgr`

### 2. 有长期在线身份，就不能只靠 Gateway

只要项目需要：

- 断线恢复
- 重复登录裁决
- 跨地图归属
- 跨局成长状态

就必须有 `PlayerAnchor` 或等价在线主状态对象。

### 3. 固定地图玩法优先走 SceneInstance

塔防和固定地图玩法不应该硬套成：

```text
Shard -> Zone -> Partition -> Cell
```

更合理的是：

```text
SceneTemplate -> SceneInstance -> WaveRuntime
```

### 4. 普通 MMO 先收 WorldHost

`Standard MMO` 的核心不是 `CellAppMgr`，而是：

- `PlayerAnchor`
- `WorldAssignment`
- `WorldHost`
- `MapInstance`
- `WorldSession`

这些没有稳定前，不进入 `Proxy / Witness / Ghost`。

### 5. Distributed MMO 不能跳过 Standard MMO

分布式世界需要复用：

- 登录票据
- 玩家锚点
- 拓扑归属
- 持久化边界
- 复制管线
- 运维观测

如果这些还没有稳定，直接做 `AuthorityTransfer` 会把复杂度放大。

## 七、升级路径

### Compact -> Standard MMO

触发条件：

- 固定地图之外开始出现常驻世界
- 需要跨地图长期在线主状态
- 场景实例数量和玩家归属需要统一调度

升级动作：

- 引入 `BaseApp(PlayerAnchor)`
- 引入 `WorldAssignment`
- 把 `SceneInstance` 中通用地图能力沉到 `WorldHost / MapInstance`
- 保留 `WaveRuntime` 作为领域组件

### Standard MMO -> Distributed MMO

触发条件：

- 单个 `WorldApp` 无法承载热点地图
- 同一地图需要多节点空间切片
- 需要无缝跨边界移动
- 实体权威必须跨节点迁移

升级动作：

- 引入 `SpacePartition`
- 引入 `Witness / Ghost`
- 引入 `AuthorityTransfer`
- 引入 `AppMgr`
- 将 `WorldApp` 中的空间权威部分分化为真正 `CellApp`

### Standard MMO -> Match / Room 旁路

触发条件：

- 活动、副本、竞技玩法以局为单位
- 不需要长期世界空间连续性

升级动作：

- 引入 `MatchCoordinator`
- 引入 `MatchInstance`
- 复用 `Gateway / PlayerAnchor / Persistence`

## 八、当前仓库映射

| 当前目录 | 当前收敛语义 | 说明 |
|----------|--------------|------|
| `apps/login-app` | 登录入口 | 认证、风控、入口分配、票据发放 |
| `apps/gateway-app` | 公网接入 | 连接、心跳、admission、短期路由 |
| `apps/base-app` | 目标是 `PlayerAnchor Host` | 当前不能继续按纯数据服务扩张 |
| `apps/cell-app` | 过渡期按 `WorldHost` 装配壳理解 | 不等于完整 BigWorld `CellApp` |
| `apps/game-server` | 调试 / sandbox / compact 候选壳 | 不应吞掉标准 MMO 主链语义 |
| `modules/game/session` | 在线主状态 | `PlayerAnchor / AnchorManager / SessionLocator` |
| `modules/game/world` | 世界运行时 | `WorldHost / MapInstance / WorldSession / AOI` |
| `modules/bigworld` | 兼容与增强层候选 | 不作为默认起步主链 |

## 九、反模式清单

当前阶段需要避免这些做法：

- 用 `cell-app` 名字证明已经进入 BigWorld。
- 让 `base-app` 继续扩张为数据库中心。
- 让 `gateway-app` 持有完整玩家在线状态。
- 在 `WorldHost` 未稳定前设计完整 `CellAppMgr`。
- 把塔防局硬套成 `Zone / Cell / Partition`。
- 用目录重命名代替语义收口。
- 为了兼容所有玩法，把所有组件默认启用。

## 十、验收问题

每次新增一个 Profile 或进程组合，都必须先回答：

1. 客户端连接入口是谁？
2. 登录票据由谁签发和校验？
3. 玩家长期在线主状态由谁持有？
4. 当前运行态由谁持有？
5. 持久化由谁执行？
6. 断线重连时谁裁决？
7. 跨地图或跨场景时谁更新归属？
8. 这个 Profile 默认关闭哪些能力？
9. 它的最小验收链路是什么？
10. 它是否提前引入了 L9 复杂度？

如果这些问题答不清楚，就不能继续新增进程或组件。

## 十一、结论

Apollo 的 topology 设计应按项目形态装配，而不是按 BigWorld 进程名倒推。

近期最稳的收敛方式是：

- 塔防和固定地图走 `Tower Defense Compact`
- 轻量 MMO 和普通 MMO 走 `Standard MMO`
- 连续大世界在后期叠加 `Distributed MMO`

这个顺序能让 Apollo 先拥有可运行主链，再逐步引入复杂的分布式世界能力。

## 相关阅读

- [轻量 MMO 与塔防适配判断](./lightweight-mmo-and-tower-defense-fit.md)
- [Topology 对比与登录分发设计](./topology-comparison-and-login-flow-design.md)
- [Shard、Zone、Instance 与 Match Topology 设计](./shard-zone-instance-match-topology-design.md)
- [MMO 组件装配目录](./mmo-component-assembly-catalog.md)
- [MMO 模块落地清单](./mmo-module-rollout-plan.md)
- [Standard MMO 任务清单](./standard-mmo-task-checklist.md)
- [Distributed World 任务清单](./distributed-world-task-checklist.md)
