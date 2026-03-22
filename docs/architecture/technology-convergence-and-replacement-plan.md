---
title: 技术收敛与替换清单
icon: arrows-spin
order: 32
category:
  - 架构
  - Apollo
tag:
  - Apollo
  - 技术收敛
  - 替换计划
  - 成熟库
---

# 技术收敛与替换清单

这篇文档解决的是 Apollo 后续实现里一个非常现实的问题：

`哪些基础设施能力应该继续自己维护，哪些应该尽快收敛到成熟库，哪些必须保留 Apollo 自己的领域语义实现。`

如果这个边界不先定下来，后面很容易再次出现两类问题：

- 底层网络、线程、服务发现重复造轮子
- 领域语义和基础设施纠缠，导致后面既难替换库，也难稳定架构

## 一、先说结论

Apollo 后续实现应该遵守 3 条硬原则：

- 基础设施优先复用成熟库
- Apollo 自研优先保留游戏领域语义
- 替换顺序先收敛主链，再逐步冻结旧实现

这里的“成熟库”不是泛指“外部依赖越多越好”，而是指那些已经被长期验证、问题边界清晰、社区和工具链成熟的能力，例如：

- `Drogon`
- `Boost.Asio / Boost.Beast`
- 稳定的 Redis / SQL 客户端
- 标准化可观测性组件

Apollo 应该真正保留的，不是这些底层轮子，而是：

- `PlayerAnchor`
- `RouteSnapshot`
- `WorldSession`
- `MapInstance`
- `Authority Transfer`
- `Ghost / Witness`
- `Script ABI`
- 进程语义与装配语义

换句话说：

- Apollo 的价值在游戏服务器语义层
- Apollo 的成本不该继续主要消耗在重复实现底层通用件

## 二、为什么现在必须技术收敛

### 1. 当前仓库已经存在明显的重复建设

例如仓库里已经可以直接看到：

- 多套 socket / transport 抽象并存
- 多套 endpoint / url 处理并存
- 多套线程池并存
- 多套 service discovery 实现并存
- RPC、channel、req-rep 抽象边界重叠

这会直接导致：

- 代码路径越来越多
- 同一问题要修多份
- 上层进程很难知道“主路径”到底是哪一条

### 2. 主链语义正在重构，底层更不能继续扩散

Apollo 现在已经开始把在线主链收回到较合理的语义：

- `LoginApp = 认证入口`
- `BaseApp = PlayerAnchor Host`
- `Gateway = route snapshot 执行器`
- `cell-app` 当前按 `WorldHost/world-app` 原型推进
- `DBMgr / PersistenceService` 单独理解

在这个阶段，如果底层还继续沿多条技术路线生长，后面只会把错误语义再次固化。

### 3. 用户目标不是“再造一个网络库”

Apollo 的目标已经很清楚：

- 做可渐进装配的在线游戏框架
- 普通在线游戏、普通 MMO、分布式世界模式都能演进
- 优先稳定、成熟、可维护

这和“持续扩写自研 HTTP、WebSocket、event loop、线程池”并不一致。

## 三、哪些能力应该明确保留 Apollo 自研

Apollo 后续应该坚定保留自研的，是领域语义层，不是底层通用件。

### 1. 在线主链与会话语义

当前已经落地并且应该继续演进的模块包括：

- `modules/game/session`
- `apps/base-app`
- `apps/gateway-app`
- `apps/login-app`

这里承载的是：

- 玩家锚点
- 会话绑定
- 世界分配
- 路由快照
- 登录票据
- 在线状态流转

这些不应该交给外部库替代。

### 2. 世界运行与世界迁移语义

当前应该继续保留并演进的模块包括：

- `modules/game/world`
- `apps/cell-app`

这里承载的是：

- `WorldSession`
- `MapInstance`
- `space / instance / world`
- transfer / suspend / resume

这部分是 Apollo 未来普通 MMO 和分布式世界能力的核心基础。

### 3. 更高层的游戏领域能力

后续继续补齐时，也应该放在 Apollo 自己的领域层：

- battle
- social
- guild
- task
- activity
- match
- chat domain
- payment domain adapter boundary

这些能力可以依赖成熟库，但不应该由成熟库定义 Apollo 的业务模型。

## 四、当前仓库里最明显的重复建设

下面这些区域，不适合继续按“多线并行自研”方式扩写。

## 五、网络与传输层重复建设

### 1. 原始 socket 与消息 socket 并存

仓库里可以直接看到：

- `modules/protocol/src/socket.cpp`
- `modules/net/tcp/src/socket.cpp`
- `apps/gateway-app/src/gateway_server.cpp`

问题在于：

- 有 NNG 风格的消息 socket
- 有原始 TCP socket
- `gateway-app` 里还有直接手写 accept / bind / listen

这会导致 transport 层没有统一主线。

