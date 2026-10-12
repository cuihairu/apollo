# 备份容灾与宕机接管设计（backup-revive）

> 状态：**设计稿（前置设计，G-2 立项批，2026-10-09；六拍板点已拍板 2026-10-10——案 A 先行，见 §4 与 ADR-010）**。定位：进程级高可用面——权威进程死亡后「服务不倒」的候选拓扑、仲裁语义与接管时序；与 attribute-sync §8.2 的分界沿既有范围声明：journal 答「数据不丢」，本件答「进程不倒」。缺口登记：**G-2 备份/容灾与宕机接管**（architecture-review §16.4 遗留登记；net-abstraction §7「P3 前置设计」G-2 段为既有骨架口径，本件将其展开为可拍板的候选拓扑）。互引：net-abstraction（§7 G-2 段/§5.7 InterServerLink/§6 sdshmem 决策行）、attribute-sync（§8.2 范围声明/§10.2 停机序列/§11 验收表）、session-and-online-directory §6（恢复矩阵）、clock-and-time（§6 跨进程不互信/§7 双时间戳）、capacity-and-benchmark（§1 容量目标/§5 指标集）、crash-capture（取证互证面）、concept-glossary「热备与接管」词条、architecture-review（§16.2/§16.4/C-51/C-52/§30 简化三刀）、36 号（§0 证据分级/问 5/问 7）、battle-instance-offload §7（实例故障域独立拍板）。

---

## 执行摘要

1. **为什么 G-2**：单写者进程死亡 = 权威瞬间消失——journal 保证数据零丢失（attribute-sync §8.2，P1-4/P1-5 已交付），但「数据在、服务停」：进程死到恢复之间其承载的全部会话与场景停摆，易失态（L3）无处置语义。三家先例只有 BigWorld 有完整此层（C-51/C-52：两家真正的代差在**备份与接管链路**，不在行存储形态）；KBE 无此层的代价 = 宕机回档到最后归档窗口（300s 级），skynet 单节点无编队。
2. **已有什么（本设计只消费不重做）**：案 A（冷重启+持久态+重连）的骨架**已在仓里**——machined 监督面（roster+线性退避重启+超限放弃+死亡事件四型）、死亡事件跨进程上报（G-1 收尾批增量①）、恢复相位跨进程化（FleetRecoveryCoordinator：排他相位+全量重报+双判据收敛）、目录镜像（"APD2" wire 族）、PlayerDirectory（anchor_epoch 裁决键+Suspended 窗口+批量反查）、PersistJournal（append/drain/replay/compact）、RecoveryCoordinator（restore→admission→ready）、baseappmgr 准入闸与 `--recovery-*` 接线。G-2 的真正增量只在两处：**状态如何到达接管方**（镜像流/双写）与**谁来裁定接管**（仲裁面）。
3. **BW 先例的机制拆解（A 级实读，2026-10-09 本轮补读）**：备份发送面按 tick 分帧摊写（`backupRemainder_` 小数余数跨 tick 结转）+ 备份链双代哈希并存切换（`entityToAppHash_`/`newEntityToAppHash_` + `isUsingNewBackup_`）+ `BackupHashChain` 换代再分布（编队扩缩容不断备份）+ reviver 独立进程（经 machined 请求拉起、machined tags 限定可复活组件型）+ secondary db 补写。BW 把备份链做成了与扩缩容深度耦合的独立子系统——二十年演进的收益面与复杂度实锤同时在场。
4. **apollo 骨架口径（既有登记，本件展开为候选）**：热备流 = PersistJournal 的第二只读消费者（journal 三用一位：恢复位点/热备镜像/落库队列，不另起备份协议）；备机 ack 的 journal 位点即接管起点；接管 = 位点重放后于 tick 边界切权（attribute-sync §10.2 停机序列的镜像路径）；**不设独立 reviver 进程**——reviver 语义进 baseappmgr 域（architecture-review §30 语义合并三刀既有登记）。net-abstraction §7 既有骨架行「backup-hash 链 + reviver 两件先行、secondary db/动态扩缩容推迟」是否成立归拍板 1/2 确认或改写——**已改写（ADR-010：backup-hash 链不先行、reviver 不设，见 §4）**。
5. **反证必须诚实**：apollo 容量目标是单进程 5000 CCU、单机 8GB（capacity-and-benchmark §1/§4），P3 编队为小拓扑七类进程——BW 热备链解决的是无缝世界跨机大编队（坦克世界 19 万 CCU 口径，C/D）里的常态单点死亡，爆炸半径与 apollo 单 baseapp 死亡不同量级；当代小/中部署主流实践（D 级，行业通型）= 进程管理器自动重启 + 持久化状态 + 客户端重连，热备的采用集中在无缝大世界与强连续性玩法。案 A 的 RTO = 秒级、RPO = 零（journal 零丢失已是 §11 指标）；热备的真实增量 = RTO 秒级→亚秒级 + 易失态保留，代价 = 备机常驻 + 镜像流带宽 + 双主仲裁复杂度。**单机编队下 standby 同机同死——拍板 1 的实质是部署形态拍板，不是纯技术选型。**
6. **候选拓扑三案**（§3）：A 冷重启+持久态+重连（全复用已交付件，增量≈0）；B 热备 standby+状态同步（BW backup-hash 同型，热备流 + 位点 ack + 接权仲裁）；C 无 standby 状态双写（journal 远端双写/重放重建——无常驻备机，RPO 由同步策略决定；active-active 副本与本仓单写者纪律冲突，如实记录为不采用形态）。三案共享同一地基：journal 位点锚 + epoch 仲裁 + 恢复相位排他 + machined 拉起——差异只在状态到达接管方的路径。
7. **拍板点六项**（§4，已拍板 2026-10-10 / ADR-010）：案 A 先行（restart-only）、接管三档、案 A 无双主不新造仲裁、RPO=0/RTO≤3s（批 0 实测定档）、镜像流不建、验收链=批 0 演练。原文「不代拍、不设候选倾向」纪律在拍板前有效，裁决记录见 §4 与 ADR-010。
8. **分期**（§6）：批 0（不依赖拍板）已交付——kill -9 演练验收链 + RTO 实测数字入库，即案 A 的完整形态；**拍板=案 A，B/C 后续三批（位点面/镜像流/接权仲裁）全部取消，G-2 关闭**（重开条件=生产数据提出亚秒 RTO/多机形态立项）。

## 0. 适用与不适用

- **现状声明（显式，沿 net-abstraction §7 口径）**：apollo 单进程阶段**无进程级高可用**——崩溃安全 = write-behind journal 数据不丢 + 重启拉起；backup/接管/迁移为零起步。这是分期不是遗漏。

| 维度 | 适用（本设计管） | 不适用（既有件/其他设计管） |
|---|---|---|
| 崩溃事后取证 | 只消费 machined 死亡行（exit 128+signo）与 dump 落盘互证 | 采集与符号化本体 = crash-capture.md（已交付） |
| 进程拉起重启 | Supervisor 事件与恢复相位的联动语义 | fork/exec + 退避重启本体 = apps/machined（批 F 已交付） |
| 数据不丢 | journal 位点作为接管锚的**消费语义** | write-behind/journal/replay 本体 = attribute-sync §8.2（已交付） |
| 单机 dev 编队 | 案 A 已覆盖——本设计只补演练验收链（批 0） | 无 standby 收益场景（standby 同机同死） |
| 多机生产编队 | 案 B/C 的镜像流与仲裁面（待拍板 1/5） | 跨机传输底座 = net-abstraction §5.7/§7（M1，未落地） |
| 单玩家掉线/重连 | 只界定与进程级接管的窗口交互（拍板 3） | resume/grace window = net-abstraction §3 + session-and-online-directory §4（已交付） |
| gateway 死亡 | 目录窗口处置的消费面确认 | 窗口处置本体已交付（§6 死亡行遗留批） |
| 无缝世界跨进程实体接管 | **不适用**——ghost 否决（36 号 #9），无 cell 边界迁移问题 | — |
| Battle 实例进程 | 拍板 2 粒度表占一行 | 实例崩溃裁决 = battle-instance-offload §7（独立拍板，互不吞并） |
| 无状态进程（login/verifier/interfaces） | 拍板 2 粒度表占一行（重启即恢复） | verifier 无 G-2 热备需求 = battle-verification-service §5 既有口径 |

