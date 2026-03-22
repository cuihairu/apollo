---
title: Shard、Zone、Instance 与 Match Topology 设计
icon: cubes
order: 29
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Shard
  - Zone
  - Instance
  - Match
---

# Shard、Zone、Instance 与 Match Topology 设计

这篇文档解决的是 Apollo 在支持多种游戏形态时，一个非常容易被混淆的问题：

`分片、分区、地图实例、房间、匹配局，这些概念到底如何统一建模。`

如果这层不清楚，后面很容易出现：

- shard / zone / instance / room 混着叫
- 世界拓扑和部署拓扑混在一起
- 普通 MMO、房间制、BigWorld 模式没有统一描述

## 一、设计目标

这层设计要解决 6 个问题：

1. `Shard / Zone / Instance / Match` 的语义边界是什么。
2. 哪些是业务拓扑概念，哪些是部署拓扑概念。
3. 普通在线、房间制、普通 MMO、BigWorld 模式如何统一描述。
4. 如何和 `PlayerAnchor`、`WorldHost`、`Distributed World` 对齐。
5. 如何避免把所有场景都套成同一种 world 模型。
6. 如何为路由、扩容、匹配、切图提供稳定术语。

## 二、参考来源

### 1. 参考 MMO 常见拓扑模型

参考点：

- 大区/分片
- 地图/分线
- 副本/实例

### 2. 参考房间制/匹配制模型

参考点：

- match queue
- room / battle instance

### 3. 参考 BigWorld 空间分布模型

参考点：

- distributed space
- partition / cell

不照搬点：

- 不把所有游戏形态都强行解释成 distributed cell world

## 三、为什么这样设计

Apollo 现在明确要支持的不是一种单一游戏形态，而是：

- 轻在线
- 房间制
- 普通 MMO
- BigWorld 模式

如果没有统一术语，后面的：

- 路由
- 配置
- profile
- topology
- app 分工

都会越来越混乱。

更合理的方式是：

- 先定义清楚业务拓扑术语
- 再决定它们在不同模式下如何映射到部署拓扑

## 四、优点

- 术语稳定
- 多种游戏形态能放进统一框架
- 路由和扩容设计更清楚
- 不会把房间制误写成 world shard，也不会把 MMO 分片误写成 match

## 五、代价与风险

- 需要同时维护多个层级概念
- 文档和代码命名要更严格
- 如果模型过多，初学者理解成本会上升

## 六、为什么不选其他方案

### 不选“所有地图都叫 world”

因为这会让 shard、instance、match 等语义全部混掉。

### 不选“所有玩法都强行解释成房间”

因为 MMO 和大世界模式不适合这样建模。

### 不选“所有玩法都强行解释成 distributed space”

因为这会把普通在线和房间制场景复杂化。

## 七、推荐术语定义

### 1. `Shard`

语义：

- 顶层玩家群体或大区分片

它更接近：

- 区服
- 大区
- 逻辑分片

### 2. `Zone`

语义：

- shard 内的业务世界分区或地图线

它可以是：

- 某张地图
- 某条分线
- 某类玩法区

### 3. `Instance`

语义：

- 某个 zone 的具体运行实例

例如：

- 某副本实例
- 某地图运行实例

### 4. `Match`

语义：

- 一次匹配局或对局运行单元

例如：

- MOBA 对局
- FPS 房间
- 回合战斗局

### 5. `Partition`

语义：

- BigWorld 模式下，instance 内进一步的空间切片

这是 distributed world 的内部术语，不应滥用到普通 MMO。

## 八、业务拓扑与部署拓扑的区别

这点必须单独说清楚。

### 业务拓扑

回答的是：

- 玩家在什么 shard
- 进入什么 zone
- 属于哪个 instance
- 是否在某个 match 里

### 部署拓扑

回答的是：

- 这些拓扑实体最终跑在哪个 app/host 上

### 设计结论

Apollo 不应该把：

- `zone`

直接等同于：

- 某个物理进程

因为：

- 同一个进程可承载多个 instance
- 同一个 instance 在 BigWorld 模式下可切成多个 partition

## 九、不同游戏形态的映射

## 十、轻在线游戏

通常：

- `Shard` 可存在
- `Zone` 很弱
- `Instance` 可选
- `Match` 很弱或没有

## 十一、房间制 / 匹配制游戏

通常：

- `Shard` 有
- `Zone` 很弱
- `Instance` 可等于房间实例
- `Match` 很强

### 设计结论

房间制更适合：

- `Match` 作为主要运行单元

## 十二、普通 MMO

通常：

- `Shard` 很强
- `Zone` 很强
- `Instance` 很强
- `Match` 只是部分玩法旁路

### 设计结论

普通 MMO 更适合：

- `Shard -> Zone -> Instance`

这条主线。

## 十三、BigWorld 模式

通常：

- `Shard` 很强
- `Zone` / `Instance` 仍然存在
- `Instance` 内再切 `Partition`
- `Match` 只针对部分玩法

### 设计结论

BigWorld 模式不是抹掉 `Shard / Zone / Instance`，

而是在 `Instance` 内再引入：

- `Partition`

## 十四、推荐对象模型

```text
TopologyModel
    ├── ShardDescriptor
    ├── ZoneDescriptor
    ├── InstanceDescriptor
    ├── MatchDescriptor
    └── PartitionDescriptor
```

### `ShardDescriptor`

职责：

- 顶层区服信息
- 玩家归属策略

### `ZoneDescriptor`

职责：

- 地图/玩法区定义
- 进入规则

### `InstanceDescriptor`

职责：

- 某个 zone 的运行实例定义
- 生命周期
- 容量

### `MatchDescriptor`

职责：

- 对局规则
- 参赛者集合
- 开始/结束状态

### `PartitionDescriptor`

职责：

- distributed world 内部空间切片定义

## 十五、和 PlayerAnchor / WorldHost 的关系

### `PlayerAnchor`

更适合持有：

- `shard`
- 当前 `zone`
- 当前 `instance`
- 当前 `match`（如果有）

### `WorldHost`

更适合承载：

- instance runtime
- partition runtime

### 设计结论

拓扑路由权威更适合由：

- `PlayerAnchor`

收口，

而不是由 gateway 或 world 自己单独决定全部归属。

## 十六、和 Matchmaking 的关系

匹配系统不应和 world 拓扑混成一层。

更合理的方式是：

- `Matchmaking`
  - 决定谁进入哪个 `Match`
- `Topology`
  - 决定这个 match 映射到哪个 instance / host

## 十七、对当前 Apollo 的直接含义

Apollo 后续如果继续推进整体框架，建议优先补：

- `ShardDescriptor`
- `ZoneDescriptor`
- `InstanceDescriptor`
- `MatchDescriptor`

并把：

- 普通 MMO
- 房间制
- BigWorld

都统一映射到这套术语上。

## 十八、结论

Apollo 要支持多种在线游戏形态，就必须先把：

- `Shard`
- `Zone`
- `Instance`
- `Match`
- `Partition`

这些概念拆清楚。

只有这样，路由、扩容、匹配、切图、分布式世界这些设计才会有稳定语言。

## 相关阅读

- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [Distributed Space 设计](./distributed-space-design.md)