### 2. HTTP / WebSocket / event loop 自研过重

相关文件包括：

- `modules/net/http/src/http.cpp`
- `modules/net/http/src/event_loop.cpp`
- `modules/net/websocket/src/websocket.cpp`

这些能力已经是成熟生态高度覆盖的区域。

如果 Apollo 继续自己长期维护：

- HTTP server/client
- WebSocket
- event loop
- connection lifecycle

收益很低，维护成本很高。

### 3. 建议收敛方向

这里应明确收敛为：

- HTTP / WebSocket 主线优先 `Drogon`
- 若有 Drogon 不覆盖或不适合的低层传输，再补 `Boost.Asio / Beast`
- Apollo 自己只保留 transport facade 和游戏语义协议边界

Apollo 不应继续把下面这些当成长期主战场：

- 手写 socket accept loop
- 自维护 HTTP parser / lifecycle
- 自维护 WebSocket transport
- 自维护 event loop 细节

## 六、URL、Endpoint 与地址解析重复建设

相关文件包括：

- `include/apollo/net/net_util.h`
- `modules/net/protocol/src/endpoint.cpp`
- `apps/login-app/src/login_server.cpp`

问题不是“有没有 endpoint”，而是 endpoint 语义正在多个位置分散出现：

- URL 组装
- host/port 拼接
- endpoint serialize / deserialize
- 服务地址解析

这类能力本质上应该有一个统一公共模块，否则后面会继续出现：

- 各 app 自己拼地址
- 各协议层自己解释 endpoint
- route 和 service discovery 无法共享统一对象模型

### 建议收敛方向

统一形成一条地址语义主线：

- `Endpoint`
- `ServiceAddress`
- `RouteAddress`
- `ClientIngressAddress`

其中：

- 字符串解析、host/port/url 处理优先复用成熟库
- Apollo 自己维护的是“进程语义地址模型”，不是底层字符串工具集合

## 七、线程池与执行器重复建设

当前至少可以看到：

- `include/apollo/utils/threading/thread_pool.hpp`
- `src/utils/threading/thread_pool.cpp`
- `include/apollo/utils/thread_pool.h`
- `modules/base/include/apollo/base/thread_pool.hpp`

这已经不是“可选实现”，而是明显的语义分裂。

继续这样发展会出现两个问题：

- 上层模块不知道应该依赖哪一套
- shutdown、exception、queue、backpressure 行为不一致

### 建议收敛方向

Apollo 应该只保留一套统一执行器抽象，例如：

- `Executor`
- `IoExecutor`
- `CpuExecutor`
- `Strand / SerializedExecutor`
- `ScheduledExecutor`

底层实现优先绑定成熟能力：

- `Drogon` 运行时线程模型
- `Boost.Asio` executor
- 或一套明确的内部统一 executor

但不应该继续保留多套 thread pool API。

## 八、RPC、Channel、Req-Rep 抽象重叠

相关文件包括：

- `modules/protocol/include/apollo/protocol/socket.hpp`
- `modules/protocol/src/socket.cpp`
- `modules/net/rpc/src/rpc_legacy.cpp`
- `modules/net/protocol/src/endpoint.cpp`
- `apps/gateway-app/src/gateway_server.cpp`

当前问题在于：

- `RpcClient`
- `Channel`
- req/rep socket
- endpoint/service registry

这些能力在不同子目录里都有承载，但边界还没有完全收口。

这会影响后续几个关键问题：

- 内部调用到底走哪条主线
- 重试、超时、断线重连放在哪层
- 未来脚本层和领域层依赖哪一个 façade

### 建议收敛方向

Apollo 应只保留一条明确的内部调用主路径：

- 外部入口协议
- 进程内/进程间 RPC façade
- 内部消息 envelope

可以保留多种底层 transport 适配器，但上层只看到统一接口，例如：

- `ServiceClient`
- `ServiceCall`
- `MessageBus`
- `RouteResolver`

旧的 `rpc_legacy` 路线应尽快进入冻结状态。

## 九、服务发现与负载均衡重复建设

相关文件包括：

- `src/apollo/ipc/service_discovery.cpp`
- `src/apollo/ipc/sqlite_service_discovery.cpp`
- `src/apollo/ipc/redis_service_discovery.cpp`
- `src/apollo/core/service_discovery.cpp`
- `include/apollo/ipc/service_discovery.h`
- `include/apollo/core/service_discovery.h`

这类问题很典型：

- service discovery 有多套入口
- storage backend 和 discovery 语义混在一起
- core 与 ipc 层边界不够清晰

后面如果再叠加：

- LVS / Nginx / 网关前置 LB
- world node route
- app manager 分配

复杂度会进一步放大。

### 建议收敛方向

Apollo 应统一成 3 层：

### 1. Service Registry

负责：

- register
- unregister
- heartbeat
- metadata