## 1. 先例证据与反证（A/B/C/D 分级沿 36 号 §0）

### 1.1 BigWorld 备份/接管全套（A 级本地源码实读）

- **备份发送面** `server/baseapp/backup_sender.hpp`：`autoBackupBase`/`backupBase` 分帧发送（architecture-review §16.2 已核 :52-61）；每 tick 只备 `offloadPerTick_` 个 base，小数余数 `backupRemainder_` 跨 tick 结转（backup_sender.cpp:114-117，clock-and-time §7 已引）；`handleBaseAppDeath`/`restartBackupCycle`/`setBackupBaseApps`——主机死亡感知与备份循环重建的钩子都在发送面（本轮实读同文件 40-70 区段）。
- **备份链数据面** `lib/server/backup_hash.hpp`：`MiniBackupHash`（prime/size/virtualSize，`hashFor(EntityID)`——头注自述「handles changing hash sizes nicely…keep most values hashing to the same bucket」）派生 `BackupHash`（实体 id → baseapp 地址）；`backup_hash_chain.hpp` 的 `BackupHashChain`——编队扩缩容时哈希换代再分布的链式结构（baseapp.hpp:38/:237/:450 持链）。**两代哈希并存切换**：backup_sender.hpp 的 `entityToAppHash_`/`newEntityToAppHash_` + `isUsingNewBackup_` + `ackNewBackupHash()`——再分布期间新旧映射并行、逐实体确认后整体切换（扩缩容不断备份的双缓冲，本轮实读）。
- **备机面** `server/baseapp/backed_up_base_app` 与 `offloaded_backups` 两族——备机持有的镜像实体记录与「offload 后」备份账目（文件清点，本轮实读）。
- **接管面** `server/reviver/` 独立进程（与 baseappmgr/cellappmgr/dbappmgr 同级入 server/ 顶层清点；component_reviver/reviver/reviver_config/reviver_interface + main 五件）：`Reviver::revive` 经 machined 请求拉起死亡进程（reviver.cpp:439-446）；`queryMachinedSettings` 读 machined tags 限定本机可复活组件型（:272-296）；`handleTimeout` 按 `ReviverPriority` 分型轮询（:300 区段）。接管语义 = 主死后备机把镜像实体升级为权威；cell 死则 cellappmgr 在幸存 CellApp 重建 cell（B 级登记：architecture-review §16.4 G-2 行与 baseappmgr 域块）。
- **持久化次级面**：secondary db 任务族 + dbappmgr 扩缩容哈希再分布（dbappmgr.cpp:472/:637，B 级已核）。

### 1.2 KBE/skynet 负先例（B 级登记）

- **KBEngine 无此层**（C-51）：无 journal、无热备、无接管——宕机恢复 = Archiver 最后归档窗口（默认 300s、非脏驱动），玩家损失窗口 = 归档间隔、会话全断；进程清单九类（`kbe/src/server/` 清点）无 reviver 对应物（architecture-review :2689 权威行）。KBE 的教训是「无此层的代价」，**不是可抄的简化形态**——它简化掉的正是 G-2 本身。
- **skynet**：单节点服务编排框架，编队/接管零内建；cluster 是消息层不是容灾层（B 级登记）。

### 1.3 反证（anti-evidence，诚实记录）

- **容量口径**：单进程 5000 CCU、单机 8GB、稳态 RSS ≤4GB（capacity-and-benchmark §1/§4）；P3 编队 = 七类进程小拓扑（machined/baseappmgr/login/gateway/baseapp/interfaces/verifier，多数每类一实例）。单 baseapp 死亡的爆炸半径 = 该 baseapp 的场景与会话，machined 退避重启 + journal replay + 目录全量重报的恢复链以秒计（目录重建 <1s @ 5000 条，session-and-online-directory §6）。BW 热备链为无缝世界跨机大编队里 baseapp 双位数实例、单台死亡为常态事件的形态而生（19 万 CCU 口径 C/D）——形态前提不同量级。
- **行业通型（D 级，存疑标注）**：当代小/中部署主流 = 进程管理器自动重启 + 持久化状态 + 客户端重连（容器编排 restart-on-fail 惯例同型）；实例制/分线制游戏服务端普遍接受「重启+重登」（EQEmu/TrinityCore 生态口径，D）。热备在 MMO 的采用集中于无缝大世界与强连续性玩法。apollo 目标玩法是副本/分线型 MMORPG（36 号 #9）。
- **BW 自身的复杂度实锤（A 级）**：两代哈希并存切换 + BackupHashChain 换代 + dbappmgr 哈希再分布 = 备份链与扩缩容深度耦合的工程代价。apollo 若走案 B，扩缩容面可沿既有登记推迟（secondary db/动态扩缩容后置），但「镜像流 + 位点 ack + 仲裁」三件是案 B 的不可再减核心。
- **单机形态下 standby 同机同死**：热备只在跨机编队有增量价值；拍板 1（部署形态）逻辑上先于拍板 5（传输形态）。

## 2. 既有件消费面（本设计不重设计，逐件对位）

| 既有件 | 位置 | G-2 消费方式 |
|---|---|---|
| machined Supervisor | apps/machined/src/supervisor.hpp/.cpp | roster 花名册（`name | component_id | zone_id | cmd args...`）+ fork/exec 拉起 + 线性退避重启（超限放弃）+ 死亡四型事件——案 A/C 的执行半边已交付；案 B 若立项 = roster 增 standby 角色列，监督面本体不动 |
| 死亡事件跨进程上报 | modules/net/discovery（G-1 收尾批增量①，op 6/7） | DeathSubscribe/DeathNotify——接管触发的**唯一事件源**；G-2 不新设心跳/死亡检测面 |
| FleetRecoveryCoordinator | modules/game/session/fleet_recovery.hpp/.cpp | Normal↔Recovering 排他相位 + FullReport 分片重组 + 双判据收敛（全报或超时开放）——baseappmgr 死恢复相位已交付；baseapp 死接管窗口若立项 = 同型相位复用 |
| DirectoryPublisher/Mirror | modules/game/session/directory_mirror.hpp/.cpp | "APD2" delta/snapshot wire + seq 续传——接管后目录面随 baseappmgr 重建自动收敛，G-2 零改动 |
| PlayerDirectory | modules/game/session/player_directory.hpp/.cpp | anchor_epoch（幂等键+竞争裁决键）+ Suspended 窗口 + `mark_suspended_by_zone/gateway` 批量反查——接管后旧主迟到消息按 epoch 失效拒绝的裁决基础 |
| PersistJournal | modules/data/orm/persist_journal.hpp/.cpp | append/drain/replay/compact + `next_sequence()`——热备镜像流的源（第二只读消费者，单写者纪律不破）与接管重放位点（journal 序号即位点标尺） |
| SaveQueue/autoSaveLoop | apps/base-app（base_server） | write-behind 消费面 + 停机五阶段（attribute-sync §10.2 已交付，CAS 幂等 + 生产者先停 + drain 超时放行）——切权序列 = 停机序列的镜像路径，零件级复用 |
| RecoveryCoordinator | modules/game/session/recovery.hpp/.cpp | restore→admission→ready 启动序列 + 三档恢复清单（lifecycle §3 矩阵代码形态）——重启方/接管方统一启动序 |
| baseappmgr 准入闸 | apps/baseappmgr（`--recovery-port/--recovery-expected/--recovery-timeout-ms`、`--suspend-window-ticks`、`set_admission_gate`） | 恢复相位排他接线已交付——接管期拒新复用同一闸门；收敛开放即发重建快照衔接镜像面 |
| TimerWheel | modules/base/timer_wheel | 喂钟驱动/单写者——接管超时、镜像滞后告警、仲裁 deadline 的计时件（deadline = tick 号，clock-and-time §8） |
| InterServerLink | net-abstraction §5.7（设计态） | authority_epoch 握手三元组 = 对端重启判据（≠ 断线）；断连 in-flight 按 invoke_mode 分——G-2 镜像流已列名为该设施既有消费方，不开第二 API 面 |

