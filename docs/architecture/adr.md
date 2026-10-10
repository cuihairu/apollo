---
title: 架构决策记录（ADR-001..017）
icon: gavel
order: 2
category:
  - 架构
tag:
  - ADR
  - 决策记录
---

# 架构决策记录（ADR-001..017）

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

**后况勘误（2026-10-10）**：P0-3 后 cell-app 拆出 `gameThread_` 跑 worldHost tick，消息 handler 由 `RepSocket` 自持 worker 线程执行——handler 写 world 与 tick 读写 world 跨线程并发，越出本 ADR「跨线程最小集」（concurrency §1.1 后况勘误已记；生产未触发仅因负载空洞）。收口前置设计已立 docs/design/cell-single-writer.md（三岔对表 + 六拍板点，候选倾向=(a) 队列化异步受理，拍板①待裁——同日全裁 ADR-015/016/017）——本 ADR 骨架期口径维持，收口批落地后随批演进。

---

## ADR-010 G-2 Topology: Restart-Only First（容灾拓扑：案 A 先行，standby 不立项）

**状态：** 已拍板（2026-10-10 巡检授权批，六拍板点一并裁决；裁决依据 = backup-revive 反证段 + 批 0 实测）。

**背景**：G-2 前置设计（backup-revive）给出三案（A 冷重启 / B 热备 standby / C 无 standby 双写）与六拍板点全悬置。反证段已录：单机编队下 standby 同机同死；5000 CCU 单机 8GB 目标下当代小/中部署主流即「进程管理器自动重启 + 持久化 + 客户端重连」（D 级行业通型）；热备的真实增量只有 RTO 秒级→亚秒级 + 易失态保留，代价是备机常驻 + 镜像流带宽 + 双主仲裁复杂度。批 0 kill -9 全链演练已交付（scripts/drill_kill9.sh，两跑断言全过）。

**决策**：
1. **案 A 先行（restart-only）**——machined 监督面退避重启 + PersistJournal 持久态 + 客户端重连；**standby 不立项**；案 B/C 不做分期预留，若生产运行数据提出亚秒 RTO 需求另行立项（届时依赖 net M1 InterServerLink）。
2. **接管粒度按进程类型分三档**（不统一单语义）：有状态重建档 = manager（全量重报，既有）+ Zone（冷重启 + journal replay + 重报收敛，批 0 已实测）；无状态重启档 = login-app / verifier（worker 池重启即恢复）；崩溃即作废档 = Battle 实例（ADR-012）。
3. **仲裁面：案 A 无双主，不新造仲裁协议**；epoch 沿用目录面既有语义（单调递增 + 陈旧 epoch 拒收——守卫实存目录面核心 player_directory.cpp:117-124 §3-③ 竞态窗口守卫：`next_epoch_` 自增发号 + resume 按 token 与 epoch 双锚拒旧，directory_mirror 层仅承载 wire 序列化），manager 单点串行维持。
4. **RPO/RTO 定档**：RPO = 0（journal write-ahead 零丢失，attribute-sync §11 既有指标升格）；RTO = 重报收敛 ≤3s / 镜像收敛 ≤3.5s（批 0 满载噪声期实测上限定档，空载更优）。
5. **镜像流不建**：journal 跨进程消费改造不做（现状单进程 SaveQueue worker 维持）；同机 shm SPSC / InterServerLink 两候选形态留档随未来 B/C 立项再裁。
6. **验收链 = 批 0 kill -9 演练链**（已交付）；双主注入演练不进门禁（案 A 无双主）。G-2 术语四词随本拍板定案：热备/接管词条标注「未立项（案 A 先行）」。

**后果**：G-2 批 1-4（位点面/镜像流/接权仲裁/双写面）全部取消——案 A 的完整形态即批 0 终态，已交付；G-2 关闭（重开条件 = 生产数据提出亚秒 RTO / 多机形态立项）。代价：进程死亡期间服务中断秒级（非亚秒）、易失态丢失（重连回落盘态）——两项均在 capacity §5 指标集显式登记。

