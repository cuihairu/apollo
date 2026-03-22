---
title: Interest Management 与 AOI Pipeline 设计
icon: eye
order: 26
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - AOI
  - InterestManagement
  - Replication
---

# Interest Management 与 AOI Pipeline 设计

这篇文档解决的是 Apollo 世界运行时继续往下细化时，一个必须先定清楚的问题：

`玩家到底该看见什么，这条可见集主链如何组织。`

如果这层不清楚，后面很容易出现：

- AOI 只是一个“附近查询”
- 可见集和复制链路断开
- witness 只是广播容器
- 分布式世界一接进来就发现普通 AOI 模型不够用

## 一、设计目标

这层设计要解决 6 个问题：

1. AOI 和 interest management 的关系是什么。
2. 普通 MMO 和 BigWorld 模式下，可见集主链如何统一。
3. `AOI / Witness / Replication` 如何串成一条 pipeline。
4. 哪些阶段适合对象模型，哪些阶段适合数据导向。
5. 如何兼容分层 AOI、四叉树、R-tree 等不同空间索引。
6. 如何避免把 AOI 简化成“九宫格广播”。

## 二、参考来源

### 1. 参考经典 AOI 设计

参考点：

- 邻近对象筛选
- 进入可见 / 离开可见
- 空间索引

### 2. 参考 KBE 的 Witness / Ghost 思想

参考点：

- 客户端看到的是 `Witness` 管理的可见世界
- `Ghost` 参与分布式近场可见性

不照搬点：

- 不把整个 interest system 完全写成重对象黑箱

### 3. 参考现代数据导向空间查询思路

参考点：

- 空间索引可数据化
- 批量候选计算可 job 化

## 三、为什么这样设计

Apollo 后续已经明确有：

- `World Tick`
- `Witness`
- `Replication Pipeline`
- `Distributed World Extension`

这意味着 AOI 不能再只是一个独立小模块。

更合理的方式应该是：

- `Interest Management`
  - 负责“谁应该看到谁”
- `AOI`
  - 负责空间候选查询
- `Witness`
  - 负责客户端可见集上下文
- `Replication`
  - 负责把可见集变化变成同步输出

也就是说：

- AOI 只是 pipeline 的一环，不是全部

## 四、优点

- 可见集语义更完整
- 更适合普通 MMO 和 BigWorld 统一
- 更适合和复制链路打通
- 为空间索引实现替换留出空间

## 五、代价与风险

- 设计会比传统 AOI 模块复杂
- witness、AOI、replication 边界需要严格定义
- 如果抽象过度，会让普通 MMO 落地太重

## 六、为什么不选其他方案

### 不选“AOI = 广播附近实体”

因为这只能处理很粗糙的单进程近邻同步。

### 不选“可见集完全靠 replication 临时判断”

因为这样可见关系状态无法稳定维护。

### 不选“强制所有项目一开始就上 distributed witness/ghost”

因为 Apollo 默认仍然要先跑通普通 MMO 模式。

## 七、推荐总体模型

建议 Apollo 的可见集主链是：

```text
Spatial Index
    -> AOI Candidate Set
    -> Interest Filter
    -> Witness Visible Set
    -> Replication Delta
```

### 含义

- `Spatial Index`
  - 只负责空间候选
- `AOI Candidate Set`
  - 得到附近可能相关对象
- `Interest Filter`
  - 按规则过滤真正关心的对象
- `Witness Visible Set`
  - 维护客户端稳定可见集
- `Replication Delta`
  - 输出进入/离开/增量同步

## 八、AOI 与 Interest Management 的区别

### AOI

回答的是：

- 空间上离得近不近

### Interest Management

回答的是：

- 即使空间上接近，是否真的应该互相可见

例如下面这些都属于 interest rule：

- 阵营
- 隐身
- 副本隔离
- 观察模式
- 详情层级

### 设计结论

Apollo 应明确：

- `AOI` 是空间候选层
- `Interest Management` 是可见性决策层

## 九、推荐空间索引抽象

Apollo 不应把 AOI 和某一种具体索引结构绑死。

建议支持统一抽象：

```text
ISpatialIndex
    insert
    update
    remove
    queryRange
    queryRadius
```

### 具体实现可以包括

- grid / 九宫格
- 分层 AOI
- quadtree
- R-tree
- sparse hash grid

### 为什么要保留多实现空间

因为不同游戏类型、地图形态、实体密度差异很大。

Apollo 不应该一开始就把某一种索引写死成“唯一正确答案”。

## 十、推荐 Pipeline 阶段

建议 interest pipeline 至少分成 5 个阶段。

### 1. Spatial Collect

职责：

- 从空间索引拉候选对象

### 2. Interest Filter

职责：

- 过滤真正应进入视图的对象

### 3. Visibility Diff

职责：

- 和上个 tick 的 visible set 做 diff

输出：

- entered
- stayed
- left

### 4. Witness Update

职责：

- 更新 witness 内部状态
- 分配 alias
- 更新 detail level

### 5. Replication Bind

职责：

- 把 visible diff 接入复制链路

## 十一、普通 MMO 与 BigWorld 的差异

### 普通 MMO 模式

通常：

- `Spatial Index` 只看本 world / map instance
- 候选对象全是本地实体
- 不需要 ghost source

### BigWorld 模式

会增加：

- real + ghost 双来源候选
- partition 边界观察
- witness 视图连续性要求

### 设计结论

Apollo 应让普通 MMO 先跑通：

- `Spatial -> Interest -> Witness -> Replication`

然后 BigWorld 再把：

- `GhostSource`
- `PartitionAwareFilter`

叠加进来。

## 十二、对象模型与数据导向边界

这块必须明确，不然后面又会回到“纯 KBE”或“纯 ECS”的争论。

### 更适合对象模型的

- `WitnessContext`
- 可见集状态
- alias 分配
- detail level 策略

### 更适合数据导向的

- 空间索引
- 候选集批量查询
- 大规模 range query

### 设计结论

Apollo 的 interest system 更合理的路线是：

- 外层可见集语义对象化
- 内层空间查询和筛选可数据导向

## 十三、推荐对象模型

```text
InterestService
    ├── SpatialIndex
    ├── CandidateCollector
    ├── InterestRuleSet
    ├── VisibilityDiffer
    ├── WitnessContext
    └── ReplicationBinder
```

### `InterestRuleSet`

职责：

- 阵营、隐身、副本、模式等规则过滤

### `VisibilityDiffer`

职责：

- 生成 entered / stayed / left

### `ReplicationBinder`

职责：

- 把可见集变化接给复制链路

## 十四、对当前 Apollo 的直接含义

Apollo 下一步如果继续推进世界运行时，建议优先补：

- `ISpatialIndex`
- `InterestService`
- `WitnessContext`
- `VisibilityDiffer`

优先先把普通 MMO 的可见集主链跑顺，

再为 BigWorld 模式扩 ghost / partition aware filter。

## 十五、结论

Apollo 的 AOI 设计不能停留在“附近实体查询”层。

更合理的方式应该是：

- AOI 只负责空间候选
- Interest Management 负责可见性决策
- Witness 负责客户端视图上下文
- Replication 负责输出同步结果

这样这套可见集主链才能真正支撑普通 MMO 和 BigWorld 两种模式。

## 相关阅读

- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
- [World Tick、Scheduling 与 Job Model 设计](./world-tick-scheduling-and-job-model.md)