## 3. 候选拓扑

### 3.0 共同地基（三案共享）

接管锚 = journal 位点**不是时间点**（clock-and-time §6：跨进程时钟不互信——双主判定与接管时序禁止 wall 差值，判定序走 epoch/位点/baseappmgr 串行）；切权 = tick 边界（复用 §10.2 停机序列镜像路径：先停旧主写入认定，再切权）；事件源 = machined 死亡事件（唯一）；拉起 = machined；恢复期排他 = 恢复相位（已交付）。**三案差异只在「状态如何到达接管方」一件事上。**

### 3.1 案 A：冷重启 + 持久态 + 重连（restart-only）

- **形态**：machined 检测死亡 → 退避重启 → RecoveryCoordinator restore（journal replay）→ admission → ready → baseapp/gateway 全量重报收敛 → 客户端 resume/重登。恢复相位期间拒新。
- **依赖**：已交付件全复用；零新增进程、零新增协议面。
- **成本**：≈0（只剩批 0 演练验收链与 RTO 观测指标）。
- **覆盖面**：持久态零丢失（journal）；目录重建 <1s @ 5000 条；易失态（L3：位置/临时 buff）按既有语义归零——玩家侧表现 = 掉线重连，位置回最后落盘态。
- **失败语义**：RTO = 退避（1s 级）+ restore + 重报 ≈ 秒~十秒级（量级口径，批 0 实测校准）；恢复期该进程承载面停摆（排他相位防拓扑再变）；重启超限（5 次）放弃 = 人工介入面。

### 3.2 案 B：热备 standby 进程 + 状态同步（BW backup-hash 同型）

- **形态**：standby 常驻消费热备流（PersistJournal 第二只读消费者，RO_MIRROR 形态）→ 周期 ack 备份位点；baseappmgr 检测主死（死亡事件）→ 仲裁 standby 接权 → standby 自位点补重放尾差 → tick 边界切权 → epoch 推进 → SessionUp 重报进目录。
- **依赖**：InterServerLink M1（镜像流承载，未落地）；journal 跨进程消费改造（现为单进程 SaveQueue worker）；位点 ack 协议（internal 域 id 900+ 分段）；双主仲裁面（拍板 3）；machined roster standby 角色列。
- **成本**：备机常驻算力（×接管粒度内进程类型数）；镜像流带宽（journal 写路径放大一倍量级）；位点协议 + 仲裁面开发；扩缩容若将来接入 = BW 两代哈希切换的对应物（推迟面，既有登记后置）。
- **覆盖面**：RPO → 0（镜像流覆盖 journal 全量且 ack 位点不丢的前提下）；易失态可纳入（镜像范围扩到 L3 则带宽再涨——拍板 4）；RTO 亚秒~秒级（尾差重放极短）。
- **失败语义**：**双主是新增失败模式**（旧主假死复归/网络分区）——仲裁必须先于接权且判据唯一（拍板 3）；镜像流滞后窗口内变更 = 接管方不可见（RPO 上界由 ack 位点差度量，指标化归批 1）；standby 自身死亡降级为案 A（监督面拉起重建，镜像流全量重放即重建 standby——重建期无保护）。

### 3.3 案 C：无 standby 但状态双写

- **C1 journal 双写**：journal append 同步或异步镜像到远端持久面（对端进程/共享存储——部署形态随拍板 5），接管方（幸存进程或 machined 新拉起进程）从远端 journal 重放。无常驻备机、无镜像 ack 面。
- **C2 关键态跨进程副本（active-active 状态机复制）**：与本仓单写者纪律直接冲突（两处可写 = 双权威，单写者的进程间推论，session-and-online-directory §1 同论证）——如实记录为**不采用形态**，列出只为完整性。
- **依赖（C1）**：远端持久面选型（部署面拍板）；重放面 = 既有 replay；位点对账（本地/远端 journal 序号一致性校验）。
- **成本**：中——无备机常驻，但 journal 写路径加一跳；同步双写把远端延迟引进 tick 预算（persist-batch 段，capacity §2），异步双写则 RPO 有窗口。
- **覆盖面**：RPO 由双写同步策略决定（同步 = 0，异步 = 窗口）；RTO 与案 A 同级或更慢（重放距离可能长于本地 journal）。
- **失败语义**：无双主（写只在 journal owner）；**新增远端存储故障面**；远端/本地 journal 一致性靠位点对账，失配处置需定义（快照重置 or 拒绝接权）。

### 3.4 三案对照

| 维度 | A 冷重启 | B 热备 | C 双写 |
|---|---|---|---|
| 新增进程 | 0 | standby ×N | 0 |
| 新增协议面 | 0 | 位点 ack + 接权指令 | journal 镜像写入 + 位点对账 |
| RPO | 0（journal 零丢失既有指标） | →0（ack 位点） | 策略决定（0~窗口） |
| RTO 量级 | 秒~十秒 | 亚秒~秒 | 秒~十秒（随重放距离） |
| 易失态（L3） | 归零 | 可纳入（带宽另计） | 归零 |
| 新增失败模式 | 无 | 双主/镜像滞后 | 远端存储故障 |
| 依赖成熟度 | 全部已交付 | InterServerLink M1 未落地 | 部署面未定 |
| 与既有登记口径的关系 | 与「单进程阶段显式无高可用」直接相容 | 与 net-abstraction §7「backup-hash 链 + reviver 两件先行」同向展开 | 无既有登记，全新面 |

## 4. 拍板点（六项已拍板 2026-10-10——案 A 先行，standby 不立项；裁决全文见 ADR-010）

以下六项原为决策面枚举（悬置期不代拍、不设候选倾向）；2026-10-10 巡检授权批按本件反证段 + 批 0 实测裁决，逐项结论：

1. **部署形态与 standby 必要性** → **案 A 先行（restart-only），standby 不立项**。单机编队下 standby 同机同死（§1.3 反证）；5000 CCU 单机目标下案 A 的 RTO 秒级/RPO 零已满足；案 B/C 不做分期预留，生产数据提出亚秒 RTO 需求时另行立项。net-abstraction §7「backup-hash 链 + reviver 两件先行」改写为「backup-hash 链不先行、reviver 不设」；architecture-review §30「不设独立 reviver 进程」确认维持。
2. **接管粒度 per 进程类型** → **按进程类型分三档**（不统一单语义）：有状态重建档 = baseappmgr（全量重报，既有）+ baseapp（冷重启 + journal replay + 重报收敛，批 0 已实测）；无状态重启档 = login-app / verifier（worker 池）；崩溃即作废档 = Battle 实例（ADR-012）。
3. **仲裁与 epoch 规则** → **案 A 无双主，不新造仲裁协议**；epoch 沿用 directory_mirror 既有语义（单调递增 + 陈旧 epoch 拒收），baseappmgr 串行仲裁独任维持；恢复相位与接管窗口排他次序按 session-and-online-directory §6 既有矩阵。
4. **RPO/RTO 目标值** → **RPO = 0**（journal write-ahead 零丢失，attribute-sync §11 既有指标升格）；**RTO = 重报收敛 ≤3s / 镜像收敛 ≤3.5s**（批 0 满载噪声期实测上限定档，空载更优；主导项 = 重启退避 1s）。易失态（L3）不纳入接管面（重连回落盘态，capacity §5 显式登记）。
5. **镜像流传输形态** → **不建**。journal 跨进程消费改造不做（现状单进程 SaveQueue worker 维持）；同机 shm SPSC / InterServerLink 两候选形态留档，随未来 B/C 立项再裁。
6. **先行试点与验收链** → **验收链 = 批 0 kill -9 演练链**（已交付，scripts/drill_kill9.sh）；双主注入演练不进门禁（案 A 无双主）。G-2 术语四词随本拍板定案：热备/接管词条标注「未立项（案 A 先行）」。

