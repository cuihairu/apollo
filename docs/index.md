---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: 轻量在线游戏服务端引擎
  tagline: 面向轻量 MMO 与塔防/固定地图玩法的 C++20 服务端底座，可按项目规模升级到普通 MMO 与 BigWorld 增强模式。
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: 架构适配判断
      link: /architecture/lightweight-mmo-and-tower-defense-fit
    - theme: alt
      text: 快速开始
      link: /guide/quick-start
features:
  - title: 轻量 MMO 优先
    details: 以 Login、Gateway、PlayerAnchor、WorldHost、Persistence 的主链支撑分线、地图实例、副本和轻社交。
  - title: 塔防可用
    details: 通过 Compact GameServer 将 Scene、AOI、Battle、Wave Runtime 合并在一个低运维成本的游戏进程内。
  - title: 渐进式装配
    details: L1-L8 作为默认稳定层，L9 BigWorld 分布式世界能力只在连续大世界项目中启用。
  - title: 模块化 C++20
    details: Base、Core、Runtime、Net、Data、Game 等模块按能力拆分，避免业务域反向污染底层内核。
  - title: 客户端协议稳定
    details: 对外 Client Protocol、Session Protocol 与内部 Envelope、Replication Protocol 分离，便于版本演进。
  - title: 工程治理内置
    details: 配置、日志、生命周期、可观测性、测试策略和部署 Profile 统一纳入框架设计。
---

## 当前定位

Apollo 不再只定义为一套重型 MMORPG 服务器框架，而是一套面向在线游戏的渐进式服务端引擎。

默认目标是两类项目：

- 轻量 MMO：分线世界、固定地图、多人在线、副本、社交和基础运营系统。
- 塔防 / 固定地图：波次、防守、建造、技能、怪物路径和局内状态同步。

连续大世界、跨 partition 权威迁移、Witness / Ghost 等 BigWorld 能力仍然保留，但它们是增强层，不是起步必选项。

## 推荐阅读

| 入口 | 说明 |
|------|------|
| [轻量 MMO 与塔防适配判断](/architecture/lightweight-mmo-and-tower-defense-fit) | 判断这两类游戏是否适合 Apollo 当前架构 |
| [架构概述](/architecture/overview) | 查看整理后的整体分层和 Profile |
| [Compact GameServer](/30-Compact_GameServer_Design) | 塔防、固定地图、轻量玩法的精简服务端形态 |
| [Shard / Zone / Instance / Match](/architecture/shard-zone-instance-match-topology-design) | 多游戏形态下的拓扑术语 |
| [快速开始](/guide/quick-start) | 构建并运行第一个 Apollo 游戏服务器 |

## 装配 Profile

```text
Profile A: Tower Defense Compact
Client -> Gateway -> CompactGameServer(Scene + AOI + Battle + Wave) -> Persistence

Profile B: Lightweight MMO
Client -> Login -> Gateway -> BaseApp(PlayerAnchor) -> WorldHost -> Persistence

Profile C: Distributed MMO
Client -> Login -> Gateway -> BaseApp -> Cell/World Partition -> AppMgr -> Persistence
```

Profile A 和 Profile B 是当前优先收敛方向。Profile C 只在明确需要连续大世界、空间切片和权威迁移时启用。
