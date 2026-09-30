# Apollo 时间与时钟模型（clock-and-time）

> 状态：设计稿（评审中）。目标：定形「时间」在 apollo 里的三种形态与各自禁入区——单调钟只做测量、tick 号只做判定序、wall 钟只做人类可读；并回答 tick 调度（固定步长/追赶/暂停）、客户端校时边界、跨进程时钟不互信、定时器的时间接口。与 `docs/design/attribute-sync.md`（§10 六阶段/§5.1 预算/§8.2 摊写——全部以本文件的 tick 号为刻度；§10.1 已论证「为什么是 tick 不是消息驱动」G-7，本文不重复）、`docs/analysis/ssengine-reference.md` §4.3（定时器轮）、`docs/analysis/mmo-mechanism-deep-dive.md` §8（定 tick 与线程编队三家先例）互为引用。缺口登记：design-gap-inventory #1。

---

## 执行摘要

1. **三钟分工是本设计的全部**：`steady_clock`（单调钟）——**唯一测量入口**（帧耗时/超时/退避/预算补给时长）；**tick 号**（uint64 逻辑钟）——**唯一世界状态判定序**（属性 seq 容器/定时器刻度/journal 位点锚/预算领取节拍）；`system_clock`（wall 钟）——**唯一人类可读与跨系统对齐**（日志行/持久化 created_at/GM 审计/报表）。三钟各有禁入区：wall 差值不测时长（NTP 跳变出负值）、tick 不换算真实时间（重放语义）、steady 不进游戏判定（跨端不一致）。
2. **tick 号判定，时间戳永不判定**：回放确定性（回放 = seed + 按 tick 对齐的输入序列——battle-determinism 的时间轴即此）、NTP/闰秒免疫、单调无回退、跨端零对表成本。BW/KBE 全部以 tick 为节拍（`DEFAULT_GAME_UPDATE_HERTZ = 10`，lib/server/common.hpp:19；`gameUpdateHertz 10`，kbengine_defaults.xml:5——deep-dive §8 实测）。
3. **固定步长 100ms（10Hz）默认，进程级单一 TickSource**：主循环 = 消息处理段 + tick 段（BW time slice 同型，cellapp.cpp:946-948）；追赶有界（落后 ≤ catch_up_max 连跑补齐，超则跳拍 + 告警 + 记 dropped_ticks——不允许无界连跑挤死 IO）；tick 号进程内单调、重启归零、跨重启连续性由 journal 持久序号承担。
4. **时钟注入纪律**：游戏逻辑不直接调 OS 时钟——经 core::di 构造注入的只读 ClockService 门面（三钟读数 + tick 计数）；单测/回放注入 fake clock。`apps/game-server/src/main.cpp:20` 的 `GameClockService` 现为演示壳（仅 `std::string name = "clock"`，无任何时钟语义）——代码阶段按本设计重写或随演示代码清理，设计不依赖其现状。
5. **客户端时间只做显示**：tick 号随快照/关键消息下发（客户端进度显示/插值对齐）；校时协议 P3 且只影响显示层；movement 通道时间戳取最新已有语义不变（net-abstraction §4.1）。任何「客户端时钟参与服务器判定」的提案按本设计直接否决。
6. **跨进程时钟不互信**：进程间「谁先谁后」比较一律用 seq/位点/epoch（net-abstraction §5.7 握手三元组已有 authority_epoch；G-2 接管锚 = journal 位点，不是时间点）。时间值不跨进程比较——两机 NTP 偏差可到毫秒级，而 tick 判定粒度就是 100ms。

---

## 1. 现状盘点（读码结论）

| 部件 | 位置 | 现状 | 判定 |
|---|---|---|---|
| `apollo::base::Time` | modules/base/include/apollo/base/time.hpp:12-60 | steady 毫秒/微秒/纳秒（:16-29）+ `unix_time`（:31-33）+ 格式化族（:35+）——单调/wall 已分离 | **保留为原语层**；缺 tick 钟与注入形态 |
| `GameClockService` | apps/game-server/src/main.cpp:20-22 | `struct { std::string name = "clock"; }`——纯演示壳；`LoginPipeline` 持引用（:24-27）只为演示 DI 装配；tick() 只递增计数（:51-53） | 代码阶段重写为真门面或随演示清理（登记：下轮代码批） |
| legacy 时间轮 | include/apollo/core/timer/{timer.h,timer_manager.h} + src/apollo/core/timer | C-23/C-24 已判死（>1 tick 永不触发/无锁 unlock UB/跨线程竞争） | 先删后建（登记簿已注）；新轮的时间接口归本设计 §8 |
| `include/apollo/utils/time.h` | utils | 移植件（旧树） | 随属性/遗留树收敛批处置，不新增依赖 |
| tick 调度 | 无 | 主循环无固定步长——game-server 演示仅 `run_once` 计数 | 本设计新建（§4） |

