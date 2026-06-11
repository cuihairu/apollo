---
title: Apollo 最终蓝图
icon: sitemap
order: 7
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 蓝图
  - MMO
  - BigWorld
---

# Apollo 最终蓝图

这篇文档回答的不是某一个局部模块，而是整个 Apollo 最终应当收敛成什么样。

它要解决 4 个核心问题：

- Apollo 最终的整体形态是什么
- 普通 MMO 模式和 BigWorld 模式怎么共存
- 平台、运维、脚本、业务域如何协同
- 当前几十篇设计稿如何压成一张完整蓝图

## 一、设计目标

Apollo 的最终目标，不是做一套只能服务单一 MMO 形态的框架，而是做一套渐进式、可组装、可向上演进的在线游戏框架。

这套框架至少要同时满足：

- 轻在线项目可以只启最小集合
- 塔防和固定地图项目可以使用 Compact GameServer 低成本落地
- 轻量 MMO 可以以 Standard MMO Profile 先上线
- 普通 MMO 可以不依赖 BigWorld 能力直接落地
- 连续大世界项目可以在稳定基础上叠加分布式空间能力
- Lua 或 Python 脚本项目都能使用同一套业务抽象
- 平台能力、运维能力、游戏业务能力各自有清晰边界

## 二、参考来源

这份蓝图主要参考了三类来源：

- Apollo 当前仓库已有模块和 app 原型
- KBE/BigWorld 的世界分层、对象模型、分布式空间经验
- 现代模块化框架在宿主、装配、配置、可观测性方面的工程化思路

参考 KBE 的重点是：

- `Proxy / Base / Cell` 的职责拆分思想
- `Witness / Ghost / Authority Transfer` 的分布式世界经验
- 统一实体定义、远程调用和复制链路的必要性

不照搬的重点是：

- 不把 BigWorld 当默认起步形态
- 不把所有运行时都做成重对象脚本模型
- 不让 KBE 风格对象语义污染所有热点系统

## 三、最终总体形态

Apollo 最终应当由 4 个部分构成：

1. 一套稳定的 10 层代码分层
2. 一组可装配的游戏域组件
3. 一套可选的分布式世界增强层
4. 一套按模式装配的 app 与部署拓扑

可以压缩成下面这张图：

```text
                +----------------------------------+
                |       L10 Assembly / Topology    |
                | starter, apps, topology, profile |
                +----------------------------------+
                           | optional upscale
                +----------------------------------+
                | L9 Distributed World Extension   |
                | witness, ghost, authority, space |
                +----------------------------------+
                |      L8 Domain Components        |
                | login/session/world/battle/etc   |
                +----------------------------------+
                |       L7 Game Foundation         |
                | schema, remote call, script ABI  |
                +----------------------------------+
                |     L6 Platform Foundation       |
                | redis/sql/repository/queue       |
                +----------------------------------+
                |      L5 Runtime Ops Host         |
                | console, watcher, otel, control  |
                +----------------------------------+
                |        L4 Runtime Host           |
                | app host, process host, world    |
                +----------------------------------+
                | L3 Transport / Messaging / Proto |
                +----------------------------------+
                |         L2 Core Kernel           |
                +----------------------------------+
                |      L1 Base Infrastructure      |
                +----------------------------------+
```

## 四、为什么这样设计

### 1. 先把框架分成“稳定层”和“可选层”

Apollo 需要先有稳定的普通 MMO 底座，再考虑向上叠加大世界能力。

所以：

- `L1-L8` 是默认稳定层
- `L9` 是可选增强层
- `L10` 是最终装配层

### 2. 先把“运行时”与“运维治理”拆开

很多 C++ 项目会把这些混在一起，最后导致：

- 宿主逻辑里塞满 watcher 和 console
- app main 里塞满 pidfile 和 supervisor 适配
- 观测和远控接口到处散落

Apollo 明确把这两层拆开：

- `L4 Runtime Host` 负责程序如何运行
- `L5 Runtime Ops Host` 负责程序如何被观测、调试和治理

### 3. 先把“平台能力”与“游戏语义”拆开

Redis、SQL、排行榜、分布式锁、队列这些能力不是游戏业务本身，但又不是纯底层库。

如果没有独立的 `L6 Platform Foundation`，这些能力一定会：

- 混进 world、social、activity
- 变成 app 内部私有工具
- 最终失去复用和统一抽象

### 4. 保留 KBE 对象语义，但只保留在需要它的边界

Apollo 需要保留：

- `Session`
- `PlayerAnchor`
- `AvatarEntity`
- `Witness`
- `GhostReplica`

这些对象边界，因为它们解决的是在线身份、分布式路由和世界权威问题。