---

## ADR-011 net M1 Chain: Minimal Gateway Fix First（网关链：最小修复先行，拓扑调研结论生效）

**状态：** 已拍板（2026-10-10 巡检授权批）。

**背景**：gateway-app 现状半成品壳（摸底三因：启动期全后端硬依赖无重试 / 默认端口错位 BaseApp 9002=自身端口、CellApp 9100 错位 / ChatApp 幽灵依赖指向无进程端口；叠加 ingress 未接线）。「何时修、按最小修复处方还是等 net M1 直接接线」挂 net M1 拍板链；gateway-topology-survey 结论「留独立 gateway-app + 自研进程间总线」为建议件未生效。

**决策**：
1. **gateway-topology-survey「留独立 gateway-app」结论生效**（拓扑定案：连接面进程 + Zone 不见客户端 + 自研总线，不学 nats 中间件族）。
2. **最小修复处方先行**（三步）：① `MessageRouter::start` 连接失败降级警告 + `available=false`（不再 throw→exit 1，复用转发面既有降级语义）② dev_fleet roster 补传三后端 URL（消端口错位）③ ChatApp 依赖摘除（仓库无 chat-app 进程）。
3. **ingress 客户端接线不随本批**（真缺口在 ingress，归 P3-2 gateway surface 随 net M1 批）；协议收敛 / P4-4 代码层三簇按 M1 阶梯（§5.6：M1 最小正确内核 = 帧定界 + contract_route 分发 + 背压水位 + 单播拓扑 + InterServerLink）推进。

**后果**：三步齐后 dev_fleet 编队不再 gateway crash-loop（exit=1 ×5 give-up 消除）；gateway 仍是无客户端入口的进程壳（预期态，非缺陷）——真正客户端面等 P3-2 + M1。三步均为启动语义与传参修正，不动转发面行为。

**实施状态（2026-10-10 勘误）**：三步尚未齐——② 已落地（dev_fleet roster 补传三后端 URL，commit 5ccfc651）；①③ 待功能代码批（`connectBackendChannel` 连接失败仍 throw——apps/gateway-app/src/gateway_server.cpp:27-34，`MessageRouter::start` 仍连接 ChatApp——gateway_server.cpp:86），crash-loop 消除以①③落地为条件；此前的 dev_fleet 冒烟 gateway exit=1×5 give-up 属预期现状非回归。

---

## ADR-012 Instance Offload: On-Demand Spawn, Stub-First, Crash-Invalidates（实例 offload 三拍板）

**状态：** 已拍板（2026-10-10 巡检授权批；三拍板点全按 battle-instance-offload 既有候选倾向定案）。

**背景**：battle-instance-offload 前置设计三拍板点悬置：生命周期协议（静态预池 vs 按需 spawn）、传输依赖序（M1 先行 vs 内存桩过渡）、崩溃裁决（作废 vs 关键帧重放恢复）。

**决策**：
1. **生命周期 = (b) 按需 spawn 请求协议**（需求方 → machined 控制面拉起，就绪后经目录可见；控制面复用 G-1 编队事件同型——Query/Advertise 先例的同向扩展；静态预池作为 (b) 就绪前的过渡形态允许）。
2. **传输依赖序 = (b) 内存桩过渡**：offload 先以同机管道（pipe/shmem SPSC）落生命周期 + internal 域信封协议 + 故障处置骨架，M1 就绪后换底座（一次底座替换换设计风险与传输解耦）。
3. **崩溃裁决 = (a) 崩溃即作废**：团战重开，玩家侧按错误面提示；(b) 关键帧重放恢复留缝（ReplayTuple 已是关键帧素材，协议面后补不预支）。

