# 备份容灾与宕机接管设计（backup-revive）

> 状态：**设计稿（前置设计，G-2 立项批，2026-10-09，拍板点全悬置）**。定位：进程级高可用面——权威进程死亡后「服务不倒」的候选拓扑、仲裁语义与接管时序；与 attribute-sync §8.2 的分界沿既有范围声明：journal 答「数据不丢」，本件答「进程不倒」。缺口登记：**G-2 备份/容灾与宕机接管**（architecture-review §16.4 遗留登记；net-abstraction §7「P3 前置设计」G-2 段为既有骨架口径，本件将其展开为可拍板的候选拓扑）。互引：net-abstraction（§7 G-2 段/§5.7 InterServerLink/§6 sdshmem 决策行）、attribute-sync（§8.2 范围声明/§10.2 停机序列/§11 验收表）、session-and-online-directory §6（恢复矩阵）、clock-and-time（§6 跨进程不互信/§7 双时间戳）、capacity-and-benchmark（§1 容量目标/§5 指标集）、crash-capture（取证互证面）、term-contract §1.3 + concept-glossary「热备与接管」词条、architecture-review（§16.2/§16.4/C-51/C-52/§30 简化三刀）、36 号（§0 证据分级/问 5/问 7）、battle-instance-offload §7（实例故障域独立拍板）。

---

## 执行摘要