## 5. 术语提案（新词先入 glossary 再入契约——本件不改两档）

已有「热备与接管 backup / reviver（G-2）」「恢复相位 recovery phase」两行命名锚；本设计若立项，以下**细化名词**先入 concept-glossary 辨析后钉死（随立项批入库）：

| 中文名（拟） | 英文名 | 代码标识（拟） | 定义一句 | 对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 热备进程 | standby | `Standby`（角色枚举 / roster 列名） | 常驻消费热备流、待命接权的进程角色 | BW = backup baseapp；KBE 无 | 禁：Slave/备胎/影子（他家词与口语） |
| 接管 | takeover | `Takeover`（事件/消息名根） | 接权方在备份位点重放后于 tick 边界升为权威写者的动作 | BW = backup 面升权（reviver 只管拉起不管升权） | 禁：failover（他家术语不入户）；与「重启」分清——重启 = machined 拉起，不含状态迁移 |
| 备份位点 | backup position | `backup_position`（u64 = journal sequence） | 热备流上接权方已确认追平的 journal 序号——接管重放起点 | ViewerState.acked_seq 同模型（属性 seq 水位的进程间实例） | 禁：「位点」裸用作代码标识（叙述层沿用） |
| 热备流 | backup stream | `BackupMirrorStream` | PersistJournal 的第二只读消费流（RO_MIRROR 形态，写只在 owner） | BW backup_sender 分帧流；net-abstraction §7「G-2 镜像流」既有口语的定名 | 禁：backup_sender（他家名）；「镜像流」裸用作代码标识 |

- **外部度量词汇注记**：RPO（恢复点目标）/RTO（恢复时间目标）为行业通型度量缩写——文档叙述面使用，不入代码标识（度量口径属文档叙述面用词，此为边界注记）。双主（split-brain / dual-primary）作失败模式描述词，同口径文档面使用。

## 6. 分期（拍板后路径）

- **拍板前**：零代码——本件为纯设计件（源码冻结面照旧）；批 0 不依赖拍板结果。
- **批 0（不依赖拍板）——验收链演练**：dev_fleet 编队 kill -9 全链（死亡上 wire → machined 退避重启 → journal replay → RecoveryCoordinator ready → 全量重报收敛 → 客户端 resume）+ RTO 实测数字进 capacity-and-benchmark §5 指标集（「崩溃回档 ≤2s」行旁补「进程恢复全链耗时」分项）。案 A 的完整形态即批 0 终态——无论后续拍板如何，此链都是案 B/C 的降级兜底与验收基座。**批 0 已交付（2026-10-09）**——scripts/drill_kill9.sh 演练脚本（独立端口段编队：machined --interval 500 --backoff 1000 + manager[恢复相位+镜像 owner] + zone-app[report+demo-anchors 3]，行首 HiRes 时间戳 + grep 轮询断言面；kill 目标定位走 `pgrep -x` 进程名精确匹配——实测 `-f` 全参匹配会误中外层 shell 命令行[roster 文本内含同名串]，ts 行管道 $|=1 行缓冲——块缓冲会把已写行压在 stdio 里致断言超时，两坑如实记）；两跑全链断言全过：死亡检测 14-55ms（exit=137 上 wire → [death] kind=child_died）→ [window] zone death suspended=3（挂起窗口扣留）→ 退避重启 1.04-1.10s（attempt 1 = 1×backoff）→ recovery ready（journal replay 空档 + Anchor restore）→ 全量重报 intake=3（restore-not-kick，窗口内恢复不 kick）→ 镜像 entries 3→0→3 收敛（2.3-2.8s，上限=快照周期 500ms 量化的观测面口径）；如实注记：①初始启动重报与 manager UDP bind 有竞争（fire-and-forget 丢失），收敛走超时判据（5s）开放——收敛双判据设计意图的正向验证，非缺陷；②journal replay/Anchor restore 段为空档演练（demo anchors 内存态、无 player 档案），实测数字是重放耗时下界，带载 RTO 复测随拍板点 4 定档；③客户端 resume 段为目录级（epoch/锚点恢复），真实会话链路随登录/gateway 批。RTO 数字已入 capacity-and-benchmark §5 指标集「进程恢复全链耗时」行。
- **批 1（拍板 B/C 后）——位点面**：journal `next_sequence` 跨进程可见（ack 位点消息进 internal 域契约，id 900+ 分段照旧，契约随实现批入库）；镜像滞后位点差指标进 MetricRegistry；RPO 度量口径入 capacity §5 指标集。
- **批 2（拍板 B 后）——镜像流**：传输形态随拍板 5（同机 shm SPSC / InterServerLink 跨机）；`BackupMirrorStream` 消费面接 journal（第二只读消费者，单写者纪律验证入测试）；standby 角色生命周期（roster standby 列 + 监督面零改动验证）；standby 迟到/重启的全量重放重建路径。
- **批 3（拍板 B 后）——接权仲裁**：双主检测（epoch 失配判据接线，判据随拍板 3）；切权序列（§10.2 停机序列镜像：先停旧主写入认定再切权，CAS 幂等复用）；baseappmgr 恢复相位联动（接管窗口排他）；双主注入演练验收。
- **批 4（拍板 C 后）——双写面**：远端持久面接线（形态随拍板 5 部署面）；位点对账（本地/远端 journal 序号一致性校验 + 失配处置随拍板 3）；重放距离指标。
- **依赖序**：批 1-4 全部依赖 net M1（InterServerLink 落地）与拍板 1/3/5 结果；批 0 无依赖先行。

## 7. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| net-abstraction | §7 G-2 骨架段 = 本件上位口径（热备流 = journal 第二消费者/位点即接管起点/tick 边界切权——本件展开为候选拓扑待拍板）；§5.7 InterServerLink（镜像流承载 + authority_epoch 重启判据 + 消息分类声明）；§6 sdshmem 决策行（同机镜像流既有候选） |
| attribute-sync | §8.2 范围声明（「数据不丢」与「进程不倒」分界——本件即「进程不倒」面）；§10.2 停机五阶段（切权序列的镜像路径零件，CAS 幂等/生产者先停复用）；§11 崩溃回档指标（RPO 目标的既有锚） |
| session-and-online-directory | §6 恢复矩阵三行（baseappmgr/gateway 已定、baseapp 行接管语义归拍板 2/3）；anchor_epoch = 接权后裁决键（PlayerDirectory 既有实现） |
| clock-and-time | §6 跨进程时钟不互信（接管锚 = 位点非时间点；双主判定禁 wall 差值）；§7 journal 双时间戳（重放定位 (tick, wall_ms) 双写照旧） |
| capacity-and-benchmark | §1 容量目标（5000 CCU 单机 8GB——拍板 1 的输入）；§5 指标集（RTO 分项与 RPO 度量的消费面）；§4 内存预算（案 B standby 常驻的量级重估） |
| crash-capture | dump + 符号栈 = 事后证据，machined 死亡行（exit 128+signo）互证——G-2 只消费死亡事件，不重复取证面 |
| battle-instance-offload | §7 故障域（实例崩溃即作废 vs 关键帧重放——拍板 2 的 Battle 行与该拍板互不吞并） |
| battle-determinism | §5 录制格式（若案 C 走 intent 重放线的格式来源——与 journal 重放分属两线：journal 重放持久变更，intent 重放推演判定序，不混用） |
| battle-verification-service | §5 部署口径（无状态 worker 池无 G-2 热备需求——拍板 2 verifier 行既有依据） |
| architecture-review | §16.4 G-2 登记（本件即该缺口立项设计面）；C-51/C-52（KBE 无此层/代差在备份链路的证据）；§30 简化三刀「backup+reviver → journal 位点 + manager 恢复相位」（既有登记，拍板 1/2 确认或改写） |
| concept-glossary | 热备与接管、恢复相位两行命名锚；§5 术语提案的入库前置（先 glossary 辨析） |
| scripts/dev_fleet.sh | 批 0 演练链的编队驱动（up/roster/status/down 既有面；crashdumps/ 与 fleet 日志目录共存惯例沿 crash-capture §2.2） |

