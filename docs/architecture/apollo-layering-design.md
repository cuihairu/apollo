---
title: Apollo 分层设计
icon: layer-group
order: 2
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 分层
  - 模块化
  - 渐进式
---

# Apollo 分层设计

这篇文档专门回答一个基础问题：

`按照现在这套新思路，Apollo 的分层到底应该怎么定。`

这里要先明确一件事：

Apollo 的“分层”不能再只是按源码目录做静态罗列。

它至少要同时回答 3 个维度：

- 代码能力如何分层
- 业务组件如何分层
- 进程装配如何分层

如果只讲其中一个维度，后面一定会继续混：

- 模块层和进程层混在一起
- 业务域和底层框架混在一起
- 平台能力和游戏语义混在一起
- 普通 MMO 和 BigWorld 增强层混在一起

## 一、先说结论

Apollo 更合理的分层应该分成 10 层：

```text
L10 Assembly / Topology
L9 Distributed World Extension
L8 Domain Components
L7 Game Foundation
L6 Platform Foundation
L5 Runtime Ops Host
L4 Runtime Host
L3 Transport / Messaging / Protocol
L2 Core Kernel
L1 Base Infrastructure
```

其中最关键的原则是：

- 下层解决通用能力
- 中层解决平台和游戏基础语义
- 上层解决业务域和世界增强能力
- 最上层解决装配与部署

BigWorld 不是默认层，而是：

- 建立在前 8 层之上的增强层

## 二、为什么要这样分

Apollo 现在的新目标已经很明确了：

- 它不是单纯一套 MMO world server
- 也不是只做 KBE 兼容层
- 而是要做一套渐进式、可组装的在线游戏框架

这意味着分层必须满足 5 个目标：

### 1. 普通在线游戏能落地

不能一开始就要求项目必须进入 `BaseApp + CellApp + Mgr`。

### 2. 普通 MMO 能完整承载

必须能支持：

- 登录
- 网关
- 玩家锚点
- world runtime
- 任务、活动、社交等业务域

### 3. 大世界模式能增量叠加

也就是：

- `Ghost`
- `Witness`
- `Authority Transfer`
- `Distributed Space`

这些能力是增强层，不是底座层。

### 4. 脚本层能统一接入

不管项目选 `Lua` 还是 `Python`，都应该挂在稳定分层上，而不是散落在业务域内部。

### 5. 运维和平台能力有清晰落点

例如：

- pidfile
- systemd/supervisor
- OTel
- Redis
- MySQL/PostgreSQL/SQLite
- leaderboard
- distributed lock

这些能力不能继续混在 world、battle、social 里。

### 6. 进程语义不能继续错位

至少要明确：

- `BaseApp(PlayerAnchor Host)` 不是数据库服务
- `DBMgr / PersistenceService` 不是玩家在线主状态宿主
- 当前 `apps/cell-app` 只是 `world-app / WorldHost` 原型

## 三、10 层分层定义

## 四、L1：Base Infrastructure

目录建议：

- `modules/base`

职责：

- 时间
- 内存
- 线程池
- 对象池
- 基础容器
- 字符串和工具函数
- 终端输出基础能力

这一层的原则很简单：

- 不引入游戏语义
- 不引入进程语义
- 不引入脚本语义

它是 Apollo 最底的纯基础设施层。

## 五、L2：Core Kernel

目录建议：

- `modules/core`

职责：

- 配置
- 日志
- 生命周期
- 事件总线
- 定时器
- 模块描述
- 基础服务发现接口
- 错误模型

这一层解决的是“框架如何作为框架运行”，不是“游戏如何运行”。

### 这一层不应该做什么

- 不应该承接 world runtime
- 不应该承接业务域逻辑
- 不应该直接承接脚本业务类

## 六、L3：Transport / Messaging / Protocol

目录建议：

- `modules/net`
- 未来可补 `modules/message`
- 协议定义目录

职责：

- 外部协议
- 内部消息 envelope
- request/reply
- one-way event
- endpoint 与 route 基础能力
- 编解码与批量发送

这一层要明确分成 4 个子层：

### 1. Client Protocol

