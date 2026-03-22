---
title: MMO 模块落地清单
icon: list-check
order: 42
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - MMO
  - Rollout
  - Module
---

# MMO 模块落地清单

这篇文档不再讨论抽象概念，只回答一个实施问题：

`Apollo 如果要从文档进入代码落地，普通 MMO 和分布式世界分别应该先写哪些模块、后写哪些模块、依赖顺序是什么。`

## 一、先说结论

Apollo 的实现顺序必须固定为两阶段：

1. 先完成 `Standard MMO`
2. 再叠加 `Distributed World`

原因很直接：

- `Distributed World` 依赖稳定的在线主链
- `Proxy / Witness / Ghost / AuthorityTransfer` 都不该在基础主链还没收住时提前落地

所以代码实施应遵守这个顺序：

```text
基础设施 -> 在线主链 -> 世界运行时 -> 接入层 -> 持久化收口 -> 分布式世界增强
```

## 二、实施边界

### 先不做的东西

- 完整 `AppMgr` 控制面
- 完整 `Witness / Ghost / AuthorityTransfer` 全家桶
- 全量脚本热更能力
- 全量社交、支付、活动域

### 第一优先级

- 登录
- 在线主状态
- 世界进入
- 断线恢复
- 普通 MMO 可运行链路

## 三、Standard MMO 模块顺序

目标主链：

```text
Client -> LoginApp -> GatewayApp -> BaseApp(PlayerAnchor) -> WorldApp
```

### 阶段 S1：基础运行时

目标：

- 先让应用宿主、配置、日志、生命周期稳定

模块：

- `modules/base`
- `modules/core`
- `modules/runtime`
- `modules/runtime/ops`

验收点：

- 应用可稳定启动/关闭
- 配置可装载
- 日志、控制台、诊断可用

### 阶段 S2：统一协议边界

目标：

- 固定客户端协议与内部服务调用边界

模块：

- `modules/net/protocol`
- `internal service client`
- `client envelope`
- `session protocol`

验收点：

- 登录票据、心跳、重连、内部 envelope 结构固定
- Gateway / Login / Base / World 不再各自发明消息格式

### 阶段 S3：在线主状态

目标：

- 建立 `PlayerAnchor` 作为唯一在线主状态中心

模块：

- `modules/game/session`
  - `PlayerAnchor`
  - `AnchorManager`
  - `SessionLocator`
  - `ReconnectPolicy`
  - `WorldAssignment`

对应应用：

- `apps/base-app`

验收点：

- 登录成功后先激活 `PlayerAnchor`
- 重复登录、踢旧接新、断线恢复都经由锚点层裁决
- `BaseApp != DBMgr`

### 阶段 S4：世界运行时

目标：

- 让普通 MMO 的地图、实例、AOI、移动跑起来

模块：

- `modules/game/world`
  - `WorldHost`
  - `WorldSession`
  - `MapInstance`
  - `AOI`
  - `Movement`

对应应用：

- `apps/world-app`
  - 如果现阶段还没有独立 `world-app`，可先由当前 `cell-app` 承接 world runtime 原型

验收点：

- 玩家可进入世界
- AOI 与移动广播稳定
- 世界运行时不持有长期在线权威

### 阶段 S5：边缘接入层

目标：

- 让 `GatewayApp` 退回接入与转发层

模块：

- `apps/gateway-app`
  - `ClientIngressServer`
  - `ConnectionRegistry`
  - `SessionAdmission`
  - `PacketDispatcher`
  - `RouteExecutor`
  - `ClientEgress`

验收点：

- Gateway 只保存连接态与短期路由快照
- Admission 必须向 `BaseApp` 校验
- Gateway 不持有玩家长期在线主状态

### 阶段 S6：登录入口收口

目标：

- 让 `LoginApp` 只保留认证、风控、入口分配、票据发放

模块：

- `apps/login-app`
  - `Authenticator`
  - `LoginRiskController`
  - `EntryAllocator`
  - `LoginTicketIssuer`

验收点：

- `LoginApp` 不再作为长期 session 权威
- 普通 MMO 返回 `GatewayApp` 接入点
- 分布式世界模式可切换返回 `BaseApp Proxy` 接入点

### 阶段 S7：持久化收口

目标：

- 把持久化从 `BaseApp` 语义中心降为支撑层

模块：

- `modules/platform`
  - `Repository`
  - `PersistenceService`
  - `RelationalStore`
  - `KvStore`

对应应用：

- `DBMgr / PersistenceService`

验收点：

- `BaseApp` 只做持久化协调
- 玩家快照由统一持久化层执行

## 四、Distributed World 模块顺序

目标主链：

```text
Client -> LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp
```