负空间：design 六份无「时钟/单调钟/steady_clock/校时」成文（gap inventory #1 证据）；BW/KBE 亦无独立时钟设计（深潜 §3/§8 只见 updateHertz 常量）——**本件为超先例自定，先例只提供「10Hz + tick 节拍」的形态佐证**。

## 2. 三钟分工（核心决策表）

| | 单调钟 steady_clock | tick 号（逻辑钟） | wall 钟 system_clock |
|---|---|---|---|
| 本体 | OS 单调时钟（`Time::now()` 族，time.hpp:16-29） | uint64 计数，进程级单源递增 | OS 日历时钟（`Time::unix_time()`，:31-33） |
| 语义 | 「过了多久」 | 「世界推进了几拍」 | 「人类社会几点」 |
| 唯一合法用途 | 测量：帧耗时、超时判定、重连退避、token 补给时长、性能计数 | 判定序：属性 seq 推进、ACK 超时（「3 个广播 tick」，attribute-sync §3.3）、优先级延迟（INSTANT=0/LOW=20 tick，§5.2）、定时器 deadline、journal 位点锚、write-behind 每 tick 定额出队（§8.2）、Lua 指令预算按帧 | 展示：日志行时间戳、持久化行 created_at、GM 审计「谁何时」、运营报表、目录命名 |
| 禁入区 | 游戏判定（跨端/跨进程不一致）；持久化对齐 | 换算真实时间（tick-时间映射只存在于调度器内部，不外泄）；客户端展示时钟 | 测量差值（跳变/闰秒出负值或跳变）；任何游戏逻辑分支 |
| 跳变行为 | 免疫（单调保证） | 免疫（自增） | 接受（OS/运维管；本层不修正） |
| 跨进程 | 不互信 | 不互信（每进程独立计数；跨进程序用 seq/epoch，§6） | 不互信（NTP 偏差可超一个 tick 粒度） |

三条使用口诀（写入代码评审红线，对应 architecture-review「纪律断言」族）：

1. **测时长，只用 steady**——`Time::now()` 差值是全仓唯一合法时长来源；`time(nullptr)` 差值出现在 diff 里即缺陷。
2. **做判定，只用 tick**——属性/定时器/预算/位点的分支条件里出现任何时钟读数即缺陷。
3. **给人看，只用 wall**——日志与持久化行带 wall；同一行再带 tick 号（§7 双写），但两者职责不互换。

## 3. tick 号：唯一判定序

- **产生**：进程级单一 `TickSource`（挂主循环，§4）每步长 +1；uint64 @10Hz 溢出不可能（584 亿年）。进程启动后首个完整 tick 计 1，0 保留为「未启动/无效」哨兵。
- **进程内广播**：多场景线程消费**同一个 tick 值**——场景线程各自的定时器轮/预算实例，但共享全局 tick 刻度。理由：journal 位点、预算统计、G-5 帧耗时聚合要跨场景可比；各场景自计数则「本进程第几拍」会有多个答案，运维查询与回放对齐全部复杂化。BW 同进程单一 updateHertz 节拍同型。
- **生命周期**：进程内单调；**重启归零**；跨重启的世界时间线连续性由持久层承担——journal 持久序号（attribute-sync §8.2）为跨重启锚，tick 号只是进程内刻度。客户端 resume 后 tick 重新同步（随 enter-view 快照下发当前值）。
- **tick-时间映射不外泄**：调度器内部用 steady 钟决定「该跑第几拍」，但该映射（何时加速/跳拍）是实现细节；任何模块拿「tick × 100ms」换算挂钟时间即违约——运维需要真实时间时直接读 wall。

## 4. tick 调度器：固定步长、有界追赶、可暂停