1. **为什么 G-2**：单写者进程死亡 = 权威瞬间消失——journal 保证数据零丢失（attribute-sync §8.2，P1-4/P1-5 已交付），但「数据在、服务停」：进程死到恢复之间其承载的全部会话与场景停摆，易失态（L3）无处置语义。三家先例只有 BigWorld 有完整此层（C-51/C-52：两家真正的代差在**备份与接管链路**，不在行存储形态）；KBE 无此层的代价 = 宕机回档到最后归档窗口（300s 级），skynet 单节点无编队。
2. **已有什么（本设计只消费不重做）**：案 A（冷重启+持久态+重连）的骨架**已在仓里**——machined 监督面（roster+线性退避重启+超限放弃+死亡事件四型）、死亡事件跨进程上报（G-1 收尾批增量①）、恢复相位跨进程化（FleetRecoveryCoordinator：排他相位+全量重报+双判据收敛）、目录镜像（"APD2" wire 族）、PlayerDirectory（anchor_epoch 裁决键+Suspended 窗口+批量反查）、PersistJournal（append/drain/replay/compact）、RecoveryCoordinator（restore→admission→ready）、baseappmgr 准入闸与 `--recovery-*` 接线。G-2 的真正增量只在两处：**状态如何到达接管方**（镜像流/双写）与**谁来裁定接管**（仲裁面）。
3. **BW 先例的机制拆解（A 级实读，2026-10-09 本轮补读）**：备份发送面按 tick 分帧摊写（`backupRemainder_` 小数余数跨 tick 结转）+ 备份链双代哈希并存切换（`entityToAppHash_`/`newEntityToAppHash_` + `isUsingNewBackup_`）+ `BackupHashChain` 换代再分布（编队扩缩容不断备份）+ reviver 独立进程（经 machined 请求拉起、machined tags 限定可复活组件型）+ secondary db 补写。BW 把备份链做成了与扩缩容深度耦合的独立子系统——二十年演进的收益面与复杂度实锤同时在场。
4. **apollo 骨架口径（既有登记，本件展开为候选）**：热备流 = PersistJournal 的第二只读消费者（journal 三用一位：恢复位点/热备镜像/落库队列，不另起备份协议）；备机 ack 的 journal 位点即接管起点；接管 = 位点重放后于 tick 边界切权（attribute-sync §10.2 停机序列的镜像路径）；**不设独立 reviver 进程**——reviver 语义进 manager 域（architecture-review §30 语义合并三刀既有登记）。net-abstraction §7 既有骨架行「backup-hash 链 + reviver 两件先行、secondary db/动态扩缩容推迟」是否成立归拍板 1/2 确认或改写。
5. **反证必须诚实**：apollo 容量目标是单进程 5000 CCU、单机 8GB（capacity-and-benchmark §1/§4），P3 编队为小拓扑七类进程——BW 热备链解决的是无缝世界跨机大编队（坦克世界 19 万 CCU 口径，C/D）里的常态单点死亡，爆炸半径与 apollo 单 Zone 死亡不同量级；当代小/中部署主流实践（D 级，行业通型）= 进程管理器自动重启 + 持久化状态 + 客户端重连，热备的采用集中在无缝大世界与强连续性玩法。案 A 的 RTO = 秒级、RPO = 零（journal 零丢失已是 §11 指标）；热备的真实增量 = RTO 秒级→亚秒级 + 易失态保留，代价 = 备机常驻 + 镜像流带宽 + 双主仲裁复杂度。**单机编队下 standby 同机同死——拍板 1 的实质是部署形态拍板，不是纯技术选型。**
6. **候选拓扑三案**（§3）：A 冷重启+持久态+重连（全复用已交付件，增量≈0）；B 热备 standby+状态同步（BW backup-hash 同型，热备流 + 位点 ack + 接权仲裁）；C 无 standby 状态双写（journal 远端双写/重放重建——无常驻备机，RPO 由同步策略决定；active-active 副本与本仓单写者纪律冲突，如实记录为不采用形态）。三案共享同一地基：journal 位点锚 + epoch 仲裁 + 恢复相位排他 + machined 拉起——差异只在状态到达接管方的路径。
7. **拍板点六项**（§4，全悬置）：部署形态与 standby 必要性；接管粒度 per 进程类型；仲裁与 epoch 规则（与 PlayerDirectory anchor_epoch 的互动）；RPO/RTO 目标值；镜像流传输形态；先行试点与验收链。本件不代拍、不设候选倾向——裁决前不进任何实现批。
8. **分期**（§6）：批 0（不依赖拍板）= kill -9 演练验收链 + RTO 实测数字入库，即案 A 的完整形态；拍板 B/C 后位点面 → 镜像流 → 接权仲裁（或双写面）三批，全部依赖 net M1 落地。

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
- **接管面** `server/reviver/` 独立进程（与 baseappmgr/cellappmgr/dbappmgr 同级入 server/ 顶层清点；component_reviver/reviver/reviver_config/reviver_interface + main 五件）：`Reviver::revive` 经 machined 请求拉起死亡进程（reviver.cpp:439-446）；`queryMachinedSettings` 读 machined tags 限定本机可复活组件型（:272-296）；`handleTimeout` 按 `ReviverPriority` 分型轮询（:300 区段）。接管语义 = 主死后备机把镜像实体升级为权威；cell 死则 cellappmgr 在幸存 CellApp 重建 cell（B 级登记：architecture-review §16.4 G-2 行与 manager 域块）。
- **持久化次级面**：secondary db 任务族 + dbappmgr 扩缩容哈希再分布（dbappmgr.cpp:472/:637，B 级已核）。

### 1.2 KBE/skynet 负先例（B 级登记）

- **KBEngine 无此层**（C-51）：无 journal、无热备、无接管——宕机恢复 = Archiver 最后归档窗口（默认 300s、非脏驱动），玩家损失窗口 = 归档间隔、会话全断；进程清单九类（`kbe/src/server/` 清点）无 reviver 对应物（architecture-review :2689 权威行）。KBE 的教训是「无此层的代价」，**不是可抄的简化形态**——它简化掉的正是 G-2 本身。
- **skynet**：单节点服务编排框架，编队/接管零内建；cluster 是消息层不是容灾层（B 级登记）。

### 1.3 反证（anti-evidence，诚实记录）

