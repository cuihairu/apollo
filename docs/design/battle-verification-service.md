# Apollo 战斗验证服务（battle-verification-service）

> 状态：设计稿（评审中；实现 M0-M5 全部随代码批——源码冻结纪律）。定位：**客户端权威战斗的反作弊对账与权威结算件**——独立服务注册进 G-1，RPC 交互：上行输入流/战报（battle-determinism §5 四元组 + 客户端 hash 申报），下行验证与结算结果（verdict + 权威结算载荷）。语言面 = **Lua 双端共享同一份战斗逻辑**（客户端 xLua / 服务端 scripting-lua）——与业界 JS 双端（Node 验证服）/ C# 双端（Unity + .NET）同型，concept-glossary「战斗验证服务」词条的展开件。与 `battle-determinism.md`（§2 四约束全量继承、§5 录制/链复用）、`scripting-lua.md`（§6.1 三级事件形态移植、§2/§4 沙盒纪律）、`attribute-sync.md`（§8.2 :285 结算前 snapshot、§9 预测边界）、`net-abstraction.md`（§7 G-1、§5.7 InterServerLink、§4.3 violation_score）、`sdk-contract.md`（§11 internal 域）互为引用。缺口登记：design-gap-inventory #17（立项即设计落盘）。

---

## 执行摘要

1. **存在前提写死**：本服务只为**客户端权威战斗**（36 号 #18 缝兑现的小房间 lockstep / 客户端演算战斗）而存在。apollo 主路线（intent-only、服务端权威）**不需要本服务**——BW/KBE/skynet 无内建同因，正因三家均为状态同步 + 服务端权威（§0）。
2. **形态**：独立进程 `verifier`（编队组件，G-1 machined+UDP 双层注册，Type 段新增 VERIFIER），InterServerLink + internal 域（id 900+）三消息族：Submit（上行四元组+hash 申报，ReliableEvent）/ Result（下行 verdict+结算载荷，OneWay）/ Query（仲裁控制面，RequestReply）。
3. **计算面 = Lua 双端影子复算**：同一份战斗逻辑 bundle（`combat_bundle_hash` 锚定）客户端 xLua 跑、服务端 verifier 沙盒重跑（一局一 `lua_State`）；确定性子集 = battle-determinism §2 四约束**全量继承** + **跨端增量三件**（运算白名单 / bundle+VM 版本锚 / 残余漂移降级 WARN——浮点位一致是工程目标不是硬承诺，三层保障防跨端微漂误伤真实玩家）。
4. **验证强度两档**：一档 hash 链终局对照（cheap 常开——客户端本地按 §5 同式算链、只上行 checkpoint+终局摘要，verifier 复算对终局）；二档逐 tick 复算回放（贵——争议仲裁/分歧定位/采样复核，复用 §5 录制策略：排位常开、普通采样）。
5. **结算权威 = 复算产出**：verdict 之外，**权威结算载荷（胜负/掉落/结算属性终态）由重模拟终局导出**——客户端申报数值仅作对照不作数据源（防谎报伤害/掉落；drop 子流重算即权威掉落）。
6. **判定出口四值 × 结算门两级**：PASS / DRIFT（数值微漂→WARN 观测不计罚）/ MISMATCH（结构性分歧→FAULT+结算挂起+violation_score）/ INCONCLUSIVE（无法复算→按责任方）；硬门（排位/带奖惩）挂起到结论，软门（普通玩法）超时放行+事后审计。三级事件形态 = scripting-lua §6.1 同型移植（独立服务版）。
7. **部署**：worker 池独立于副本（instance）服务——异步验证不阻塞战斗（战斗结束才提交，结算门只延迟发放不延迟战斗）；无状态可重投（worker 宕机 = machined 拉起+队列重投，无 G-2 热备需求）；Compact 单机形态内嵌 verifier-kernel 库（M1 产物直接消费）。
8. **命名纪律**：battle 词专属战斗逻辑域（验证/结算——concept-glossary §0 裁决 1-②）；空间/实例命名一律 instance 族，永不回用。

