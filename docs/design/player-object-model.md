# Apollo 玩家对象模型与 Cell 精简（player-object-model）

> 状态：设计稿（评审中，2026-10-03 讨论定稿口径，待用户审核）。定位：**玩家对象的分层与归属**——登录后玩家对象在哪、断线保留归谁、跨场景（进副本/换线）谁动谁不动；并给出 **Cell 的精简边界**（砍什么/留什么）。缺口登记 design-gap-inventory #18（2026-10-03 用户指认「玩家登录之后就在 BaseApp 又在 Zone」前后矛盾后补立）。互引：concept-glossary §2.1（scene/副本/Zone 粒度）；session-and-online-directory（#12 登记锚点/顶号/掉线保活窗口）；login-flow（#13 落点链）；battle-determinism（tick 边界确定性）；attribute-sync（§4.4 RO_MIRROR、L3 易失态）；36号 #9（否决无缝）；base-cell-proxy-model.md（B 档参考件——KBE 三层机制证据源，非本设计）。

---

## 0. 矛盾与裁决

**矛盾（用户 2026-10-03 指认，实锤）**：三处口径互相对不上——

1. base-cell-proxy-model.md（B 档参考件）：「Session/Proxy、PlayerAnchor/Base、AvatarEntity/Cell **三层必须分层**」（KBE 机制）；
2. concept-glossary Zone 词条：「刻意**不拆 cell/base 两族进程**」（apollo 决策面）；
3. login-flow：「Zone 创建**会话实体**」。

玩家对象到底几个、登录后在哪、断线保留归谁、进副本谁动谁不动——没有权威载体（#12 只管在线登记/顶号，不管对象迁移归属）。且 Zone 词条「场景实例粒度」与 §2.1「承载一个或多个 scene」自相抵触。

**裁决（用户 2026-10-03）**：**A——三层对象模型保留；设计重心 = Cell 精简**。用户原话：「我理解之前重新设计是想避免 cell 的复杂度，其实主要设计在 cell 的精简上」——BW/KBE 的 cell 族复杂度（ghost 双写/边界协商/witness 全套/无缝迁移）是要砍的对象；砍完的 Cell 只剩「场景内空间权威」一件事。

**一句话收口**：**对象分层 ≠ 进程分层**。Proxy/Base/Cell 是职责分层，三者同驻 Zone 进程（36号 #9「不拆进程族」的本意即此）；BW/KBE 是对象分层 + 进程分离——分离是它们为无缝世界付的学费，apollo 只收洞察不交学费。

## 1. 三层对象职责

| 对象 | 职责 | 生命周期 | 所在 |
|---|---|---|---|
| **Proxy/Session** | 连接承载：四通道会话、resume 重连、顶号切换挂载 | 随连接，可失可重挂 | gateway 连接面（逻辑挂 Zone 会话实体） |
| **Base（PlayerAnchor）** | 长期逻辑归属：断线保留、跨场景锚点、背包/成长等长期状态、玩法进出协调 | 登录创建 → 登出销毁 | **Home Zone**（永不随场景切换迁移） |
| **Cell（Avatar）** | 场景内空间权威：移动/战斗/AOI 广播 | **进 scene 生、出 scene 死** | 当前 scene 所属 Zone（或 instance 进程） |

分离理由（吸收 base-cell-proxy 的正确洞察）：断线保留与跨场景并发需要一个**不动的锚**——若单对象整体迁移，「玩家进副本时主世界状态归谁」被迫落回持久化层（太慢）或全局缓存（多余）；Base 就是这个锚。但**三层不跨进程**：协作在 Zone 进程内完成，无 Mercury 式跨进程税。

**反方案记录（单对象整体迁移，2026-10-03 二选一落选）**：概念最少，副本制下多数场景够用；落选因断线保留与换幕期状态归属无自然落点，且与 decision 面「不拆 base/cell」叙事冲突——用户裁决选 A。

## 2. Cell 精简清单（设计主刀口）

| BW/KBE 原装复杂度 | 处置 | 理由 |
|---|---|---|
| ghost 双写/边界协商 | **砍**（决策 #9 已否决无缝） | Cell 一生绑一个 scene，永不跨进程搬 |
| cell 间 entity migration | **砍** | 跨场景 = 「Cell 换幕」（§3）：销毁重建，不做对象搬迁 |
| witness 全套 priority/双序号 | **精简**：viewer set + 三通道 seq 单向广播 | BW dumpAoI 实读（witness.cpp:2470-2516）保留为参考件证据，实现面不背 priority 机制 |
| 断线处理 | **升到 Base**（§3） | Cell 不管连接态——这是 Base 存在的意义 |
| 战斗/移动权威 | **收敛 Cell tick 边界** | battle-determinism 既有口径（判定域唯一=场景线程 tick 边界）不变 |

Cell 职责收窄后只剩三件：**移动权威（reconcile）、战斗权威、AOI 广播**。连接、持久化、跨场景、断线全部不归它。

## 3. 关键时序

**换幕（进副本/换线/跨场景）**：Base 不动、Cell 换幕——
1. 玩法准入/组队开局（玩法层）→ 目标 scene（或 instance 进程）创建；
2. 旧 scene 销毁 Cell → 新 scene 重建 Cell（**状态从 Base 投影**：装备/属性/长期 buff）；
3. 场景内易失态（临时 buff/CD/坐标）**不带走**——进场按玩法规则重置（坐标=出生点/门口），结算类结果（掉落/奖励/任务进度）玩法结束**单向落回 Base**（instance 结果落回既有口径，36号 #16）。

**断线**：Cell 场景内挂机 N 秒（Suspended 态——#12 掉线保活窗口，窗口与 resume token TTL 同源一个值）→ 窗口满 Cell 移除；**Base 驻留保状态**，目录条目按 #12 §4 处置，存档走 write-behind。

