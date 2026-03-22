---
title: Distributed Space 设计
icon: cubes
order: 14
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BigWorld
  - CellApp
  - Ghost
---

# Distributed Space 设计

这篇文档是 Apollo 从普通 MMO world runtime 继续往 BigWorld 模式推进时，第一篇真正进入 distributed space 的设计稿。

它回答的是这个问题：

`Apollo 什么时候才需要真正的 CellApp，进入之后又该先补哪些对象。`

前面的文档已经把普通 MMO 模式的主链收住了：

- `LoginApp`
- `BaseApp(PlayerAnchor Host)`
- `Gateway`
- `WorldHost`
- `WorldSession`
- `DBMgr / PersistenceService`

接下来如果项目明确需要连续大世界，才进入这篇文档的范围。

## 一、先说结论

distributed space 不是“把 world 再拆成几个进程”这么简单。

它真正要解决的是：

- 同一空间如何分布到多个节点
- 哪个节点对某个实体拥有实时权威
- 跨节点后客户端视野如何连续
- 节点边界附近如何维持观察关系
- 实体迁移时消息如何不断链

所以一旦进入 BigWorld 模式，Apollo 最关键的新对象不是更多 app 名字，而是：

- `SpacePartition`
- `CellNode`
- `GhostReplica`
- `Witness`
- `AuthorityTransfer`
- `CellAppMgr`

## 二、什么时候才需要进入 distributed space

只有当项目明确出现下面这些需求时，才应该开启这层：

- 单地图实例不能再绑定单进程
- 热点区域需要空间级水平扩容
- 玩家跨区域移动不能接受 loading
- 空间边界附近仍需持续观察周边对象
- 一个空间需要多节点共同承载

如果项目还不满足这些条件，那么继续维持：

- `WorldHost + MapInstance + WorldSession`

通常更合理。

## 三、为什么普通 world runtime 不够了

普通 MMO world runtime 默认假设：

- 一个地图实例归一个 world 进程
- 这个 world 进程对地图内全部实体有实时权威
- AOI 范围只在单进程内求解

一旦进入大世界模式，这三个假设都会失效。

### 失效点 1：空间不再是单进程容器

一个 space 可能需要切成多个 partition，由多个 cell 节点共同运行。

### 失效点 2：实体不再永远只存在一个本地视图

实体会出现：

- 一个 real authority
- 多个 ghost replica

### 失效点 3：AOI 不再是本地查询问题

视野内对象可能跨 partition，甚至跨 cell 节点。

这时单纯的本地 `AOIManager` 已经不够了。

## 四、Apollo 中的 distributed space 应该怎么定位

Apollo 里更合理的关系应该是：

```text
普通 MMO 模式
    WorldHost
        └── WorldSpace

BigWorld 模式
    WorldHost
        └── DistributedSpaceRuntime
                ├── CellNode
                ├── SpacePartition
                ├── GhostReplica
                ├── Witness
                └── AuthorityTransfer
```

也就是说：

`Distributed Space = WorldHost 之上的增强层`

而不是推翻 `WorldHost` 另起炉灶。

## 五、推荐核心对象

### 1. `CellNode`

职责：

- 表示一个承载空间分片的运行节点
- 管理本节点上的 real entity
- 管理本节点上的 ghost replica
- 执行 partition 内 tick

建议理解成：

- `CellApp` 在运行时里的节点对象

### 2. `SpacePartition`

职责：

- 表示一个 space 的局部分片
- 维护分片边界
- 维护分片内实体集合
- 对接本分片 AOI 和 replication

这个对象很关键。

如果没有显式 partition 对象，后续 ghost、迁移、边界观察都会散。

### 3. `GhostReplica`

职责：

- 表示非权威节点上的实体投影
- 维护来自 real entity 的同步快照
- 为边界观察和消息路由提供近场视图

### 4. `Witness`

职责：

- 表示客户端视角上下文
- 决定客户端当前可见实体集合
- 聚合 real entity 和 ghost replica 的输出

### 5. `AuthorityTransfer`

职责：

- 管理实体从一个 cell 节点迁到另一个 cell 节点
- 管理迁移中的消息不断链
- 控制 transfer 生命周期和回滚

### 6. `CellAppMgr`

职责：

- 管理 cell 节点负载
- 管理 space 到 partition 的拓扑
- 管理 partition 到 cell 的映射
- 协调扩容、迁移和恢复

## 六、real entity 和 ghost replica 的关系

BigWorld 模式里必须明确一件事：

`一个实体在任意时刻只能有一个实时权威节点。`

也就是说：

- 一个 `real entity`
- 零到多个 `ghost replica`

### `real entity`

负责：

- 权威移动
- 权威属性更新
- 权威战斗判定
- 迁移源数据导出

### `ghost replica`

负责：

- 近场可见性补充
- 邻接分片观察
- 边界消息接力

不负责：

- 最终权威判定

如果 ghost 也开始独立修改实体核心状态，系统会很快乱掉。

## 七、为什么 `Witness` 是必须的

很多实现一开始会把 distributed AOI 简化成：

- 跨分片查附近实体

这还不够。

因为客户端真正需要的是：

- 一个连续、稳定、可版本化的“可见世界视图”

这个视图必须由 `Witness` 收口。

### `Witness` 负责什么

- 管理当前客户端可见的 entity set
- 判断 enter view / leave view
- 决定发 real 还是发 ghost 的同步内容
- 合并多个分片来源的可见集

所以：

- `AOI` 负责空间关系
- `Witness` 负责客户端视图关系

## 八、推荐分层关系

建议把 distributed space 分成下面几层。

### 层 1：Partition 管理

对象：

