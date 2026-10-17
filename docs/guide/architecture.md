---
title: 架构总览
icon: layers
order: 7
prev: /guide/configuration
---

# 架构总览

Apollo 面向以房间和对局为边界的轻量多人游戏，例如塔防、合作玩法、竞技场和副本。一个房间承载一场玩法；一个 `RoomServer` 进程可以运行多个房间。Apollo 当前不以持久化无缝大世界为设计目标，也不采用按空间切片的进程模型。

## 目标模型

```mermaid
flowchart LR
    Client[客户端] --> Login[Login<br/>账号认证与登录凭证]
    Client <-->|游戏连接| Gateway[Gateway<br/>连接与消息路由]
    Gateway --> RoomServer[RoomServer<br/>房间承载与对局模拟]
    RoomServer --> Room[Room<br/>一场玩法实例]
    Room --> Player[Player / Entity<br/>单一权威运行对象]
    Room --> AOI[AOI]
    Room --> Battle[游戏逻辑与结算]
    RoomServer --> Store[玩家持久数据]
```

| 名字 | 职责 |
|---|---|
| `Login` | 认证账号，签发进入游戏的凭证。 |
| `Gateway` | 接受客户端游戏连接，并按会话路由消息。它不持有权威对局状态。 |
| `RoomServer` | 创建、运行和销毁房间；执行房间内权威游戏逻辑。一个进程可承载多个房间。 |
| `Room` | 一场对局的生命周期和临时状态。对局结束后回收。 |
| `Player` / `Entity` | 玩家在当前对局中的运行对象。每个玩家同一时刻只有一个权威对象。 |

跨服玩法是玩家权威对象的迁移：源 `RoomServer` 暂停该对象的权威处理，目标进程接收状态并确认接管，然后更新 Gateway 路由，最后清理源对象。迁移期间可能同时存在传输中的状态数据，但只能有一端处理请求、推进状态和提交权威写入。

## 更新边界

Gateway 负责连接和路由，不会自动提供不停机更新。RoomServer 滚动更新可通过停止分配新房间、让已有房间自然结束，再退出旧进程来实现服务持续可用；这不保证已有 TCP 连接不间断，也不保证正在进行的房间跨版本继续运行。若要求在线房间连续运行，必须另行实现并验证房间状态迁移、路由切换和失败回滚。

## 代码现状

上图是目标概念模型，不是当前部署拓扑。仓库仍有 `login-app`、`gateway-app`、`base-app`、`room-server`、`baseappmgr`、`machined` 等历史原型程序；`room-server` 是已迁移到目标职责名称的原型程序，目前仍未实现完整的多 Room 生命周期。各程序的实际代码状态见[服务器应用](../apps/index.md)。BigWorld/KBEngine 术语只在[兼容层和引擎对照文档](../33-BigWorld_Compatibility.md)中作为参考。