**后果**：拍板后路径启动：协议消息契约批（internal 域，随实现入库）→ offload 骨架批 → M1 就绪换底座 → manager 域落点接入。Battle 实例故障域与 Zone 故障域独立（battle-instance-offload §7），ADR-010 三档粒度表的作废档由此承接。

---

## ADR-013 LoggerApp In, VerifierApp Deferred（日志进程立项，验证进程暂缓）

**状态：** 已拍板（2026-10-10 巡检授权批）。

**背景**：term-contract §1.3 定稿两进程名：日志收集进程 `LoggerApp`（日志双出口：本地文件真相源 + push）、战斗验证进程 `VerifierApp`（客户端权威战斗的服务端复算对账，gap #17）。logging 批 C 已交付结构化行格式（六键固定序 + FileAppender structuredOutput 开关默认关，apps 接线批打开）；VerifierApp 的复算引擎依赖 Lua 面（scripting-lua 未入主线）。

**决策**：
1. **LoggerApp 立项**：apps/LoggerApp 进程壳 + 各 app FileAppender structuredOutput 开关打开——本地文件真相源维持（人读/机读双出口），push 出口（上游 collector）留接口不实现（collector 面 M1 后）。
2. **VerifierApp 暂缓**：复算引擎依赖 Lua 面未入主线，battle-verification-service §5 既有口径（无状态 worker 池、无 G-2 热备需求）维持为设计件；立项随 Lua/玩法批。

**后果**：structured 行格式获得进程级消费面（批 C 遗留「apps 接线未做」消账）；VerifierApp 缺口（gap #17）保持登记状态。

**落地注记（2026-10-10）**：决策 1 的「开关打开」前置还有一层——apps 接线现状 = 零（七 app 全 `std::cout` 直写，log 模块零接入；唯一触点 game-server crash 可见化行），apps 先接 log 模块（L1）再翻开关（L2）；落地形态前置设计已立 docs/design/logger-app.md（v1 采集形态三岔 + 五拍板点，候选倾向 = tail 拉）。

---

## ADR-014 Process Rename: Manager and Zone（进程改名：baseappmgr→manager，base-app→zone-app）

**状态：** 已拍板（2026-10-10 巡检授权批；定名依据 = term-contract v1.0 定稿，2026-10-03 用户审定）。

**背景**：P3-1 遗留「base/baseappmgr 进程名更名（走新术语流程定名，此前不变名）」。term-contract v1.0 已定稿：§1.1 Zone 行禁用名表「cellapp / baseapp → apollo 不拆两族进程 → `Zone`」；§1.3 管理进程行「manager / `ManagerApp`（BW 对照 = baseappmgr+cellappmgr）禁 mgr 缩写作标识」。命名先例：`Machined` 同为契约定稿名直接入户（P3-1 批 C 勘误）。

**决策**：
1. **baseappmgr → manager**：进程名 / 二进制 / roster 标识 / 日志身份改 `manager`（类名 BaseAppMgr 域代码随批内一致性整理，CTest 套件名不改语义——BaseAppMgrTests 保持既有名避免无谓断言改名）。
2. **base-app → zone-app**：进程名 / 二进制 / roster 标识改 `zone-app`（Zone = 逻辑服进程定稿名；代码标识 `Zone` 不与进程名混淆）。cell-app 为世界运行时原型，名不动（其合并/去留随玩法批，不在本拍板）。
3. 改名面 = apps 目录 + CMake target + dev_fleet/drill roster 与文档引用；纯进程身份变更，零行为变更（端口/参数/监督语义不动）。

**后果**：术语契约定稿名全量入户，仓库内不再出现 baseappmgr/base-app 进程名（他家对照词仅存于文档证据引用）；改名批后 dev_fleet/drill 全链复验为验收面。

---

## ADR-015 Cell Single-Writer: Async Queue Acceptance First（单写者主案：队列化异步受理）

