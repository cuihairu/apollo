---
title: AppMgr 设计
icon: sitemap
order: 15
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - BaseAppMgr
  - CellAppMgr
  - ControlPlane
---

# AppMgr 设计

这篇文档解决的是 BigWorld 模式里另一个经常被低估的问题：

`有了 BaseApp 和 CellApp 还不够，谁来做分配、调度、拓扑和控制面。`

在 KBE 里，这一层由：

- `BaseAppMgr`
- `CellAppMgr`

承担。

Apollo 如果后续进入真正的 BigWorld 模式，也必须把这层单独设计出来。

这篇文档默认接受下面这个前提：

- `BaseApp` 指 `PlayerAnchor / Proxy / Online Authority Host`
- `CellApp` 指 distributed world node
- 当前 `apps/cell-app` 还不是这里说的真正 `CellApp`

如果需要先统一这些术语，先看：

- [进程语义重定义](./process-semantics-redefinition.md)

## 一、先说结论

`AppMgr` 不是“服务发现列表”，而是 BigWorld 模式下的控制面核心。

它至少要解决：

- Base 节点分配
- Cell 节点分配
- 玩家入口路由
- 空间拓扑维护
- partition 到 cell 的映射
- 负载采集和迁移协调

也就是说：

- `BaseApp / CellApp` 是数据面
- `BaseAppMgr / CellAppMgr` 是控制面

## 二、KBE 里这层为什么重要

从本地源码看：

- [baseappmgr.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseappmgr/baseappmgr.h)
  - 有 `findFreeBaseapp`
  - 有 `updateBestBaseapp`
  - 有 `registerPendingAccountToBaseapp`
  - 有 `sendAllocatedBaseappAddr`
  - 有 `queryAppsLoads`
- [cellappmgr.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellappmgr/cellappmgr.h)
  - 有 `findFreeCellapp`
  - 有 `updateBestCellapp`
  - 有 `querySpaces`
  - 有 `updateSpaceData`
  - 有 `setSpaceViewer`

这说明在 KBE 里：

- `BaseAppMgr` 重点管理玩家入口和 base 侧负载
- `CellAppMgr` 重点管理空间拓扑和 cell 侧负载

这两者都不是简单的注册中心。

## 三、Apollo 为什么也需要这一层

一旦 Apollo 进入真正的 BigWorld 模式，就会出现这些问题：

- 玩家应该分到哪个 `BaseApp`
- 某个 `space` 该展开在哪些 `CellNode`
- 哪个 partition 当前最热
- 哪个 cell 已经过载
- 某次 authority transfer 由谁协调

如果没有显式 manager 层，这些逻辑就会散落到：

- `LoginApp`
- `BaseApp`
- `CellApp`
- `Gateway`

最后很难维护。

## 四、推荐分层

建议 Apollo 把 manager 层明确分成两块。

### 1. `BaseAppMgr`

职责：

- base 节点注册
- base 节点负载采集
- 玩家登录后的 base 分配
- 重复登录时目标 base 定位
- 玩家锚点迁移协调

### 2. `CellAppMgr`

职责：

- cell 节点注册
- cell 节点负载采集
- space 拓扑维护
- partition 到 cell 的调度
- authority transfer 协调
- space query / debug query

### 3. 可选公共层：`ComponentRegistry`

职责：

- 所有 app 基本注册
- 心跳
- 状态变化
- 统一 watcher query

这一层可以作为更通用的底层，

但：

- `BaseAppMgr`
- `CellAppMgr`

仍然需要保留各自的业务控制语义。

## 五、`BaseAppMgr` 应该管什么

### 1. base 节点负载

最少要跟踪：

- 当前玩家锚点数
- 当前在线玩家数
- 当前待保存数量
- 当前平均请求延迟
- 当前 load score

### 2. 玩家入口分配

登录成功后，至少要能回答：

- 这个玩家应该去哪个 `BaseApp`

### 3. 玩家归属查询

至少要能回答：

- 某个 `playerId` 当前归属于哪个 `BaseApp`
- 某个玩家是否已在线

### 4. base 迁移协调

后续如果某个 base 过载，需要支持：

- 玩家锚点迁移
- 路由更新
- 恢复重绑定

## 六、`CellAppMgr` 应该管什么

### 1. cell 节点负载

最少要跟踪：

- 当前 partition 数
- 当前实体数
- 当前 ghost 数
- 当前 witness 数
- tick load
- replication 吞吐

### 2. space 拓扑

最少要维护：

- 当前有哪些 `Space`
- 每个 `Space` 被切成哪些 `Partition`
- 每个 `Partition` 当前在哪个 `CellNode`