对客户端暴露的协议层。

### 2. Session Protocol

登录、心跳、重连、接管等会话协议层。

### 3. Internal Message Envelope

服务之间、实体之间的统一内部消息层。

### 4. Replication Protocol

客户端可见世界同步协议层。

### 为什么这一层必须独立出来

因为 Apollo 后面不管是：

- 普通在线游戏
- 普通 MMO
- BigWorld 模式

都会依赖统一协议边界。

## 七、L4：Runtime Host

目录建议：

- `modules/runtime`

职责：

- `ApplicationHost`
- `ProcessHost`
- `WorldHost`
- 运行循环
- 健康检查原语
- 启停流程原语

这一层只回答：

- 进程和宿主怎么运行

不负责：

- pidfile
- systemd/supervisor
- OTel 导出
- watcher/debug tree

这些应上移到专门的运维宿主层。

## 八、L5：Runtime Ops Host

目录建议：

- `modules/runtime/ops`

职责：

- console
- pidfile
- systemd/supervisor 交互
- health check
- watcher tree
- diagnostics
- profiler/exporter
- OTel tracing/metrics 接入
- 远程调试和远程控制
- 入口运维适配

这层解决的是：

- 进程运行治理
- 运维观测
- 调试控制

更完整设计见：

- [Runtime Ops Host 设计](./runtime-ops-host-design.md)

## 九、L6：Platform Foundation

目录建议：

- `modules/platform`
- `modules/data`

职责：

- Redis
- MySQL/PostgreSQL/SQLite 抽象
- repository base
- cache
- leaderboard
- distributed lock
- rate limiter
- queue

这层既不是纯驱动层，也不是具体游戏业务层。

它是：

- 平台基础能力层

需要补一层理解：

- `Redis / SQL / Queue / Lock` 的抽象属于这一层
- `DBMgr / PersistenceService` 是对这一层能力的装配使用者
- 因此 `Platform Foundation` 不等于 `DBMgr`

更完整设计见：

- [Platform Foundation 设计](./platform-foundation-design.md)

## 十、L7：Game Foundation

目录建议：

- `modules/game/core`
- `modules/script`
- 未来的 schema / replication / remote call 模块

这是 Apollo 新分层里非常关键的一层。

职责：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `PersistenceSchema`
- `RemoteEntityRef`
- `RemoteEntityCall`
- `Replication Pipeline`
- 统一脚本层 `Script ABI`

### 为什么这些必须放在同一层

因为它们共同解决的是：

- 游戏对象如何被定义
- 如何被调用
- 如何被复制
- 如何被脚本扩展

这层既不是纯底层基础设施，也还不是具体业务域。

它是：

- 游戏框架基础语义层

## 十一、L8：Domain Components

目录建议：

- `modules/game/session`
- `modules/game/world`
- `modules/game/battle`
- 未来的 `modules/game/social`
- 未来的 `modules/game/task`
- 未来的 `modules/game/activity`
- 未来的 `modules/game/platform-business`

这一层是 Apollo 最需要“可组装”的层。

职责不是一种，而是一组标准业务域组件。

### 推荐拆成三大类

### 1. 世界与在线域

包括：

- 登录
- 网关接入
- 会话
- 玩家锚点
- world runtime
- map instance

### 2. 通用业务域

包括：

- 战斗
- 技能
- buff
- 社交
- 工会
- 好友
- 任务
- 成就
- 活动
- 匹配

### 3. 平台业务域

包括：

- 支付业务接入
- 风控业务接入
- 运营业务编排

### 这一层的关键原则

- 业务域是组件，不默认等于独立进程
- 业务域可以按项目类型装配

## 十二、L9：Distributed World Extension

目录建议：

- `modules/bigworld`
- 或未来独立 `modules/world/distributed`

这是 BigWorld 模式对应的增强层。

职责：

- `GhostReplica`
- `Witness`
- authority transfer
- distributed space
- `CellAppMgr`
- `BaseAppMgr`
- space partition / topology

### 为什么它必须单独成层

因为这层复杂度非常高，而且不是所有项目都需要。

Apollo 的关键原则应该是：