## 0. 适用与不适用（写死）

| 判定 | 场景 | 理由与去处 |
|---|---|---|
| **适用** | 客户端权威战斗玩法：lockstep 小房间（1v1 竞技场类，36 号 #18 缝）、客户端演算战斗（休闲/卡牌自动战斗类） | 判定在客户端 → 服务端无权威数值可校，只能**同逻辑复算对账**——本服务唯一存在理由 |
| **不适用** | 服务端权威战斗（apollo 主路线，intent-only） | 服务端自己就是判定者，复算自己无反作弊收益——attribute-sync §9 预测/纠正已覆盖表现层漂移；BW/KBE/skynet 无内建验证服务同因（三家皆服务端权威，deep-dive §18） |
| **不适用** | 观战 / 精彩回放 | battle-determinism §5 事件流重演（不重模拟，cheap），不走本服务 |
| **不适用** | 物理层作弊（加速齿轮/改内存/协议篡改） | net-abstraction §5.9 威胁模型边界（「加密防网络第三方，不防持客户端的玩家」）；输入合理性归 intent 校验域 + §4.3 限流，客户端完整性检测是反外挂客户端件非服务端域。本件只管**逻辑层对账**：客户端算出的战斗结果是否与同输入的忠实复算一致 |

**裁决句**：一个玩法要不要接本服务，判据只有一个——**战斗判定在哪端**。判定在服务端（哪怕脚本跑 Lua），不需要；判定在客户端，需要。

## 1. 形态与拓扑

### 1.1 三方数据流

```text
客户端（xLua 战斗 VM，判定者）
  │  四通道 intent（movement/events…）＋ 终局申报（checkpoint hash 集 + 终局 hash + 结束事件）
  ▼
副本实例（instance 进程）＝ 输入汇聚点 + 录制器 + 结算门持有者
  │  战斗终局/中断点提交：BattleVerifySubmit（四元组 + 申报摘要）   ── ReliableEvent
  ▼
verifier（worker 池）＝ 复算 + 权威结算导出
  │  BattleVerifyResult（verdict + 权威结算载荷 + 对照报告）        ── OneWay
  ▼
instance 结算门 → 结算落库/排行/奖励（attribute/journal）
```

- **录制源 = instance**：它本来就是该玩法全部参战者 intent 的汇聚点（Gate 透传后解帧所在），输入序列按 (session, seq, tick) 记录、与属性 seq 同源（battle-determinism §5 输入序列行）——**不经客户端转手**，客户端谎报输入无通道。
- **申报源 = 客户端**：hash 链在客户端本地算（每 tick 末按 entity_id 规范序 `h_t = H(h_{t-1} ‖ 事件集_t)`，§5 同式），上行只有摘要（checkpoint 每 N tick 一个 + 终局一个）——量级 KB，不逐 tick 上链。
- **复算源 = verifier**：从起始快照位点重模拟到终局，重算自己的链与结算态。

### 1.2 注册与消息（G-1 + internal 域）

- **注册**：verifier = G-1 编队组件（net-abstraction §7），ServerID Type 段新增 VERIFIER；machined 拉起/重启，编队配置声 worker 数。实例发现 = G-1 双层既有语义，零新增发现机制。
- **消息族**（sdk-contract §11 internal 域，id 900+ 段，XSD 锁段边界）：
  - `BattleVerifySubmit`：instance → verifier。四元组（`combat_bundle_hash`/world_seed/起始快照位点/输入序列句柄）+ 申报摘要（checkpoint hash 数组 + 终局 hash）+ 玩法档位（硬门/软门）+ 校验模式（一档/二档）。战报体量大走录制存储句柄（journal/录制文件引用）不内联。
  - `BattleVerifyResult`：verifier → instance。verdict 四值 + 权威结算载荷（胜负/掉落/结算属性终态导出）+ 对照报告（分歧 tick 位点、DRIFT 统计）。instance 结算门**幂等消费**（按战报 id 去重）。
  - `BattleVerifyQuery`：GM/仲裁面 → verifier。RequestReply 低频控制面（重算指定战报、取对照报告、采样参数查询——remote-entity-call invoke_mode 三分口径：OneWay 默认、RequestReply 只限控制面）。