但 Apollo 不应该把战斗、属性、移动、AI 热路径也一并做成重对象调用模型。

所以 Apollo 的折中路线是：

- 外层对象语义
- 内层热点系统可数据导向

## 五、最终模式划分

Apollo 最终需要显式支持三种主 Profile。

## 六、Tower Defense Compact Profile

塔防 / 固定地图模式启用：

- `L1-L8`
- `L10` 中 Compact GameServer 对应的装配与部署拓扑

典型进程形态：

- `gateway-app`
- `compact-game-app`
- `persistence-app`
- 可选 `platform-app`

这里的重点是：

- 不需要 `CellApp / Partition / Witness / Ghost`
- 不需要把每张固定地图拆成独立分布式世界
- 需要稳定的 Scene Tick、Wave Runtime、Battle Runtime、快照和重连

典型主链应当是：

```text
Client
  -> GatewayApp(Session)
  -> CompactGameServer(SceneInstance + WaveRuntime + BattleRuntime)
  -> PersistenceService
```

## 七、Lightweight MMO / 普通 MMO Profile

普通 MMO 模式启用：

- `L1-L8`
- `L10` 中普通 MMO 对应的装配与部署拓扑

典型进程形态：

- `login-app`
- `gateway-app`
- `base-app`
  当前目录名保留，但目标语义应是 `BaseApp(PlayerAnchor Host)`
- `world-app` 语义的当前 `cell-app`
- `db-app / PersistenceService`
- 可选的平台类服务进程

这里的重点是：

- 可以没有 `Witness / Ghost / Authority Transfer`
- 可以没有分布式空间切片
- 但必须有完整在线主链

完整在线主链应当是：

```text
Client
  -> LoginApp
  -> GatewayApp(Session)
  -> BaseApp(PlayerAnchor)
  -> WorldApp(WorldSession + AvatarEntity)
```

## 八、BigWorld / Distributed MMO Profile

BigWorld 模式是在普通 MMO 模式稳定后叠加：

- `L9 Distributed World Extension`
- 更复杂的 `L10` 部署拓扑

典型新增能力：

- `Witness`
- `GhostReplica`
- `AuthorityTransfer`
- `DistributedSpace`
- `SpacePartition`
- `AppManager`

这里的重点不是“多几个进程名”，而是：

- 世界状态开始分布式承载
- 可见集与复制目标开始跨 partition 计算
- 玩家和实体权威开始发生迁移

## 九、最终对象模型

Apollo 最终必须把几个对象明确拆开。

### 1. `Session`

表示公网连接和接入态上下文。

负责：

- 连接
- 心跳
- 重连
- 接管
- 网关态临时信息

### 2. `PlayerAnchor`

表示玩家长期在线归属对象。

负责：

- 登录归属
- 当前 gateway 绑定
- 当前 world 归属
- 重连恢复
- 跨图稳定身份

### 3. `AvatarEntity`

表示玩家在世界中的表现体。

负责：

- 在 world 内参与移动、战斗、AOI
- 承担世界对象生命周期
- 与复制和可见集对接

### 4. `WitnessContext`

表示某个客户端的可见世界上下文。

负责：

- 可见对象集合
- 复制目标过滤
- 跨边界观察窗口

### 5. `GhostReplica`

表示非权威世界中的投影对象。

负责：

- 跨 partition 可见性和边界连续性
- 迁移窗口过渡

## 十、脚本层最终位置

脚本层的最终定位必须非常明确：

- 脚本层是统一 ABI
- Lua 和 Python 是可替换后端
- 一个项目通常二选一，不是全都同时启用

Apollo 的脚本设计应当是：

```text
Game Domain / Entity Hooks
        |
    Script ABI
     /      \
   Lua      Python
```

脚本层适合承接：

- 业务规则
- 配置驱动逻辑
- 任务、活动、AI 编排
- 对象生命周期钩子

脚本层不适合承接：

- 高频复制编码
- 热路径移动积分
- 大规模战斗批处理

## 十一、平台与运维的最终协同

Apollo 最终不能再把工具类和平台类能力散在各处。

### `L5 Runtime Ops Host` 负责

- console 完美打印
- debug watcher
- 远程命令
- pidfile
- systemd / supervisor adapter
- OTel exporter
- runtime diagnostics

### `L6 Platform Foundation` 负责

- Redis 适配
- MySQL / PostgreSQL / SQLite 抽象
- Repository / UnitOfWork
- leaderboard
- distributed lock
- cache / queue / rate limiter

这样分的直接好处是：

- 运维问题不再污染业务模块
- 平台抽象不再绑死在单个 app 内部

## 十二、按业务域的最终装配形态

