---
title: 服务器应用
icon: server
order: 5
category:
  - 应用
tag:
  - 服务器
---

# 服务器应用

Apollo 的目标模型使用常见的职责名：`Login`、`Gateway`、`World` 和 `Instance`。这是架构概念清单，不代表仓库已经有同名且完整可部署的进程。当前仓库包含若干原型程序，名称保留在本节只用于说明实现现状；它们不构成另一套目标架构。

| 当前程序 | 当前作用 | 与目标模型的关系 |
|---|---|---|
| `login-app` | 认证、在线目录和落点控制面 | 托管 `OnlineDirectory`；不保存玩家游戏档案 |
| `gateway-app` | 客户端接入和消息路由原型 | 对应 `Gateway` |
| `world-app` | 模拟运行时和玩家档案宿主 | 托管 `PlayerStore`，复用 World、Scene、Instance；完整副本业务流程仍待接通 |
| `machined` | 本机进程监督原型 | 部署需要时使用，不承载游戏业务 |
| `game-server` | 单进程装配样例 | 用于展示模块装配，不代表完整生产拓扑 |
| `logger`、`admin`、`fleetctl` | 运维与管理工具原型 | 不属于副本业务模型 |

`apps/common` 是应用共用实现库，不是额外进程。`player_store::PlayerStore` 在 World 内持有 PlayerAnchor、档案、SaveQueue 和 PersistJournal；`online_directory::OnlineDirectory` 在 Login 内持有在线目录、路由投影与迁移编排。Gateway 只缓存路由并持有客户端连接。玩家游戏档案不写入 Login。两项服务使用独立链路端点，分别由所属应用启动、停止。

`HostedServicesTests` 使用真实 TCP 验证登录激活、会话绑定、Gateway 目录查询、缺失档案拒绝和停机后的档案读取。多 World 分配、迁移恢复与完整副本结算仍以 `todo.md` 的验收项为准。

副本是一次对局，`World` 是承载副本的进程；一个进程可以承载多个副本。玩家在对局中只有一个权威运行对象。跨服时迁移该对象的权威归属并更新 Gateway 路由，不复制出可同时运行的第二个权威对象。

BigWorld/KBEngine 的 `BaseApp`、`CellApp`、`BaseAppMgr` 等名字只用于引擎对照资料，不是 Apollo 的目标进程名称。详见[架构总览](../guide/architecture.md)。