- **客户端侧契约零新增域**：终局申报走 client 域既有 events 通道一条消息（字段随契约批，client 域字段号兼容纪律照旧）——本服务的 internal 域不与客户端握手面相交。

## 2. 计算面：Lua 双端影子复算

### 2.1 同一份 bundle，两端跑

- **战斗逻辑 = 一份 Lua bundle**（战斗规则脚本 + 查表资产 + 规则参数），`combat_bundle_hash`（内容 hash，schema_hash 族算法）为唯一版本锚：客户端随资源管线下发并在开局握手申报；服务端 verifier 从 scripting-lua §3.2 版本指针库装载。**双端 hash 失配 = 拒开局**（在源头锚版本，不是事后拒验证——四元组内版本可复算的前提）。
- **服务端侧 = 一局一沙盒**：每 verification 一个独立 `lua_State`（scripting-lua §2 单写者纪律在 verifier 侧的形态：worker 线程内串行跑一局，无共享状态）；指令预算、沙盒禁令、`apollo.*` 注入全按 scripting-lua §4/§6 既有纪律——verifier 不新起脚本设施。
- **双端 VM 版本线一致**：服务端 Lua 5.5 主线随 vcpkg（scripting-lua §2）；bundle manifest 声明目标 VM 版本，xLua 侧失配拒载。xLua 的 JIT 编译路径不预设禁令——漂移风险由 §2.3 第三层保障兜底，经 M4 采样数据再裁。

### 2.2 确定性子集：§2 四约束全量继承 + 跨端增量

battle-determinism §2 四约束（判定域唯一/浮点纪律/迭代序/随机数子流）在**服务端权威域**的原口径是「同 binary 同平台 = 同确定域」；本服务把判定放到客户端，确定域**跨端**，故继承之外加三件增量：

| 增量 | 口径 | 消除的变量 |
|---|---|---|
| ① 运算白名单 | 验证域内 double 只允许 `+ − × ÷`、比较与位确定 math 白名单（floor/ceil/abs/min/max——IEEE754 位确定）；禁 math 三角/超越（§2 已定——角度/距离走 C++ 定点角度+查表注入 `apollo.battle`，**查表数据随 bundle 同源下发，双端同一份表数据同一索引**）；禁 string↔number 往返、`string.format` 不进判定 | libc/编译器实现差异 |
| ② 版本锚 | `combat_bundle_hash` + VM 版本线双锚（§2.1）；结算段按 target entity_id 全序、判定路径禁 unordered 迭代序/`pairs`、PCG32 `(world_seed, tick, stream_id)` 子流——照 §3/§4 原文 | 代码与数据不同版 |
| ③ 漂移降级 | 跨端浮点位一致是**工程目标非硬承诺**：残余微漂（不同 CPU/VM 实现的末位差异）由 §3 verdict 分级降为 DRIFT→WARN 观测，漂移率进容量批指标集——**不作故障、不计 violation_score** | 硬承诺误伤真实玩家 |

### 2.3 结算权威 = 复算产出

- **权威结算载荷由 verifier 重模拟终局导出**：胜负、掉落（drop 子流重算——客户端申报掉落无数据源地位）、结算属性终态（HP/积分等）。客户端申报值只在对照报告里出现。
- 这与「对账平了就信客户端」不同：**服务端复算结果才是结算数据源**，验证是复算的副产品（verdict = 复算态与申报态的对照结论）。逻辑作弊（改数值/改结果）在此无逃生通道——除非改到与忠实复算一致（那就等于没改）。

