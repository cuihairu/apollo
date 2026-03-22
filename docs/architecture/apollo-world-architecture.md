---
title: Apollo 世界架构路线
icon: map
order: 4
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - BigWorld
  - 架构演进
---

# Apollo 世界架构路线

这篇文档回答的是 Apollo 目前最关键的一个架构问题：

`Apollo 到底应该先做普通 MMO，还是一步走到 BigWorld/KBE 风格？`

结论先说：

Apollo 不应该默认全量 BigWorld 化，而应该设计成一套可裁剪、可渐进升级的世界架构。

也就是说：

- 默认形态是普通 MMO 架构
- 当项目需要更强的玩家锚点时，再引入 `BaseApp(PlayerAnchor Host)`
- 当项目需要连续大世界时，再引入 `CellApp(Distributed World Node)`

这比一开始就把所有项目都套进 `BaseApp / CellApp / BaseAppMgr / CellAppMgr / Machine` 更稳。

如果要和当前仓库目录名对应，先统一按下面理解：

- `apps/base-app`
  当前不是最终语义上的 `BaseApp`，只是偏数据服务原型
- `apps/cell-app`
  当前先按 `world-app / WorldHost` 装配壳理解

## 一、Apollo 现在真正缺的不是进程数量，而是架构收口

Apollo 现在的问题，不是“服务拆得不够多”，而是很多关键能力还没有形成统一骨架：

- 统一进程宿主
- 统一 world tick
- 统一实体定义
- 统一内部消息模型
- 统一会话锚点
- 统一空间运行时
- 统一观测与控制面

如果这些没有收住，单纯增加更多 `app` 只会让系统更散。

所以 Apollo 的目标不应该是“先把 KBE 里的进程名凑齐”，而应该是“先把每一层该承担的语义收住”。

## 二、Apollo 应该明确区分两种模式

Apollo 最好把世界架构从一开始就定义成两种运行模式。

### 1. 普通 MMO 模式

这是默认模式。

适合：

- 分线世界
- 房间制地图
- 副本制玩法
- 切图允许 loading
- 地图实例固定绑定单逻辑进程

特点：

- 不启用 `CellApp(Distributed World Node)`
- 不启用 `CellAppMgr`
- 不做 ghost / witness / cross-cell authority
- 一个地图实例通常只归一个 world process

### 2. BigWorld 模式

这是增强模式。

适合：

- 连续大世界
- 热点区域独立扩容
- 空间切片后无缝迁移
- 跨区仍需保留观察关系
- 需要 ghost / witness / authority transfer

特点：

- 启用 `BaseApp(PlayerAnchor Host)`
- 启用 `CellApp(Distributed World Node)`
- 启用 `BaseAppMgr`
- 启用 `CellAppMgr`
- 启用 distributed space 相关运行时

## 三、两种模式应该共用同一套底座

Apollo 不能做成两套完全不同的代码路径，否则后面维护成本会爆炸。

普通 MMO 模式和 BigWorld 模式，应该共享以下基础设施：

- `Base`
- `Core`
- `Runtime`
- `Net`
- `Data`
- `Game` 的基础实体能力
- 配置、日志、生命周期
- 统一消息编解码
- 统一 watcher / registry

也就是说：

`BigWorld 模式 = 普通 MMO 模式 + 分布式空间增强层`

而不是重新发明另一套框架。

## 四、Apollo 的推荐分层

结合现有仓库结构，更合理的分层建议如下。

### Level 1: Base

目录建议：

- `modules/base`

职责：

- 时间
- 内存
- 对象池
- 线程池
- 基础容器与工具

这一层保持纯基础设施，不引入游戏语义。

### Level 2: Core

目录建议：

- `modules/core`

职责：

- 配置
- 日志
- 生命周期
- 事件总线
- 定时器
- 服务发现接口

这一层是统一框架内核。

### Level 3: Runtime

目录建议：

- `modules/runtime`

职责：

- `ApplicationHost`
- `ProcessHost`
- `WorldHost`
- 信号处理
- shutdown 流程
- 健康检查
- 控制台/诊断入口

这一层是 Apollo 现在最应该补强的一层。

