[English](36-MMO_Frameworks_Comparative_Analysis.en.md) | [中文](36-MMO_Frameworks_Comparative_Analysis.md)

# 36. 多人游戏服务器常见名称对照

本文只解释其他游戏服务器/引擎中实际使用的名称及其职责，帮助开发者阅读现有架构。表格中的“近似”表示职责重叠，不表示实现相同。Apollo 的目标名称见[概念与命名](design/concept-glossary.md)，目标拓扑见[架构总览](guide/architecture.md)。

## 按项目对照

### KBEngine

| 名称 | 职责 |
|---|---|
| `loginapp` | 登录入口与认证流程协调。 |
| `baseapp` | Base 实体和客户端代理相关逻辑；承载非空间实体。 |
| `cellapp` | Space 内实体与空间逻辑。 |
| `baseappmgr` | BaseApp 选择、负载信息和登录分配协调。 |
| `cellappmgr` | CellApp 选择、Space/Cell 创建与恢复协调。 |
| `dbmgr` | 数据库访问和实体持久化协调。 |
| `machine` | 机器发现与进程管理辅助。 |

KBEngine 的 Base/Cell 是其实体与空间模型的一部分。Apollo 不以这些词命名目标进程。

### BigWorld

| 名称 | 职责 |
|---|---|
| `loginapp` | 登录入口。 |
| `baseapp` | Base 实体、客户端代理和玩家非空间状态。 |
| `cellapp` | Cell 实体与 Space 空间模拟。 |
| `baseappmgr` | BaseApp 分配与负载管理。 |
| `cellappmgr` | CellApp/Space 管理和恢复协调。 |
| `dbapp` / `dbappmgr` | 数据库任务处理及其管理。 |
| `bwmachined` | 机器发现、监控与进程管理支持。 |

BigWorld 的 Cell/ghost 机制服务于跨 Cell 的连续空间；这些名称仅作为 BigWorld 术语使用。

### TrinityCore / AzerothCore

| 名称 | 职责 |
|---|---|
| `authserver` | 账号认证与 Realm 入口。 |
| `worldserver` | Realm 运行时，承担玩家连接和大量游戏世界逻辑。 |

这是公开开源实现中的命名，不能据此推断商业 MMO 的内部拓扑。

### EQEmu

| 名称 | 职责 |
|---|---|
| `loginserver` | 登录服务。 |
| `world` | 世界级协调和客户端/Zone 连接管理。 |
| `zone` | 运行一个游戏区域及其相关玩法。 |
| `ucs` | 独立通信服务。 |

`zone` 在 EQEmu 中是其区域运行单元的名称；它不是 Apollo 的进程名。

### Pomelo / LordOfPomelo

| 名称 | 职责 |
|---|---|
| `gate` | 初始接入并选择 Connector。 |
| `connector` | 保持客户端连接、维护会话并转发消息。 |
| `area` | 地图区域的实体和游戏逻辑。 |
| `master` | 配置和管理服务进程。 |

Pomelo 的 `gate` 与 `connector` 是两个角色；`connector` 的职责更接近通常所说的客户端 Gateway。

### Ryzom Core

| 名称 | 职责 |
|---|---|
| `FES` (Front End Service) | 客户端接入和消息前端。 |
| `EGS` (Entities Game Service) | 实体和核心游戏逻辑。 |
| `AIS` (AI Service) | AI 逻辑。 |
| `MS` (Mirror Service) | 服务间实体数据镜像。 |
| `NS` (Naming Service) | 服务发现/命名。 |
| `AES` (Administration Executor Service) | 服务启停与管理。 |

这些缩写属于 Ryzom 的服务划分，不建议移植为 Apollo 的额外概念。

### Skynet

Skynet 提供通用服务运行时，本身不规定 MMO 进程拓扑。其 Gate 示例处理客户端连接和消息转发；Agent、Watchdog 等名称来自示例服务组织方式，具体职责由应用定义。

### Colyseus 与 Photon

Colyseus 以 `Room` 表达一个有状态的多人会话/对局，Room 由游戏服务器进程承载。Photon 常用 `Master Server` 负责会话发现/分配、`Game Server` 承载房间与对局。两者的 `Room` 都是玩法会话概念，不是服务器进程名称。

### Agones

Agones 的 `GameServer`、`Fleet` 和 `FleetAutoscaler` 是 Kubernetes 上的游戏服务器实例及其编排资源名称。它解决部署和容量编排问题，不定义玩家对象或游戏房间的业务模型。

## Apollo 采用的常见名称

| Apollo 目标名称 | 职责 | 其他项目中相近的概念 |
|---|---|---|
| `Login` | 账号认证和登录凭证。 | `loginapp`、`authserver`、`loginserver` |
| `Gateway` | 客户端游戏连接和消息路由。 | Pomelo `connector`、Ryzom `FES`、Skynet Gate |
| `RoomServer` | 承载多个房间并运行对局权威逻辑。 | Colyseus 的游戏服务器进程、Photon `Game Server` |
| `Room` | 一场对局及其临时状态。 | Colyseus/Photon `Room`、匹配实例 |

Apollo 不再为单一房间/对局运行时并列使用 `CellApp`、`ZoneServer`、`AreaServer`、`SceneServer` 等名称。空间对象在代码层可使用 `Scene`；对外的玩法和承载单位分别称 `Room` 与 `RoomServer`。一个 RoomServer 可以承载多个 Room。

## Gateway 与热更新

`Gateway` 是常见的接入职责名，不代表它天然支持不停机更新。多 Gateway 可以在旧实例停止接收新连接后继续由新实例接入新玩家；既有 TCP 连接要跨进程保持，需要连接交接或让旧实例保留连接直至排空。活动房间跨版本继续运行还需要房间状态迁移、版本兼容、权威接管、路由切换和失败回滚。以上是不同机制，必须分别验证。

## 参考来源

- [KBEngine entity definitions](https://kbengine.github.io/docs/programming/entitydef.html) and [startup/shutdown](https://kbengine.github.io/docs/startup_shutdown.html)
- [TrinityCore networking](https://trinitycore.info/install/Networking)
- [EQEmu server installation](https://github.com/EQEmu/eqemu-docs-v2/blob/main/docs/server/installation/server-installation-windows.md)
- [Pomelo terminology](https://github.com/NetEase/pomelo/wiki/Terminologies) and [LordOfPomelo server introduction](https://github.com/NetEase/pomelo/wiki/LordOfPomelo-server-introduction)
- [Ryzom Core services](https://github.com/ryzom/ryzomcore/wiki/architecture_ryzom_services)
- [Skynet GateServer](https://github.com/cloudwu/skynet/wiki/GateServer)
- [Colyseus rooms](https://docs.colyseus.io/room/)
- [Agones concepts](https://agones.dev/site/docs/)
