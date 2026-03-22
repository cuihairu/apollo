---
title: Witness 与 Ghost 设计
icon: eye
order: 16
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - Witness
  - Ghost
  - Replication
---

# Witness 与 Ghost 设计

这篇文档是 Apollo 进入 BigWorld 模式后，最关键的一篇运行时专题。

它要解决两个很容易被混淆的问题：

1. 客户端到底“看见什么”。
2. 分布式空间里，边界附近实体到底“以什么形式存在”。

如果这两个问题不拆开，系统很容易写成：

- AOI 查询
- 直接广播实体

这种实现单进程 world 里还能勉强工作，到了 distributed space 就会很快崩掉。

## 一、先说结论

Apollo 进入 BigWorld 模式后，必须明确拆开这两层：

- `GhostReplica`: 非权威节点上的实体投影
- `Witness`: 面向客户端的可见世界上下文

也就是说：

- `Ghost` 解决的是跨节点近场存在问题
- `Witness` 解决的是客户端可见集问题

这两个东西相关，但绝不是同一个概念。

## 二、KBE 里这两个对象的证据

从本地源码看：

- [witness.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/witness.h)
  - 持有 `viewRadius`
  - 持有 `ViewTrigger`
  - 持有 `viewEntities`
  - 提供 `onEnterView/onLeaveView`
  - 提供 `sendToClient`
  - 提供 `addUpdateToStream`
- [ghost_manager.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/ghost_manager.h)
  - 管理 `realEntities_`
  - 管理 `ghost_route_`
  - 管理待同步 `messages_`
  - 提供 route 和同步能力

这已经很清楚地说明：

- `Witness` 关注客户端视图
- `GhostManager` 关注实体投影和消息接力

这篇文档讨论的是 BigWorld 增强态下的运行时对象，
不是当前 `apps/cell-app` 里已经完整落地的能力。

## 三、为什么单纯 AOI 不够

普通 MMO world 里，AOI 常常可以理解成：

- 查询附近实体
- 广播进入视野
- 广播离开视野

但 distributed space 里还有两层额外问题：

### 问题 1：附近实体可能不在本节点

某个玩家在 partition A 的边界附近，另一个实体在 partition B。

这时只查本地实体集是不够的。

### 问题 2：客户端可见集不能直接等于“本节点实体表”

客户端看到的世界应该是一个连续的、稳定的视图。

这个视图可能同时包含：

- 本节点的 real entity
- 邻接节点同步过来的 ghost replica

如果没有 `Witness` 收口，客户端视图会非常不稳定。

## 四、`GhostReplica` 的定位

`GhostReplica` 是非权威节点上的实体投影。

它存在的目的不是替代 real entity，而是：

- 让边界附近的观察关系不断链
- 让相邻 partition 能看到近场对象
- 给迁移窗口提供临时接力

### `GhostReplica` 应该持有什么

- `entityId`
- `sourceCellId`
- `authorityRole = Ghost`
- 位置/朝向快照
- 关键同步属性
- 路由版本

### `GhostReplica` 不应该做什么

- 不应该做最终权威判定
- 不应该独立决定战斗结果
- 不应该写入长期实体状态

## 五、`Witness` 的定位

`Witness` 是客户端视角上下文。

它不是：

- 实体本身

也不是：

- 单纯附近查询算法

它更准确的角色是：

- 客户端“我现在能看见什么”的运行时容器

### `Witness` 应该持有什么

- 当前 view radius
- 当前可见实体集合
- 进入视图和离开视图状态
- 可见实体 alias / 压缩 ID
- 发包缓冲
- 当前 route version

### `Witness` 应该负责什么

- 判定 enter view / leave view
- 决定给客户端发哪些实体
- 决定是发 real 数据还是 ghost 数据
- 聚合多个来源的可见集

## 六、两者的关系

建议明确成下面这条关系：

```text
RealEntity / GhostReplica
    -> 参与 AOI / replication source
Witness
    -> 面向客户端挑选和组织可见集
Gateway
    -> 负责真正把数据发到客户端
```

也就是说：

- `GhostReplica` 是视图来源的一部分
- `Witness` 是视图组织者
- `Gateway` 是网络出口

## 七、推荐对象关系

建议 Apollo 后续形成下面这组对象：

```text
ReplicationService
    ├── WitnessContext
    ├── ViewTrigger
    ├── GhostReplica
    ├── GhostRouteTable
    └── AliasAllocator
```

### `WitnessContext`

职责：

- 维护客户端当前可见集
- 跟踪 enter/leave 状态
- 聚合发包

### `ViewTrigger`

职责：

- 处理空间边界事件
- 通知 witness 某个对象进入或离开观察范围

### `GhostRouteTable`

职责：

- 记录某个实体当前 ghost 路由
- 在迁移窗口中兜底消息转发