- **容量口径**：单进程 5000 CCU、单机 8GB、稳态 RSS ≤4GB（capacity-and-benchmark §1/§4）；P3 编队 = 七类进程小拓扑（machined/manager/login/gateway/Zone/interfaces/verifier，多数每类一实例）。单 Zone 死亡的爆炸半径 = 该 Zone 的场景与会话，machined 退避重启 + journal replay + 目录全量重报的恢复链以秒计（目录重建 <1s @ 5000 条，session-and-online-directory §6）。BW 热备链为无缝世界跨机大编队里 baseapp 双位数实例、单台死亡为常态事件的形态而生（19 万 CCU 口径 C/D）——形态前提不同量级。
- **行业通型（D 级，存疑标注）**：当代小/中部署主流 = 进程管理器自动重启 + 持久化状态 + 客户端重连（容器编排 restart-on-fail 惯例同型）；实例制/分线制游戏服务端普遍接受「重启+重登」（EQEmu/TrinityCore 生态口径，D）。热备在 MMO 的采用集中于无缝大世界与强连续性玩法。apollo 目标玩法是副本/分线型 MMORPG（36 号 #9）。
- **BW 自身的复杂度实锤（A 级）**：两代哈希并存切换 + BackupHashChain 换代 + dbappmgr 哈希再分布 = 备份链与扩缩容深度耦合的工程代价。apollo 若走案 B，扩缩容面可沿既有登记推迟（secondary db/动态扩缩容后置），但「镜像流 + 位点 ack + 仲裁」三件是案 B 的不可再减核心。
- **单机形态下 standby 同机同死**：热备只在跨机编队有增量价值；拍板 1（部署形态）逻辑上先于拍板 5（传输形态）。

## 2. 既有件消费面（本设计不重设计，逐件对位）

| 既有件 | 位置 | G-2 消费方式 |
|---|---|---|
| machined Supervisor | apps/machined/src/supervisor.hpp/.cpp | roster 花名册（`name | component_id | zone_id | cmd args...`）+ fork/exec 拉起 + 线性退避重启（超限放弃）+ 死亡四型事件——案 A/C 的执行半边已交付；案 B 若立项 = roster 增 standby 角色列，监督面本体不动 |
| 死亡事件跨进程上报 | modules/net/discovery（G-1 收尾批增量①，op 6/7） | DeathSubscribe/DeathNotify——接管触发的**唯一事件源**；G-2 不新设心跳/死亡检测面 |
| FleetRecoveryCoordinator | modules/game/session/fleet_recovery.hpp/.cpp | Normal↔Recovering 排他相位 + FullReport 分片重组 + 双判据收敛（全报或超时开放）——manager 死恢复相位已交付；Zone 死接管窗口若立项 = 同型相位复用 |
| DirectoryPublisher/Mirror | modules/game/session/directory_mirror.hpp/.cpp | "APD2" delta/snapshot wire + seq 续传——接管后目录面随 manager 重建自动收敛，G-2 零改动 |
| PlayerDirectory | modules/game/session/player_directory.hpp/.cpp | anchor_epoch（幂等键+竞争裁决键）+ Suspended 窗口 + `mark_suspended_by_zone/gateway` 批量反查——接管后旧主迟到消息按 epoch 失效拒绝的裁决基础 |
| PersistJournal | modules/data/orm/persist_journal.hpp/.cpp | append/drain/replay/compact + `next_sequence()`——热备镜像流的源（第二只读消费者，单写者纪律不破）与接管重放位点（journal 序号即位点标尺） |
| SaveQueue/autoSaveLoop | apps/base-app（base_server） | write-behind 消费面 + 停机五阶段（attribute-sync §10.2 已交付，CAS 幂等 + 生产者先停 + drain 超时放行）——切权序列 = 停机序列的镜像路径，零件级复用 |
| RecoveryCoordinator | modules/game/session/recovery.hpp/.cpp | restore→admission→ready 启动序列 + 三档恢复清单（lifecycle §3 矩阵代码形态）——重启方/接管方统一启动序 |
| baseappmgr 准入闸 | apps/baseappmgr（`--recovery-port/--recovery-expected/--recovery-timeout-ms`、`--suspend-window-ticks`、`set_admission_gate`） | 恢复相位排他接线已交付——接管期拒新复用同一闸门；收敛开放即发重建快照衔接镜像面 |
| TimerWheel | modules/base/timer_wheel | 喂钟驱动/单写者——接管超时、镜像滞后告警、仲裁 deadline 的计时件（deadline = tick 号，clock-and-time §8） |
| InterServerLink | net-abstraction §5.7（设计态） | authority_epoch 握手三元组 = 对端重启判据（≠ 断线）；断连 in-flight 按 invoke_mode 分——G-2 镜像流已列名为该设施既有消费方，不开第二 API 面 |

