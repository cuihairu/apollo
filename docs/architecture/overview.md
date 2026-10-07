---
title: 架构总览
icon: sitemap
order: 1
category:
  - 架构
tag:
  - 总览
  - 数据流
  - 职责分层
---

# 架构总览

从客户端一次进入对战到结算落档，请求沿七层下行，每层一个权威归属（全部对应仓库实件，2026-10-07 对账）：

```text
Client
  ↓
Connection      接入面（gateway ingress）
  ↓
Session         会话归属与路由（BaseAppMgr 目录 + gateway session）
  ↓
Player          玩家长期态（PlayerAnchor）
  ↓
Scene           空间运行时边界（Scene + AOI）
  ↓
Instance        副本生命周期（八态机）
  ↓
Entity / Battle 实体模拟与战斗判定（Avatar / BattleRuntime）
```

## 各层职责

### Connection —— 接入面

客户端 TCP/WebSocket 连接的受理与包分发。实件在 `apps/gateway-app`（`ingress/` 四件：`client_ingress_server` 受理、`client_packet_dispatcher` 分发、`gateway_connection_registry` 连接注册表、`session_admission_service` 准入）。边界：只认连接与字节流，不理解玩家语义。

### Session —— 会话归属与路由

「这个玩家现在在哪个 world、经哪个 gateway」的唯一裁决点。实件：`BaseAppMgr`（`apps/baseappmgr`）维护 player→SessionBinding / player→WorldAssignment 全局目录并做落点裁决（`assignWorld` / `clearWorldAssignment`）；gateway 侧 `session_manager` 维护接入会话与心跳。边界：目录与裁决，不承载玩家数据。

### Player —— 玩家长期态

跨副本持续存在的玩家档案与锚点。实件 `PlayerAnchor`（`modules/game/session`）：登录激活（`AnchorManager::activate` 幂等）、长期进度（`progress` 累计、`mark_dirty` 变更即脏）、登出回收（`deactivate`）。边界：常驻态唯一写点——战斗结算单向落这里，副本不持有长期态。

### Scene —— 空间运行时边界

AOI 所有权归场景。实件 `Scene` + `SceneAoi`（`modules/game/world`）：九宫格差集计算、`ViewerState` 逐观察者水位、Enter/Sync/Leave 事件面。边界：空间可见性权威——谁看得见谁由 Scene 说了算；Scene 上的实体经 `Avatar` 承载移动/战斗/AOI 广播三职责。

### Instance —— 副本生命周期

一局一实例的一等公民，八态单向状态机（仅相邻推进合法，终态封死）：

```text
Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed
```

实件 `Instance`（`modules/game/world`）：Waiting 窗口挂玩法负载与预进人、`start` 内部校验并进入 Running、结算尾四段单向推进。边界：生命周期与参战者收集——判定域 tick 空间归 Instance，战斗输入注入走 `BattleInput` 显式面（两者不可混用抢拍）。

### Entity / Battle —— 实体模拟与战斗判定

确定性战斗域。实件 `BattleRuntime`（`modules/game/battle`）：五段相位（Created→Entering→Battling→Rewarding→Finished）、严格递增 tick + 排序输入、`CombatRoll` 子流随机数（确定性指纹 hash 链）、结算按玩家伤害合计落 `IRewardSink`。实体身份经 `EntityId`/`PlayerId` 强分型。边界：战斗内权威——结算之后的一切归 Player 层。

## 横切关注

- **持久化**：Player 层变更即脏，`SaveQueue`/`PersistJournal` write-behind 落档（append→定额 drain→快照压薄→崩溃 replay），见[数据架构白皮书](/18-Data_Architecture_Whitepaper)
- **重连恢复**：Session 层重连窗口 + Player 锚点会话归属，见[进程架构与玩家生命周期](/architecture/bigworld-lifecycle)
- **换幕**：Scene 边界的进出经 `scene_transfer`（prepare→detach→attach→resume + 失败回滚）
- **编队与监督**：进程级编队归 machined（UDP 广播发现 + roster 拉起重启），见[服务器应用](/apps/)

## 相关文档

- [AOI九宫格系统详解](/architecture/aoi)
- [BigWorld 架构深度解析](/architecture/bigworld)
- [BigWorld 进程架构与玩家生命周期](/architecture/bigworld-lifecycle)
- [术语命名契约](/design/term-contract)
