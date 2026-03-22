---
title: Apollo 重构路线图
icon: route
order: 6
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 重构
  - 路线图
  - MMO
---

# Apollo 重构路线图

这篇文档不是讲“理想架构长什么样”，而是直接回答：

`Apollo 从当前仓库状态，接下来应该先改什么，后改什么。`

目标不是一次性做完 BigWorld/KBE，而是按收益排序，把 Apollo 先收敛成一套稳定的普通 MMO 架构，再为 `BaseApp` 和 `CellApp` 预留升级路径。

进程层的重组基线见：

- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)

如果从产品形态上理解，这条路线也意味着：

- Apollo 要先成为一套渐进式在线游戏框架
- 再按项目类型装配成轻在线、房间制、普通 MMO、BigWorld MMO

## 一、先定三条原则

### 1. 先收口，再扩容

Apollo 当前最缺的是统一世界运行时，不是更多进程名。

所以优先级应该是：

- 先收口 `world runtime`
- 再收口 `player anchor`
- 最后才做 distributed space

### 2. 先改语义，再决定是否改名

当前仓库里：

- `apps/base-app` 的实现偏数据服务
- `apps/cell-app` 的实现更像单机 world 原型

这两个名字和当前实现已经有语义偏差。

但这不意味着第一步就必须改目录名。

更合理的顺序是：

- 先在文档和架构语义里校正
- 再根据演进结果决定是否改名

### 3. BigWorld 能力必须是增强层，不是默认层

Apollo 的默认目标应该是：

- 普通 MMO 模式可用

而不是：

- 默认要求所有项目都跑 `BaseApp + CellApp + Mgr`

## 二、阶段总览

建议把重构拆成 5 个阶段：

1. 收口普通 MMO 世界服
2. 收口玩家锚点与会话层
3. 收口控制面与运维面
4. 预留 distributed space 扩展点
5. 按需落真正的 BigWorld 模式

这 5 个阶段不是平均重要，前 3 个才是当前主线。

## 三、阶段 1：先把普通 MMO 世界服做对

这是最高优先级。

### 当前问题

当前 Apollo 已有：

- `modules/runtime/ApplicationHost`
- `modules/game/core/Entity`
- `modules/game/world/Scene`
- `apps/cell-app` 的 world 原型

但这些东西还没有被收成一个统一的世界宿主。

### 当前阶段目标

在这个阶段，Apollo 要先拥有一套真正可用的普通 MMO world runtime：

- 一个世界服可以承载地图实例
- 实体有稳定生命周期
- 世界更新由统一 tick 驱动
- AOI、场景、玩家进入离开流程统一

### 应新增的核心对象

建议落点：

- `modules/runtime/include/apollo/runtime/world_host.hpp`
- `modules/runtime/src/world_host.cpp`
- `modules/game/world/include/apollo/game/world/world_space.hpp`
- `modules/game/world/include/apollo/game/world/map_instance.hpp`
- `modules/game/world/include/apollo/game/world/world_session.hpp`

#### `WorldHost`

职责：

- 挂到 `ApplicationHost` 之上
- 持有 world services
- 负责 world tick
- 负责 scene/map instance 生命周期

#### `WorldSpace`

职责：

- 表示一个运行中的世界空间
- 管理 entity 容器
- 承接 AOI
- 暴露 update 接口

#### `MapInstance`

职责：

- 表示某个地图/副本实例
- 管理实例级配置和运行状态
- 管理进入/离开规则

#### `WorldSession`

职责：

- 表示玩家在世界服内的在线上下文
- 承接 session 到实体的绑定
- 不直接等于公网连接

### 当前 app 层怎么处理

在这个阶段，建议：

- `apps/cell-app` 在架构语义上按 `world-app` 理解
- 暂时不强制改目录名
- 先把它改造成 `WorldHost` 的装配壳

也就是说，这一阶段不是改名字，而是先改结构。

### 当前阶段不做什么

不要在这一阶段引入：