## 3. 候选拓扑

### 3.0 共同地基（三案共享）

接管锚 = journal 位点**不是时间点**（clock-and-time §6：跨进程时钟不互信——双主判定与接管时序禁止 wall 差值，判定序走 epoch/位点/manager 串行）；切权 = tick 边界（复用 §10.2 停机序列镜像路径：先停旧主写入认定，再切权）；事件源 = machined 死亡事件（唯一）；拉起 = machined；恢复期排他 = 恢复相位（已交付）。**三案差异只在「状态如何到达接管方」一件事上。**

### 3.1 案 A：冷重启 + 持久态 + 重连（restart-only）

- **形态**：machined 检测死亡 → 退避重启 → RecoveryCoordinator restore（journal replay）→ admission → ready → Zone/gateway 全量重报收敛 → 客户端 resume/重登。恢复相位期间拒新。
- **依赖**：已交付件全复用；零新增进程、零新增协议面。
- **成本**：≈0（只剩批 0 演练验收链与 RTO 观测指标）。
- **覆盖面**：持久态零丢失（journal）；目录重建 <1s @ 5000 条；易失态（L3：位置/临时 buff）按既有语义归零——玩家侧表现 = 掉线重连，位置回最后落盘态。
- **失败语义**：RTO = 退避（1s 级）+ restore + 重报 ≈ 秒~十秒级（量级口径，批 0 实测校准）；恢复期该进程承载面停摆（排他相位防拓扑再变）；重启超限（5 次）放弃 = 人工介入面。

### 3.2 案 B：热备 standby 进程 + 状态同步（BW backup-hash 同型）

- **形态**：standby 常驻消费热备流（PersistJournal 第二只读消费者，RO_MIRROR 形态）→ 周期 ack 备份位点；manager 检测主死（死亡事件）→ 仲裁 standby 接权 → standby 自位点补重放尾差 → tick 边界切权 → epoch 推进 → SessionUp 重报进目录。
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

## 4. 拍板点（全部悬置——不代拍、不设候选倾向、不标推荐）

以下六项为决策面枚举；裁决权在用户，拍板前不进任何实现批（批 0 验收链除外——其不预设任何拓扑结论）。

1. **部署形态与 standby 必要性**：5000 CCU 目标下编队是单机还是多机；多机形态下 standby 是否立项——案 A / 案 B / 案 C 三选一，或分期组合（如 A 先行、B/C 视生产运行数据另行立项）。本拍板同时确认或改写 net-abstraction §7「backup-hash 链 + reviver 两件先行」与 architecture-review §30「不设独立 reviver 进程」两处既有登记。
2. **接管粒度 per 进程类型**：manager（已定：全量重报重建，session-and-online-directory §6）/ gateway（已定：目录窗口处置）/ Zone（本拍板：接管 or 冷重启——Zone 是当前唯一持 journal 权威态的进程类型）/ login-app / verifier（无状态 worker 池，battle-verification-service §5 既有口径）/ Battle 实例（battle-instance-offload §7 独立拍板）。是否统一一条接管语义，还是按进程类型分档（有状态接管 / 无状态重启 / 有状态但崩溃即作废三档）。
3. **仲裁与 epoch 规则**：接权后 anchor_epoch 由谁推进（standby 接权自延 vs manager 重挂）——PlayerDirectory `next_epoch_` 现为 manager 进程内单调，接管语义必须与其互动方式显式定义；双主检测判据（authority_epoch 失配——net-abstraction §5.7 既有握手三元组 vs manager 串行仲裁独任，或两者叠加）；恢复相位与接管窗口的排他次序（接管进行中 manager 又死亡的复合故障处置）。
4. **RPO/RTO 目标值**：崩溃回档 ≤2s journal 零丢失（attribute-sync §11 既有指标）是否升格为接管面 RPO 目标；易失态（L3）是否纳入接管面（不纳入 = 重连后位置回落盘态；纳入 = 镜像范围与带宽重估）；RTO 验收数字定档（秒级 vs 亚秒级）——数字决定案 A 是否足够。
5. **镜像流传输形态**（若案 B/C 立项）：同机 shm SPSC 环（net-abstraction §6 sdshmem 决策行既有候选——主机单写 journal 追加、备机单读消费）vs 跨机 InterServerLink；ack 通道归 control 面是否维持（既有登记）；镜像流的消息分类声明（net-abstraction §5.7：服务器间不 trim、丢弃资格归消息分类——镜像流可声明丢旧帧、位点兜底，是否照此）。
6. **先行试点与验收链**（若拍板 B/C）：先行试点进程类型选谁（候选：Zone——当前唯一持 journal 状态的权威进程；或 gateway——无权威态、面最薄）；kill -9 演练链（死亡→仲裁→切权→目录重报→客户端 resume）作为验收对象；双主注入演练（网络分区模拟）是否进门禁。

