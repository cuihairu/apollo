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

本页只对照外部引擎职责，不给 Apollo 定义另一条登录或部署主线。Apollo 的目标是 gateway、loginServer、baseServer、worldServer、cell、database、machined，详见[架构总览](../guide/architecture.md)。

## 进程职责

BigWorld 的 LoginApp 提供登录入口，BaseApp 承载非空间实体与客户端代理相关逻辑，CellApp 承载空间模拟；数据库应用处理实体存储任务。客户端代理与数据库访问不是同一种职责，不能把 BaseApp 简化为数据库进程。

## 生命周期主线

从外部模型可以分别考察认证、长期实体加载、空间实体进入、运行同步、断线处理和持久化结束。具体消息、对象和进程名以对应引擎源码为准。

Apollo 采用独立 gateway 管连接，Base 管长期资料，World 管理 cell 与 Space/Cell，cell 运行模拟及 Instance，database 管实际存储。跨服只改变场景位置，保留 Base 与数据归属；协议更新可以分别替换 Base、cell 或 World 管理端。默认每 Space 一个 Cell，预留同空间分区与无缝地图；原词与现有代码见[体系对照](../design/bigworld-apollo-mapping.md)。

## 推荐阅读

- [玩家对象模型](../design/player-object-model.md)
- [Base Cell Proxy 对照](./base-cell-proxy-model.md)
- [跨服场景](../design/cross-server-scenes.md)
