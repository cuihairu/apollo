---
title: 架构概述
icon: architecture
order: 1
category:
  - 架构
tag:
  - 架构
  - 概述
---

# Apollo 架构概述

## 整体架构

Apollo 现在更合理的理解方式，不是旧的“按模块目录罗列层级”，而是：

- 一套渐进式的代码分层
- 一组可装配的业务域组件
- 一层可选的 BigWorld 分布式增强能力
- 最后才是进程和部署拓扑

按新思路，Apollo 更推荐采用 10 层分层：

```
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

## 层级说明

### L1: Base Infrastructure

纯基础设施，无框架语义。

| 组件 | 功能 |
|------|------|
| Time | 时间工具、高精度计时 |
| ThreadPool | 线程池、任务调度 |
| IdPool | 唯一ID生成器 |
| ObjectPool | 对象池、内存复用 |
| String | 字符串处理工具 |
| Memory | 内存池、Buffer管理 |
| Terminal | 终端适配、ANSI彩色输出 |

### L2: Core Kernel

框架内核，公共运行语义。

| 组件 | 功能 |
|------|------|
| Config | 配置管理、热更新 |
| Log | 日志系统、分级输出 |
| Lifecycle | 应用生命周期管理 |
| Event | 事件总线 |
| Timer | 定时器调度 |
| Module | 模块注册与加载 |
| ServiceDiscovery | 服务发现 |

### L3: Transport / Messaging / Protocol

统一协议与消息边界。

| 子层 | 功能 |
|------|------|
| Client Protocol | 客户端请求/响应协议 |
| Session Protocol | 登录、心跳、重连、接管 |
| Internal Envelope | 进程间和实体间统一消息信封 |
| Replication Protocol | 实体同步、可见集更新 |

### L4: Runtime Host

宿主运行时，程序如何运行。

| 组件 | 功能 |
|------|------|
| ApplicationHost | 应用宿主 |
| ProcessHost | 进程宿主 |
| WorldHost | 世界宿主 |
| ServiceHost | 服务宿主 |
| Signal | 信号处理 |
| ShutdownHook | 关闭钩子 |

### L5: Runtime Ops Host

运行时治理、运维与调试层。

| 组件 | 功能 |
|------|------|
| Console | 本地控制台 |
| ProcessGuardian | pidfile、单实例、shutdown 协调 |
| SupervisorAdapter | systemd/supervisor 交互 |
| Watcher / Diagnostics | 观测、调试、状态查询 |
| TelemetryBridge | OTel tracing/metrics 导出 |
| Remote Control | 远程调试、远程控制 |

### L6: Platform Foundation

平台基础能力层。

| 组件 | 功能 |
|------|------|
| Redis / KvStore | 键值存储与缓存基础能力 |
| RelationalStore | MySQL/PostgreSQL/SQLite 统一访问 |
| RepositoryBase | 统一数据访问抽象 |
| Leaderboard | 排行榜组件 |
| DistributedLock | 分布式锁 |
| Queue / RateLimit | 通用平台组件 |

### L7: Game Foundation

游戏框架基础语义层。

| 组件 | 功能 |
|------|------|
| EntitySchema | 统一实体定义 |
| RemoteEntityCall | 统一远程实体调用 |
| Replication Pipeline | 统一复制链路 |
| Script ABI | 统一脚本宿主接口 |
| Core Entity Runtime | 实体基类、属性、方法、生命周期 |

### L8: Domain Components

可装配业务域组件层。

| 组件 | 功能 |
|------|------|
| Session | 登录、会话、玩家锚点 |
| World | 地图、实例、AOI、世界运行 |
| Battle | 技能、buff、战斗规则 |
| Social | 聊天、好友、工会、邮件 |
| Task/Activity | 任务、成就、活动 |
| Platform Business | 支付、风控接入、运营业务能力 |

### L9: Distributed World Extension

BigWorld 风格增强层。

| 组件 | 功能 |
|------|------|
| GhostReplica | 非权威实体投影 |
| Witness | 客户端可见世界上下文 |
| Authority Transfer | 权威迁移 |
| Space Partition | 空间切片与拓扑 |
| AppMgr | 分布式调度与控制 |

### L10: Assembly / Topology

最终装配与部署层。

| 组件 | 功能 |
|------|------|
| Starter | 模块装配 |
| Apps | 最终可执行进程 |
| Topology | 部署拓扑与环境配置 |

## 依赖关系

```
L1 <- L2 <- L3 <- L4 <- L5 <- L6 <- L7 <- L8 <- L9 <- L10
```

**硬约束**：
- `L2` 不反向依赖 `L6/L7/L8`
- `L3` 不直接承载业务域逻辑
- `L6` 不依赖具体业务域实现
- `L9` 是增强层，不反向污染 `L7`
- `L10` 只做装配，不承载核心框架能力

## 模式映射

### 普通 MMO 模式

默认启用：

- `L1-L8`

按需最小启用：

- `L9`

### BigWorld 模式

在 `L1-L8` 稳定后，再叠加：

- `L9`
- `L10` 中对应的大世界部署拓扑

## 当前仓库的直接含义

基于当前 Apollo 仓库状态，最需要优先收口的是：

- `modules/runtime`
- `modules/runtime/ops`
- `modules/platform`
- `modules/game/core`
- `modules/game/world`
- `modules/script`

语义上需要继续校正的是：

- 当前 `apps/base-app` 的实现偏数据服务
- 目标语义应演进为玩家锚点宿主
- 当前 `apps/cell-app` 更像 world runtime 原型
- `DBMgr / PersistenceService` 应单独理解为持久化执行层

更完整的分层说明见：

- [Apollo 分层设计](./apollo-layering-design.md)

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [进程语义重定义](./process-semantics-redefinition.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [持久化进程与 DBMgr 设计](./persistence-process-and-db-manager-design.md)
- [Apollo 最终蓝图](./apollo-final-blueprint.md)
- [Apollo 实施清单](./apollo-implementation-checklist.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [技术收敛与替换清单](./technology-convergence-and-replacement-plan.md)
- [技术收敛实施计划](./technology-convergence-execution-plan.md)
- [KBE 参考原则与 ECS 兼容取舍](./kbe-reference-principles.md)
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
- [Apollo 世界架构路线](./apollo-world-architecture.md)
- [Apollo 架构差距分析](./apollo-architecture-gap-analysis.md)
- [Apollo 渐进式游戏框架理论设计](./apollo-progressive-game-framework.md)
- [统一脚本层设计](./script-layer-design.md)
- [Lua Backend 设计](./lua-backend-design.md)
- [Python Backend 设计](./python-backend-design.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [WorldHost 设计稿](./world-host-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [Gateway 接入 Facade 设计](./gateway-ingress-facade-design.md)
- [Topology 对比与登录分发设计](./topology-comparison-and-login-flow-design.md)
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
- [Internal Service Client 与 Envelope 设计](./internal-service-client-and-envelope-design.md)
- [Replication Pipeline 设计](./replication-pipeline-design.md)
- [BigWorld 架构深度解析](./bigworld.md)
- [BigWorld 进程架构与玩家生命周期](./bigworld-lifecycle.md)
- [KBEngine 源码分析](./kbe-source-analysis.md)
