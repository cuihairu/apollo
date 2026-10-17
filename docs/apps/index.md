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
| `login-app` | 登录服务原型 | 对应 `Login` |
| `gateway-app` | 客户端接入和消息路由原型 | 对应 `Gateway` |
| `world-app` | 使用 World 目标命名的场景/对局运行时原型 | World、Scene、Instance 状态机已存在；小队创建、通关结算与资源回收尚未端到端接通 |
| `base-app` | 玩家状态、会话和持久化原型 | 相关职责尚需按副本模型收敛；不作为必须独立存在的目标进程 |
| `baseappmgr` | 玩家目录与落点管理原型 | 目标架构没有要求必须保留独立同名管理进程 |
| `machined` | 本机进程监督原型 | 部署需要时使用，不承载游戏业务 |
| `game-server` | 单进程装配样例 | 用于展示模块装配，不代表完整生产拓扑 |
| `logger`、`admin`、`fleetctl` | 运维与管理工具原型 | 不属于副本业务模型 |

`base-app` 当前承载 PlayerAnchor 和持久化职责；`baseappmgr` 含目录与落点原型。这些现有职责的收敛需要明确设计和测试，进程改名不会自动把持久写入转交给 Login，也不增加目标进程。持久玩家资料与对局中的单一权威实体职责分开。

副本是一次对局，`World` 是承载副本的进程；一个进程可以承载多个副本。玩家在对局中只有一个权威运行对象。跨服时迁移该对象的权威归属并更新 Gateway 路由，不复制出可同时运行的第二个权威对象。

BigWorld/KBEngine 的 `BaseApp`、`CellApp`、`BaseAppMgr` 等名字只用于引擎对照资料，不是 Apollo 的目标进程名称。详见[架构总览](../guide/architecture.md)。