## 5. 术语提案（新词先入 glossary 再入契约——本件不改两档）

term-contract §1.3 已有「热备与接管 backup / reviver（G-2）」「恢复相位 recovery phase」两行命名锚；本设计若立项，以下**细化名词**按契约 §0 规则 4 走流程（先 concept-glossary 辨析、再契约钉死，随立项批入库）：

| 中文名（拟） | 英文名 | 代码标识（拟） | 定义一句 | 对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 热备进程 | standby | `Standby`（角色枚举 / roster 列名） | 常驻消费热备流、待命接权的进程角色 | BW = backup baseapp；KBE 无 | 禁：Slave/备胎/影子（他家词与口语） |
| 接管 | takeover | `Takeover`（事件/消息名根） | 接权方在备份位点重放后于 tick 边界升为权威写者的动作 | BW = backup 面升权（reviver 只管拉起不管升权） | 禁：failover（他家术语不入户）；与「重启」分清——重启 = machined 拉起，不含状态迁移 |
| 备份位点 | backup position | `backup_position`（u64 = journal sequence） | 热备流上接权方已确认追平的 journal 序号——接管重放起点 | ViewerState.acked_seq 同模型（属性 seq 水位的进程间实例） | 禁：「位点」裸用作代码标识（叙述层沿用） |
| 热备流 | backup stream | `BackupMirrorStream` | PersistJournal 的第二只读消费流（RO_MIRROR 形态，写只在 owner） | BW backup_sender 分帧流；net-abstraction §7「G-2 镜像流」既有口语的定名 | 禁：backup_sender（他家名）；「镜像流」裸用作代码标识 |

- **外部度量词汇注记**：RPO（恢复点目标）/RTO（恢复时间目标）为行业通型度量缩写——文档叙述面使用，不入代码标识（term-contract §0 规则 2 管**机制名**入户，度量口径不属机制名，此为边界注记）。双主（split-brain / dual-primary）作失败模式描述词，同口径文档面使用。

## 6. 分期（拍板后路径）

- **拍板前**：零代码——本件为纯设计件（源码冻结面照旧）；批 0 不依赖拍板结果。
- **批 0（不依赖拍板）——验收链演练**：dev_fleet 编队 kill -9 全链（死亡上 wire → machined 退避重启 → journal replay → RecoveryCoordinator ready → 全量重报收敛 → 客户端 resume）+ RTO 实测数字进 capacity-and-benchmark §5 指标集（「崩溃回档 ≤2s」行旁补「进程恢复全链耗时」分项）。案 A 的完整形态即批 0 终态——无论后续拍板如何，此链都是案 B/C 的降级兜底与验收基座。
- **批 1（拍板 B/C 后）——位点面**：journal `next_sequence` 跨进程可见（ack 位点消息进 internal 域契约，id 900+ 分段照旧，契约随实现批入库）；镜像滞后位点差指标进 MetricRegistry；RPO 度量口径入 capacity §5 指标集。
- **批 2（拍板 B 后）——镜像流**：传输形态随拍板 5（同机 shm SPSC / InterServerLink 跨机）；`BackupMirrorStream` 消费面接 journal（第二只读消费者，单写者纪律验证入测试）；standby 角色生命周期（roster standby 列 + 监督面零改动验证）；standby 迟到/重启的全量重放重建路径。
- **批 3（拍板 B 后）——接权仲裁**：双主检测（epoch 失配判据接线，判据随拍板 3）；切权序列（§10.2 停机序列镜像：先停旧主写入认定再切权，CAS 幂等复用）；manager 恢复相位联动（接管窗口排他）；双主注入演练验收。
- **批 4（拍板 C 后）——双写面**：远端持久面接线（形态随拍板 5 部署面）；位点对账（本地/远端 journal 序号一致性校验 + 失配处置随拍板 3）；重放距离指标。
- **依赖序**：批 1-4 全部依赖 net M1（InterServerLink 落地）与拍板 1/3/5 结果；批 0 无依赖先行。

