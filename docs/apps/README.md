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

Apollo 项目提供了一系列开箱即用的服务器应用程序，这些应用基于 BigWorld 架构设计，可以单独部署或组合使用。

## 应用列表

| 应用 | 说明 | 文档 |
|------|------|------|
| **GatewayApp** | 网关服务器，处理客户端连接和消息路由 | [详情](/apps/BigWorld服务器应用实现.md#gatewayapp-网关服务器) |
| **LoginApp** | 登录服务器，处理账号认证和网关分配 | [详情](/apps/BigWorld服务器应用实现.md#loginapp-登录服务器) |
| **BaseApp** | 数据库服务器，处理数据持久化和缓存 | [详情](/apps/BigWorld服务器应用实现.md#baseapp-数据库服务器) |
| **CellApp** | 游戏逻辑服务器，处理游戏逻辑和 AOI | [详情](/apps/BigWorld服务器应用实现.md#cellapp-游戏逻辑服务器) |

## 相关文档

- [BigWorld 服务器应用实现](BigWorld服务器应用实现.md) - 完整的实现细节和使用指南
- [BigWorld 架构深度解析](/architecture/bigworld.md) - 架构设计概述
- [AOI 九宫格系统详解](/architecture/aoi.md) - AOI 系统详解