## 3. 验证强度两档

| 档 | 做什么 | 成本 | 触发 |
|---|---|---|---|
| **一档：终局对照**（cheap，常开） | verifier 重模拟全程，重算链取终局 hash 与申报终局对照——一致即 PASS（不逐 tick 比对） | 每场战斗一次重模拟（时长 ≈ 战斗时长的模拟开销） | 全量提交的战报 |
| **二档：逐 tick 回放**（贵，按需） | checkpoint 逐段比对定位分歧 tick；接 battle-determinism P3 离线重放器（同工具族）做人工仲裁取证 | 一档的数倍 + 工具链 | MISMATCH 定位、争议仲裁、采样复核、DRIFT 率异常排查 |

- **checkpoint**：客户端每 N tick 上一个链值摘要（N 随契约批定，量级 100）；二档定位 = checkpoint 二分 + 局部重模拟，收敛到单 tick。
- **录制策略继承 §5 原文**：排位/带奖惩战斗输入录制**常开**；普通战斗采样开（采样率随容量批 §5 定）；movement intent 全录高压缩（<100B/条 net-abstraction 预算口径）。
- 战报存储 = journal/录制文件既有载体（attribute-sync §8.2 位点），Submit 只带句柄——战报不双份存。

## 4. 判定出口与结算门

### 4.1 verdict 四值 → 三级事件（scripting-lua §6.1 同型移植）

| verdict | 判据 | 事件级（§6.1 形态） | 处置 |
|---|---|---|---|
| PASS | 终局 hash 一致 | — | 结算放行，权威载荷落结算 |
| DRIFT | 分歧晚且事件序列**结构一致**（数值微漂——§2.3 增量③的残余） | WARN（计数观测、去抖，不处置） | 结算放行（以**复算值**为准）；漂移率进指标集 |
| MISMATCH | 事件集结构不同/分歧早/输入对不上忠实复算 | FAULT（结算挂起） | 硬门挂起进仲裁；**violation_score 计数**（net-abstraction §4.3 单桶，账号级）；对照报告留证 |
| INCONCLUSIVE | 录制缺件/版本失配/输入流不完整——**无法复算** | WARN（工程事故面，非玩家面） | 按责任方：服务侧缺件 = 事故通道不计玩家；客户端拒交/残交申报 = 按 MISMATCH 从严（隐匿战报视同结构性分歧） |

- 三级事件在 verifier 侧的落法 = §6.1 **同型移植**：冷路径单点（verifier 版 `onVerifyFault`——ERROR 旁路日志 + MetricRegistry 计数器，禁故障路径同步 IO/跨进程调用）、TRIP 级 = 连续 MISMATCH 同 bundle 源时该玩法验证面**熔断降级**（转软门+告警，参数随容量批）——§6.1 的模块熔断换算到「战报源」粒度。
- verifier 自身脚本故障（预算超限/沙盒违例）走 scripting-lua §6.1 原生三级处置（FAULT 中止该局复算 → INCONCLUSIVE 返还）。

### 4.2 结算门两级（instance 持有）

| 门 | 玩法 | 行为 |
|---|---|---|
| **硬门** | 排位/带奖惩 | 结算挂起直到 verdict ∈ {PASS, DRIFT} 或人工仲裁结论——**宁延迟发放不放行未验证结算** |
| **软门** | 普通玩法 | 挂起有超时（参数随容量批），超时放行 + 事后审计标记；后判 MISMATCH 走回收/风控追责（执行位 = GM 面 scripting-lua §7.5 + violation_score 账号级准入） |

### 4.3 与属性面/风控面的边界

