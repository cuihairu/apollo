---
title: 架构决策记录（ADR-001..009）
icon: gavel
order: 2
category:
  - 架构
tag:
  - ADR
  - 决策记录
---

# 架构决策记录（ADR-001..009）

> 目的（任务书 §36）：把已定且已落地的架构裁决写成可引用的记录，**防止未来架构继续失控**。每篇四段：状态 / 背景 / 决策 / 后果。「实件」列给出仓库锚点，改代码前先对 ADR。

---

## ADR-001 Player Runtime Authority（玩家运行时权威）

**状态**：已落地（P0-2 Player 对象模型交付）。

**背景**：玩家数据若可被副本/战斗/网关多方改写，长期态归属发散，掉线/换副/回档时无唯一真相。

**决策**：玩家长期态的唯一权威是 `PlayerAnchor`（`modules/game/session`）。登录经 `AnchorManager::activate` 幂等激活在场，长期进度只在锚上累计（`progress` + `mark_dirty`，变更即脏）；副本与战斗不持有长期态——战斗结算经 `IRewardSink` **单向**落锚，落完即忘。登出 `deactivate` 回收。

**后果**：任何模块要改玩家长期态，只能落锚（单向口），不存在第二写点；代价是结算路径必须显式过 `AnchorRewardSink` 一跳。

---

## ADR-002 Scene as Runtime Boundary（场景即运行时边界）

**状态**：已落地（P0-3 Scene 模型交付）。

**背景**：空间权威若悬空（既在全球服务又在进程局部），可见性与实体归属会出现两套真相。

**决策**：空间运行时边界是 `Scene`（`modules/game/world`）——场景内的实体归属、坐标、可见性由 Scene 独占裁决；Apollo 不做跨进程连续大世界（无缝地图/cell 分片/ghost/实体迁移均被否决，架构裁决 #9），世界的自然边界就是房间/场景/副本/比赛。

**后果**：跨 Scene 的实体转移必须走显式换幕（ADR-004），不存在隐式跨边界漫游；大于单 Scene 的世界由多 Scene/多实例编排表达。

---

## ADR-003 Instance as First-Class Object（副本是一等公民）

**状态**：已落地（P0-4 / P4-2 批 A 全链验证）。

**背景**：「一局对战」若散落为若干回调与临时容器，生命周期无人看管，泄漏与半死态频发。

**决策**：一局一实例——`Instance`（`modules/game/world`）是八态单向状态机 `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`，仅相邻推进合法（`is_transition_valid` 校验表），Destroyed 终态封死；Waiting 窗口挂玩法负载与预进人，`start` 内部校验参战非空后进 Running。

**后果**：副本的创建/调度/恢复/回收有唯一句柄可管；代价是所有推进必须过状态机（无跳跃旁路），骨架期新增能力须先定位自己的态窗口。

---

## ADR-004 Scene Transfer（换幕）

**状态**：已落地（P1-1 scene_transfer 交付）。

**背景**：跨场景移动若用「就地改坐标」，目标场景的实体登记/视野重建/失败回滚无处安放。

**决策**：跨 Scene 边界一律走换幕四段 `prepare → detach → attach → resume`，任一段失败回滚到源场景（`modules/game/world/scene_transfer`）。

**后果**：换幕是事务式的——要么完整到达，要么留在原地；代价是换幕路径延迟高于普通移动，换幕期间实体处于过渡态不可交互。

---

## ADR-005 AOI Ownership（AOI 归属）

**状态**：已落地（P1-3 / P3-2 交付）。

**背景**：兴趣管理若与场景分离（全局 AOI 管理器），scene_id 与空间格子两套坐标必然漂移。

**决策**：AOI 归场景所有——`SceneAoi` 单实现由 Scene 独享（`scene.aoi()` 持有，scene_id 隔离），九宫格差集 + `ViewerState` 逐观察者水位，事件面 Enter/Sync/Leave 经 `set_event_sink` 下发（sink 缺省静默）。

