---
layout: home
title: Apollo
titleTemplate: false
hero:
  name: Apollo
  text: 场景实例制在线游戏服务端引擎
  tagline: C++20 内核 + Lua 业务层——副本（instance）/Zone 粒度的 MMO 框架；塔防/房间制玩法一等公民（Compact 形态）。
  image:
    src: /apollo.png
    alt: Apollo
  actions:
    - theme: brand
      text: 术语表（新读者首站）
      link: /design/concept-glossary
    - theme: alt
      text: 塔防/Compact 形态
      link: /30-Compact_GameServer_Design
features:
  - title: 场景实例制（Zone/副本）
    details: 场景实例粒度 Zone 承载实体权威，不拆 cell/base、不做 ghost 无缝世界；副本（instance）短生命周期独立可配，房间制玩法即副本形态。
  - title: 网络四层自建
    details: L0 epoll reactor → L1 帧（magic+seq+CRC32C）→ L2 会话（四通道 QoS/四级水位/resume 重连续传）→ L3 GameConnection；进程间 InterServerLink 稳定连接一族。
  - title: 契约一源多端
    details: XML+XSD 契约（attrs/messages/entities/errors）+ 生成器独立二进制；8 位可见域掩码、schema_hash 握手、按域分段 id——服务端契约变更零重编。
  - title: Lua 5.5 白名单业务层
    details: 原生 C API 绑定（版本随 vcpkg）；脚本可写面由契约生成；热更走换表协议（冒烟+回滚）；线程模型单写者、协程 tick 边界 resume。
  - title: write-behind journal 持久化
    details: 先追加日志后快照两段式；journal 同为 G-2 热备镜像流与崩溃恢复位点；storage.xml 语句即数据（MySQL 8 主存）。
  - title: 集群 G-1/G-2（P3 前置已定）
    details: 进程发现 = machined 守护 + UDP 广播双层（不引 etcd/consul）；G-2 案 A 先行 restart-only（standby/reviver 未立项，ADR-010）；恢复相位排他；指标同源 G-5。
---

## 当前定位与架构现状

**形态**：单进程起步（P1-P2），集群能力 P3 接入——分期是显式声明，不是遗漏。现行拓扑：

```text
Client ── Gate（透传/解码装配分工）
          └─ World / manager 域（最轻分配、准入闸门、恢复相位）
              ├─ Zone ×N（场景实例粒度进程，单写者 tick 10Hz）
              ├─ AOI 服务（网格+四叉树+shard，可独立扩缩；Compact 形态内嵌）
              ├─ 副本实例（instance，可选独立进程；塔防房间 = 内嵌副本）
              └─ db-app（storage.xml + write-behind journal）
进程发现：machined 守护 + UDP 广播（G-1，不引注册中心）    G-2：案 A 先行 restart-only（standby/reviver 未立项，ADR-010）
```

**已定关键裁决**（全表见 36 号 §1 决策追溯表 #1-#19）：网络四层自建/nng 退役；契约 XML+XSD；副本制否决无缝世界（#9）；AOI 独立服务（#10）；Lua 白名单否决 Python（#12）；进程发现 machined+UDP 双层、不引 etcd/consul（#13，2026-09-30）；Lua 5.5 随 vcpkg、弃 sol2（#12 修订）；帧同步只留适配缝（#18）。

**设计缺口排程**：design-gap-inventory #1-#11 已全部 CLOSED；#12 会话与在线目录（[session-and-online-directory](/design/session-and-online-directory)）与 #17 战斗验证服务（[battle-verification-service](/design/battle-verification-service)）已落盘 CLOSED；**#13-#16 OPEN**（登录链路/入站第三方对接/Bots 压测/地图空间数据管线——均以 #12 目录为前置）。

## 文档地图（权威分级）

| 层 | 位置 | 内容 |
|------|------|------|
| **术语基座** | [design/concept-glossary](/design/concept-glossary) | 通用概念 × 出现引擎 × apollo 立场；三条术语裁决（副本 instance 定名/battle 词留给战斗验证域/不引注册中心）——**新概念先入表再落文档** |
| **权威设计** | docs/design/（十二份） | net-abstraction（网络四层+集群前置）、sdk-contract（契约）、attribute-sync（属性/同步/journal）、scripting-lua（Lua）、xml-generation（生成器）、logging、clock-and-time、battle-determinism（回放/复算）、battle-verification-service（战斗验证/权威结算）、session-and-online-directory（会话与在线目录/顶号/掉线保活）、capacity-and-benchmark、concept-glossary |
| **对比分析** | docs/analysis/ | architecture-review（全仓审计+登记簿，判定权威）、mmo-mechanism-deep-dive（36 号 #1-#18 实现级取证）、design-gap-inventory（缺口账本）、ssengine-reference |
| **框架对比** | [36-MMO_Frameworks_Comparative_Analysis](/36-MMO_Frameworks_Comparative_Analysis) | 十框架对比 + 决策追溯表 #1-#19（apollo 每项设计的出处与理由） |
| **轻量形态** | [30-Compact_GameServer_Design](/30-Compact_GameServer_Design) | 塔防/固定地图精简形态（AOI 内嵌/副本内嵌/单机多线程） |
| **批次计划** | [todo](/todo) | 批次 1-9（契约已交付；db-app/cell-appmgr/脚本/SDK/监控排程） |
| **参考件区** | [architecture/README](/architecture/README) | 70+25 份全量状态表（A 实引 4 份/B 引擎分析/C 被取代稿/D 历史路线图）——**该目录不读作 apollo 现行设计** |
| **QA 答疑** | docs/qa/（130 问）、[QA](/QA) | 面试级问答库（BW/KBE 机制分析结论，参考件） |

其余目录（guide/、api/、modules/、apps/、sdks/、public/）为早期手册与模块说明，随对应代码批次重写；根目录编号文档（03/18/32-36）状态以各文档头部标注与 36 号口径为准。已删文档（00/04/05/17/25 等）git 历史可溯。

## 快速上手（读者路线）

1. [术语表](/design/concept-glossary) → 2. [36 号决策追溯表](/36-MMO_Frameworks_Comparative_Analysis)（为什么这样设计）→ 3. docs/design/ 六份核心（net-abstraction/sdk-contract/attribute-sync/scripting-lua/logging/clock-and-time）→ 4. [architecture-review §16.10.2 登记簿](/analysis/architecture-review)（什么还没做、等什么）→ 5. [todo](/todo)（批次排程）。
