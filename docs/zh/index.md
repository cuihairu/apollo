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
    details: Base 长期对象与 Cell 模拟实体分开；每个实体只有一个模拟权威，跨服保持存储归属，Base 承载可独立迁移。
  - title: 清晰的接入职责
    details: login 管认证与目录，gateway 管连接，base 管长期资料，world 管拓扑与执行端，cell 管模拟，database 管存储，machined 管本机监督。
  - title: 现代 C++20 模块
    details: 网络、数据、实体契约、AOI、战斗和运行时模块；README 与模块文档说明当前实现状态。
---



## 目标架构

业务消息经 gateway 直达所属 Base 或当前 cell；world 管理分配与拓扑。实线表示业务请求与结果，虚线表示注册与管理。

<ArchitectureDiagram kind="system" />

## 外部 HTTP 回调与开发入口

支付和渠道回调由可选 interfaces 的 HTTP/HTTPS 入口接收；可靠保存后，按稳定玩家身份解析当前 Base，由 Base 校验并经 database 幂等发货。玩家离线不依赖 gateway 会话，Base 承载迁移后重新解析归属。游戏 WS/WSS 属于 gateway 的游戏消息入口，与 TCP 共用同一条已验收的游戏帧路径，不进入 HTTP handler。

<ArchitectureDiagram kind="callbacks" />

实现状态：gateway 游戏 TCP/WS/WSS 入口已验收（含 WSS 证书链/主机名错误拒绝与密钥配置负例）；interfaces 受理链（原始请求帧逐字节保留、请求超时与受理队列预算、有界单写者）与渠道验签 gate（HMAC-SHA256 按剥离签名头行的原始帧验签，缺签/改签/改体 401，签名格式与体长声明错误 403，摘要定长比较）、订单事务域（持久受理与待办、按归属代次解析当前 Base、发货去重凭据原子提交）与 apolloc init 脚手架均已落地并有测试覆盖；渠道回调 → 订单受理接线与发货 executor、base 写权生命周期（奖励落账）仍规划中。对照证据见 [Mistlane 与旧 FOGS 核对](./analysis/mistlane-review.md)，开发任务见根目录 todo.md。

## 进程、工具与 CI

- 运行进程：gateway、login、base、world、database、interfaces、machined；cell 为 world 二进制的执行角色。gateway 提供游戏 TCP/WS/WSS 入口（已验收），interfaces 提供 HTTP/HTTPS 回调入口（受理链已落地）。
- CLI 工具：apolloc（项目初始化 init，已交付）、fleetctl（编队量测断言）、admin 与 logger（观测辅助）——都是开发工具，不是在线业务进程。
- CI：GitHub Actions（构建/测试、契约、文档）在仓库执行，不参与在线业务。

## 项目定位

Apollo 是轻量副本制多人在线游戏服务器引擎，适合塔防、合作 PvE/PvP、竞技场和副本。MMO 是可能的产品规模或产品形态，不是服务端架构名称；当前不实现无缝地图，预留同 Space 多 Cell、邻接可见性与权威交接。

目标链路：客户端经 gateway 分别访问所属 Base 的基础业务和当前 cell 的场景模拟；Base 经 database 读写长期资料，login 提供认证与目录，machined 按需监督本机进程。跨服只改变场景位置，保留原存储归属，Base 承载按独立调度更新。

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

BigWorld/KBEngine 的 BaseApp、CellApp 等真实名称保留用于外部比较。Apollo 固定使用 base 管长期资料、world 管 Space/Cell，cell 管模拟，完整名称见词汇表。
