---
title: Space Partition 与 Topology 设计
icon: border-all
order: 18
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - SpacePartition
  - Topology
  - CellAppMgr
---

# Space Partition 与 Topology 设计

这篇文档解决的是 distributed space 里另一个基础问题：

`空间到底怎么切，切完之后谁维护这张拓扑图。`

如果这层没有显式设计，后面 authority transfer、ghost、witness 都会失去稳定边界。

因为这些机制都默认依赖一件事：

- 系统必须知道“这个实体当前属于哪个 partition，这个 partition 当前在哪个 cell，它和哪些 partition 相邻”

这就是 topology 层的意义。

## 一、先说结论

BigWorld 模式里，`space` 不能只理解成一个 `spaceId`。

它至少要同时具备三层语义：

- 空间标识
- 空间运行时容器
- 空间分片拓扑

更准确地说：

- `Space` 是运行中的世界空间
- `Partition` 是空间的局部分片
- `Topology` 是所有 partition 的连接关系和放置关系

## 二、为什么这层必须单独存在

前面的文档已经有：

- `Distributed Space`
- `Witness / Ghost`
- `Authority Transfer`

但这些都默认依赖 topology。

### 如果没有 topology，会出现什么问题

- 无法稳定判断实体接近哪个边界
- 无法知道某个 partition 的邻居是谁
- 无法知道迁移目标该选哪个 cell
- manager 层无法做空间可视化和调度

所以 topology 不是附属信息，而是整个 distributed space 的底图。

## 三、KBE 里的直接证据

从本地源码和前面分析可以确认：

- `cellappmgr/space.h`
  - manager 层持有 `Space` 视图
- `cellappmgr/cellappmgr.h`
  - 有 `querySpaces`
  - 有 `updateSpaceData`
  - 有 `setSpaceViewer`
- `cellapp/spacememory.h`
  - cell 侧也持有 space 运行时对象

这说明在 KBE 里：

- space 不是只在 cell 本地存在
- manager 层也持有一份空间拓扑视图

Apollo 如果要走这条路，也必须这么分。

同样需要明确：

- 当前 `apps/cell-app` 还不是这里描述的分布式 `CellApp`
- 这里的 topology 设计属于后续大世界增强层

## 四、推荐核心对象

建议 Apollo 至少形成下面这些对象：

```text
SpaceTopology
    ├── SpaceDescriptor
    ├── SpacePartition
    ├── PartitionBoundary
    ├── PartitionGraph
    └── PartitionPlacement
```

### `SpaceDescriptor`

职责：

- 描述 space 的静态元数据
- 地图类型
- 几何资源
- 坐标系
- 默认分片策略

### `SpacePartition`

职责：

- 表示一个运行中的局部分片
- 自身边界
- 自身实体集
- 当前 authority cell

### `PartitionBoundary`

职责：

- 明确分片边界形状
- 提供边界接近检测
- 提供跨边界邻接关系

### `PartitionGraph`

职责：

- 维护 partition 邻接图
- 支持查询某个 partition 的邻居集合

### `PartitionPlacement`

职责：

- `partitionId -> cellNodeId`
- `cellNodeId -> partitions`

## 五、推荐 `SpaceDescriptor`

建议至少包含：

```text
SpaceDescriptor
    space_id
    space_type
    geometry_asset
    coordinate_system
    logical_width
    logical_height
    partition_strategy
```

### 为什么要分 `descriptor` 和 `runtime`

因为：

- `descriptor` 偏静态定义
- `runtime topology` 偏运行期展开

这两层混在一起，后面热更新、恢复和调试都会很乱。

## 六、推荐 `SpacePartition`

建议至少包含：

```text
SpacePartition
    partition_id
    space_id
    authority_cell_id
    boundary
    neighbor_ids
    load_score
    entity_count
    ghost_count
```

### `SpacePartition` 的职责边界

它不负责：

- 具体迁移协议执行

它负责：

- 告诉系统“这个局部空间块是什么、在哪、归谁管、旁边是谁”

## 七、推荐边界模型

边界模型不要一开始就过度复杂。

建议先支持下面两种：

### 1. 矩形分片

优点：

- 简单
- 好实现
- 调试容易

适合 Apollo 第一阶段。

### 2. 网格分片

优点：

- 规则
- 容易扩展成热点拆分

### 暂时先不要做

- 任意多边形边界
- 动态复杂切割

因为 Apollo 当前阶段更需要：