**状态：** 已拍板（2026-10-10 巡检续派 r2 授权，按 cell-single-writer 前置设计候选倾向裁定拍板①；②水位/③只读快路径/④Manager 归并/⑤停机时序仍悬置）。

**背景**：P3-2 批 B 竞争实证——cell-app `gameThread_` tick 读写 World/Scene 图，`RepSocket` worker 线程 handler 直触 `world_`，二线程间零保护（cell-single-writer §1 A 级实读）。前置设计三岔：(a) 队列化异步受理 / (b) 队列化+同步等待 / (c) 最小互斥回退；候选倾向 (a)，(c) 以 tick 中途容器突变（迭代器失效 + battle-determinism 迭代序破坏）反证否决，(b) 队头阻塞（workerLoop 单线程 × tick 时长乘积越界即吞吐崩塌）。

**决策**：

1. **主案 = (a) 队列化异步受理**：RPC worker 只做解码+入队（意图信封），`gameThread_` 在 tick 边界消费、执行、发回应答。全部变更 tick 对齐（attribute-sync §10.1 G-7「固定边界内可见、有序、可序号化」）；应答延迟 ≤1 tick（10Hz 上界 100ms）；调用面已 `sendRequestAsync` 回调式，延迟增量无语义破坏；受理面即 net M1 接收端骨架（(b) 的同步面在 M1 下废弃重做）。
2. **(b) 保留为 (a) 水位语义不足时的升级案**，(c) 维持否决。**后况勘误（2026-10-10 / ADR-017）**：(b) 同步等待机制经拍板⑥彻底否决——水位语义不足的升级路径是水位调参/扩容而非同步等待；M1 延时应答正解覆盖 (b) 的应答即时性诉求。
3. **拍板②-⑤维持悬置**（水位形态/只读快路径/Manager 归并/停机时序）——水位与过载的决策面另立前置设计（受理队列水位语义），供后续巡检裁。**已裁（2026-10-10 / ADR-016）**：②水位两级 + 过载回调错误码 + tick 预算锚定；③不设快路径；④Manager mutex 保留；⑤停机 G-3 镜像。

**后果**：受理面批（意图信封 + 队列 + tick 消费 + pending reply，cell-app 功能代码）待文档/脚本边界放行；水位与过载批紧随其后（决策面设计随 ADR-015 落地）；Manager 归并复核批随拍板④；net M1 接收端接线只换传输底座（受理面=骨架直接复用）。battle-instance-offload 的 spawn 请求协议控制面复用同一受理形态（ADR-012）。**后况勘误（2026-10-10 / ADR-017）**：受理面批应答面 = 受理点回执（ack-on-accept），pending reply 为 M1 升级增量（传输 lockstep 卡点与本 ADR 决策 1「发回应答」措辞的修正见 ADR-017）。

---

## ADR-016 Cell Acceptance: Watermark, Overload, and Companion Rules（受理队列水位与过载，含②-⑤收敛）

**状态：** 已拍板（2026-10-10 巡检续派授权，按 queue-watermark 前置设计候选倾向裁定拍板②；③只读快路径/④Manager 归并/⑤停机时序配套口径一并收敛）。

**背景**：ADR-015 决策 3 悬置四项（②水位/③快路径/④Manager/⑤停机）。水位与过载决策面已立 queue-watermark 前置设计（三岔：两级/四级全量/无界；过载响应：回调错误码/契约新消息/静默；数值口径：tick 预算锚定）。

**决策**：