`L9 = 可选增强层，不是默认起步层`

## 十三、L10：Assembly / Topology

目录建议：

- `modules/starter`
- `apps/*`
- 部署清单和配置

这一层负责：

- 模块装配
- 宿主装配
- 最终进程装配
- 环境配置
- 部署拓扑

### 这一层解决的问题

不是“功能是什么”，而是：

- 哪些层被启用
- 哪些组件被装配
- 最终拆成哪些 app

## 十四、这 10 层和旧分层的关键区别

和之前更偏“模块目录清单”的分层相比，新分层有 7 个重要变化。

### 1. 不再把 `apps` 和框架层混看

`apps` 是装配结果，不是核心框架层。

### 2. 把 `Runtime Host` 和 `Runtime Ops Host` 拆开

这样：

- 宿主运行
- 运维治理与调试

不会继续混在一起。

### 3. 单独抽出了 `Platform Foundation`

这样：

- Redis
- SQL
- leaderboard
- distributed lock

这批能力有了稳定落点。

### 4. 单独抽出了 `Game Foundation`

把：

- schema
- remote call
- replication
- script ABI

收成统一基础语义层。

### 5. 单独抽出了 `Domain Components`

这样聊天、任务、活动、社交、战斗这些能力才有清晰归属。

### 6. BigWorld 变成增强层

不再默认压在整个架构中间。

### 7. 明确区分“模块层”和“拓扑层”

这对后续做渐进式装配非常重要。

## 十五、依赖规则

建议 Apollo 明确以下依赖方向：

```text
L1 -> 无依赖
L2 -> 依赖 L1
L3 -> 依赖 L1-L2
L4 -> 依赖 L1-L3
L5 -> 依赖 L1-L4
L6 -> 依赖 L1-L5
L7 -> 依赖 L1-L6
L8 -> 依赖 L1-L7
L9 -> 依赖 L1-L8
L10 -> 依赖 L1-L9
```

### 关键硬约束

- `L2` 不反向依赖 `L6/L7/L8`
- `L3` 不直接承载业务域逻辑
- `L5` 不承接存储和平台语义
- `L6` 不依赖具体业务域实现
- `L9` 不能反向污染 `L7`
- `L10` 只做装配，不承载核心业务能力

## 十六、普通 MMO 和 BigWorld 模式如何映射

### 普通 MMO 模式

启用：

- `L1-L8`

不启用或仅最小启用：

- `L9`

### BigWorld 模式

启用：

- `L1-L10`

但注意：

- `L9` 是在 `L1-L8` 已稳定的前提下叠加

## 十七、对应到当前 Apollo 仓库的调整建议

基于当前仓库状态，更现实的调整方向应该是：

### 优先收口

- `modules/runtime`
- `modules/runtime/ops`
- `modules/platform`
- `modules/game/core`
- `modules/game/world`
- `modules/script`

### 逐步补齐

- `modules/game/session`
- `modules/game/battle`
- `modules/game/social`
- `modules/game/task`
- `modules/game/activity`

### 暂时不急着做大规模目录重命名

但文档语义上要先校正：

- 当前 `apps/base-app` 的实现偏数据服务
- 目标语义应演进为玩家锚点宿主
- 当前 `apps/cell-app` 更像 world runtime 原型
- `DBMgr / PersistenceService` 应单独理解为持久化执行层

## 十八、结论

Apollo 现在最需要的不是继续追加更多进程名，而是先把分层本身设计清楚。

更合理的分层应该是：

- 先分清底层框架
- 再分清运行治理和平台能力
- 再分清游戏基础语义
- 再分清业务域组件
- 最后再讨论 BigWorld 增强层和进程装配

这样 Apollo 才能同时满足：

- 轻在线项目可用
- 普通 MMO 可用
- BigWorld 模式可增量升级

## 相关阅读

- [架构概述](./overview.md)
- [进程语义重定义](./process-semantics-redefinition.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [统一脚本层设计](./script-layer-design.md)
- [EntitySchema 设计](./entity-schema-design.md)
- [RemoteEntityCall 设计](./remote-entity-call-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
