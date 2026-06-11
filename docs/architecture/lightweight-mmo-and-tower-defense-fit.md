---
title: 轻量 MMO 与塔防适配判断
order: 2
category:
  - 架构
  - Apollo
tag:
  - 轻量 MMO
  - 塔防
  - Compact GameServer
  - 架构判断
---

# 轻量 MMO 与塔防适配判断

这篇文档回答一个实际问题：

`Apollo 当前这套架构，是否适合做轻量 MMO 和塔防类游戏。`

结论先给出：

- 轻量 MMO 可以使用 Apollo 当前架构，但应默认采用 `L1-L8 + Standard MMO Profile`，不要一开始启用 BigWorld 分布式世界层。
- 塔防 / 固定地图类游戏可以使用 Apollo 当前架构，但应采用 `Compact GameServer Profile`，把 Scene、AOI、Battle、Wave Runtime 放在同一个游戏进程内。
- 当前架构方向是可行的，但要把文档和后续实现从“重型 MMORPG 默认形态”改成“轻量在线游戏引擎 + 可选大世界增强”。

## 一、目标游戏类型

### 轻量 MMO

这里的轻量 MMO 指：

- 分线或区服明确
- 地图数量可控
- 单地图在线人数有限
- 有登录、会话、社交、背包、任务、活动、排行榜等常见在线系统
- 有世界或副本运行时，但不要求连续大世界空间切片

典型例子：

- 分线 RPG
- 小型开放地图 RPG
- 多人副本型 RPG
- 带主城、野外、活动地图的轻量 MMO

### 塔防 / 固定地图

这里的塔防指：

- 固定地图或少量地图模板
- 波次驱动
- 怪物沿路径移动
- 玩家建塔、升级、释放技能或布置单位
- 局内状态实时同步，但地图空间和玩家规模通常有限

典型例子：

- 传统塔防
- 绿色循环圈类玩法
- 多人合作防守
- 固定路线刷怪 + 技能组合玩法

## 二、可用性判断

| 游戏类型 | 是否适合 Apollo | 推荐 Profile | 不建议默认启用 |
|----------|----------------|--------------|----------------|
| 轻量 MMO | 适合 | Standard MMO Profile | BigWorld L9 |
| 塔防 / 固定地图 | 适合 | Compact GameServer Profile | 独立 Cell / Partition |
| 连续大世界 MMO | 可支持，但属于后续增强 | Distributed MMO Profile | Compact 模式 |
| 纯房间竞技 | 可复用底座，但需要强化 Match / Room | Room / Match Profile | WorldHost 主线 |

## 三、为什么轻量 MMO 适合

轻量 MMO 需要的核心能力，Apollo 已经在架构上覆盖：

| 能力 | Apollo 对应层 |
|------|---------------|
| 登录认证 | Domain Components / Login |
| 公网连接与会话 | Gateway / Session Protocol |
| 玩家在线主状态 | PlayerAnchor |
| 地图与实例 | WorldHost / Instance |
| AOI 与状态同步 | Interest Management / Replication |
| 战斗与属性 | Battle / Attribute / ECS |
| 持久化与缓存 | Platform Foundation / Persistence |
| 运行时治理 | Runtime Host / Runtime Ops Host |

关键约束是：轻量 MMO 不应该从 `CellApp + Witness + Ghost + AuthorityTransfer` 起步。

更合理的主链是：

```text
Client
  -> LoginApp
  -> GatewayApp
  -> BaseApp(PlayerAnchor Host)
  -> WorldHost
  -> PersistenceService
```

这条链路足够支撑分线、地图实例、副本、轻社交和常规运营系统。

## 四、为什么塔防也适合

塔防类玩法的核心不是分布式世界，而是：

- 稳定 tick
- 波次调度
- 路径与寻路边界
- 建造与技能规则
- 怪物状态同步
- 局内快照和重连恢复

这些能力可以复用 Apollo 的通用底座，但不需要完整 MMO 进程拓扑。

推荐主链是：

```text
Client
  -> GatewayApp
  -> CompactGameServer
       -> SceneInstance
       -> WaveRuntime
       -> BattleRuntime
       -> AOI / Visibility
       -> Script Hooks
  -> PersistenceService
```

`CompactGameServer` 的价值是降低服务拆分成本：一个进程就能承载固定地图、AOI、战斗、波次和脚本钩子，后续玩家量或地图量上来后，再按接口拆成 `WorldHost + BattleService + Match/Instance Host`。

## 五、整理后的三种目标 Profile

