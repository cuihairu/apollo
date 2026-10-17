[English](session-and-online-directory.en.md) | [中文](session-and-online-directory.md)

# 会话与在线路由

本文定义房间制架构下的会话和玩家位置数据。目标拓扑与命名以[架构总览](../guide/architecture.md)和[概念与命名](./concept-glossary.md)为准。

## 职责

- `Gateway` 维护客户端连接和会话，并把玩家消息发往当前 RoomServer。
- `RoomServer` 是当前房间内玩家对象的权威持有者。
- 在线路由数据回答“玩家在哪个 Gateway、RoomServer 和 Room”，用于消息转发和跨服迁移。
- `Login` 验证账号并签发登录凭证，不负责承载活动对局。

在线路由是数据职责，不预设一个独立 `Manager` 进程。路由表的具体所有者、存储方式和高可用协议，需在实现集群接线前确定；不得从旧的 `baseappmgr` 原型推导为目标拓扑。

## 路由记录

记录至少需要将 `player_id` 关联到当前 `session_id`、`gateway_id`、`room_server_id` 和 `room_id`。重复登录或连接恢复时使用会话代次/版本拒绝旧连接的迟到消息。离线或对局结束时清理对应记录。

目录只保存寻址所需的路由状态，不成为玩家游戏数据的第二权威副本。玩家对象状态由当前 RoomServer 权威持有，跨对局数据由持久化能力保存。

## 跨服迁移

跨服迁移应按一次权威交接执行：

1. 源 RoomServer 暂停玩家对象的权威处理并生成迁移状态。
2. 目标 RoomServer 恢复状态并确认接管。
3. 更新在线路由和 Gateway 转发目标。
4. 源端删除旧对象并确认迁移完成。

迁移状态在途时可短暂存在两端，但旧端必须冻结且不得处理玩家输入或提交权威写入。目标拒绝或超时则恢复源端权威；路由更新失败不得让两个端点同时对外服务。

迁移请求需有唯一标识，目标端以此去重；迁移后业务写入继续使用自身幂等语义。可靠传输不等同业务恰好执行一次。

## Gateway 边界

Gateway 依据在线路由转发消息。多 Gateway 可承载不同客户端连接；单个连接由其所在 Gateway 持有。更新 Gateway 时，旧实例可停止接收新连接并继续服务现有连接直至排空。把既有 TCP 连接移到另一 Gateway 需要专门的连接交接机制；路由表切换本身不会迁移 socket。

## 实现状态

仓库有 `SessionLocator`、`PlayerDirectory`、`WorldAssignment` 和 `baseappmgr` 原型代码。这些是现有实现部件，不等于目标模型已经确定了目录进程或存储方案。实际跨进程消息路由、Gateway 接管和端到端迁移仍需按本文目标验收。