### 3. 迁移协调

至少要能看到：

- 当前有哪些 transfer 正在进行
- 哪些 partition 边界最热
- 哪些 cell 正在接管或释放 authority

### 4. 观测与调试

至少要支持：

- `query_spaces`
- `query_partitions`
- `query_cells`
- `query_transfers`

## 七、推荐对象关系

建议 Apollo 后续形成下面这组对象关系：

```text
ComponentRegistry
    ├── BaseAppMgr
    │     ├── BaseNodeTable
    │     ├── PlayerPlacementTable
    │     └── PendingLoginRegistry
    └── CellAppMgr
          ├── CellNodeTable
          ├── SpaceTopologyTable
          ├── PartitionPlacementTable
          └── TransferRegistry
```

### `PendingLoginRegistry`

职责：

- 保存登录激活中的短期挂起状态
- 避免重复分配
- 配合 `LoginApp` 完成 base 入口绑定

### `PlayerPlacementTable`

职责：

- `playerId -> baseAppId`
- 作为玩家锚点归属目录

### `SpaceTopologyTable`

职责：

- `spaceId -> partitions`
- `partition -> neighbors`
- `partition -> geometry/meta`

### `TransferRegistry`

职责：

- 记录迁移上下文
- 观察迁移状态
- 故障回滚

## 八、推荐接口方向

### `LoginApp -> BaseAppMgr`

- `allocate_base_for_player`
- `query_player_base`
- `register_pending_login`

### `BaseApp -> BaseAppMgr`

- `report_base_load`
- `report_player_anchor_count`
- `update_player_base_binding`

### `CellApp -> CellAppMgr`

- `report_cell_load`
- `report_space_partition_state`
- `report_transfer_state`

### `BaseApp / WorldHost -> CellAppMgr`

- `allocate_partition`
- `query_space_topology`
- `request_transfer_coordination`

## 九、推荐 Apollo 中的目录落点

建议未来新增：

- `modules/runtime/include/apollo/runtime/component_registry.hpp`
- `modules/runtime/include/apollo/runtime/load_report.hpp`
- `modules/bigworld/include/apollo/bigworld/base_app_manager.hpp`
- `modules/bigworld/include/apollo/bigworld/cell_app_manager.hpp`
- `modules/bigworld/include/apollo/bigworld/player_placement_table.hpp`
- `modules/bigworld/include/apollo/bigworld/space_topology_table.hpp`
- `modules/bigworld/include/apollo/bigworld/transfer_registry.hpp`

以及对应实现：

- `modules/bigworld/src/*`

## 十、和当前 Apollo 的关系

现在 Apollo 还没到要立刻实现完整 `AppMgr` 的阶段。

但应该先把边界写清楚：

### 当前阶段

- 继续用普通 MMO world runtime
- `BaseApp` 先收口玩家锚点
- `WorldHost` 先收口 world entry/transfer

### 下一阶段

- 把 manager 层抽象立出来
- 先做负载上报和查询
- 再做真正分配和拓扑控制

### 之后再进入

- `CellAppMgr`
- `BaseAppMgr`
- distributed space 调度

## 十一、为什么不能只靠服务发现

很多系统会误以为：

- 有个 `ServiceDiscovery` 就够了

这在普通微服务里可能成立，但在 BigWorld 模式里不够。

因为 manager 层关心的不是“服务在不在线”，而是：

- 当前负载是多少
- 玩家该落在哪
- partition 该展开在哪
- 迁移是否完成

这些都不是通用服务发现能直接回答的。

## 十二、近期最小可交付版本

建议 manager 层先做一个非常小的第一步。

### 最小范围

- 统一 `ComponentRegistry`
- `BaseApp` 上报负载
- `WorldHost`/未来 `CellApp` 上报负载
- 支持 `query_apps_loads`
- 支持 `query_spaces`

### 暂时不做

- 自动迁移
- 自动扩缩容
- 完整 partition 调度

### 为什么这样更稳

因为 Apollo 当前更缺的是：

- “能看到系统在跑什么”

而不是立刻做自动调度。

## 十三、结论

Apollo 如果后续进入 BigWorld 模式，`BaseAppMgr / CellAppMgr` 这一层必须独立出来。

它们的意义不是增加进程名，而是补齐控制面：

- `BaseAppMgr` 管玩家入口与 base 分配
- `CellAppMgr` 管空间拓扑与 cell 调度

只有这层立住，Apollo 才能从“有 distributed 数据面”继续升级成“有完整 control plane 的大世界框架”。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