- **HP snapshot 边界**（attribute-sync §8.2 :285 原文：「关键 L3（如战斗结算前 HP）由业务显式 snapshot」）：结算门挂起时以该显式 snapshot 冻结为展示态；复算终态为准后落结算。§9（:319）「表现与状态短期不一致是允许的」的容忍边界在此收口——**状态以服务端复算为准**。
- **violation_score 接法**（net-abstraction §4.3 单桶）：限流计数 + schema 校验失败 + 权威纠正计数 + **本件 MISMATCH 计数**同一桶——逻辑作弊与灌包/语义非法在风控面同权；DRIFT 不计（§2.3 增量③，防误伤）；断开/准入处置仍走 §4.3 三级与 login-app 闸门，本件只供数不执法。
- **仲裁**：MISMATCH 战报进仲裁队列（GM 面 §7.5 消费 + 二档回放取证）；仲裁结论回写结算门。

## 5. 部署与容量

- **独立 worker 池**：apps/verifier-app（M2 起），与副本（instance）服务分进程部署——故障域隔离双向成立（副本死不影响在途验证、验证滞后不阻塞战斗）；扩缩容按队列水位（G-5 指标同源）。
- **异步不阻塞战斗**：验证全链在战斗实时路径之外——战斗结束才提交，结算门只延迟**结算发放**不延迟战斗本身。
- **无状态可重投**：战报在录制存储/journal，verifier 内存只有进行中的复算——worker 宕机 = machined 拉起 + 队列重投，**无 G-2 热备需求**（与 Zone 热备的差别写明：无自有权威状态）。
- **Compact 内嵌形态**：docs/30 单机形态不独立进程——内嵌 M1 verifier-kernel 库同进程同步复算（单机玩法体量下重模拟成本可吃），消息族退化为进程内接口，契约不双份。
- **容量口径**：复算吞吐 = worker 数 ×（单 worker 每秒可模拟 tick 数）；预算形态并入 capacity-and-benchmark §5（录制回放形态同源），降级序 = 队列积压 → 自动降采样率 → 普通玩法转软门；**硬门不降**。

## 6. 命名纪律与边界

- **battle 词专属此域**（concept-glossary §0 裁决 1-②）：`battle-verification`/`BattleVerify*` 消息/verdict 术语只用于战斗验证与结算域；空间/实例命名一律 instance 族（副本/room/dungeon 别名照 glossary）——历史错名「Battle」已废，永不回用。
- **行业同型对照**（glossary 词条口径）：JS 双端（客户端 JS + Node 验证服）/ C# 双端（Unity + .NET，可热更）为社区通型；apollo 形态 = **Lua 双端**（xLua + scripting-lua）——跨端差异点不在语言而在**版本锚**（C# 双端同需程序集哈希锚，问题同型；apollo 以 `combat_bundle_hash` + VM 版本线双锚解）。
- **不做清单**：不做通用反作弊平台（本件 = 逻辑层对账单件）；不做实时拦截（实时性归 intent 校验 + §9 权威纠正，本件是事后异步）；不做客户端完整性检测（反外挂客户端域）；不吞并战斗结算逻辑本身（结算规则在 bundle，verifier 只复算与导出，结算落库归 instance）。

## 7. 分期落地（M0-M5，全部随代码批；验收口径先行）