- `SpacePartition`
- `PartitionMap`
- `PartitionBoundary`

职责：

- 划分空间
- 维护边界
- 管理分片拓扑

### 层 2：Authority 层

对象：

- `RealEntity`
- `GhostReplica`
- `AuthorityRole`

职责：

- 标记权威归属
- 管理同步来源

### 层 3：Transfer 层

对象：

- `AuthorityTransfer`
- `TransferContext`
- `MigrationRoute`

职责：

- 实体迁移
- 消息不断链
- 失败回滚

### 层 4：View 层

对象：

- `Witness`
- `ViewTrigger`
- `ReplicationTarget`

职责：

- 客户端可见集
- 跨 partition 视图拼接

## 九、推荐迁移流程

实体跨 partition 迁移，建议明确收成四段式。

### 阶段 1：准备迁移

1. 当前 real cell 检测实体接近分片边界
2. `AuthorityTransfer` 创建 transfer context
3. 通知目标 cell 准备接管

### 阶段 2：建立双态窗口

1. 源 cell 仍保持 real
2. 目标 cell 建立预备接管状态
3. 相关消息开始通过 migration route 兜底

### 阶段 3：切换权威

1. 目标 cell 升级为 real
2. 源 cell 降级为 ghost 或清理
3. `CellAppMgr` 更新 partition authority 记录

### 阶段 4：收尾

1. 更新 route version
2. 刷新 witness 侧可见集
3. 清理旧迁移上下文

### 为什么需要双态窗口

因为如果直接“一删一建”：

- 网络延迟
- 乱序消息
- 边界观察

都会出问题。

## 十、推荐 `TransferContext`

建议至少包含：

```text
AuthorityTransferContext
    entity_id
    entity_type
    source_cell_id
    target_cell_id
    source_partition_id
    target_partition_id
    position
    direction
    volatile_snapshot
    route_version
    transfer_epoch
```

### 为什么要有 `transfer_epoch`

因为大世界里，连续多次迁移可能发生得很快。

如果没有 epoch，很难判断某条延迟消息属于哪一次迁移窗口。

## 十一、`CellAppMgr` 应该管什么

Apollo 如果后续补 `CellAppMgr`，不能做成薄心跳中心。

它至少要掌握下面这些信息：

- 当前有哪些 `CellNode`
- 每个 `CellNode` 的 load
- 当前有哪些 `Space`
- 每个 `Space` 被切成哪些 `Partition`
- 每个 `Partition` 当前在哪个 `CellNode`
- 哪些实体正在迁移

### `CellAppMgr` 的关键职责

- partition 调度
- cell 负载均衡
- 空间拓扑维护
- 迁移协调
- 故障恢复

## 十二、Apollo 中建议新增的抽象

建议至少提前留下面这些头文件级抽象：

- `modules/game/world/include/apollo/game/world/space_partition.hpp`
- `modules/game/world/include/apollo/game/world/partition_id.hpp`
- `modules/game/world/include/apollo/game/world/authority_role.hpp`
- `modules/game/world/include/apollo/game/world/ghost_replica.hpp`
- `modules/game/world/include/apollo/game/world/witness_context.hpp`
- `modules/game/world/include/apollo/game/world/authority_transfer.hpp`
- `modules/game/world/include/apollo/game/world/migration_route.hpp`

如果这些抽象不先立住，后面很容易把 distributed 能力散着补到 `CellServer`、`EntityManager`、`AOIManager` 里。

## 十三、和当前 `cell-app` 的关系

当前 Apollo 的 `apps/cell-app` 还不是 KBE 语义上的真正 `CellApp`。

它更接近：

- 单进程 world 原型

所以后续建议这样演进：

### 第一步

- 把当前 `cell-app` 继续按 world 原型理解
- 先完成 `WorldHost + WorldSession + entry/transfer`

### 第二步

- 在 `modules/game/world` 先补 distributed 抽象
- 但暂时不开真正多 cell

### 第三步

- 当项目明确需要大世界，再引入真正 `CellNode`
- 再把 `apps/cell-app` 升级成分布式 cell 节点进程

## 十四、近期最小可交付版本

不要一上来就实现完整 BigWorld。

建议先做一个非常小的第一步。

### 最小范围

- 明确 `PartitionId`
- 明确 `AuthorityRole`
- 明确 `GhostReplica` 抽象
- 明确 `AuthorityTransferContext`
- `WorldHost` 保持当前单节点运行

### 暂时不做

- 完整多 cell 拓扑
- 真正跨进程 ghost 同步
- 完整 witness 合流
- 自动负载均衡

### 为什么这样做

因为这一步的目标不是立刻做完大世界，而是：

- 先把普通 world runtime 写成未来还能升级的形态

## 十五、推荐演进顺序

最稳的顺序是：

1. 先完成普通 MMO world runtime
2. 再完成 `WorldSession / transfer / recovery`
3. 再补 distributed 抽象层
4. 再补 `CellAppMgr`
5. 最后按需落真正多 cell 运行

如果顺序反过来，很容易在基础还没稳定时就进入最复杂的一层。

## 十六、结论

Apollo 如果要参考 KBE/BigWorld，真正难的不是把进程名凑齐，而是把 distributed space 的几个关键运行时对象补全：

- `SpacePartition`
- `GhostReplica`
- `Witness`
- `AuthorityTransfer`
- `CellAppMgr`

只有这些对象立住，Apollo 才能从“单进程 world”平滑升级到“多节点连续大世界”。

否则看起来像有了 `CellApp`，实际上仍然只是多个地图服的变体。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Space Partition 与 Topology 设计](./space-partition-topology-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [WorldHost 设计稿](./world-host-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