### 2. Service Discovery

负责：

- discover
- watch
- cache
- resolve best candidate

### 3. Routing / Load Strategy

负责：

- least-load
- affinity
- shard key route
- topology aware route

存储后端可以有多种：

- in-memory
- Redis
- SQLite
- 配置中心

但上层只应依赖统一 discovery / route façade。

## 十、哪些区域应该冻结，不再继续扩写

下面这些区域更适合进入“冻结、只修必要问题、不再继续扩写功能”的状态：

- `modules/net/http/src/http.cpp`
- `modules/net/http/src/event_loop.cpp`
- `modules/net/websocket/src/websocket.cpp`
- `modules/net/rpc/src/rpc_legacy.cpp`
- 多套 thread pool 旧接口
- 多套历史 service discovery 入口

冻结不等于立刻删除，而是：

- 不再新增功能
- 只做兼容性修补
- 后续逐步由统一 façade 转接出去

## 十一、哪些区域应该尽快适配成熟库

优先级建议如下。

### P0：直接影响主链稳定性的基础设施

- Gateway 外部接入传输层
- HTTP / WebSocket
- 统一 executor
- 统一 endpoint / address 解析

### P1：跨进程调用与注册发现

- RPC façade
- service registry / discovery
- route / load balance 策略抽象

### P2：平台基础能力

- Redis client
- SQL client
- repository / unit of work 适配层
- leaderboard / distributed lock 组件

这里要强调：

- Apollo 可以封装 Redis 排行榜
- Apollo 可以封装 distributed lock

但底层 Redis 连接、连接池、命令执行，不应再优先自研。

## 十二、哪些区域应该继续演进

这些区域应该继续按 Apollo 领域语义推进：

- `modules/game/session`
- `modules/game/world`
- `apps/base-app`
- `apps/gateway-app`
- `apps/login-app`
- `apps/cell-app`
- 协议层里的玩家在线主链消息定义
- 后续的 battle / social / task / activity / guild / match 领域模块

因为这些区域才真正定义 Apollo 是什么。

## 十三、建议的三阶段落地路线

### 第一阶段：先统一主路径

目标：

- 明确在线主链唯一主路径
- 明确 transport、RPC、discovery 的唯一 façade

动作：

- Gateway 对外连接逐步转向成熟网络栈
- Base/Login/Gateway/World 只依赖统一 service client / route resolver
- 停止继续扩写 legacy transport

### 第二阶段：做适配层替换

目标：

- 外部库接管通用基础设施
- Apollo 保留语义 façade 和领域模型

动作：

- `Drogon` 接管 HTTP / WebSocket / 对外服务接入
- `Boost.Asio / Beast` 处理必要的底层补位
- Redis / SQL 使用成熟客户端
- 旧实现通过 adapter 逐步退场

### 第三阶段：清理与模块收口

目标：

- 删除或归档历史重复实现
- 让目录结构与新架构一致

动作：

- 冻结目录进入归档或兼容层
- 新功能只能落在统一模块边界上
- 文档、starter、profile、assembly 全部指向新主线

## 十四、一个简单判断标准

后续每新增一个模块或能力，都先问 3 个问题：

### 1. 这是 Apollo 的领域语义吗

如果是：

- 继续自研

例如：

- PlayerAnchor
- WorldSession
- Authority Transfer

### 2. 这是通用基础设施吗

如果是：

- 优先复用成熟库

例如：

- HTTP
- WebSocket
- event loop
- SQL driver
- Redis client

### 3. 这层是 façade 还是重新造轮子

如果只是为了把外部库包装成 Apollo 领域可用接口：

- 可以保留轻量 adapter / façade

如果已经开始重写完整底层能力：

- 就应该停止并收敛

## 十五、结论

Apollo 后续最合理的技术策略，不是“所有东西都自己实现”，而是：

- 用成熟库承接通用基础设施
- 用 Apollo 自己定义游戏服务器领域语义
- 用统一 façade 把两者稳定连接起来

这样做的直接收益是：

- 主链更容易稳定
- 替换成本更低
- 文档、目录、代码语义更一致
- 后续 battle、social、task、guild、activity、distributed world 才能在稳定底座上推进

这也是 Apollo 从“原型集合”走向“可长期维护框架”的必要步骤。

## 相关阅读

- [Apollo 分层设计](./apollo-layering-design.md)
- [Platform Foundation 设计](./platform-foundation-design.md)
- [Runtime Ops Host 设计](./runtime-ops-host-design.md)
- [模块目录重组设计](./module-reorganization-design.md)
- [Starter 与模块装配设计](./starter-and-module-assembly-design.md)
- [玩家在线主链设计](./player-online-flow.md)
- [BaseApp 演进设计](./base-app-evolution.md)
- [Gateway 会话设计](./gateway-session-design.md)