Apollo 最终应当支持按业务域做可组装装配。

标准业务域建议至少包括：

- 登录与认证
- 网关与会话
- 世界与地图
- 战斗、技能、buff
- 社交、好友、工会、聊天
- 活动、任务、成就
- 匹配与实例编排
- 敏感词过滤
- 支付与平台业务

这些业务域不是默认都要拆成独立进程，而是：

- 先做成稳定组件
- 再按项目规模和负载做进程装配

## 十三、推荐的最终进程装配思路

Apollo 最终更合理的进程装配方式应该是“宿主 + 组件装配”，而不是“先定死一套巨大的进程全家桶”。

### 小型轻在线 / 塔防 Compact

- `gateway-app`
- `compact-game-app` 或 `service-app`
- 可选 `persistence-app`

### 轻量 MMO / 普通 MMO

- `login-app`
- `gateway-app`
- `base-app`
  目标语义是 `BaseApp(PlayerAnchor Host)`
- `world-app` 语义宿主
- 可选 `db-app / persistence-app`
- 可选 `platform-app`

### 大世界 MMO

- `login-app`
- `gateway-app`
- `base-app`
  目标语义是 `BaseApp(PlayerAnchor Host)`
- 多个 `cell/world` 宿主
- `app-manager`
- 可选独立 `db/platform` 服务

## 十四、优点

这套蓝图的主要优点是：

- 塔防和轻量 MMO 可以先落地，不被大世界复杂度绑架
- Compact GameServer 给固定地图玩法保留低运维成本形态
- BigWorld 能力可以后加，而不是先天压满
- KBE 的核心经验被保留，但历史包袱没有被整体复制
- 平台、运维、业务域、世界运行时边界清晰
- 脚本层能稳定承接 Lua 或 Python 项目
- 后续做工程化装配、配置、测试、监控都有明确落点

## 十五、代价与风险

这套方案也有明确代价。

### 1. 抽象层增多

从一个简单 world server 走到 10 层分层，学习和维护成本会明显增加。

### 2. 过渡期语义会有历史包袱

例如当前仓库里的：

- `apps/base-app`
- `apps/cell-app`

在一段时间内还会存在“目录名”和“目标语义”不完全一致的问题。

### 3. 对文档纪律要求高

如果后续设计稿不继续写清楚：

- 为什么这样设计
- 优点
- 代价
- 为什么不选别的方案

这套架构很快又会退化成结论堆砌。

## 十六、为什么不选其他方案

### 1. 为什么不直接照搬 KBE

因为 Apollo 的目标比 KBE 更宽，而且希望保留现代工程化和热点系统优化空间。

### 2. 为什么不做成纯 ECS 服务器

因为在线身份、跨进程路由、玩家锚点、分布式世界权威这些问题，纯 ECS 无法自然表达。

### 3. 为什么不把所有业务都做成独立微服务

因为 MMO 领域中很多状态仍然需要强时序、低延迟和世界内协同，过度微服务化会引入额外复杂度和一致性成本。

### 4. 为什么不把平台和运维都放进 core

因为这样会把底层内核持续污染成“大杂烩”，最终既不稳定也不可复用。

## 十七、最终落地判断标准

如果 Apollo 最终演进正确，应当至少满足下面这些判断标准：

### 基础判断

- 普通 MMO 模式可以独立运行
- 塔防 Compact 模式可以独立运行
- 世界主链、玩家主链、复制主链、观测主链都明确可落地
- Lua 或 Python 任一后端能稳定挂载

### 进阶判断

- 能按项目类型装配不同业务域组合
- 能按负载和模式切换不同拓扑
- 能在不推翻前 8 层的情况下叠加大世界增强层

## 十八、结论

Apollo 最终不应被定义成“某个固定 MMO 进程组合”，而应被定义成：

- 一套稳定分层
- 一组可组装业务域
- 一层可选分布式世界增强
- 一套按模式装配的宿主与拓扑体系

这套蓝图的核心价值在于：

- 塔防和轻量 MMO 先可用
- 大世界能力后可扩
- 参考 KBE，但不照搬 KBE
- 保留对象模型，也保留热点系统的数据导向空间

## 相关阅读

- [架构概述](./overview.md)
- [轻量 MMO 与塔防适配判断](./lightweight-mmo-and-tower-defense-fit.md)
- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [Apollo 分层设计](./apollo-layering-design.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [KBE 参考原则与 ECS 兼容取舍](./kbe-reference-principles.md)
- [Base Cell Proxy 对象模型](./base-cell-proxy-model.md)
- [WorldHost 设计稿](./world-host-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [统一脚本层设计](./script-layer-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