## 7. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| net-abstraction | §7 G-2 骨架段 = 本件上位口径（热备流 = journal 第二消费者/位点即接管起点/tick 边界切权——本件展开为候选拓扑待拍板）；§5.7 InterServerLink（镜像流承载 + authority_epoch 重启判据 + 消息分类声明）；§6 sdshmem 决策行（同机镜像流既有候选） |
| attribute-sync | §8.2 范围声明（「数据不丢」与「进程不倒」分界——本件即「进程不倒」面）；§10.2 停机五阶段（切权序列的镜像路径零件，CAS 幂等/生产者先停复用）；§11 崩溃回档指标（RPO 目标的既有锚） |
| session-and-online-directory | §6 恢复矩阵三行（manager/gateway 已定、Zone 行接管语义归拍板 2/3）；anchor_epoch = 接权后裁决键（PlayerDirectory 既有实现） |
| clock-and-time | §6 跨进程时钟不互信（接管锚 = 位点非时间点；双主判定禁 wall 差值）；§7 journal 双时间戳（重放定位 (tick, wall_ms) 双写照旧） |
| capacity-and-benchmark | §1 容量目标（5000 CCU 单机 8GB——拍板 1 的输入）；§5 指标集（RTO 分项与 RPO 度量的消费面）；§4 内存预算（案 B standby 常驻的量级重估） |
| crash-capture | dump + 符号栈 = 事后证据，machined 死亡行（exit 128+signo）互证——G-2 只消费死亡事件，不重复取证面 |
| battle-instance-offload | §7 故障域（实例崩溃即作废 vs 关键帧重放——拍板 2 的 Battle 行与该拍板互不吞并） |
| battle-determinism | §5 录制格式（若案 C 走 intent 重放线的格式来源——与 journal 重放分属两线：journal 重放持久变更，intent 重放推演判定序，不混用） |
| battle-verification-service | §5 部署口径（无状态 worker 池无 G-2 热备需求——拍板 2 verifier 行既有依据） |
| architecture-review | §16.4 G-2 登记（本件即该缺口立项设计面）；C-51/C-52（KBE 无此层/代差在备份链路的证据）；§30 简化三刀「backup+reviver → journal 位点 + manager 恢复相位」（既有登记，拍板 1/2 确认或改写） |
| term-contract / concept-glossary | §1.3 热备与接管、恢复相位两行命名锚；§5 术语提案的入库前置（先 glossary 后契约） |
| scripts/dev_fleet.sh | 批 0 演练链的编队驱动（up/roster/status/down 既有面；crashdumps/ 与 fleet 日志目录共存惯例沿 crash-capture §2.2） |

---

*基线：apollo main @ f4d56ac6。BW 证据 = 官方 14.4.1 包本地实读（2026-10-09 本轮补读 backup_sender.hpp/backup_hash.hpp/backup_hash_chain.hpp/baseapp.hpp/reviver 族文件清点；既有行号沿用 architecture-review §16.2/§16.6 已核记录）；分级口径沿 36 号 §0（A 本地源码实读 / B 本仓库文档 / C 官方文档 / D 社区资料——§1.3 行业通型行全节 D 级存疑标注）。既有件消费面以 2026-10-09 工作副本实读为准（apps/machined、apps/baseappmgr、modules/game/session 七件、modules/data/orm/persist_journal、apps/base-app base_server、modules/base/timer_wheel）。拍板点六项全部悬置——本件不代拍、不设候选倾向、不标推荐；拍板前仅批 0 可先行。*