| 段 | 内容 | 验收判据 | 前置 |
|---|---|---|---|
| **M0** 契约冻结 | internal 域三消息字段表 + verdict 枚举 + checkpoint/门参数表定稿（sdk-contract §11 XSD；随契约批） | XSD 过闸 + 字段面评审通过（本稿 §1.2/§4.1 即字段源） | 契约批窗口 |
| **M1** 复算内核 | verifier-kernel 库（同进程）：bundle 装载 + 四元组驱动重模拟 + 链计算 | **双跑自证**：同四元组同 bundle 双跑 hash 全等（服务端内确定性，不涉跨端）；沙盒/预算纪律随 scripting-lua 实现批 | C-34 ECS 收敛（battle-determinism P1 同前置）；scripting-lua 实现批 |
| **M2** 独立服务 + 结算门 | apps/verifier-app + G-1 VERIFIER 型注册 + Submit/Result 链 + instance 结算门挂起/放行 | 正常战报全链走通：提交 → 复算 → PASS → 结算落库；Compact 内嵌形态同批 | G-1/InterServerLink 实现批；M1 |
| **M3** 判定出口全量 | verdict 四值 + 三级事件移植 + violation_score 接桶 + 仲裁队列 | 伪造战报（改 hash/改输入/谎报掉落）三用例全部 MISMATCH → 挂起 → score 计数 | M2 |
| **M4** 采样容量降级 | 采样率/队列水位/降级序 + 熔断参数落容量批指标集 | 与 gap #15 bots 互为验收：bots 模拟作弊客户端（伪造/延迟/残交）跑出全 verdict 覆盖 | M3；**#15 bots** |
| **M5** 分歧定位 | checkpoint 二分 + 对接 battle-determinism P3 离线重放器 | 给定人工注入分歧的战报，定位到具体 tick | M4；battle-determinism P3 |

- **与 battle-determinism 分期的咬合**：本件 M1 = 其 P2 录制/hash 链的复算侧消费者；M5 = 其 P3 离线重放器的在线兄弟（同工具族，一处实现两端用）。

## 8. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| battle-determinism | §2 四约束全量继承 + 跨端增量三件（本稿 §2.2）；§5 四元组/hash 链/录制策略复用——`binary_id` 取值域差异：服务端权威复算 = 引擎+契约 hash，客户端权威验证 = `combat_bundle_hash`+VM 版本线；P2/P3 与本件 M1/M5 咬合 |
| scripting-lua | §2/§4 沙盒与 VM 版本线（verifier 复用不新起）；§6.1 三级事件形态移植（§4.1）；§3.2 版本指针 = bundle 服务端侧装载锚；§8 异步交接不适用本件（复算本身是 worker 全程任务，非场景线程内异步段） |
| attribute-sync | §9 预测边界 = 服务端权威形态不需要本件的前提；§8.2 :285 结算前 L3 显式 snapshot = 结算门冻结面；§8.2 journal = 战报存储/起始快照位点来源 |
| net-abstraction | §7 G-1 注册（VERIFIER 型）；§5.7 InterServerLink invoke_mode 三分（ReliableEvent/OneWay/RequestReply 各归其位）；§4.3 violation_score 单桶新增第四计数源 |
| sdk-contract | §11 internal 域 id 900+ 三消息族（M0）；client 域零新增域（终局申报走既有 events 通道一条消息） |
| concept-glossary | 「战斗验证服务」词条（本稿为其展开件）；命名纪律 §0 裁决 1-②（battle 词专属）与裁决 1-①（副本 instance 定名）双执行 |
| capacity-and-benchmark | §5 复算吞吐/降级序并入；DRIFT 率/MISMATCH 率/门超时率进指标集（M4 落参数） |
| architecture/combat-runtime-and-ecs-boundary-design.md | C 档参考件——其战斗运行时边界的历史讨论由本稿 + battle-determinism 承接（architecture/README C 档行指针） |

---

*基线：apollo main @ 4299dd99（文档态）。行号引用为本轮实读（attribute-sync :285/:319、scripting-lua §6.1 :168-186、sdk-contract §11 :546-565、net-abstraction §4.3 :123-129）；跨端确定性增量对 battle-determinism §2「不做定点」口径的关系已在 §2.2 写明（原口径成立域 = 服务端单侧判定；跨端档不靠定点、靠运算白名单+版本锚+漂移降级三层，定点查表仅作为 `apollo.battle` 注入件继承）。M0-M5 全部随代码批，本稿零源码改动。缺口登记：design-gap-inventory #17（2026-09-30 立项即设计落盘）。*