## 8. 发版更新与玩家迁移（2026-10-12 增补）

> 定位：把「发新版本」放到既有接管面（§2 既有件 + §6 分期 + ADR-010）上回答——玩家怎么从旧版本进程走到新版本进程。两条路：路 1 现状已具备，路 2 挂后续批。

**路 1：等断线重连再更新（现状已具备，先做）**

- 链路 = §6 批 0 既有面：旧版本进程停止（kill）→ machined 退避拉起（新版本二进制就位后重启即新版）→ 恢复相位排他拒新 → 各报告方全量重报收敛（drill 实测 2.71s）→ 锚点从 journal 重放 → 客户端断线重连回原进程。
- 收益 = **断线后能快速重连**（参考火龙果，用户 2026-10-12 口径；不引伸其他收益）。
- 缺口 = 更新期间 WorldAssignment（落点指派）要跳过正在更新的进程——否则重启窗口内新玩家会被指到将死的旧进程（machined 会兜住重启，但玩家多挨一次断线）。
- **缺口已接（2026-10-13）**：排空闸（SIGUSR1 开/SIGUSR2 关 → 1s 主循环拍消费 → `set_drain`，排空期 bindSession/assignWorld 拒新不拒链，apps/baseappmgr）+ fleetctl 探针（对 baseappmgr M1 控制面 assign/resolve，exit 0=录取 / 2=排空拒新 / 3=链不通）+ 端到端演练脚本。

**路 1 验收演练（2026-10-13 实测，两跑全绿）**

命令：`scripts/drill_update.sh`（拓扑 = machined:19610 + baseappmgr:19013（`--m1-port 19105` 探针面）+ base-app:19014（`--demo-anchors 3`）；种 3 份 `player_N.json` 真档案——flush 以现档为基线回写，roundtrip 字节恒等，故逐玩家 sha256 前后比对可作存档一致性断言）。分段实测（两跑并列）：

| 断言面 | 实测 |
|---|---|
| 排空闸信号→生效（USR1→ON / USR2→OFF） | 0.749 / 0.952s；0.659 / 0.921s（≤1s 主循环拍，等待必须 ≥1s） |
| 更新期新玩家落点 | fleetctl assign 双探针 exit 2（ASSIGN_REJECTED）；排空窗内镜像快照 `entries=3 parts=1` 恒定 = 零新落点 |
| 在线玩家掉线数 | **0**（重启后 full report `sessions=3 intake=3`） |
| 优雅停（SIGTERM→child death exit=0） | 0.506 / 0.863s；停机日志 `journal pending: 0`（五阶段：M1 收线→shutdown_flush→journal 排空） |
| 退避重启（backoff 1s） | 1.511 / 1.868s |
| 恢复相位→全量重报收敛 | 2.441 / 2.805s（对照 kill -9 全链 2.71s） |
| 目录镜像收敛（entries=3） | 2.442 / 3.808s（量化上限 = 快照周期 500ms） |
| 存档一致性 | 逐玩家 sha256 前后比对 **3/3 字节恒等**（非抽查） |
| 换版证据 | cp+mv 原子替换（同 fs rename），运行中 exe inode 14551279→14592675；重启后 `/proc/<pid>/exe` inode = 磁上换版件。缺省同版重部署（sha256 相同，脚本如实记录） |

对应 §9.9 第 5 项（落点排除）就此接线完毕；余项（跨进程快照拉取 / gateway 上行缓冲 / epoch 持久化）仍挂路 2 批。

**路 2：新 baseapp 装好新版本，再把玩家切过去（不掉线/可灰度，挂后续批）**

三件必做：

1. **跨进程取数据**：新 baseapp 向旧 baseapp 拉玩家快照（旧的先 flush 再发）——保持 ADR-007 零外部存储（玩家档案 = JSON 原子写 + PersistJournal write-behind，不引 MySQL/Redis）。
2. **写权交接**：用既有 anchor_epoch 递增裁决——新进程持新 epoch，旧进程迟到的回执按旧 epoch 拒绝（player_directory.hpp 注释即此语义；接管窗口排他防双写，§4 拍板 3 既有裁决）。
3. **目录改指 + gateway 路由快照立即刷新**：PlayerDirectory 条目改指新 baseapp 后，gateway 的路由镜像要立即刷新——现行为事件 delta 即时推送 + baseappmgr 全量快照周期兜底（`--mirror-snapshot-ms` 默认 10000，apps/baseappmgr/src/main.cpp:182）；切换语义要求目录改指后快照立即下发，不等周期到点（触发面挂后续批定）。

**验收（只此一条）**：玩家在切换中不掉线、数据不多不少。

**结论**：先做路 1（能马上发版）；路 2 挂后续批，动工前改动清单先报用户。——**§9 已把路 2 升为主案（用户 2026-10-12 拍板），本节历史口径保留不改写；主案与兜底的分工见 §9.1。**

## 9. 整体不停机更新（滚动换版本）（2026-10-12 增补）

> 定位：把「整机换版本」放在既有接管面上回答——玩家级迁移是主案，断线重连是兜底。与 §4 案 A（restart-only，ADR-010）不冲突：主案是案 A 之上的前推面——玩家在更新前已被搬走，被换进程手上已无会话，案 A 仍是主案的降级兜底。
>
> **主案（用户 2026-10-12 拍板，原文）**：老 baseapp 标记停止分配新玩家 → 排空时把玩家复制到新 baseapp → gateway 把该玩家切到新 baseapp → 老进程换二进制。
>
> **主案一句话**：不重启承载玩家的进程，先把玩家逐个搬到新版本进程，搬空了再换老进程的二进制——老进程换版本时手上已无会话，重启代价从「全量重报 + 玩家重连」降到「空进程拉起」。
>
> 纪律：本节不新增词条（用户令）；全部表述为动作描述，不作代码标识。唯一拟定的代码标识（迁移并发度参数，§9.3）标「待定」，接线时定名。别家做法一律「未核实」（§9.7）。

### 9.1 状态机（七角色 × 五态）

五态名（用户 2026-10-12 给定的大白话）：**正常服务 → 停止分配新玩家 → 排空 → 换二进制重启 → 恢复-全量重报收敛**。四栏：进入条件 / 退出条件 / 超时 / 失败回退。

**baseapp（主案主角）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 启动完成 ready，或上一轮恢复收敛 | 排空指令到达（更新序轮到本进程） | — | — |
| 停止分配新玩家 | 排空指令生效（目录侧不再把落点指给本进程——§8 路 1 记的缺口接线） | 确认新玩家零落点（快照周期内不再收到新落点） | 10s（路由快照周期兜底 `--mirror-snapshot-ms` 10000，apps/baseappmgr/src/main.cpp:182） | 指令未生效 = 不进排空，回正常服务并上报（不静默前进） |
| 排空 | 已确认无新玩家落点 | 在途玩家全部走完五步（§9.2） | 无硬上限；不收敛 = 升级人工介入 | 单玩家五步失败 = 撤销该玩家的①②，回老进程继续服务，排空继续；连续失败超阈 = 中止本轮，老进程回正常服务 |
| 换二进制重启 | 排空完成（手上无会话） | 新版本 binary 拉起且 ready | 退避 × 重启次数（attempt N = N×backoff，`--backoff` 1000 时 attempt 1 实测 1.04-1.10s），超限 5 次放弃 | 新 binary 起不来 = 回滚旧 binary 再拉（手上无会话，回滚代价 = 一次拉起） |
| 恢复-全量重报收敛 | 新版本 ready 后目录重报 | 收敛双判据成立（全量重报齐 或 超时判据开放） | drill 实测 2.71s；超时判据 5s | 超时开放（既有双判据——收敛或超时，批 0 记录为设计意图非缺陷） |