**后果**：可见性权威唯一（Scene 说谁看得见谁）；广播量口径 = Sync 差集事件数；网关下发面接事件 sink 即可，AOI 核心不再被多方轮询。

---

## ADR-006 Battle Runtime（战斗运行时）

**状态**：已落地（P2-1 / P4-2 批 A 验证）。

**背景**：战斗若直接改玩家数据，判定不可重放、结算无凭据；若与副本 tick 混用同一时间轴，双驱动抢拍。

**决策**：战斗判定域独立——`BattleRuntime`（`modules/game/battle`）五段相位（Created→Entering→Battling→Rewarding→Finished），严格递增 tick + 排序输入 + `CombatRoll` 子流随机数（确定性指纹 = hash 链），判定结果经 `IRewardSink` 单向结算（按玩家 Hit 伤害合计）。判定域时间轴归 `Instance::tick` 内部驱动，战斗输入走 `BattleInput` 显式注入面——**两条时间轴不可混用抢拍**。参战者收集期 = 副本 Created/Entering 窗口（`Instance::enter` 转发），Battling 起进人只观战。

**后果**：同种子同输入序列必得同战果（可审计可回放）；代价是输入必须显式注入且排序，旁路改状态即破坏确定性。

---

## ADR-007 Persistence Model（持久化模型）

**状态**：已落地（P1-4 / P1-5 交付；audit A8 配置收口同口径）。

**背景**：外部 DB 依赖会把「跑起来」的门槛拉高，且骨架期存储语义未定时接 DB 是负资产。

**决策**：现状零外部存储依赖——玩家档案 = JSON 文件（tmp+rename 原子写）+ `PersistJournal` write-behind（append 落盘 → 定额 drain → 快照压薄 → 崩溃 replay 续接）；加载未命中即失败（无默认档 bootstrap）；MySQL/Redis/ClickHouse 均为规划态未接线。

**后果**：开发/CI 环境无需任何存储服务即可全链跑通；代价是单机容量与查询能力受限于文件形态，外部 DB 接线批次须保 replay 语义对等。

---

## ADR-008 Recovery Model（恢复模型）

**状态**：已落地（P1-5 Recovery / Reconnect 交付）。

**背景**：掉线与进程崩溃是两类恢复——前者有活进程可归位，后者要靠盘上状态重建；混用一套机制两头都做不干净。

**决策**：分两条恢复线——**重连**走 Session 层重连窗口 + Player 锚点会话归属（`BaseAppMgr` 目录裁决「回到哪个 world、经哪个 gateway」）；**崩溃恢复**走 `PersistJournal` replay 续接（write-ahead 日志重放到最后一致点），不做跨进程快照迁移。

**后果**：掉线玩家的长期态与在场副本由目录唯一裁决，重连即归位；进程崩溃后的恢复点 = journal 最后一致点，丢失窗口 ≤ drain 配额。

---

## ADR-009 Concurrency Ownership（并发所有权）

**状态：** 已落地（骨架期口径，随批演进）。

**背景**：多线程模拟会让确定性（ADR-006）与状态机（ADR-003）全部失效，锁面扩散不可审计。

**决策**：模拟域单线程驱动——`Instance`/`BattleRuntime` 由单线程 tick 驱动、喂钟注入不起内部线程（discovery 同口径）；跨线程面收敛为最小集：日志 `LogContext`（proc 互斥 + tick 原子量）、`MetricRegistry` 持锁 find-or-create + owning 引用热路径免锁、machined 主循环 `waitpid(WNOHANG)` 非阻塞收割；服务宿主 `ApplicationHost` 帧驱动单线程 run_once。

**后果**：模拟逻辑零锁零竞争（确定性成立的前提）；代价是吞吐上限 = 单线程 tick 容量，多核扩展走实例横向编排而非域内多线程。

---

## 相关文档

- [架构总览](/architecture/overview)——七层下行数据流（ADR 的分层视角）
- [术语命名契约](/design/term-contract)——命名的裁决记录
- [架构审计与现状](/analysis/architecture-review)——审计全量证据
