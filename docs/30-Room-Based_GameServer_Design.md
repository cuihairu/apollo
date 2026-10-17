[English](30-Room-Based_GameServer_Design.en.md) | [中文](30-Room-Based_GameServer_Design.md)

# 房间制 GameServer 设计

本文定义 Apollo 面向塔防、合作玩法、竞技场和副本的目标运行模型。房间（Room）是一场对局及其临时状态；RoomServer 是承载房间的进程，一个 RoomServer 可以运行多个 Room。

## 1. 进程与对象职责

| 名称 | 职责 |
|---|---|
| `Login` | 账号认证与登录凭证。 |
| `Gateway` | 客户端连接、会话关联和消息路由。 |
| `RoomServer` | 创建、运行、销毁房间并执行权威游戏逻辑。 |
| `Room` | 一场玩法实例；包含场景实体、AOI、战斗和临时对局数据。 |
| 玩家持久数据 | 保存跨对局数据；由持久化模块提供，不要求独立进程。 |

`Player` 是玩家身份，`Entity` 是玩法运行对象。玩家进入对局后由该房间所在的 RoomServer 持有唯一权威运行对象；结束后按游戏流程保存结算并销毁对局对象。

## 2. 运行流程

```text
Client → Login（认证）
Client → Gateway（游戏连接）→ RoomServer → Room
Room：实体 / AOI / 游戏逻辑 / 结算
```

匹配或房间创建决定目标 RoomServer 和 Room。Gateway 根据会话路由请求。房间结束时提交结算结果并释放临时状态。

## 3. 跨服迁移

跨服玩法将玩家对象的权威从源 RoomServer 转交给目标 RoomServer：源端冻结对象并产生迁移状态；目标端恢复并确认接管；路由目录与 Gateway 切换到目标端；源端删除对象。失败时必须保留或恢复源权威，不能出现两个进程同时接受该玩家操作。迁移机制必须对请求去重并明确不可重放的业务写入。

“复制对象”只表示传递对象状态，不表示允许两个可运行的玩家对象。传输中的快照可以短暂同时存在于两端，权威执行者始终只有一个。

## 4. 扩展与更新

扩展容量时增加 RoomServer 实例并将新建房间分配给可用进程。无需把单个地图或世界切成多个空间进程。

RoomServer 可通过停止接收新房间、等待当前房间结束后退出，进行滚动更新。若要求对局不中断，就必须另外实现房间状态迁移、版本兼容、路由切换、失败回滚并进行端到端验证。Gateway 多实例可以让新连接继续接入；既有 TCP 连接如何跨 Gateway 进程存续，需要独立连接交接机制，不能由“有 Gateway”推导出来。

## 5. 当前实现边界

本设计是目标模型。当前仓库仍有 `base-app`、`cell-app`、`baseappmgr` 等原型程序；这些名字及其拆分方式不构成目标概念。实现状态见[服务器应用](apps/index.md)，当前总体架构见[架构总览](guide/architecture.md)。BigWorld/KBEngine 的 Base/Cell 进程只在对照材料中作为外部引擎术语。