**形态**（挂 modules/runtime 主循环，BW/KBE 同型——消息到达即处理 + 固定节拍推进，两者并存）：

```
loop:
  process_messages(until next_tick_deadline)   # 消息段：上行分发/异步回调（attribute-sync §10 阶段 0）
  tick = TickSource.next()                     # 判定 tick 边界到了（steady 钟唯一用途：算 deadline）
  for scene in scenes: scene.tick(tick)        # 各场景线程按 §10 六阶段推进（阶段 1-6）
  sleep_until(next_tick_deadline += step)      # 剩余预算让出
```

- **步长**：默认 100ms（10Hz）——BW `DEFAULT_GAME_UPDATE_HERTZ = 10`（common.hpp:19，server_app_config.cpp:28 注册）、KBE `gameUpdateHertz 10`（kbengine_defaults.xml:5）两家一致；步长进配置（core::config），**运行期不改**（改步长 = 改全系统超时常量的语义，走重启）。
- **追赶有界**：一拍超时（如断点/长 GC）后，若落后 ≤ `catch_up_max`（默认 2 拍）则连跑补齐——保住「tick 语义 ≈ 游戏世界 100ms」的近似；落后更多则**跳拍**：TickSource 直接跳到当前拍，跳过的拍数记 `dropped_ticks` 计数器（G-5 检测原语上报）+ WARN 日志。**不允许无界连跑**——补 20 拍等于把 IO 饿死一个 RTT，跳拍让世界「慢放」而不是「卡死」。
- **暂停（tick pause ≠ 进程暂停）**：三类合法暂停点——① 在线调试 eval/状态采样（scripting-lua §7，tick 边界沙盒）；② 脚本热替换原子换表窗口（scripting-lua §3.2）；③ 手动运维挂起。暂停期间 TickSource 停走、wall 照走；恢复后**不追拍**（记 `gap_ticks = wall 差值 / step` 供观测）——调试者改的 world 不能因追赶瞬间快进。停机序列（attribute-sync §10.2）不走暂停路径——停机要把 journal flush 干净，暂停只是冻结。
- **帧预算监控**：每拍 steady 差值（六阶段耗时）进 G-5 检测原语（帧耗时/队列水位族）；「连续 N 拍 > 预算 80%」即过载信号，接准入闸门（BW LoginConditions 同型）。

## 5. 客户端时间：显示对齐，不进判定

- **tick 下发**：enter-view 快照与关键控制消息携带当前服务器 tick 号；客户端用于——进度/冷却显示对齐、回放对齐（#2 战斗确定性共用此轴）、「落后几拍」的自检（movement 插值漂移检测）。
- **校时（P3，可选）**：握手/心跳带服务器 wall 时间戳，客户端算 RTT/2 偏移——**产物只进显示层**（聊天时间戳、活动倒计时）。判定面零消费：AOI 插值用本地钟 + 服务器位置序列（movement 带时间戳取最新，net-abstraction §4.1——该时间戳是「序」的用途，非挂钟对齐）。
- **红线**：客户端上报的任何时间值不进服务器判定（反作弊面：客户端钟完全可控）。上行只带 seq/intent，不带「我以为现在几点」。

## 6. 跨进程时钟：不互信

- 比较原语已有，不用时间：进程间先后 = seq/位点（InterServerLink ReliableEvent 续传、RO_MIRROR journal 位点 ack）；「对端是不是重启了」= authority_epoch 失配（net-abstraction §5.7 握手三元组）；「接管从哪继续」= journal 位点（G-2）。
- 跨进程消息带 tick 号（信封可选字段，InternalMessageEnvelope——net-abstraction:351 信封族）仅作**观测对齐**（两进程日志行按 tick 对表排查），不参与路由/重试/接管判定。
- 集群不引全局时钟服务（NTP/PTP 精确同步不在设计面）：需要「全局先后」的场景归单点 owner 裁决（G-1 编队事件经 mgr 定序）——单写者纪律的时间版。

## 7. 持久化双时间戳

journal 行 / 快照行 / GM 审计行统一双写 **(tick, wall_ms)**：