1. **水位形态 = (a) 两级**（告警线 + 顶格拒收，不丢旧不静默）——(b) 四级全量是客户端体验语义误植进程内（受理面无消费者族分档需求）、(c) 无界排队 = 内存炸弹（net-abstraction §5.7 明文纪律），均不取。
2. **过载响应 = (A) 回调错误码**（类型化 overload=queue-full，复用 §5.7「类型化原因不代掷猜测」哲学，零新协议）；(B) 契约 internal 域新消息留 M1 批评估（进程内一跳不需 wire 面，留缝不预支）；(C) 静默反证。
3. **水位数值 = tick 预算锚定**——队列深度 × 单请求处理时长 ≤ tick 预算（100ms@10Hz，clock-and-time §4）的比例起步；静态顶格先取保守值，benchmark 接入受理面后按 capacity §2.1 三模型实测锚点同型校准。
4. **③ 只读快路径不设**——全量入队单形态；PING/查询类延迟 1 tick 无害，多一条快路径即多一个并发形态要审。**后况勘误（2026-10-10 / ADR-017）**：PING 在 M1 前于受理点回 Pong（worker 侧纯 wire echo 无 world 访问面，非快路径）——现行传输 lockstep 下 Pong 入队则无发送路径可回；M1 延时应答落地后 Pong 随队列入队。
5. **④ Manager 三件 mutex 保留**——WorldSessionManager/AnchorManager 已是「跨线程最小集」形态（数据面小、竞争烈度低），迁移收益不抵改形风险；SessionLocator 不动；单写者收口只指 World/Scene 图。
6. **⑤ 停机时序 = G-3 五阶段的 cell-app 镜像**——停受理（断流回维护中，base-app `dispatchRequest` 维护闸同型）→ 队列 drain → gameThread join → worldHost stop。

**后果**：受理面批（意图信封 + 队列 + tick 消费 + pending reply，cell-app 功能代码）落地即含③④⑤配套口径；水位数值的 benchmark 校准随受理面接入 capacity 锚点体系；M1 接收端接线只换传输底座（受理面=骨架直接复用）。**后况勘误（2026-10-10 / ADR-017）**：受理面批应答面 = 受理点回执（ack-on-accept，含 PING 受理点回 Pong 特例勘误见决策 4 注记），pending reply 为 M1 升级增量。

---

## ADR-017 Cell Acceptance Reply Wiring: Ack-on-Accept Now, Deferred Reply at M1（受理面应答接线：受理点回执先行，延时应答随 M1）

**状态：** 已拍板（2026-10-10 巡检续派授权，按 cell-single-writer 拍板⑥ 候选倾向裁定——两阶段接线：ack-on-accept 现行 + 延时应答 M1 升级；(iii) 阻塞适配否决）。

**背景**：受理面批（ADR-015 (a) 落地）开工摸底发现传输层卡点——(a) 要求「worker 只做解码+入队，gameThread_ 在 tick 边界消费、执行、**发回应答**」（延时应答），但现行传输不可实现：

- `RepSocket::workerLoop`（socket.cpp:139-172，nng_rep0）严格 lockstep：`nng_recvmsg` → `handler_(data)` 同步返回应答字节（socket.hpp:119，RequestHandler = `std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>`）→ 发送 → 下一轮 recv。应答必须由 recv 同一线程同步产出，worker 在 handler 返回前无法收下一请求——延时应答不可实现。
- **桩传输事实**：:165 `if (!response.empty())` 空应答跳发送——现行变更类 handler 回 `{}`，真实 nng_rep0 下 REP 状态机欠应答即卡死；全仓走桩分支（:431-488）正因如此（nng 依赖解禁评估为传输层重写级断裂后回退，模块保持禁用，M1 自研内核待落地——契约差异清欠 §34.3 / P1-6 批记）。
- 消费面现状：真实消费方 = gateway 转发（单向 `sendPayload`，ADR-011 半成品壳）+ 协议单测 + liveness；变更类回 `{}`、PING 回 Pong，**无结果承载应答**存在。

三候选倾向（cell-single-writer §5 拍板 6）：(i) 等 M1 传输 / (ii) ack-on-accept 适配 / (iii) 阻塞适配（(b) 同步等待换皮）。

**决策**：