### Profile A: Tower Defense Compact

适合：

- 塔防
- 固定地图合作防守
- 小规模多人局内玩法

装配：

```text
L1 Base
L2 Core
L3 Transport / Protocol
L4 Runtime Host
L5 Runtime Ops
L6 Platform Foundation
L7 Game Foundation
L8 Domain: Gateway + CompactGameServer + Persistence
```

进程：

```text
gateway-app
compact-game-app
persistence-app
platform-app optional
```

需要补齐的专项模块：

- `WaveRuntime`
- `PathTrack / LaneRuntime`
- `TowerPlacement`
- `MonsterSpawn`
- `SceneSnapshot`
- `ReconnectToScene`

### Profile B: Lightweight MMO

适合：

- 分线 MMO
- 地图切换 MMO
- 副本型 RPG
- 轻量开放地图 RPG

装配：

```text
L1-L8
L10 Assembly / Topology
```

进程：

```text
login-app
gateway-app
base-app       target: PlayerAnchor Host
world-app      current cell-app can evolve into this role
persistence-app
platform-app optional
```

需要补齐的专项模块：

- `WorldAssignment`
- `Zone / Instance Descriptor`
- `WorldSession`
- `EntitySchema`
- `Replication Pipeline`
- `Repository / UnitOfWork`

### Profile C: Distributed MMO

适合：

- 连续大世界
- 热点区域需要空间切片
- 实体权威需要跨节点迁移

装配：

```text
L1-L10
L9 Distributed World Extension enabled
```

进程：

```text
login-app
gateway-app
base-app
cell/world-app
app-manager
persistence-app
```

这是 Apollo 的增强目标，不是轻量 MMO 和塔防的默认目标。

## 六、架构边界调整

为了让 Apollo 真正适配这两类游戏，后续文档和代码应遵守这些边界：

1. `BigWorld` 是增强层，不是默认层。
2. `WorldHost` 是普通 MMO 的主运行时，不应被 `CellApp` 名称绑死。
3. `CompactGameServer` 是塔防和固定地图的一等 Profile，不是临时 demo。
4. `Match / Instance` 必须独立于 `World / Zone` 建模，避免把塔防局硬套成大世界地图。
5. `BattleRuntime` 和 `WaveRuntime` 应保留数据导向热路径，不要强制走重对象远程调用。
6. `PlayerAnchor` 负责跨域在线主状态，不能让 Gateway 或 World 各自持有完整玩家归属真相。

## 七、当前风险

| 风险 | 影响 | 处理方式 |
|------|------|----------|
| 文档仍有大量 MMORPG / BigWorld 默认表述 | 容易误导起步架构 | 首页、概述、导航先重定向到 Profile |
| Compact GameServer 还偏设计稿 | 塔防落地缺少实现清单 | 增补 Wave / Scene / Snapshot 任务 |
| `cell-app` 名称和普通 WorldHost 语义混杂 | 进程职责不清 | 明确当前 `cell-app` 可演进为 `world-app` shell |
| 战斗、AOI、复制链路还未完全收口 | 影响端到端示例 | 先做塔防或轻 MMO 的最小闭环 |

## 八、建议的近期落地顺序

1. 文档层先完成 VitePress 和架构入口重整。
2. 代码层先打通 `Tower Defense Compact` 最小闭环。
3. 再打通 `Lightweight MMO` 在线主链。
4. 最后再推进 `Distributed MMO` 的 L9 能力。

最小闭环建议是：

```text
Gateway connection
  -> session bind
  -> enter scene
  -> spawn wave
  -> build tower / cast skill
  -> battle tick
  -> state replication
  -> snapshot / reconnect
```

## 九、结论

Apollo 这套架构可以支撑轻量 MMO 和塔防类游戏。

但正确用法不是从完整 BigWorld 拓扑起步，而是：

- 轻量 MMO 使用 `Standard MMO Profile`
- 塔防使用 `Compact GameServer Profile`
- BigWorld 作为后续 `Distributed MMO Profile`

这样既能复用 Apollo 现有基础设施，也能避免把轻量项目过早拖入大世界架构复杂度。

## 相关阅读

- [架构概述](./overview.md)
- [Compact GameServer 设计方案](/30-Compact_GameServer_Design.md)
- [Shard、Zone、Instance 与 Match Topology 设计](./shard-zone-instance-match-topology-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [WorldHost 设计稿](./world-host-design.md)
- [Combat Runtime 与 ECS 边界设计](./combat-runtime-and-ecs-boundary-design.md)
