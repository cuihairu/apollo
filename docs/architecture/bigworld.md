---
title: BigWorld 架构深度解析
icon: server
order: 30
category:
  - 架构
  - BigWorld
tag:
  - BigWorld
  - 兼容层
  - BaseApp
  - CellApp
---

# BigWorld 架构深度解析

这是一页收口版入口。

BigWorld 在 Apollo 中是可选的分布式世界增强层，不是默认起步形态。当前主线仍然是 `Standard MMO` 和 `Compact GameServer`。

## 语义边界

- `BaseApp` 更接近 `PlayerAnchor + Proxy Host`
- `CellApp` 更接近空间实时权威节点
- `LoginApp` 负责认证与入口分配
- `GatewayApp` 只在 Apollo 的边缘接入装配里启用

## 兼容层

Apollo 已有 BigWorld 兼容层实现，见：

- [BigWorld Compatibility Layer](../33-BigWorld_Compatibility.md)

## 推荐阅读

- [BigWorld 进程架构与玩家生命周期](./bigworld-lifecycle.md)
- [Witness 与 Ghost 设计](./witness-ghost-design.md)

## 旧版长文

- [BigWorld 架构深度解析（旧版）](../BigWorld架构深度解析.md)