- `Ghost`
- `Witness`
- `CellAppMgr`
- authority transfer

因为这会打断主线。

## 四、阶段 2：补玩家锚点层

当普通 MMO 世界服收住之后，再做这一层。

### 当前问题

Apollo 当前还没有清晰对象代表：

- 玩家长期归属
- session 绑定
- 跨图稳定身份

当前更准确的判断应该是：

- `apps/base-app` 现在更像偏 `PersistenceService` 的原型
- 它还不是最终语义上的 `BaseApp(PlayerAnchor Host)`

### 当前阶段目标

把 Apollo 补到“有真正玩家锚点”的程度。

### 应新增的核心对象

建议落点：

- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`
- `modules/game/session/include/apollo/game/session/session_locator.hpp`
- `modules/game/session/include/apollo/game/session/world_assignment.hpp`

#### `PlayerAnchor`

职责：

- 玩家长期身份
- 当前在线状态
- 当前归属 gateway/session
- 当前归属 world/map

#### `AnchorManager`

职责：

- 创建和管理玩家锚点
- 登录后分配
- 重连后恢复

#### `SessionLocator`

职责：

- 定位某个玩家当前在哪个 session
- 定位某个 session 当前属于谁

#### `WorldAssignment`

职责：

- 玩家当前归属哪个 world
- 切图或切实例时如何重新分配

### `apps/base-app` 在这个阶段怎么处理

建议：

- 保留 `apps/base-app` 名字
- 但逐步把它从“数据服务原型”升级为“玩家锚点服务”

这意味着它要开始承接：

- `PlayerAnchor`
- 玩家登录后归属
- 玩家到 world 的分配

而不仅仅是：

- 数据加载与保存

### 当前阶段是否拆数据服务

不建议现在就拆。

更现实的做法是：

- 先在 `base-app` 内部补锚点语义
- 如果后续数据职责与锚点职责耦合过重，再拆独立 `db-app / persistence-app`

这一阶段的代码级设计稿见：

- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [LoginApp 收口设计](./login-app-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
- [Space Partition 与 Topology 设计](./space-partition-topology-design.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [EntitySchema 设计](./entity-schema-design.md)

## 五、阶段 3：补控制面与运维面

这个阶段应该和阶段 2 部分并行推进，但不要先于阶段 1。

### 当前问题

Apollo 虽然有 `service_discovery`，但还没有真正的：

- watcher tree
- app load report
- world query
- player/session query

### 当前阶段目标

让 Apollo 至少具备“能看见自己在跑什么”的能力。

### 应新增的核心对象

建议落点：

- `modules/core/include/apollo/core/watcher.hpp`
- `modules/core/include/apollo/core/watcher_registry.hpp`
- `modules/core/include/apollo/core/load_report.hpp`
- `modules/core/include/apollo/core/component_registry.hpp`

### 最少需要支持的查询

- 当前 app 类型
- 当前实例 ID
- 当前连接数
- 当前玩家数
- 当前实体数
- 当前 map/scene 列表
- 玩家当前 world 归属

### 当前阶段的 app 改造

至少要让：

- `login-app`
- `gateway-app`
- `base-app`
- 当前 `cell-app/world-app`

都接入同一套观测接口。

## 六、阶段 4：预留 distributed space 扩展点

这一阶段不是立即做 BigWorld，而是先把接口边界留出来。

### 为什么要做这一层

因为如果前 3 个阶段完全按“纯单机场景服”去写死，后面再补 `CellApp` 会很痛苦。

所以要提前留扩展点，但不立即实现全部能力。

### 应该先预留什么

建议在 `modules/game/core` 和 `modules/game/world` 提前留这些抽象：

- `RemoteEntityRef`
- `AuthorityRole`
- `ReplicationTarget`
- `SpacePartitionId`
- `TeleportContext`

### 为什么这些抽象重要

因为它们决定未来 world runtime 是否还能平滑升级成 distributed world。

### 这一阶段不要做什么

不要急着实现：

- 完整 `Ghost`
- 完整 `Witness`
- `CellAppMgr`

这里只做抽象边界和接口留口。

## 七、阶段 5：按需落真正的 BigWorld 模式

只有在项目明确需要连续大世界时，再进入这一阶段。

### 当前阶段目标

在已有 world runtime 之上，引入真正的 distributed space。

### 应新增的核心对象

建议落点：

- `modules/world/distributed/` 或未来明确模块

核心对象：

- `CellNode`
- `GhostReplica`
- `Witness`
- `AuthorityTransfer`
- `SpacePartition`
- `CellAppMgr`
- `BaseAppMgr`

### 这时再考虑的事情

- 当前 `cell-app` 是否恢复为真正的 `CellApp`
- 是否新增真正 `world-app`
- 是否拆独立 `db-app`

这些问题在此之前都可以先不动。

## 八、命名策略建议

当前阶段最容易吵起来的就是名字。

我建议按下面方式处理。

### 1. 短期不急着改目录名

短期建议：

- `apps/base-app` 保留
- `apps/cell-app` 保留

但在文档里明确：

- 当前 `base-app` 还不是完整 KBE `BaseApp`
- 当前 `cell-app` 当前按 `world-app` 原型理解

这样可以降低一次性改动成本。

### 2. 中期改语义，不急着改名字

等阶段 1、2 完成后：

- `base-app` 开始真正承接玩家锚点
- `cell-app` 若仍然只是 world server，就在文档中继续按 `world-app` 说明

### 3. 长期再决定是否改名

只有在以下情况出现时，再考虑真正改目录名：

- `base-app` 最终没有往玩家锚点方向演进
- `cell-app` 长期只是单机 world server
- 未来真的要新建一个真正分布式 `CellApp`

那时再做目录重命名，成本和收益才匹配。

## 九、建议的目录落点

为了避免后续继续散，建议按职责补目录，而不是继续把能力塞进 app 里。

### 推荐新增目录

- `modules/game/session/`
- `modules/game/world/runtime/` 或直接补到 `modules/game/world/`
- `modules/core/ops/` 或统一 watcher 相关目录

### 推荐先不要新增的目录

暂时不要急着新增：

- `modules/world/distributed/`
- `apps/baseappmgr`
- `apps/cellappmgr`

先把前三个阶段跑顺再说。

## 十、按文件级别的近期行动清单

### 第一优先级

- 为 `modules/runtime` 新增 `WorldHost`
- 为 `modules/game/world` 新增 `WorldSpace`
- 为 `modules/game/world` 新增 `MapInstance`
- 让当前 `apps/cell-app` 使用统一 world host

### 第二优先级

- 为 `modules/game/session` 新增 `PlayerAnchor`
- 为 `modules/game/session` 新增 `AnchorManager`
- 让 `apps/base-app` 开始持有玩家锚点而不仅是 DB 服务

### 第三优先级

- 为 `core/runtime` 增加 watcher / load report
- 让所有主要 app 暴露统一运行时指标
- 收口 starter / profile / bootstrap 装配体系
- 明确 `runtime-ops` 与 `platform` 两层模块边界
- 定义 `ModuleManifest / ModuleRegistry / DependencyGraph`
- 定义统一 `AppBootstrap` 生命周期阶段
- 定义 `HostBuilder / ServiceCollection / ServiceProvider`
- 定义统一 `Configuration / Profile / Override` 规则
- 定义 `Capability / Feature Flag` 词汇表和开关边界
- 定义分层、装配、模式、场景的验证策略
- 定义业务域协作的 `Command / Event / Query` 边界
- 定义 `Repository / UnitOfWork / Outbox` 的数据落地边界
- 定义 `Proxy / Anchor / Avatar` 的显式生命周期状态机
- 定义统一 `World Tick / Phase / Job` 调度模型
- 定义 `Interest / AOI / Witness / Replication` 的可见集主链
- 定义战斗运行时与 ECS/数据导向的边界
- 定义导航、移动、边界物理校验三层模型
- 定义 `Shard / Zone / Instance / Match / Partition` 统一术语
- 定义 `Watcher / Metrics / Trace / Runtime Query` 统一观测层
- 定义 session/world/platform 的分层故障恢复主线

### 第四优先级

- 为未来 distributed world 预留 remote entity / authority 抽象
- 收口统一 replication pipeline，避免各系统各自拼同步包

## 十一、一个更现实的里程碑定义

### 里程碑 A：普通 MMO 可用

满足条件：

- `login-app` 可登录
- `gateway-app` 可转发
- `world-app` 语义的当前 `cell-app` 可承载场景
- 玩家可以稳定进入、离开、切实例
- watcher 可观测

这时 Apollo 就已经有真实价值了。

### 里程碑 B：玩家锚点可用

满足条件：

- `base-app` 不只是数据服务
- 玩家有长期归属对象
- 切图和重连不再强依赖 world 进程本地状态

### 里程碑 C：BigWorld 增强层可选

满足条件：

- distributed space 抽象落地
- `CellApp` / `BaseAppMgr` / `CellAppMgr` 才真正开始出现

## 十二、结论

Apollo 现在最合理的路线不是“立刻补全 KBE 全家桶”，而是：

1. 先把当前 `cell-app` 收成真正的普通 MMO 世界服
2. 再把当前 `base-app` 收成真正的玩家锚点服务
3. 再补统一控制面
4. 最后按需升级到 BigWorld/KBE 风格

这个顺序能最大化复用现有代码，也能避免在还没有稳定 world runtime 时，就过早进入复杂的 distributed space 设计。

如果需要一篇把目标形态压缩成总图的文档，见：

- [Apollo 最终蓝图](./apollo-final-blueprint.md)

如果需要一篇更偏执行项的文档，见：

- [Apollo 实施清单](./apollo-implementation-checklist.md)

## 相关阅读

- [Apollo 最终蓝图](./apollo-final-blueprint.md)
- [Apollo 实施清单](./apollo-implementation-checklist.md)
- [WorldHost 设计稿](./world-host-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [LoginApp 收口设计](./login-app-design.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [Distributed Space 设计](./distributed-space-design.md)
- [AppMgr 设计](./app-manager-design.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)
- [Authority Transfer 设计](./authority-transfer-design.md)
- [Space Partition 与 Topology 设计](./space-partition-topology-design.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [模块目录重组设计](./module-reorganization-design.md)
- [Module Manifest 与 Registry 设计](./module-manifest-and-registry-design.md)
- [App Bootstrap 生命周期设计](./app-bootstrap-lifecycle-design.md)
- [Host Builder 与 DI 设计](./host-builder-and-di-design.md)
- [Configuration 与 Profile 设计](./configuration-and-profile-design.md)
- [Capability 与 Feature Flag 设计](./capability-and-feature-flag-design.md)
- [Testing 与 Verification 策略](./testing-and-verification-strategy.md)
- [Domain Event 与 Message Bus 设计](./domain-event-and-message-bus-design.md)
- [Persistence、Repository 与 Unit of Work 设计](./persistence-repository-unitofwork-design.md)
- [Entity Lifecycle 与 State Machine 设计](./entity-lifecycle-and-state-machine-design.md)
- [World Tick、Scheduling 与 Job Model 设计](./world-tick-scheduling-and-job-model.md)
- [Interest Management 与 AOI Pipeline 设计](./interest-management-and-aoi-pipeline-design.md)
- [Combat Runtime 与 ECS 边界设计](./combat-runtime-and-ecs-boundary-design.md)
- [Navigation、Movement 与 Physics Boundary 设计](./navigation-movement-and-physics-boundary-design.md)
- [Shard、Zone、Instance 与 Match Topology 设计](./shard-zone-instance-match-topology-design.md)
- [Observability、Watcher 与 Runtime Introspection 设计](./observability-watcher-and-runtime-introspection-design.md)
- [Reliability、Failover 与 Recovery 设计](./reliability-failover-and-recovery-design.md)