- 稳定的基础拓扑

## 八、推荐 topology 图关系

建议用显式图结构管理 partition 邻接。

```text
PartitionGraph
    partition_101 -> [102, 201]
    partition_102 -> [101, 103, 202]
    partition_201 -> [101, 202]
```

### 为什么必须显式维护邻接图

因为 authority transfer、ghost 建立、witness 边界观察，全部要用它：

- 谁是合法迁移目标
- 哪些 partition 应该建立 ghost
- 哪些边界要预热

## 九、推荐 placement 关系

topology 只描述空间结构还不够，还必须知道实际放置在哪个 cell。

建议显式维护：

```text
PartitionPlacement
    partition_id -> cell_node_id
    cell_node_id -> [partition_id...]
```

### 为什么 placement 要独立对象

因为：

- partition 本身描述“是什么”
- placement 描述“现在放在哪”

这样后面调度时，只改 placement，不需要反复篡改 partition 定义本身。

## 十、推荐 partition 调度流程

### 初始展开

1. `CellAppMgr` 读取 `SpaceDescriptor`
2. 按策略生成 partition 集合
3. 构建 `PartitionGraph`
4. 根据 load 和可用 cell 生成初始 `PartitionPlacement`
5. 下发给各 `CellNode`

### 运行期迁移

1. 发现某个 partition 过热
2. `CellAppMgr` 评估目标 cell
3. 更新调度计划
4. 触发相关 authority transfer
5. partition placement 更新

## 十一、和 authority transfer 的关系

两者关系必须明确：

- topology 决定“能往哪迁”
- authority transfer 决定“怎么迁”

也就是说：

- topology 是静态/半静态边界
- transfer 是运行时协议

如果把这两层混在一起，调试会非常困难。

## 十二、和 ghost / witness 的关系

### 对 ghost

ghost 建立的优先区域通常来自：

- partition 邻接关系

### 对 witness

witness 要不要纳入另一个 partition 的对象，通常也来自：

- 边界距离
- 邻接关系

所以 topology 层实际是：

- ghost 和 witness 的前置数据面

## 十三、和 `CellAppMgr` 的关系

`CellAppMgr` 应该是 topology 的控制面宿主。

它至少要能做：

- 创建 space topology
- 查询 topology
- 更新 placement
- 广播 topology version

### topology version 为什么重要

因为：

- partition placement 变化后，旧 view 和旧 route 可能过期

有版本号后，其他系统才能判断自己持有的拓扑是否陈旧。

## 十四、推荐版本模型

建议至少维护两个版本：

- `space_descriptor_version`
- `topology_version`

### `space_descriptor_version`

- 地图定义、边界策略变更时增长

### `topology_version`

- partition placement、邻接修正、运行时重平衡时增长

## 十五、Apollo 中建议新增的抽象

建议新增：

- `modules/game/world/include/apollo/game/world/space_descriptor.hpp`
- `modules/game/world/include/apollo/game/world/space_topology.hpp`
- `modules/game/world/include/apollo/game/world/space_partition.hpp`
- `modules/game/world/include/apollo/game/world/partition_boundary.hpp`
- `modules/game/world/include/apollo/game/world/partition_graph.hpp`
- `modules/game/world/include/apollo/game/world/partition_placement.hpp`
- `modules/bigworld/include/apollo/bigworld/space_topology_service.hpp`

## 十六、近期最小可交付版本

建议先从非常小的一版开始。

### 最小范围

- 一个 `SpaceDescriptor`
- 矩形网格分片
- 一个 `PartitionGraph`
- 一个 `PartitionPlacement`
- `CellAppMgr` 支持 `query_spaces / query_partitions`

### 暂时不做

- 动态不规则切分
- 自动拓扑重构
- 热点区域自适应细分

### 验证标准

至少要能验证：

1. 一个 space 被稳定切成多个 partition
2. 能查询每个 partition 当前在哪个 cell
3. 能查询每个 partition 的邻居
4. authority transfer 只会在合法邻接 partition 之间发生

## 十七、结论

Apollo 如果要真正进入 BigWorld 模式，space topology 必须成为一等对象。

它的价值不是“画个分区图”，而是给下面所有系统提供统一底图：

- `CellAppMgr`
- `AuthorityTransfer`
- `Ghost`
- `Witness`

只有 topology 层立住，distributed space 才不是一堆分散的局部技巧，而是一套可推理、可观测、可调度的运行时系统。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