### `AliasAllocator`

职责：

- 给当前视图内实体分配短 alias
- 降低同步包大小

## 八、推荐客户端视图流程

### 标准 view update 流程

1. `AOI/Trigger` 发现对象接近可见边界
2. `WitnessContext` 计算该对象是否应进入视图
3. 若对象来自本节点 real，则登记 real source
4. 若对象来自邻接节点 ghost，则登记 ghost source
5. `WitnessContext` 更新 visible set
6. `ReplicationService` 组装增量包
7. 通过 `Gateway` 发给客户端

### 关键点

客户端看到的是：

- 由 `WitnessContext` 决定后的可见世界

而不是：

- 本地实体表原样输出

## 九、推荐边界观察流程

### 场景

玩家 A 在 partition 1 的边界附近。

玩家 B 在 partition 2，距离很近。

### 推荐流程

1. partition 2 上的 real entity 为 B
2. partition 1 为 B 建立 `GhostReplica`
3. A 的 `Witness` 可以从 partition 1 直接看到 B 的 ghost
4. 若 B 真正跨边界迁移，ghost 再参与过渡

### 为什么不能只临时查远端

因为实时每帧远程查找：

- 延迟高
- 难控
- 顺序不稳定

`GhostReplica` 的意义就是把边界附近近场数据提前拉到本地。

## 十、推荐迁移窗口里的 ghost 路由

迁移窗口最容易出问题的是：

- 旧权威刚切走
- 新权威刚建立
- 某些消息还在路上

这时需要一层显式 ghost 路由表。

### 推荐流程

1. 源 cell 为迁移实体写入 `GhostRoute`
2. 延迟到达的消息先查 route
3. 若 real entity 已不在本地，则按 route 转发到目标 cell
4. 目标 cell 接住后交给新的 real entity
5. 迁移窗口结束后清理 route

### 为什么这层必须显式存在

因为迁移不是原子切换。

没有 route table，就只能赌消息刚好不乱序。

## 十一、推荐 `WitnessContext` 状态

建议至少维护下面这些状态：

- `Building`
- `Active`
- `Refreshing`
- `Suspended`
- `Closed`

### `Building`

- 正在初始化客户端视图

### `Active`

- 正常提供视图增量

### `Refreshing`

- 切图或 route 变化后重建视图

### `Suspended`

- 断线后短时暂停

### `Closed`

- 彻底关闭并回收

## 十二、推荐 `GhostReplica` 状态

建议至少维护：

- `Shadowing`
- `Bridging`
- `Expiring`
- `Closed`

### `Shadowing`

- 正常做边界近场投影

### `Bridging`

- 正处于迁移窗口，参与消息接力

### `Expiring`

- 权威已经远离，准备回收

### `Closed`

- 已回收

## 十三、推荐接口方向

### `ReplicationService`

- `update_witness_context`
- `build_visibility_delta`
- `flush_to_gateway`

### `GhostManager`

- `create_ghost_replica`
- `update_ghost_replica`
- `install_ghost_route`
- `resolve_ghost_route`
- `retire_ghost_replica`

### `WitnessContext`

- `enter_view`
- `leave_view`
- `refresh_view`
- `suspend_view`
- `close_view`

## 十四、Apollo 中建议新增的抽象

建议未来新增：

- `modules/game/world/include/apollo/game/world/witness_context.hpp`
- `modules/game/world/include/apollo/game/world/view_trigger.hpp`
- `modules/game/world/include/apollo/game/world/ghost_replica.hpp`
- `modules/game/world/include/apollo/game/world/ghost_route_table.hpp`
- `modules/game/world/include/apollo/game/world/replication_service.hpp`
- `modules/game/world/include/apollo/game/world/alias_allocator.hpp`

## 十五、和当前 Apollo 的关系

Apollo 当前还没实现真正 distributed space，所以这层先不要急着全量落代码。

更稳的做法是：

### 当前阶段

- 先把 `WorldSession` 和 `WorldHost` 跑顺
- 单进程 world 先把客户端视图语义抽出来

### 下一阶段

- 补 `WitnessContext`
- 让客户端视图不再直接依赖本地 AOI 广播

### 再下一阶段

- 补 `GhostReplica`
- 补 `GhostRouteTable`
- 进入真正的 distributed space

## 十六、结论

Apollo 如果要真正进入 BigWorld 模式，`Witness` 和 `Ghost` 一定要拆开理解：

- `Ghost` 解决“实体在非权威节点如何存在”
- `Witness` 解决“客户端到底看见什么”

只有这两层分开，分布式 AOI、边界观察、迁移不断链和客户端同步才会真正稳定。

否则系统很容易退化成：

- 一个能跨节点查附近对象

但无法长期稳定运行的大世界原型。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