这部分只有在 `Standard MMO` 已稳定后才应启动。

### 阶段 D1：Proxy 化 BaseApp

目标：

- 让 `BaseApp` 同时承接 `PlayerAnchor + Proxy`

模块：

- `ClientAttachment`
- `ClientBinding`
- `Proxy`
- `ReconnectTakeover`

验收点：

- 客户端可直接接到 `BaseApp`
- `GatewayApp` 不再作为分布式世界默认前置

### 阶段 D2：Cell 侧空间权威

目标：

- 让 `CellApp` 成为空间实时权威节点

模块：

- `CellRuntime`
- `SpaceRuntime`
- `CellRouting`
- `AvatarEntity`

验收点：

- 玩家进入 `CellApp`
- 空间内 AOI、移动、战斗在 cell 侧收口

### 阶段 D3：可见性增强

目标：

- 引入客户端视野上下文与跨分区投影

模块：

- `Witness`
- `WitnessBridge`
- `Ghost`

验收点：

- 视野同步经由 `Witness -> Proxy -> Client`
- 跨 cell 可见实体不再靠硬编码特判

### 阶段 D4：权威迁移

目标：

- 让实体可以跨 cell 平滑切换权威

模块：

- `AuthorityTransfer`
- `SpacePartition`
- `MigrationState`

验收点：

- 跨边界迁移不丢控制权
- 迁移期间客户端附着点不变

### 阶段 D5：控制面增强

目标：

- 规模上来后再引入控制面

模块：

- `BaseAppMgr`
- `CellAppMgr`
- `TopologyRegistry`
- `MigrationControl`

验收点：

- 可以对 Base / Cell 做负载观测与调度
- 控制面不侵入在线主链语义

## 五、模块依赖关系

### Standard MMO

```text
runtime
  -> protocol
  -> session
  -> world
  -> gateway
  -> login
  -> persistence
```

### Distributed World

```text
standard mmo stable
  -> proxy base
  -> cell runtime
  -> witness ghost
  -> authority transfer
  -> app mgr
```

## 六、哪些模块可以并行

可以并行推进：

- `runtime/ops` 与 `platform`
- `gateway ingress` 与 `login cleanup`
- `world runtime` 与 `persistence adapter`

不建议并行推进：

- `PlayerAnchor` 和 `Proxy`
- `Witness / Ghost` 和 `AuthorityTransfer`
- `AppMgr` 和基础 world runtime

原因：

- 这几组模块强耦合，边界不先固定，后面会反复返工

## 七、每阶段最小验收链路

### Standard MMO 最小验收链路

1. 客户端登录
2. `LoginApp` 发放登录票据
3. `GatewayApp` 校验票据
4. `BaseApp` 激活 `PlayerAnchor`
5. `WorldApp` 创建 `WorldSession`
6. 玩家进入地图并可断线重连

### Distributed World 最小验收链路

1. 客户端登录
2. `LoginApp` 返回 `BaseApp Proxy` 入口
3. `BaseApp` 绑定 `Proxy`
4. `CellApp` 创建或恢复实体
5. 客户端可接收 `Witness` 视野数据
6. 玩家跨 cell 时完成 authority transfer

## 八、当前仓库的直接落点

按当前仓库状态，更合适的近期工作顺序是：

1. 稳定 `modules/game/session`
2. 稳定 `modules/game/world`
3. 收口 `apps/login-app`
4. 收口 `apps/gateway-app`
5. 明确 `apps/world-app` 或让当前 `cell-app` 临时承接 world runtime
6. 抽出独立 `PersistenceService`
7. 再进入 `Proxy / Cell / Witness / Ghost`

## 九、结论

Apollo 的代码落地顺序不能按“进程名补齐”来推进，而应按“语义链路收口”推进。

先把普通 MMO 主链做稳：

- `Login -> Gateway -> BaseApp(PlayerAnchor) -> WorldApp`

再叠加分布式世界增强：

- `Login -> BaseApp(Proxy + PlayerAnchor) -> CellApp`

只有按这个顺序实施，Apollo 才不会再次陷入：

- `BaseApp` 语义混乱
- `Gateway` 角色膨胀
- `CellApp` 和 `WorldApp` 混写
- BigWorld 名词先行、代码基础却没收住

## 相关阅读

- [MMO Topology 范围与组合设计](./mmo-topology-scope-and-composition-design.md)
- [MMO 组件装配目录](./mmo-component-assembly-catalog.md)
- [玩家在线主链设计](./player-online-flow.md)
- [LoginApp 收口设计](./login-app-design.md)
- [Gateway 接入 Facade 设计](./gateway-ingress-facade-design.md)
- [BigWorld 进程架构与玩家生命周期](./bigworld-lifecycle.md)