**重连**：Session 重挂 Base（resume token 恢复 L2 会话）→ Cell 按场景规则重建（原地/复活点/主城——玩法规则决定，引擎不统一）。

## 4. 承载关系（三轴收口）

```text
进程轴   gateway(连接) / Zone(逻辑服) / manager·login-app·DB·machined·logger
              │ 一个 Zone 承载 1..N 个 scene；「instance 进程」= 给单个 scene 单开的进程（offload 形态）
空间轴   scene = 唯一空间单元（地图、分区、副本统统是 scene）
              副本/instance = scene 的玩法生命周期别名（不再单独一层）
              房间(room) = 副本口语（docs/30）；分线(line) = 同图并行 scene 实例（非一级概念）
对象轴   Proxy/Session(gateway) + Base(Home Zone，不动) + Cell(随 scene 生死)
```

一句话（glossary §2.1 同款）：**scene 是「一块世界」，副本是「一块世界的一次运行」，Zone 是「跑世界的机器格子」**；玩家「在哪」的答案 = **锚在 Base，空间态在 Cell**。

## 5. 定位论证：无无缝地图仍是 MMO

**MMO 的判据是「大规模同时在线 + 持久世界交互」，不是无缝地图**。最硬反例即行业天花板：WoW（区域过图 + 副本实例 + 位面分线，从来不是无缝）、FF14（区域分割加载 + 副本制）、EQEmu 系（zone 进程制）——均为公认 MMO；真无缝（BDO/EVE）反而是少数派。Massively 说的是同世界承载的玩家规模与交互密度（持久角色、多人副本、公会/经济/对抗），与脚下地图连不连贯无关。

反过来算账：无缝的代价就是 §2 砍掉的那整套（ghost/迁移/边界协商/跨进程调试不可单步）；分区/分线制换来副本故障域隔离、玩法实例独立扩缩、Zone 粒度容量模型——对 5000+ CCU SLO 更实在。

**定位句**：Apollo = 副本/分线制的**轻量 MMO 服务器引擎**——覆盖 WoW/FF14 型主流 MMO 形态（区域世界 + 副本实例 + 分线），不追求无缝大世界（BDO/EVE 型）。

## 6. 进程模型对比（先例证据）

开源/可查证（36号 八面 + mmo-frameworks 分析件实读）：

| 引擎 | 进程拓扑 | 玩家对象在哪 | 空间模型 |
|---|---|---|---|
| BigWorld | machined（守护）→ loginapp → baseappmgr/cellappmgr → baseapp×N + cellapp×N + dbapp | Base 在 baseapp、空间态在 cellapp（两族进程分离） | 无缝：cell 间 ghost 双写 |
| KBEngine | machine → loginapp → 双 mgr → baseapp×N + cellapp×N + dbmgr + interfaces | 同 BW（同型） | 同 ghost + entity migration |
| skynet | 无预设拓扑：单进程千级 service + gate/cluster 跨节点 | 全业务自定义 | 自建九宫格 |
| TrinityCore（WoW 官方不公开，最可查证仿真） | authserver + worldserver（单世界服）+ auth/characters/world 三库 | worldserver 进程内对象 | 世界服内 map/instance 线程，副本=进程内实例 |
| Ryzom Core | 一个 shard = 专职服务群（FES/EGS/AIS/GPMS/NS/WS/TS/MS/SU…13+） | 分散各专职服务 | 服务化最细，位置服务独立（GPMS） |
| EQEmu | zone 一进程 | zone 进程内 | 场景进程制 |
| **Apollo** | login-app / manager / gateway-app / Zone×N / instance / DB / machined / logger | **Base/Cell 对象两层，同驻 Zone** | scene=唯一空间单元，副本/分线制 |

闭源游戏（官方不公开进程模型，只判空间形态）：WoW/FF14 = 分区过图 + 副本实例 + 分线；Lineage = 地图切换；BDO = 无缝；EVE = 单 shard。

**两条谱系**：① Base/Cell 分离派（BW/KBE）——断线保留优雅、无缝代价全套；② 世界服/场景服单进程派（TrinityCore/EQEmu/Ryzom）——无跨进程税。**Apollo 杂交站位**：对象分层照 BW/KBE + 进程合体照 EQEmu zone + 服务发现照 machined + AOI/位置独立服务吸收 Ryzom GPMS 思想（36号 #10）。

## 7. 与既有设计的咬合

| 既有件 | 咬合点 |
|---|---|
| #12 session-and-online-directory | 登记锚点 = Base；顶号 = Session 重挂 Base；Suspended 窗口 = Cell 挂机窗口（同源一个值） |
| #13 login-flow | manager 落点即分配 Home Zone（Base 归属在登录时一次定妥） |
| battle-determinism | Cell tick 边界 = 判定域唯一；换幕重建不破确定性（状态投影来自 Base 长期态） |
| attribute-sync | L3 易失态（HP/位置/buff 剩余）不落档、换幕不带走；RO_MIRROR 只读镜像（§4.4）不改 |
| #17 battle-verification | 复算对账的权威侧 = Cell（判定在 Cell tick 边界产出） |
| 36号 #9/#16 | 否决无缝不变；instance 结果单向落回 Base |

## 8. 待办（代码批）

1. 存量 login-app「preparePlayerOnline 直呼 base 三连」迁移面（login-flow §10 已登记）按本设计口径执行——Base/Home Zone 概念进代码批注释与命名；
2. 换幕易失态规则（buff/CD 不带走）落到玩法 SDK 约定（scripting-lua 侧）；
3. glossary「玩家对象模型」词条已加（§1 表）；Zone 词条口径已修（2026-10-03）。