### Level 4: Transport / Messaging

目录建议：

- `modules/net`
- 或未来补一个 `modules/message`

职责：

- 外部连接
- 内部连接
- 消息 envelope
- bundle / batch send
- 组件间消息注册
- 内外网连接语义区分

这一层不能只停留在通用 TCP/HTTP，要向 KBE 的内部组件消息模型靠近。

### Level 5: Entity Runtime

目录建议：

- `modules/game/core`

职责：

- 实体基类
- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `RemoteEntityRef`
- 实体容器
- world tick 接口

这层是普通 MMO 和 BigWorld 模式都要共用的核心。

### Level 6: MMO Session Layer

目录建议：

- `modules/game/session`
- 或 `modules/world/base`

职责：

- 玩家会话锚点
- 登录态与在线态
- 玩家路由
- `Proxy/Session`
- Base 风格的长期状态归属

这一层在普通 MMO 模式里也有价值，不必等到完整 BigWorld 才有。

### Level 7: Space Runtime

目录建议：

- `modules/game/world`

职责：

- `WorldSpace`
- `MapInstance`
- `CoordinateSystem`
- `AOI`
- `Witness`
- `Replication`

这层普通 MMO 模式先只需要单进程版本。

### Level 8: Distributed Space Extension

目录建议：

- `modules/bigworld`
- 或未来拆出 `modules/world/distributed`

职责：

- `CellApp(Distributed World Node)`
- `GhostReplica`
- authority transfer
- cross-cell teleport
- distributed space routing
- `CellAppMgr`

这是 BigWorld 模式才启用的增强层。

### Level 9: Control Plane

目录建议：

- `modules/runtime`
- `modules/core/discovery`
- `modules/ops`

职责：

- component registry
- load report
- watcher tree
- 管理 API
- space query
- app query

这层对两种模式都需要，只是 BigWorld 模式会更重。

### Level 10: Apps

目录建议：

- `apps/*`

职责：

- 最终可执行进程
- 只做装配，不承载底层框架逻辑

## 五、普通 MMO 模式的推荐部署形态

Apollo 默认应该先把这套跑顺。

### 核心进程

- `login-app`
- `gateway-app`
- `world-app`
  在当前仓库里可先由 `apps/cell-app` 承担其装配壳
- `db-app / PersistenceService`
  或短期先以内嵌数据模块存在

### 语义分工

#### `login-app`

负责：

- 账号认证
- 角色选择
- 分配世界入口

#### `gateway-app`

负责：

- 连接承载
- 协议握手
- 限流与连接管理
- 转发到玩家绑定的 world/session 节点

#### `world-app`

负责：

- 地图实例
- 战斗与移动
- 副本逻辑
- AOI
- 地图内实体权威

#### `db-app / PersistenceService`

负责：

- 数据持久化
- 缓存
- 异步存取

### 这个模式的特点

- 足够支撑大多数 MMORPG
- 架构清晰
- 调试成本低
- 不需要 ghost / witness / cross-cell migration

### 对 Apollo 当前阶段的意义

Apollo 先把这一层做扎实，比急着补全 `CellApp(Distributed World Node)` 更重要。

## 六、进阶模式：引入 `BaseApp(PlayerAnchor Host)`，但仍不启用 `CellApp`

这是 Apollo 很适合采用的过渡阶段。

### 为什么需要这一步

因为很多项目虽然不需要分布式空间，但会逐渐需要：

- 玩家长期在线锚点
- 登录后稳定会话归属
- 跨地图、跨副本时保持玩家状态归属
- 非地图逻辑和地图逻辑拆分

这时可以先引入：

- `BaseApp(PlayerAnchor Host)`
- `BaseAppMgr`

但地图逻辑仍然放在单进程 `world-app` 或单实例世界服里。

### 这一步解决什么问题

- 把 socket/session 和玩家状态分离
- 把长期玩家归属从地图实例里抽出来
- 让地图切换、实例切换更稳定
- 为后续 `CellApp(Distributed World Node)` 打基础

### 这一步不解决什么问题

- 不解决连续大世界
- 不解决跨 cell 迁移
- 不解决 ghost / witness

