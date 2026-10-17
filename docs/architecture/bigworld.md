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

Apollo 的 BigWorld 兼容模块提供熟悉的 C++ API facade，不实现 BigWorld 的分布式无缝世界运行时。Apollo 目标架构是房间制多人游戏服务端，详见[架构总览](../guide/architecture.md)。

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
