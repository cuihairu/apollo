---
title: 服务器应用
icon: server
order: 5
category:
  - 应用
tag:
  - BigWorld
  - 服务器
  - 应用
---

# 服务器应用

Apollo 项目提供了一系列可装配的服务器应用程序，用于实现两种 MMO 主拓扑：

- `Standard MMO`
- `Distributed World`

需要先说明两个边界：

- `BaseApp` 不是数据库服务器
- `GatewayApp` 不是 BigWorld / KBEngine 语义里的默认必选核心

更准确地说：

- 普通 MMO 默认主链是 `LoginApp -> GatewayApp -> BaseApp(PlayerAnchor) -> WorldApp`
- 分布式世界默认主链是 `LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp`

## 应用列表

| 应用 | 说明 | 文档 |
|------|------|------|
| **GatewayApp** | 边缘接入层，处理客户端连接、接入校验和消息转发 | [详情](/apps/BigWorld服务器应用实现) |
| **LoginApp** | 登录入口，处理账号认证、入口分配和会话票据生成 | [详情](/apps/BigWorld服务器应用实现) |
| **BaseApp** | 玩家锚点宿主，处理 `PlayerAnchor`、会话归属和重连恢复 | [详情](/apps/BigWorld服务器应用实现) |
| **CellApp** | 世界运行时宿主，处理 AOI、实体、战斗和空间逻辑 | [详情](/apps/BigWorld服务器应用实现) |

## 相关文档

- [BigWorld 服务器应用实现](BigWorld服务器应用实现.md) - 完整的实现细节和使用指南
- [BigWorld 架构深度解析](/architecture/bigworld) - 架构设计概述
- [BigWorld 进程架构与玩家生命周期](/architecture/bigworld-lifecycle) - 进程职责与生命周期
- [MMO Topology 范围与组合设计](/architecture/mmo-topology-scope-and-composition-design) - 两种 MMO 主拓扑
- [Distributed World Topology 实施设计](/architecture/distributed-world-topology-implementation-plan) - 分布式世界实施桥接
- [AOI 九宫格系统详解](/architecture/aoi) - AOI 系统详解
