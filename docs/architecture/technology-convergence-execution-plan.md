---
title: 技术收敛实施计划
icon: list-timeline
order: 33
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 技术收敛
  - 实施计划
  - 重构
---

# 技术收敛实施计划

这篇文档不再只讲原则，而是直接回答：

`Apollo 接下来应该按什么顺序，把重复建设收敛成一条稳定主线。`

如果还没看过原则基线，先看：

- [技术收敛与替换清单](./technology-convergence-and-replacement-plan.md)

## 一、实施目标

这份计划只做 4 件事：

- 明确哪些模块继续演进
- 明确哪些模块冻结，不再扩写
- 明确哪些区域优先接成熟库
- 明确每个阶段的验收标准

这里要继续强调一个边界：

- 这份计划处理的是基础设施收敛
- 不是重新定义 `BaseApp`、`Gateway`、`WorldHost` 的领域语义

领域语义主线仍然是：

- `LoginApp = 认证入口`
- `BaseApp = PlayerAnchor Host`
- `Gateway = route snapshot 执行器`
- `cell-app` 当前按 `WorldHost/world-app` 原型推进

## 二、当前仓库的处理分组

从执行角度看，当前代码可以先分成 3 组。

### 1. 继续演进组

这些模块应该继续写：

- `modules/game/session`
- `modules/game/world`
- `apps/base-app`
- `apps/gateway-app`
- `apps/login-app`
- `apps/cell-app`
- `modules/protocol` 中与在线主链直接相关的协议定义

因为这些模块承载的是 Apollo 自己的领域语义。

### 2. 收敛替换组

这些模块应该优先适配成熟库或统一 façade：

- `modules/net/http`
- `modules/net/websocket`
- `modules/net/tcp`
- `modules/net/rpc`
- `modules/net/protocol`
- `modules/protocol/src/socket.cpp`
- 多套 thread pool
- 多套 service discovery

### 3. 冻结兼容组

这些区域短期保留，但不再继续扩展：

- `modules/net/http/src/http.cpp`
- `modules/net/http/src/event_loop.cpp`
- `modules/net/websocket/src/websocket.cpp`
- `modules/net/rpc/src/rpc_legacy.cpp`
- 历史 thread pool 旧接口
- 历史 discovery 旧入口

## 三、阶段 0：先定唯一主线

这是开始任何替换前必须完成的事情。

### 目标

让所有后续实现都知道“应该依赖哪一条主线”。

### 必做项

- 明确外部接入主线
- 明确内部 RPC façade 主线
- 明确 endpoint / address 主线
- 明确 executor 主线
- 明确 service discovery / route resolver 主线

### 产出物

建议形成下面这些统一抽象：

- `TransportFacade`
- `ServiceClient`
- `RouteResolver`
- `Endpoint`
- `Executor`

### 验收标准

- `Gateway` 不再直接扩写新 socket 逻辑
- 新 app 不再绕过 façade 直接拼 transport
- 新模块不再新增一套 thread pool 或 discovery 接口

## 四、阶段 1：收敛公网接入与传输栈

这是技术收益最高、风险也最容易继续扩散的一段。

### 目标

把外部接入从“历史自研 transport 集合”收敛到统一成熟栈。

### 当前重点区域

- `apps/gateway-app/src/gateway_server.cpp`
- `modules/net/http/src/http.cpp`
- `modules/net/http/src/event_loop.cpp`
- `modules/net/websocket/src/websocket.cpp`
- `modules/net/tcp/src/socket.cpp`

### 建议动作

#### 1. Gateway 对外连接先抽 façade

不要继续让 `gateway_server.cpp` 长期持有：

- bind
- listen
- accept
- recv
- send

这些细节。

应先抽出：

- `ClientIngressServer`
- `ClientConnection`
- `ClientSessionTransport`

然后再决定底层是否绑定：

- `Drogon`
- `Boost.Asio`

#### 2. HTTP / WebSocket 直接收口到成熟库

建议原则：

- 不再给 `modules/net/http` 新增功能
- 不再给 `modules/net/websocket` 新增功能
- 新需求直接走成熟库适配层

#### 3. Apollo 只保留协议与会话语义

Apollo 自己继续保留的是：

- login ticket
- session bootstrap
- gateway admission
- heartbeat policy
- route snapshot pull / refresh

### 验收标准

- Gateway 对外接入已不依赖手写 accept loop 继续扩张
- 外部连接生命周期能通过统一 façade 管理
- HTTP / WebSocket 新需求不再落到旧自研实现里

## 五、阶段 2：收敛地址模型与 endpoint 语义

### 目标

把地址解析、URL 拼接、服务地址语义统一到一套对象模型。

### 当前重点区域

- `include/apollo/net/net_util.h`
- `modules/net/protocol/src/endpoint.cpp`
- `apps/login-app/src/login_server.cpp`
- `apps/gateway-app/src/gateway_server.cpp`

### 建议动作

统一形成一套地址模型：

- `Endpoint`
- `ServiceAddress`
- `ClientIngressAddress`
- `WorldRouteAddress`

并明确边界：

- host/port/url 的解析交给稳定工具
- Apollo 维护的是进程语义和路由语义

### 验收标准

- app 层不再散落 `host + ":" + port` 拼接
- route / discovery / rpc 共用一套地址对象
- 新协议不再私自定义地址字符串格式

## 六、阶段 3：收敛 executor 与线程模型

### 目标

把当前多套 thread pool API 收成一套执行器边界。

### 当前重点区域