1. **现行接线 = (ii) ack-on-accept**：worker 解码+校验+入队（意图信封，ADR-016 水位两级闸）后**受理点立即回执**；gameThread_ 在 tick 边界消费执行，结果不落应答（当前无结果承载调用方）。变更类回空 ack（= 现行 handler 返回值，桩传输下逐字节同现状）；过载回过载错误包（ADR-016 #2，闭环在受理点）。竞争收口与传输重写解耦——race 是 P3-2 批 B 修复对象，不等 M1。
2. **(iii) 阻塞适配否决**：worker 入队后 condvar 阻塞等应答 = 方案 (b) 机制。反证三条：(a) 单请求在途 → 队列深度恒 ≤1，ADR-016 水位机（两级告警/顶格拒收）全队列死码；(b) RPC 吞吐塌至 tick 率（≤10/s）——ADR-015 §3(b)/§4 弃 (b) 的「队头阻塞×tick 时长」原判；(c) 与 ADR-015 已裁「采 (a) 弃 (b)」直接冲突。
3. **PING 的 Pong 受理点回**（worker 侧纯 wire echo，不触 world_）——③「不设快路径」禁令指绕过队列触 world 的并发面，Pong echo 零 world 访问不构成第二并发形态；且 Pong 入队则 gameThread_ 无发送路径可回（:165 + lockstep 双重封锁）。对 ADR-016 #4「全量入队」的 Ping 特例勘误：M1 延时应答落地后 Pong 随队列入队。
4. **延时应答（原 (a) 接线）随 M1 升级**：InterServerLink recv/send 解耦后加 pending-reply 关联面（header.sessionId 承载）与 gameThread_ 侧发送路径；受理面骨架（意图信封/队列/水位/tick 消费/单写者）零废弃——cell-single-writer §6「M1 只换传输底座」口径不变。ack-on-accept 不是 (i) 的替代终局，是 (i) 的前置阶段；真传输 REP 锁步要求每请求一非空应答帧，ack 帧型随 M1 契约批定义。
5. **倾向口径勘误**：拍板⑥ 记录原倾向「(i) 为正本（传输层根因，适配是绕路）」针对**终局接线**——裁定保留 (i) 为 M1 时终局；「绕路」指责对 ack-on-accept 不成立：它收竞争、保 wire 语义、骨架复用 M1，是解耦而非绕路。

**后果**：受理面批（意图信封 + 队列 + tick 消费 + 受理点回执，cell-app 功能代码）设计闭环，待文档/脚本边界放行后开工（决策面全裁：ADR-015 主案 + ADR-016 配套 + 本 ADR 应答接线）；水位与过载批随受理面批同批；M1 接线批新增「延时应答升级」增量（pending-reply 关联面 + gameThread_ 发送路径），受理面零返工；gateway 转发/单测/liveness 消费面 wire 行为逐字节不变。其余 RepSocket 消费方（login-app/base-app/baseappmgr）同步应答语义不受影响，M1 升级路径全协议模块同型适用。

**证据**：modules/protocol/src/socket.cpp:139-172（workerLoop lockstep）、:165（空应答跳发送）、:431-488（桩分支）、:101（nng_rep0_open）；modules/protocol/include/apollo/protocol/socket.hpp:119（RequestHandler 同步返回）；apps/cell-app/src/cell_server.cpp:166-330（handler 返回值：变更类 `{}`/PING Pong）；docs/design/cell-single-writer.md §3(a)/§5 拍板 6；todo.md P1-6 批记（nng 未装两树同走桩）、契约差异清欠 §34.3（nng 解禁评估回退、M1 随 P3-2 批重估）；ADR-015 §3(b)/§4（弃 (b) 队头阻塞原判）。

---

## 相关文档

- [架构总览](/architecture/overview)——七层下行数据流（ADR 的分层视角）
- [术语命名契约](/design/term-contract)——命名的裁决记录
- [架构审计与现状](/analysis/architecture-review)——审计全量证据
