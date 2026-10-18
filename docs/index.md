---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: 轻量副本制多人在线游戏服务器引擎
  tagline: C++20 运行时，面向塔防、合作玩法、竞技场和副本；MMO 是可能的产品形态，当前优先副本玩法，预留无缝地图设计。
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
    details: 一个 Instance 对应一场玩法；cell 进程可承载多个 Space/Cell 及 Instance，对局结束后回收临时状态。
  - title: 长期资料与模拟分权
    details: Base 长期对象与 Cell 模拟实体分开；每个实体只有一个模拟权威，跨服保持原 Base。
  - title: 清晰的接入职责
    details: loginServer 管认证与目录，gateway 管连接，baseServer 管长期资料，worldServer 管拓扑与执行端，cell 管模拟，database 管存储，machined 管本机监督。
  - title: 现代 C++20 模块
    details: 网络、数据、实体契约、AOI、战斗和运行时模块；README 与模块文档说明当前实现状态。
---



## 项目定位

Apollo 是轻量副本制多人在线游戏服务器引擎，适合塔防、合作 PvE/PvP、竞技场和副本。MMO 是可能的产品规模或产品形态，不是服务端架构名称；当前不实现无缝地图，预留同 Space 多 Cell、邻接可见性与权威交接。

目标链路：客户端经 gateway 分别访问所属 Base 的基础业务和当前 cell 的场景模拟；Base 经 database 读写长期资料，loginServer 提供认证与目录，machined 按需监督本机进程。跨服只改变场景位置，保留 Base 和数据归属。

## 文档入口

| 主题 | 文档 |
|---|---|
| 目标架构 | [架构总览](/guide/architecture) |
| 目标名称与边界 | [概念与命名](/design/concept-glossary) |
| 当前程序原型 | [服务器应用](/apps/) |
| World 和跨服迁移设计 | [副本制 GameServer 设计](/30-Instance_GameServer_Design) |
| 代码构建 | [构建指南](/BUILDING) |
| BigWorld 与 Apollo 体系对照 | [职责、概念与代码映射](/design/bigworld-apollo-mapping) |
| 外部引擎对照 | [MMO 框架比较](/36-MMO_Frameworks_Comparative_Analysis) |

BigWorld/KBEngine 的 BaseApp、CellApp 等真实名称保留用于外部比较。Apollo 固定使用 baseServer 管长期资料、worldServer 管 Space/Cell，cell 管模拟，完整名称见词汇表。