- tick = 重放定位与位点语义（journal replay range 按位点）；wall = 运维查询（「昨晚 3 点发生了什么」直接 SQL，不需 tick 换算）。
- 两者不互换：以 wall 推导位点（时钟跳变即错位）或以 tick 换算挂钟（暂停/跳拍即漂移）都违约。
- BW 先例佐证「以 tick 为预算单位」的成熟度：BackupSender 小数余数跨 tick 结转（backup_sender.cpp:114-117，`numToBackUpFloat = bases.size()/periodInTicks + backupRemainder_`）——节拍换算只存在于机制内部，与 §3「映射不外泄」同款纪律。

## 8. 定时器轮的时间接口（接 ssengine-reference §4.3）

- **deadline 单位 = tick 号，不是毫秒**：`after(n_ticks)` / `at(tick)`；时间轮槽位以 tick 步长为刻度（skynet 五层轮按 2.5ms 网格是独立线程范式的配套——apollo 轮挂主循环，刻度天然 = tick）。毫秒→tick 的换算由**调用方**在注册时完成（`after_ms(x)` 即 `after(ceil(x/step))`，向上取整——宁迟一拍不早一拍），轮内不再出现时钟。
- **到期回调在 owning 场景线程 tick 边界执行**（§4.3 既定），回调签名带 tick 值——回调内判定「现在第几拍」读参数，不读钟。
- 暂停期间轮冻结（刻度是 tick，§4 暂停 = 轮不走）；恢复后 deadline 不变（世界慢放，定时器语义跟随世界而非挂钟）——与 BW `secondsToTicks` 换算（baseapp_config.cpp:84 backupPeriodInTicks）同语义。
- legacy core/timer 删除前不接本接口（C-24 已判死）。

## 9. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| attribute-sync | §10 六阶段/§5.1 token bucket/§5.2 优先级 tick 表/§8.2 每 tick 定额摊写——全部消费本文件 tick 号；§10.1（G-7 范式论证）是本文 §4 的前置，不重复 |
| ssengine-reference §4.3 | 定时器轮 deadline = tick 号（本文 §8 补时间接口）；驱动权 game loop 不变 |
| net-abstraction | §3 线程模型 tick 交界 flush 以 tick 边界为准；movement「时间戳取最新」是序语义非对钟；InterServerLink/信封 tick 可选字段（§6） |
| scripting-lua | §3.2 热替换窗口 = 合法暂停点（本文 §4）；§6 指令预算按帧 = 按 tick；§7 调试 eval 在 tick 边界、暂停语义本文定义 |
| logging | 日志行双写 (wall, tick)——崩溃取证四件套的日志可与 journal 位点对表 |
| sdk-contract | 快照/控制消息携带 tick 号 = 契约字段（P2 随客户端下发定型；进 messages.xml 内部/客户端域） |
| battle-determinism（gap #2，随战斗批） | 回放时间轴 = 本文件 tick 号；确定性四约束以「判定序唯一」为第一条 |
| architecture-review | GameClockService 重写/清理与 legacy core/timer 删除登记入代码影响项（§16.10.2 登记簿既有行追加注） |

## 10. 分期落地

- **P1**：`TickSource` + `ClockService` 只读门面（modules/base——Time 原语之上，core::di 构造注入；game-server 重写演示为真实装配）；主循环固定步长 + 有界追赶/跳拍落 modules/runtime（ServiceHost run_once 升级）；定时器轮新接口（deadline=tick）随 ssengine §4.3 实现批；三口诀进代码评审红线。
- **P2**：tick 号随快照/控制消息下发（契约字段定型）；客户端显示对齐（movement 插值自检）。
- **P3**：校时协议（可选，显示层）；跨进程 tick 观测对齐（信封可选字段）；G-2 接管/journal 位点与 tick 双写的落库核验（容量批 §5 基准含「跳拍率」指标）。

---

*基线：apollo main @ 05919db9（modules/base/time.hpp、apps/game-server/src/main.cpp 本轮实读：:16-29 steady/:31-33 wall/:20-22 GameClockService 演示壳/:51-53 tick 计数）；BW/KBE/skynet 行号沿用 mmo-mechanism-deep-dive §8/§16.6 已核记录（common.hpp:19、kbengine_defaults.xml:5、cellapp.cpp:806-808/:946-948、backup_sender.cpp:114-117、baseapp_config.cpp:84、skynet_start.c:131-140）。缺口登记：design-gap-inventory #1；本件落盘后回填状态列。*
