---
title: 架构总览
icon: layers
order: 7
prev: /guide/configuration
---

# 架构总览

Apollo 当前面向塔防、合作玩法和小队副本，预留未来无缝地图。术语按 [BigWorld 与 Apollo 对照](../design/bigworld-apollo-mapping.md)解释：已有六个角色保持原名，增加已讨论的 cell 执行职责；worldServer 管理 cell，不再同时定义为逐帧模拟宿主。

## 目标模型

<ArchitectureDiagram kind="system" />

| 角色 | 职责与生命周期 |
|---|---|
| gateway | 客户端 socket、会话、Base 与模拟端点路由；更换后端不迁移 socket。 |
| loginServer | 认证、凭证、服务目录与玩家在线关联；保存位置投影，不裁决 Cell 拓扑。 |
| baseServer | `base::Avatar` 等长期对象、基础业务与资料写权；不随副本销毁或跨服转移。 |
| worldServer | 类似 CellAppMgr 的管理职责：cell 注册、容量、Space/Cell 分配、拓扑、排空和迁移。 |
| cell | 类似 CellApp 的执行职责：承载本地 Cell，运行 Avatar/Entity、AOI、战斗、Lua 与 tick。 |
| database | 资料加载、保存、存储路由、队列/journal 及持久结果；不制定奖励规则。 |
| machined | 按需监督本机进程；不裁决跨服、玩家资料或空间归属。 |

## 空间与玩法对象

Space 是独立运行空间，对应现有 Scene 的空间语义；Scene 是当前代码名，不新增同义 Scene→Space 容器链。Cell 是 Space 的空间分区，cell 是承载分区的进程，两者身份分开。默认仅一个执行进程，每个 Space 一个覆盖其范围的 Cell；一个进程可承载多个 Space 的 Cell。

Instance 继续表示小队、目标、计时、结算与回收的临时玩法生命周期，复用八态状态机。它关联 Space，不是另一种空间分区；持续开放的 Space 可以没有临时 Instance。

<ArchitectureDiagram kind="space" />

## 玩家与数据归属

Base 持有长期资料并提供经校验的玩法快照；cell 只修改其 Cell 内的权威模拟状态。奖励、消耗与任务结果由 cell 请求各玩家所属 Base 校验，再经 database 持久提交。接收 ACK 不等于保存成功。跨服保留原 Base、区服和数据库归属，目的地不获取原服数据库凭证。

## 注册、寻址与迁移

Base/World 服务向 loginServer 目录注册；cell 向所属 worldServer 注册并发布就绪。worldServer 是 Space/Cell 拓扑与分配的权威，在线目录保留玩家 Base、World、Space、Cell、执行端点及会话的位置投影。gateway 缓存 Base 与 cell 路由，正常消息直达对应业务端点。

源 cell 在模拟边界冻结，目标 cell 恢复为未激活状态；worldServer 按预期源与代次提交模拟归属，再更新路由投影、激活目标、清理源。跨 World 交接须有协调和持久提交依据，不能由两个管理端各自激活。结果不明先查记录，禁止双端模拟。worldServer/cell 独立部署均不要求同机 machined。

## 更新边界

支持协议更新的进程替换：启动新进程、验证新旧消息与状态兼容、恢复准备、交接权威和路由、源端清理后退出。Base 迁移长期对象及在途业务；cell 迁移完整必要模拟状态；worldServer 迁移管理拓扑及在途分配/迁移记录。三者不是同一份快照。

仅代码更新保持消息编号、字段和 wire 不变；Lua 在所属执行边界更新，C++ 使用进程替换。保留 gateway 可保留其 TCP；替换 gateway 需排空或重连恢复。完整 Instance 连续必须包含成员、怪物、目标、计时和必要脚本状态，不能只搬玩家。详见[更新边界](../design/gateway-topology-survey.md)。

## 源码分层

以下九层按职责归纳，不是目录别名。所有行号对应 2026-11-08 的源码 HEAD。

### L0 进程壳与生命周期

`apps/CMakeLists.txt` 按目标可用性装配进程：运维进程无门控，gateway 和 legacy 应用依赖协议目标，world 依赖 game 目标（`apps/CMakeLists.txt:34`）。入口负责参数、崩溃捕获和启停（`apps/gateway/src/main.cpp:9`）；`GameServer` 提供 PreInit→Ready、组件启动、主循环和统计生命周期（`include/apollo/server/game_server.h:146`）。Gateway 配置 schema 在 `apps/gateway/include/gateway/config.hpp:12`，World 角色与拓扑配置在 `apps/world/include/world_app/config.hpp:12`；专属 wire 与路由表在 `sdks/cpp/generated/contract_route.json:1`。

### L1 应用装配与状态面

Gateway 把 ingress registry、dispatcher、session manager、M1 客户端、dashboard 和后台线程装配成同一实例（`apps/gateway/include/gateway/gateway_server.hpp:200`）；登录服务持有目录、注册、准入和 Base 客户端面（`apps/login/include/login/login_server.hpp:60`）。这类状态常驻内存：Gateway 会话表见 `apps/gateway/include/gateway/session_manager.hpp:136`，World 注册与分配表见 `apps/world/include/world_app/world_app.hpp:233`，Admin 指标快照见 `apps/admin/include/admin/metrics_hub.hpp:69`。专属文案在[服务器应用](../apps/index.md)。