- `include/apollo/utils/threading/thread_pool.hpp`
- `src/utils/threading/thread_pool.cpp`
- `include/apollo/utils/thread_pool.h`
- `modules/base/include/apollo/base/thread_pool.hpp`

### 建议动作

统一定义：

- `Executor`
- `IoExecutor`
- `CpuExecutor`
- `ScheduledExecutor`

然后把旧 thread pool：

- 标记为 legacy
- 停止新增调用点
- 逐步转接到统一 executor

### 验收标准

- 新代码只依赖统一 executor 接口
- shutdown、backpressure、task exception 行为一致
- 旧 thread pool 不再继续长新能力

## 七、阶段 4：收敛内部 RPC 与消息主线

### 目标

让 Apollo 的内部调用只剩一条可维护主线路径。

### 当前重点区域

- `modules/protocol/include/apollo/protocol/socket.hpp`
- `modules/protocol/src/socket.cpp`
- `modules/net/rpc/src/rpc_legacy.cpp`
- `modules/net/protocol/src/endpoint.cpp`
- `apps/base-app/src/base_server.cpp`
- `apps/gateway-app/src/gateway_server.cpp`

### 建议动作

统一内部分层：

#### 1. Service Client

负责：

- request / response
- timeout
- retry
- reconnect

#### 2. Internal Envelope

负责：

- message type
- metadata
- tracing context
- route hint

#### 3. Route Resolver

负责：

- service resolve
- best node select
- route snapshot resolve

旧的 `rpc_legacy` 只保留兼容职责，不再作为新功能承载点。

### 验收标准

- Base/Login/Gateway/World 只依赖统一内部调用 façade
- timeout / retry / reconnect 不再分散在各 app 自己处理
- legacy RPC 不再新增调用方

## 八、阶段 5：收敛服务发现与负载策略

### 目标

把 service registry、discovery、route strategy 的边界拆清楚。

### 当前重点区域

- `src/apollo/ipc/service_discovery.cpp`
- `src/apollo/ipc/sqlite_service_discovery.cpp`
- `src/apollo/ipc/redis_service_discovery.cpp`
- `src/apollo/core/service_discovery.cpp`

### 建议动作

统一为三层：

- `ServiceRegistry`
- `ServiceDiscovery`
- `RoutingStrategy`

同时把 backend 从语义层剥离：

- 内存实现
- Redis 实现
- SQLite 实现

这些只是 backend adapter，不应成为上层直接依赖对象。

### 验收标准

- 上层 app 不再直接依赖具体 Redis/SQLite discovery 类
- route 选择策略可单独替换
- 后续接 `LVS / Nginx / AppMgr` 时不会继续打散边界

## 九、阶段 6：收敛平台基础能力

这一步不是先做，而是在线主链和基础通信主线收住之后再做。

### 目标

把 Redis、SQL、排行榜、分布式锁这些能力收成平台层，而不是散在 app 里。

### 建议动作

优先形成这些边界：

- `KvStore`
- `RelationalStore`
- `Repository`
- `UnitOfWork`
- `LeaderboardService`
- `DistributedLockService`

### 原则

- 底层驱动使用成熟库
- Apollo 只保留平台语义 façade
- 业务域不再直接碰裸驱动

### 验收标准

- app 层不再直接耦合裸 Redis / SQL 细节
- `BaseApp(PlayerAnchor Host)` 与持久化执行层边界清晰
- 排行榜、锁、队列等能力可跨业务域复用

## 十、每个阶段都必须遵守的约束

后续代码推进时，统一遵守下面这些规则：

### 1. 新功能不落 legacy 模块

例如：

- 不再扩写 `rpc_legacy`
- 不再扩写历史 HTTP / WebSocket 自研实现
- 不再新增 thread pool 变体

### 2. 先 façade，后替换

也就是说：

- 先把 app 层对底层能力的依赖抽成稳定接口
- 再替换底层实现

不要直接把 app 和第三方库硬绑死。

### 3. 领域语义不下沉到基础设施

例如：

- `PlayerAnchor`
- `WorldSession`
- `RouteSnapshot`

这些必须留在 Apollo 领域层。

### 4. 外部库不直接定义 Apollo 的业务模型

外部库负责：

- transport
- io
- parser
- driver

Apollo 负责：

- 游戏语义
- 进程语义
- 组装语义

## 十一、建议的近期实施顺序

如果按当前收益排序，建议从这里开始：

1. 抽 Gateway 对外接入 façade，停止继续扩写手写 socket 主线
2. 统一 endpoint / address 模型
3. 统一 executor 抽象，冻结多套 thread pool
4. 统一内部 `ServiceClient + RouteResolver` 主线
5. 统一 discovery / route strategy
6. 再推进 platform foundation 的驱动适配层

这个顺序的原因很简单：

- 先稳外部接入
- 再稳内部通信
- 最后再做数据与平台能力收口

## 十二、结论

Apollo 现在最需要的，不是继续把基础设施铺得更散，而是：

- 明确唯一主线
- 冻结历史分叉
- 用成熟库承接通用能力
- 让领域层继续按正确语义演进

只有这样，后面继续推进：

- battle
- social
- task
- activity
- guild
- distributed world

才不会建立在一套继续发散的底层上。

## 相关阅读

- [技术收敛与替换清单](./technology-convergence-and-replacement-plan.md)
- [Apollo 实施清单](./apollo-implementation-checklist.md)
- [Apollo 重构路线图](./apollo-refactor-roadmap.md)
- [进程重组与实施路线](./process-reorganization-and-rollout-plan.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