它只是补“玩家锚点”，不是补“分布式空间”。

## 七、BigWorld 模式：只在明确需要大世界时开启

当项目满足下面条件时，再进入 BigWorld 模式：

- 单地图实例不能绑定单进程
- 热点区需要独立扩容
- 玩家跨区域不希望切图
- 空间分片后仍要保持观察关系
- 场景逻辑必须分布式运行

这时再引入：

- `CellApp(Distributed World Node)`
- `CellAppMgr`
- `Ghost`
- `Witness`
- authority transfer
- distributed AOI

### 为什么不该提前做

因为这套系统的复杂度非常高：

- 进程更多
- 权威边界更复杂
- 消息链路更长
- 调试成本更高
- 容错要求更严

如果游戏本身不需要连续大世界，这套复杂度基本就是负资产。

## 八、Apollo 推荐的世界对象模型

不管是哪种模式，Apollo 都应该尽快把核心世界对象模型定下来。

### 1. `Session`

语义：

- 连接与玩家在线态
- 公网连接锚点
- 协议与身份绑定

它不应该直接等于游戏实体。

### 2. `PlayerAnchor`

语义：

- 玩家长期归属
- 跨地图不变
- 承接社交、任务、邮件、背包等长期状态

这可以类比 KBE 里的 `BaseApp + Proxy` 组合语义。

### 3. `WorldSpace`

语义：

- 场景/地图运行时对象
- 空间内实体容器
- AOI 与副本逻辑

普通 MMO 模式下，它可以只绑定一个进程。

### 4. `AvatarEntity`

语义：

- 玩家在空间内的实体表现
- 位置、朝向、战斗
- 可被 AOI 和 replication 管理

### 5. `RemoteEntityRef`

语义：

- 跨进程实体引用
- 为未来 BigWorld 模式提前打基础

即使普通 MMO 模式里暂时不大量使用，也建议底层提前设计出来。

## 九、Apollo 推荐的管理面

Apollo 早晚都要补控制面，不如早点纳入设计。

建议至少统一：

- `ComponentRegistry`
- `WatcherTree`
- `AppLoadReport`
- `SpaceQuery`
- `SessionQuery`

### 最少需要暴露的内容

- 当前 app 类型和实例 ID
- 玩家数、实体数、连接数
- tick load
- 消息吞吐
- space 列表
- 某个玩家当前归属

这类信息对两种模式都同样重要。

## 十、Apollo 从当前仓库到目标架构的演进建议

### 阶段 1：收口进程宿主

先做：

- 统一 `ApplicationHost`
- 统一 app 初始化/关闭流程
- 统一健康检查与配置加载

不要再让每个 app 自己长宿主逻辑。

### 阶段 2：收口 world runtime

先做：

- `WorldHost`
- world tick
- 实体容器
- 基础 AOI

这个阶段先只支撑普通 MMO 模式。

### 阶段 3：收口实体定义

先做：

- `EntitySchema`
- property/method 描述
- replication 描述
- persistence 描述

这一步非常关键。

### 阶段 4：补会话锚点

先做：

- `Session`
- `PlayerAnchor`
- `BaseApp(PlayerAnchor Host)` 风格的玩家归属服务

这样普通 MMO 模式和未来 BigWorld 模式就开始统一了。

### 阶段 5：补控制面

先做：

- watcher
- registry
- load report
- world query

### 阶段 6：按需引入 `CellApp(Distributed World Node)`

只有真的需要时，再做：

- distributed world space
- ghost
- witness
- migration
- authority transfer

## 十一、Apollo 最终应该是什么样

Apollo 最终不应该被定义成“一个 BigWorld 兼容层项目”，而应该是：

`一个可运行普通 MMO，也可按需升级为 BigWorld/KBE 风格大世界的服务器框架`

这个定位更稳，也更符合实际项目需求。

### 换句话说

Apollo 最好满足三件事：

1. 普通 MMO 项目开箱可用
2. 架构上能够平滑升级到 Base/Cell 分权
3. 需要时再开启 BigWorld 式分布式空间

这才是最有价值的路线。

## 相关阅读

- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