**cellapp（主案不覆盖，走兜底）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 启动完成 ready | 更新序轮到本 cell（排在所有 baseapp 之后） | — | — |
| 停止分配新玩家 | 目录侧停落点 | 快照周期内不再有新落点 | 10s | 未生效 = 不前进 |
| 排空 | 停落点已生效 | 自然断线清场，或到等待上限 | **缺省不设上限——直接换**。场景内玩家不会自己离开，cell 排不空（主案只搬 baseapp 的锚点，不搬 cell 上的 Avatar） | 到上限 = 直接换二进制，其上的玩家各一次自动重连 |
| 换二进制重启 | 清场或到上限 | 新版本 ready | 退避 × n，5 次放弃 | 回滚旧 binary |
| 恢复-全量重报收敛 | 新版本 ready | 收敛双判据 | 2.71s 口径 | 超时开放 |

cellapp 是七角色里唯一跳过「排空」的角色——它的排空态不可达（场景内玩家不自离），换二进制时其上的玩家各经历一次自动重连（挂起窗口保位、Avatar 从锚点重挂、L3 回落盘态——§4 拍板 4 既有语义）。**本设计对「掉线数 = 0」的口径因此限定为迁移路径玩家**（§9.8）。

**gateway（本滚动轮不动）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 全程不动（客户端 TCP 锚 + 路由 hub） | 不在本滚动轮 | — | — |
| 停止分配新玩家 | **不适用**——gateway 不分配玩家，它持有全体在线玩家的客户端连接 | — | — | — |
| 排空 | **不适用**——gateway 排空 = 全体在线玩家断线 = 停机更新 | — | — | — |
| 换二进制重启 | 另窗（本设计范围外） | — | — | — |
| 恢复-全量重报收敛 | 另窗 | — | — | — |

gateway 是唯一在「排空」态即等于全体掉线的角色，所以它不参与滚动。它的换版本是另一个维护窗的问题，不在本节。

**login（无状态档，ADR-010 拍板 2）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 启动完成 ready | 任意时点 | — | — |
| 停止分配新玩家 | 不适用（无分配语义） | — | — | — |
| 排空 | **不适用**——login 不持有会话，只处理登录请求本身；无可排空对象 | — | — | — |
| 换二进制重启 | 任意时点 | 新版本 ready | 秒级（无恢复相位、无目录重报——无状态重启即恢复） | 回滚旧 binary；或双跑交替（新实例先起、旧实例后停）把窗口压到零——需 roster 双条目与不同端口（部署面，挂注） |
| 恢复-全量重报收敛 | 不适用 | — | — | — |

**manager / baseappmgr（第一站）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 启动完成 ready | 更新序轮到自己（第一站） | — | — |
| 停止分配新玩家 | 排他窗口开启前，准入闸关闭（`set_admission_gate`，apps/baseappmgr） | 全编队静止（无进程在恢复相位中） | — | — |
| 排空 | 准入闸已关 | 确认全编队无恢复相位在飞 | 5s 超时判据 | 有恢复在飞 = 不进重启，等其收敛 |
| 换二进制重启 | 排他窗口内 | 新版本 ready | 退避 × n，5 次放弃 | 回滚旧 binary |
| 恢复-全量重报收敛 | 新版本 ready | 收敛双判据 | drill 实测 2.71s | 超时开放 |

manager 的恢复相位（FleetRecoveryCoordinator Normal↔Recovering）是全编队**唯一的串行闸**：同一时刻只允许一个进程处于恢复相位。滚动更新全程借用这一闸——排他窗口就是滚动序的节拍器。manager 自身换版本是第一个、也是唯一一个「全编队静止」的窗口；此后每个角色换二进制各自占一个排他窗口，串行不重叠。

**machined（排除在滚动序列外）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 全程不动 | 不在本滚动序列 | — | — |
| 停止分配新玩家 | 不适用 | — | — | — |
| 排空 | 不适用 | — | — | — |
| 换二进制重启 | **不可行**——`terminate_all` 对全部存活子进程发 SIGTERM（apps/machined/src/supervisor.cpp:249-255），machined 换版本 = 全编队一起死，与「不停机」直接冲突 | — | — | — |
| 恢复-全量重报收敛 | 不适用 | — | — | — |

machined 自身换版本的两个出口：单独停机窗（不在本设计内），或双监督交接（未立项，挂后续批）。本节不把 machined 列入滚动序。

**logger（后台，随时换）**

| 状态 | 进入条件 | 退出条件 | 超时 | 失败回退 |
|---|---|---|---|---|
| 正常服务 | 全程 | 任意时点 | — | — |
| 停止分配新玩家 | 不适用 | — | — | — |
| 排空 | **不适用**——日志面为 kReliable 有界队列 + 重连补投（恰一次），换版本 = 短暂断流后补投续写 | — | — | — |
| 换二进制重启 | 任意时点 | 新版本 ready | 退避 × n | 回滚 |
| 恢复-全量重报收敛 | 不适用（无目录）——补投续写即恢复 | — | — | — |

logger 不在玩家数据路径上，换版本玩家零感知。

### 9.2 单玩家五步切换（主案核心）

每个玩家严格按序：**① 停写 → ② 拉新 → ③ 交写权 → ④ 目录改指 → ⑤ 放行**。前一步未确认完成，后一步不许开始。不做边玩边拷——边玩边拷会同时出现丢写（拷贝期间老进程又写）与双写（两进程都在写同一玩家）。

| 步 | 动作 | 进入条件 | 退出条件 | 失败回退 |
|---|---|---|---|---|
| ① 停写 | 老 baseapp 对该玩家停写（状态写入与模拟停止，进入只读）；gateway 起上行缓冲（玩家上行暂存，不再转发老 baseapp） | 目录确认该玩家当前在老 baseapp 上 | 老 baseapp 确认写入已停 | 停写不生效 = 不开始拷贝，回退后重发停写；连续失败 = 本轮跳过该玩家 |
| ② 拉新 | 新 baseapp 从老 baseapp 拉该玩家快照（**老的先 flush 再发**——§8 路 2 必做 1，保持 ADR-007 零外部存储） | 停写已生效 | 快照到齐且校验通过 | 拉取失败/超时 = 撤销①（老进程恢复写入，gateway 停缓冲），本轮跳过该玩家 |
| ③ 交写权 | `anchor_epoch + 1` 交新进程；老进程对已切走玩家的迟到回执按旧 epoch 丢弃 | 快照已在新进程落地 | 新进程确认接权（新 epoch 生效） | 新进程拒绝接权 = 撤销①②（新进程丢快照、老进程恢复写），本轮跳过。**③ 确认之后不可撤销**——epoch 单调递增，旧 epoch 已被判陈旧 |
| ④ 目录改指 | PlayerDirectory 条目改指新 baseapp + **改指立即推送 gateway**（不许等 10s 快照周期） | 新 epoch 已生效 | gateway 确认改指生效（下游路由指向新进程） | **不撤销写权**（③ 不可逆）。停留在「写权已交、路由未改」态——此态无害，因为①的缓冲覆盖了②③④全程，无上行在飞；重试到成功 |
| ⑤ 放行 | gateway 停缓冲、改向新 baseapp 放行（缓冲内积压一次排空）；老进程摘除该玩家 | gateway 确认改指生效 | 首个上行到达新进程且正常受理 | 同④：不撤销，玩家停留在缓冲态；重试上限（缺省不设）到达 = 升级人工介入 |

**两条不变量**（本节的正确性论证）：

