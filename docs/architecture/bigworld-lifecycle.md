---
title: BigWorld 进程架构与玩家生命周期
icon: flow
order: 31
category:
  - 架构
  - BigWorld
tag:
  - BigWorld
  - 进程架构
  - 玩家生命周期
  - 登录流程
---

# BigWorld 进程架构与玩家生命周期

这是一页收口版入口，用来把 BigWorld 语义下的进程职责讲清楚。

当前 Apollo 更推荐的主链不是“BaseApp 做数据库服务器”，而是：

`Client -> LoginApp -> BaseApp(PlayerAnchor + Proxy) -> CellApp`

如果需要再叠加边缘接入治理，才引入独立 `GatewayApp`。

## 进程职责

- `LoginApp`：认证、入口分配、票据生成
- `BaseApp`：玩家长期在线主状态、会话归属、重连恢复
- `GatewayApp`：连接管理、路由和边缘防护
- `CellApp`：空间内实时逻辑、AOI、战斗、跨边界迁移

## 生命周期主线

1. 登录
2. 激活 `PlayerAnchor`
3. 分配世界/空间入口
4. 进入 `CellApp`
5. 运行时同步
6. 断线保护与恢复
7. 持久化收口

## 推荐阅读

- [玩家在线主链设计](./player-online-flow.md)
- [World 进入与切图设计](./world-entry-transfer-design.md)
- [Gateway 会话设计](./gateway-session-design.md)
- [PlayerAnchor 设计稿](./player-anchor-design.md)
- [BaseApp 演进设计](./base-app-evolution.md)

## 旧版长文

- [BigWorld 进程架构与玩家生命周期（旧版）](../BigWorld进程架构与玩家生命周期.md)
