---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: 轻量副本制多人在线游戏服务器引擎
  tagline: C++20 运行时，面向塔防、合作玩法、竞技场和副本；MMO 是可能的产品形态，不以无缝大世界为目标。
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: 架构总览
      link: /guide/architecture
    - theme: alt
      text: 概念与命名
      link: /design/concept-glossary
features:
  - title: 副本与对局
    details: 一个 Instance 对应一场玩法；World 进程可承载多个 Instance，对局结束后回收临时状态。
  - title: 单一权威玩家对象
    details: 玩家在对局中只有一个权威运行对象。跨服玩法迁移对象状态、接管权威并更新 Gateway 路由。
  - title: 清晰的接入职责
    details: Login 负责账号认证，Gateway 负责客户端连接与消息路由，World 负责对局模拟。
  - title: 现代 C++20 模块
    details: 网络、数据、实体契约、AOI、战斗和运行时模块；README 与模块文档说明当前实现状态。
---

## 项目定位

Apollo 是轻量副本制多人在线游戏服务器引擎，适合塔防、合作 PvE/PvP、竞技场和副本。MMO 是可能的产品规模或产品形态，不是服务端架构名称；Apollo 当前不以持久化无缝大世界为目标。

目标链路为 `Client → Gateway → World → Instance`。一个 Instance 是一场对局；一个 World 可以运行多个 Instance。跨服玩法迁移玩家对象的权威归属，同一时刻只有一个进程能够处理该对象的权威操作。

## 文档入口

| 主题 | 文档 |
|---|---|
| 目标架构 | [架构总览](/guide/architecture) |
| 目标名称与边界 | [概念与命名](/design/concept-glossary) |
| 当前程序原型 | [服务器应用](/apps/) |
| World 和跨服迁移设计 | [副本制 GameServer 设计](/30-Instance_GameServer_Design) |
| 代码构建 | [构建指南](/BUILDING) |
| 外部引擎对照 | [MMO 框架比较](/36-MMO_Frameworks_Comparative_Analysis) |

BigWorld/KBEngine 的 Base/Cell 等名称保留在外部引擎比较资料中，只用于解释那些引擎的实现，不作为 Apollo 的目标架构术语。