1. **任意时刻唯一权威**：③ 之前老进程持写权、③ 之后新进程持写权，中间没有重叠——所以无双写；老进程对已切走玩家的迟到回执按旧 epoch 丢弃（既有裁决：token 与 epoch 双锚，旧会话/旧 epoch 拒绝，modules/game/session/src/player_directory.cpp:122-124）。
2. **无丢写**：① 起的上行缓冲覆盖②③④⑤全程，玩家上行在网关暂存、不落地丢失；⑤ 一次性排空到权威进程。所以③→④ 之间的路由滞后是**无害的**——正是这个缓冲让「先交写权、后改路由」这个顺序可以成立。

### 9.3 迁移粒度与停写窗口

- **粒度 = 单个玩家**（用户 2026-10-12 更正）：一个玩家的①~⑤全部完成后才开始下一个玩家。玩家之间**默认串行**（并发度 = 1）。
- **并发度写成可配参数，缺省 1**（拟 `--player-migrate-concurrency`——**待定**，命名挂接线批）。**只有实测出单玩家切换耗时之后才允许放开小并发。**
- **理由（用户令）**：粒度越细，每个玩家的停写窗口越短，才能真正做到无感。停写窗口 = ① 停写时刻到 ⑤ 放行时刻的时长；它只覆盖**这一个玩家**的快照拉取（单玩家档案量级），而不是一个批次或一个场景的快照量级。
- **停写窗口是毫秒级**（设计意图）：窗口期间玩家上行由 gateway 缓冲、下行由权威进程照推——玩家侧表现为一帧内无变化。**gateway 现在没有上行缓冲**（`pending_` 是下行应答关联表，apps/gateway-app/src/m1_egress.cpp:340，`request_id → session_id`），上行缓冲是本设计新增面（§9.9-2）。
- **待实测**：单玩家切换耗时是唯一的待定档数字——实测前不定档，缺省只给指标定义（§9.8），不给目标值。

### 9.4 更新顺序与依赖（gateway 保留不动前提下）

| 顺位 | 角色 | 依赖 | 为什么在这个位置 |
|---|---|---|---|
| 1 | manager | 无（先手） | 排他窗口是全编队串行闸；先换 manager，后续每个 baseapp 的重启都在新版 manager 主持下，目录改指/落点排除语义也跑在新版上 |
| 2 | baseapp | manager 在线（落点排除 + 目录改指） | 主案。新 baseapp（新二进制）先起空载 → 老 baseapp 停分配新玩家 → 逐个玩家五步迁移 → 排空后才换二进制。**排空完成前老进程不换 binary**（用户令） |
| 3 | cellapp | 排在 baseapp 之后（玩家锚点已在新 baseapp，Avatar 重挂时指向新 baseapp） | 兜底路（§9.1 cellapp 行）：代价最小，放在最后 |
| 4 | login | 无 | 无状态档，任意时点；建议与 baseapp 迁移并行前完成 |
| 5 | logger | 无 | 后台，任意时点 |
| — | machined | **排除** | `terminate_all` 带走全编队（supervisor.cpp:249-255） |
| — | gateway | **不动** | 客户端连接锚；排空 = 全体掉线 |

- **manager 的恢复相位排他窗口怎么用**：它就是滚动序的节拍器——任何时刻全编队至多一个进程处于恢复相位。manager 自身换版本时，排他窗口内全编队静止一个 RTO 周期（drill 实测 2.71s）；此后每个角色换二进制各占一个排他窗口，串行不重叠。
- **baseapp 与 cellapp 的先后**：baseapp 先（主案，0 掉线），cellapp 后（兜底路，掉线数计入 cellapp 段）。理由：cellapp 的换二进制会让其上的玩家重连，重连时 Avatar 要挂到**已经换过版本**的 baseapp 上——baseapp 先就位，cellapp 段的重连才有新版承接。
- **新进程先起、老进程后停**：任何时刻至少有一个 baseapp 在服务新玩家——这是「更新期间新玩家可登录」的结构性保证。
- **老进程换二进制发生在排空之后**：此时手上无会话，重启不触发玩家重连，重启代价 = 空进程拉起 + 目录重报（收敛双判据 2.71s 口径）。

### 9.5 数据面

**journal 何时 flush**

- **每个玩家切换时**：① 停写 → 老进程对该玩家先 drain 该玩家的 journal 段 → ② 才发快照（用户主案「旧的先 flush 再发」= §8 路 2 必做 1）。
- **老进程换二进制前**：排空完成后全量 drain（attribute-sync §10.2 停机五阶段的 drain 段——CAS 幂等 + 生产者先停 + drain 超时放行）。
- **新进程重启后**：RecoveryCoordinator restore（journal replay）——排空后已 drain，重放距离短，RTO 小于满载场景。
- **不变量**：任何时刻 journal 写只在权威进程（单写者纪律不破）——③ 之前写只落老进程、③ 之后只落新进程。

**锚点重放**

- 新 baseapp 起空载时就从 journal 重放一次锚点（此时只含已迁移玩家的锚点）；此后每迁进一个玩家，追加一次。锚点重放是既有恢复序列（restore → admission → ready）的一部分，不是新机制。
- 批 0 的 Anchor restore 段为空档演练（demo anchors 内存态、无 player 档案），带载重放耗时挂实测批——现有数字是下界。

**`anchor_epoch` 重启后保持连续（用户点 3 的核心，也是主案的前置缺口）**

- **现状**：`next_epoch_` 是内存自增（modules/game/session/src/player_directory.cpp:33），**无持久化、无恢复路径**——baseappmgr 重启后归零。
- **后果**：同账号第 N 次会话与第 N+1 次会话的 epoch 可能倒退（新会话 epoch 小于锚点已持久化的 epoch）。既有裁决（陈旧 epoch 拒收）会把合法新会话当旧会话丢掉——**同账号第 N 次会话会乱**。
- **本设计要求**：epoch 计数器随锚点一起持久化（journal 内），重启后从恢复值续起；迁移中的交写权 = 显式 +1。
- **状态**：**待接线**（§9.9-4）——这是主案能成立的前置条件，不是可选优化。

**玩家断线窗口与 Suspended 条目的处置**

- **主案路径上不产生 Suspended**：玩家全程不断线（停写窗口毫秒级 + gateway 缓冲 + 放行），目录条目状态不变。
- **兜底路径产生 Suspended**：cellapp 换二进制、老进程换二进制时的残留玩家、④⑤ 重试升级的人工介入——这些玩家断线，条目进 `mark_suspended` 保活窗口（modules/game/session/src/player_directory.cpp:67；`--suspend-window-ticks` 默认 30 = 30s，1s 拍，apps/baseappmgr/src/main.cpp:237）。窗口内恢复**不 kick**（restore-not-kick，drill intake=3）；窗口满由主循环 sweep 终结进 SessionDown（进镜像面）。
- **死亡行入口**（既有，不改）：zone 死亡 → `suspend_zone_sessions`；gateway 死亡 → `suspend_gateway_sessions`（apps/baseappmgr/src/main.cpp:425-443）。

### 9.6 复用已有机制（逐条点名出处）

