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
| **GatewayApp** | 边缘接入层，处理客户端连接、接入校验和消息转发 | [详情](/apps/BigWorld%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E7%94%A8%E5%AE%9E%E7%8E%B0.html#gatewayapp-%E8%BE%B9%E7%BC%98%E6%8E%A5%E5%85%A5%E5%B1%82) |
| **LoginApp** | 登录入口，处理账号认证、入口分配和会话票据生成 | [详情](/apps/BigWorld%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E7%94%A8%E5%AE%9E%E7%8E%B0.html#loginapp-%E7%99%BB%E5%BD%95%E6%9C%8D%E5%8A%A1%E5%99%A8) |
| **BaseApp** | 玩家锚点宿主，处理 `PlayerAnchor`、会话归属和重连恢复 | [详情](/apps/BigWorld%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E7%94%A8%E5%AE%9E%E7%8E%B0.html#baseapp-%E7%8E%A9%E5%AE%B6%E9%94%9A%E7%82%B9%E5%AE%BF%E4%B8%BB) |
| **CellApp** | 世界运行时宿主，处理 AOI、实体、战斗和空间逻辑 | [详情](/apps/BigWorld%E6%9C%8D%E5%8A%A1%E5%99%A8%E5%BA%94%E7%94%A8%E5%AE%9E%E7%8E%B0.html#cellapp-%E6%B8%B8%E6%88%8F%E9%80%BB%E8%BE%91%E6%9C%8D%E5%8A%A1%E5%99%A8) |

## 相关文档

- [BigWorld 服务器应用实现](BigWorld服务器应用实现.md) - 完整的实现细节和使用指南
- [BigWorld 架构深度解析](/architecture/bigworld.md) - 架构设计概述
- [MMO Topology 范围与组合设计](/architecture/mmo-topology-scope-and-composition-design.md) - 两种 MMO 主拓扑
- [Distributed World Topology 实施设计](/architecture/distributed-world-topology-implementation-plan.md) - 分布式世界默认主链
- [AOI 九宫格系统详解](/architecture/aoi.md) - AOI 系统详解