### L2 传输与会话边界

基础连接、packet 解析、event loop、session manager 和 HTTP/WebSocket 属于面向客户端的传输层（`include/apollo/net/session.h:86`、`include/apollo/net/session_manager.h:150`）。Gateway 的 TCP/WS/WSS 准入、fan-in、限流与会话保持实现挂在 `apps/gateway/src/ingress/session_admission_service.cpp:311`；配置 schema 是 `apps/gateway/include/gateway/config.hpp:12`，页面文案挂 `apps/gateway/src/gateway_dashboard_page.hpp:1`。

### L3 服务间协议与路由

`M1LinkServer` 从契约路由 JSON 装载服务间面（`apps/world/include/world_app/m1_link_server.hpp:30`），Gateway 用 `M1Egress` 把会话意图转发到 World（`apps/gateway/src/gateway_server.cpp:190`），Login 服务注册用同一 JSON 协议声明端点（`apps/login/include/login/service_registry.hpp:24`）。协议 schema 与生成器位于 `sdks/contract/apollo.xsd:1`、`sdks/contract/messages.xml:1` 和 `sdks/gen/src/main.cpp:105`；SDK 契约说明挂[SDK 契约](../design/sdk-contract.md)。

### L4 IPC 与服务发现

IPC 层提供共享内存、Socket、异步 I/O 与 QoS 通道（`include/apollo/ipc/channel.h:20`、`src/apollo/ipc/async_io.cpp:23`）。服务发现可落在 SQLite、Redis 或进程内存：SQLite 建库与表见 `src/apollo/ipc/sqlite_service_discovery.cpp:137`，Redis key/PubSub 约定见 `include/apollo/ipc/redis_service_discovery.h:184`，内存注册表见 `include/apollo/core/service_discovery.h:175`。

### L5 契约生成与客户端 SDK

XML/XSD 契约由 `sdks/gen` 解析为 C++、Lua、Proto、JSON 与路由表（`sdks/contract/apollo.xsd:1`、`sdks/gen/src/main.cpp:105`）。客户端侧 Unity、Cocos、Laya 各自装配网络、认证、路由与属性同步（`skds/unity/ApolloSDK/ApolloClient.cs:1`、`skds/cocos/ApolloSDK/ApolloClient.ts:1`）。契约 schema 挂[XML 契约生成](../design/xml-generation.md)，SDK 目录文案挂 `skds/README.md:1`。

### L6 游戏内核与 cell 执行

属性、匹配、排行、定时轮和对象池组成通用玩法内核（`include/apollo/game/attributes/attribute.hpp:70`、`include/apollo/algorithm/timing_wheel.h:1`）。cell 执行器把意图队列、Scene/Space 映射、tick、冻结闸和 Lua 扩展绑定到进程内权威模拟（`apps/world/include/world_app/cell_app.hpp:124`）；cell 配置 schema 在 `apps/world/include/world_app/cell_app.hpp:52`，契约处理器挂 `apps/world/scripts/contract_handlers.lua:1`。

### L7 数据归属与持久化

PlayerStore 同时提供内存缓存、write-ahead journal 和原子 JSON 落盘：存储抽象在 `apps/common/include/player_store/storage.hpp:11`，journal 装载在 `apps/common/src/player_store.cpp:790`，临时文件 rename 在 `apps/common/src/database_service.cpp:104`。`PlayerStoreConfig` 同时保留 MySQL 目标和当前 `dataDir`/`journalPath` schema（`apps/common/include/player_store/config.hpp:25`）；数据库 RPC 调用面在 `apps/common/include/database/client.hpp:13`。

### L8 观测、监督与运维

日志应用扫描、tail、归档并可经 HTTP 推送（`apps/logger/include/logger/archive_writer.hpp:20`）；Admin 用被动链路收 931 快照，再经 `/metrics` 导出（`apps/admin/include/admin/metrics_hub.hpp:39`）。Machined 用 fork/exec 启动子进程、写 PID 文件并收割死亡事件（`apps/machined/src/supervisor.cpp:190`）。运行文案挂 `docs/zh/apps/index.md:1` 与 `docs/zh/design/logger-app.md:1`。

## 无缝地图预留

现在不实现空间切分、Ghost 复制和跨边界战斗。设计保留同一 Space 多 Cell、全空间坐标、稳定实体 ID、邻接拓扑、权威代次、完整状态交接和连续可见性。显式换 Space 与同 Space 跨 Cell 是不同操作，不能复用离场重置来实现无缝迁移。

## 代码现状

当前 WorldApp、World、Scene 仍将管理和模拟合在一起；独立 Base/database 原型已提供远程加载/保存；cell 执行入口、目录注册、业务迁出和整体路由仍待完成。SceneTransfer 是同进程换场，不是完整 Cell 状态迁移。已有原语和测试保留其实际范围，不作为目标拓扑完成证据。见[服务器应用](../apps/index.md)与根目录 todo.md。