| 机制 | 出处 | 本设计怎么用 |
|---|---|---|
| 恢复相位（拒新直到全量重报收敛，drill 实测 2.71s） | modules/game/session/fleet_recovery.hpp/.cpp（FleetRecoveryCoordinator：Normal↔Recovering 排他相位 + 全量重报分片 + 双判据收敛）；批 0 已交付（scripts/drill_kill9.sh） | 全编队串行闸；manager 排他窗口 = 滚动序节拍器 |
| mark_suspended 窗口 | modules/game/session/src/player_directory.cpp:67；apps/baseappmgr/src/main.cpp:237（`--suspend-window-ticks` 默认 30）；死亡行入口 :425-443 | 兜底路径的保位窗口——断线玩家不掉槽，窗口内恢复不 kick |
| machined 退避重启 | apps/machined/src/supervisor.cpp:213-226（线性退避 attempt N = N×backoff；drill 实测 attempt 1 = 1.04-1.10s）；超限放弃 :218-225 | 换二进制的执行半边；排空后老进程重启 = 空进程拉起 |
| 准入闸 | apps/baseappmgr（`--recovery-port` / `--recovery-expected` / `--recovery-timeout-ms`、`set_admission_gate`） | 「停止分配新玩家」态与恢复期拒新共用同一闸门 |
| 路 1 等断线重连（缺口 = 更新期间 AssignWorld 跳过该进程） | §8 路 1；WorldAssignment（modules/game/session/world_assignment.hpp） | cellapp 段与残留玩家的兜底；该缺口正是主案「停止分配新玩家」要接的线 |
| 路 2 新 baseapp 装好再切玩家 | §8 路 2：跨进程拉快照（旧先 flush 再发，保持 ADR-007 零外部存储）/ anchor_epoch 写权交接 / 目录改指 + 路由快照立即刷新 | 主案的机制本体；本设计把「切换」细化到单玩家五步 |
| anchor_epoch 双锚拒收 | modules/game/session/src/player_directory.cpp:122-124（token 与 epoch 双锚，旧会话/旧 epoch 拒绝） | 老进程对已切走玩家的迟到回执按旧 epoch 丢弃——本设计无双写的既有基础 |
| 目录镜像 delta 即时 + 全量快照周期兜底 | modules/game/session/directory_mirror.hpp/.cpp（"APD2" wire，delta/snapshot + seq 续传）；周期 `--mirror-snapshot-ms` 默认 10000（apps/baseappmgr/src/main.cpp:182） | 改指的兜底通道；本设计在其上加「改指立即推送」的要求 |
| RecoveryCoordinator restore→admission→ready | modules/game/session/recovery.hpp/.cpp | 换二进制后新进程的启动序；锚点重放在此序列内 |
| gateway 下行应答关联 | apps/gateway-app/src/m1_egress.cpp:340（`pending_` 关联表，`ack_timeout_ms` 5000） | **对照面，非复用面**：它是下行应答关联，不是上行缓冲——上行缓冲为新增面（§9.9-2） |

### 9.7 差异点（本设计自己的做法；别家一律未核实）

**本设计自己的做法（三条）**：

1. **客户端连接不断**：gateway 全程保留不动——它既是客户端 TCP 锚又是路由 hub。更新期不重建玩家与网关之间的会话，只重建网关到 baseapp 的下游指针。
2. **按角色滚动**：每个进程角色走自己的状态机（§9.1）——无状态角色（login/logger）直接换、有状态角色（baseapp/cellapp）先排空再换、不参与滚动的两个角色（machined/gateway）各自有理由（§9.4）。
3. **状态机 + 单玩家五步**：状态机的「排空」态在 baseapp 上展开为逐个玩家的 ①~⑤（§9.2）——迁移是玩家级的、串行的、每步有确认点、有两条不变量，不做边玩边拷。

**别家（BigWorld / KBE / 商业引擎）在「不停机发版」上的具体做法：未核实。**无本地源码或官方文档可引；本仓 docs 内 C-51/C-52 只覆盖 BigWorld 的备份与接管链路（进程死亡后的备机接权），未覆盖其发版流程。禁止推测。可核实的相关对照只有两条，且均非发版面：

- BigWorld 有 backup/reviver 进程与备份链（本地 14.4.1 实读，§1.1）——那是「进程死亡后备机接权」，不是「主动换版本」；本设计不走这条路（ADR-010 已裁决不设 reviver）。
- KBE 无 journal / 热备 / 接管（§1.2，C-51）——那是发版无关的容灾面。

### 9.8 验收指标（可测）

| 指标 | 目标 | 怎么测 |
|---|---|---|
| 更新期间新玩家可登录 | 全程可登录（失败数 0） | 滚动期间连续注入新登录请求，计数失败；结构性保证 = 新 baseapp 先起、老 baseapp 后停（§9.4）。login 自身换版本窗内另记（无状态档秒级重启窗，双跑交替可压到 0） |
| 在线玩家掉线数 | **0（迁移路径口径）** | 迁移期与换二进制期统计客户端 TCP 断开数。cellapp 段单独记「自动重连次数」——cellapp 的排空态不可达（§9.1），其上的玩家各一次自动重连 |
| 单玩家切换耗时 | **待实测**（缺省只给定义，不定档） | 每玩家记 ① 停写时刻到 ⑤ 放行时刻的差；跑一批取分布（p50 / p99）。**这个实测数字是放开并发度的前提（§9.3）** |
| 停写窗口时长 | 同上（= 单玩家切换耗时） | 同上；这是「无感」的直接度量 |
| 重启前后存档一致 | 数据不丢 | 迁移前后比对锚点内容；老进程 drain 位点与新进程恢复位点一致（journal 位点为标尺——clock-and-time §6：跨进程不互信，用位点不用墙钟） |
| 无双写 / 无丢写 | 任意时刻唯一权威 | 迁移期审计：任一玩家的写入只落一处（journal 写审计 + epoch 单调递增） |
| 收敛时间 | 附现有 drill 数据 | 全量重报收敛 **2.71s**；镜像收敛 **2.3-2.8s**（上限 = 快照周期量化的观测面口径）；死亡检测 **14-55ms**；退避重启 **1.04-1.10s**；超时判据 **5s**。来源：scripts/drill_kill9.sh，批 0 实测（§6）。路 1 更新窗整链（SIGTERM→镜像收敛）**2.44-3.81s**，排空闸生效 **0.66-0.95s**——来源：scripts/drill_update.sh 两跑实测（§8 验收演练） |

### 9.9 待接线清单（本设计引入的新增面）

主案的前置条件，按依赖序：

1. **玩家级快照拉取（跨进程）**——路 2 必做 1 的机制化：新 baseapp 向老 baseapp 拉单玩家快照（旧先 flush 再发）。wire 未立项。
2. **gateway 上行缓冲队列**——停写窗口期暂存玩家上行。gateway 现有 `pending_` 是下行应答关联表（m1_egress.cpp:340），不是上行缓冲。**没有它，①~⑤ 的「玩家无感」不成立。**
3. **改指立即推送 + gateway 动态路由选择面**——目录改指 delta 推到 gateway；gateway 现有路由选择面是静态配置（gateway_server.cpp:95 注释「M1 单 cell 端点：路由快照仅存档，选择面随多 cell 批回归」）——选择面接线未做，属多 cell 批。
4. **`anchor_epoch` 持久化**——`next_epoch_` 现内存自增无恢复（player_directory.cpp:33），主案的前置条件（§9.5）。
5. **停止分配新玩家（落点排除）**——§8 路 1 已记的缺口：更新期间 AssignWorld 跳过该进程。**已接线（2026-10-13）**：drain 闸 + fleetctl 探针 + drill_update.sh 演练实测（§8 验收演练）。
6. **迁移并发度参数**（拟 `--player-migrate-concurrency`，缺省 1；**待定**命名）——接线时定名。
7. **④⑤ 重试上限与升级路径**——缺省不设上限；停留缓冲态到多久算升级人工介入，未定。

**结论**：接线清单第 1-5 项是主案的前置——缺任一项，只能退回路 1（断线重连）发版。验收指标里「单玩家切换耗时」是唯一待定档的数字，需实测批给出。

---

*基线：apollo main @ f4d56ac6。BW 证据 = 官方 14.4.1 包本地实读（2026-10-09 本轮补读 backup_sender.hpp/backup_hash.hpp/backup_hash_chain.hpp/baseapp.hpp/reviver 族文件清点；既有行号沿用 architecture-review §16.2/§16.6 已核记录）；分级口径沿 36 号 §0（A 本地源码实读 / B 本仓库文档 / C 官方文档 / D 社区资料——§1.3 行业通型行全节 D 级存疑标注）。既有件消费面以 2026-10-09 工作副本实读为准（apps/machined、apps/baseappmgr、modules/game/session 七件、modules/data/orm/persist_journal、apps/base-app base_server、modules/base/timer_wheel）。拍板点六项全部悬置——本件不代拍、不设候选倾向、不标推荐；拍板前仅批 0 可先行。*
