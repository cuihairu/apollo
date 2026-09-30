# Apollo 容量模型与基准设施（capacity-and-benchmark）

> 状态：设计稿（评审中）。定位：把散落在六份设计里的容量数值（5000 CCU / 25k msg/s / movement <100B / 32KB/s token bucket……）集中为**单进程容量预算表并给出推导链**；回答「net-abstraction §5.6『M2+ 每项先有基准再动手』的基准从哪来」；补 C++ 侧内存与对象池预算（Lua 侧上限已有 scripting-lua §6，C++ 侧此前零对应物）。缺口登记：design-gap-inventory #5（容量模型/基准设施）+ #7（内存对象池，同批）。与 `clock-and-time.md`（tick 10Hz 口径/跳拍率指标）、`attribute-sync.md`（§5 带宽/§10 六阶段/§11 验收表）、`net-abstraction.md`（§4 水位/§4.3 上行限流/§5.6 演进阶梯）、`scripting-lua.md`（§5.2 钩子预算/§6 沙盒上限）、`sdk-contract.md`（§10.6 性能量化）、`battle-determinism.md`（§5 录制格式——基准负载复用）互为引用。

---

## 执行摘要

1. **容量目标一句话**：单进程 5000 CCU 稳态、单场景 100 人同屏战斗 p99 达标（attribute-sync §11 全表）——其余数值全部从这条推出，不再各自为政。
2. **推导链四步**：CCU → 实体规模与 AOI 密度 → 变更/消息率（上行 25k/s 全服 = 5000×5 条/秒）→ 帧预算与带宽分配（10Hz × 100ms/帧；下行 per-client 32KB/s token bucket）→ 内存预算（每实体字节 × 规模 + Lua 状态 + net 环）。
3. **口径统一**：主循环 tick = **10Hz**（clock-and-time §4 定案）；movement **20Hz 是客户端上报节拍**（net-abstraction §4.1），不是主循环频率——sdk-contract §10.6 量化段的「20Hz 主线程」为估算旧口径，本文以 clock-and-time 为准重述（帧占比结论不变，sdk-contract :500 的 8%/4% 按 10Hz 重算为 ~4%/2%，仍在帧预算 10% 内）。
4. **帧预算表**（§2）：每帧 100ms 分配到消息段 + tick 六阶段 + 富余；各设计既有预算（脚本钩子窗口 ≤0.5ms/帧、collect/flush 超时截断）归位到同一张表，预算超时截断机制沿用 attribute-sync §10。
5. **内存与对象池（#7）**：池化三类已知热点（帧对象/属性收集缓冲/发送环切片），free-list 挂 owning 线程（单写者无锁）；**jemalloc/tcmalloc 不引入**——分配热点已被单写者+池化收敛，全局分配器替换是过早优化，列 M2+ 基准准入（分配占帧 >5% 才评估）。
6. **基准三形态**（§5）：微基准（单机制，ctest，PR 级门槛）/ 场景基准（apps/bench，合成 intent 流——确定性 RNG 派生，可复现）/ 录制回放（线上采样 intent 序列 = battle-determinism §5 同格式，录制既是回放验证也是压测负载）。M2+ 的准入基准 = 场景基准的 before/after。
7. **系统级验收指标集**（§5 末表）：各设计 §11 类验收汇总 + 新增系统级（跳拍率/dropped_ticks——clock-and-time §10 P3 引用的「容量批 §5」即此表；水位触发率；violation_score 断开率；录制体积与 hash 链分歧率——battle-determinism §8 P3 接入点）。

## 0. 现状：数值散落与空白（gap 证据）

- 散落数值（出处实读）：5000 CCU 与 25k msg/s（sdk-contract :476/:500）、movement <100B（sdk-contract :395）、32KB/s per-client token bucket（attribute-sync §5.1）、100 人同屏压测（attribute-sync §11/§12 P1 验收）、zstd >256B 阈值（sdk-contract :395——「P1 落地时基准化，与 attribute-sync §11 度量一并」的欠账本文兑现）、钩子指令预算 200k≈0.5ms（scripting-lua §5.2）、Lua 每状态内存上限（scripting-lua §6——机制有、数值无）。
- 空白：「对象池/内存池/pmr」design 六份零命中（#7 证据）；系统级容量模型与基准 harness 零成文（#5 证据）——「M2+ 每项先有基准再动手」无设施支撑即空话。

## 1. 口径与顶层目标

| 项 | 值 | 出处/说明 |
|---|---|---|
| 主循环 | tick 10Hz，帧 100ms | clock-and-time §4（BW/KBE 两家一致） |
| 容量目标 | 5000 CCU/进程 稳态 | sdk-contract :476 既有口径，本文升格为顶层目标 |
| 极端场景 | 单场景 100 人同屏战斗 | attribute-sync §11 压测口径 |
| 实体规模 | 玩家 5000 + NPC/怪物 ~20k（按场景配额） | 估算口径，配置化 |
| 上行画像 | 人均 5 条/秒 → 全服 25k msg/s | sdk-contract :500 既有估算 |
| 属性变更率 | 人均 20 变更/秒（AOI 广播放大后下行 ~200 帧/秒/客户端） | 估算口径，基准校准 |
| 部署形态 | 单进程阶段单机 8GB；P3 编队后按进程角色分表（本文以 game 进程为准） | — |

## 2. 帧预算表（每帧 100ms，单场景线程口径）

| 段 | 预算（初始分配，基准校准后进 config 常量） | 依据/机制 |
|---|---|---|
| 消息段（process_messages：上行分发/异步回调） | ~20ms（不足用完即止，剩余让出） | clock-and-time §4 主循环 |
| simulate（业务/脚本写属性） | ~30ms（含脚本钩子窗口 ≤0.5ms，scripting-lua §5.2） | attribute-sync §10 阶段 1 |
| recalc（派生重算，含 Lua 公式调用） | ~10ms | 阶段 2 |
| aoidecay（viewer set 增量） | ~5ms | 阶段 3 |
| collect（delta/快照组装） | ~10ms | 阶段 4 |
| flush（预算装包+发送） | ~10ms | 阶段 5 |
| persist-batch（journal 交 DB 线程） | ~5ms | 阶段 6 |
| 富余/长尾吸收 | ~10ms | 连续 N 拍 >80% 即过载信号（clock-and-time §4） |

- 超支处理沿用既有机制：阶段 4/5 预算超时立即截断、剩余下 tick（attribute-sync §10）；整帧超支走有界追赶/跳拍（clock-and-time §4 catch_up_max=2）。
- 多场景线程：每线程独立 100ms（§10 单写者），帧预算表同表复用；全进程 CPU 预算 = 场景线程数 × 帧预算 + IO 线程 + DB 线程（部署参数：场景线程数 ≤ 物理核 − 2）。

## 3. 消息与带宽预算（汇总既有 + 推导）

| 面 | 值 | 出处 |
|---|---|---|
| 上行总率 | 25k msg/s 全服（人均 5/s）；per-session 限流四通道参数表 | sdk-contract :500；net-abstraction §4.3 |
| 下行 per-client | token bucket 32KB/s（动态下调），稳态目标 ≤80% | attribute-sync §5.1/§11 |
| movement 帧 | 20Hz × <100B ≈ 2KB/s/客户端 稳态 | net-abstraction §4.1；sdk-contract :395 |
| attributes 批 | 10Hz 批，>256B 才 zstd（阈值 P1 微基准化——sdk-contract :395 欠账） | attribute-sync §7.1 |
| 全服出口带宽 | 5000 × 32KB/s = 160MB/s 打满上限，实际 ≤80% ≈ 128MB/s ≈ 1Gbps——单机 10Gbps 网卡内；多进程编队后按进程分摊 | 推导 |
| 帧头开销 | 16B/帧（net-abstraction §3）——movement 类小帧占比 ~16%，bundle 攒包（M2+）是对策 | net-abstraction §3 |

## 4. 内存与对象池预算（#7）

**每进程内存预算表（初始，量级口径，基准校准）**：

| 域 | 预算 | 推导 |
|---|---|---|
| 实体与属性 | ≤1.5GB | 玩家实体 ~40KB（values 桶 ~5KB + ChangeHistory 1024 条 ~20KB + 组件 ~15KB）× 5000 ≈ 200MB；NPC ~15KB × 20k ≈ 300MB；ViewerState 对（100 同屏² × 16B × 场景数）与峰值余量 |
| Lua 状态 | 每状态 ≤64MB（scripting-lua §6 上限的数值化）× 场景线程数（≤8）≈ 512MB | 实体数据不进 Lua（scripting-lua §1），状态只装逻辑 |
| net 收发 | 每会话发送环 64KB + 接收缓冲 64KB × 5000 ≈ 640MB | 环容量 = 突发上限（水位 cut @95% 的物理基础，net-abstraction §5.8） |
| journal/batch 缓冲 | ≤512MB | write-behind 每 tick 定额出队（attribute-sync §8.2）的有界性 |
| 其他（AOI 网格/契约表/观测） | ≤1GB | — |
| **合计目标** | **稳态 RSS ≤4GB（8GB 机）** | 告警线 6GB / 熔断线 7GB（进程级，运维面） |

**对象池三类**（已知热点，free-list 池挂 owning 线程——单写者无锁，归还零同步）：

1. **帧对象**（MsgPtr 族）：每 tick 消息量峰值参数化；池耗尽 = 走 trim/降级路径**不现分配**（OOM 防护优先于丢帧——与水位 cut 同向）。
2. **属性收集缓冲**（collect 阶段的 per-viewer 批组装）：按 (viewer × 通道) 预留，复用跨 tick。
3. **发送环切片**：环本身即预分配（§5.8 发送环提交语义），池化指切片句柄/视图对象。

- **池大小 = 容量参数**（config 常量，随 §1 目标推导），启动期分配、运行期不增长——内存上界在启动即确定，不用预算换吞吐。
- **jemalloc/tcmalloc：不引入**——单写者纪律 + 三类池已把分配热点收敛到可数位置；全局分配器替换改变整进程行为（碎片策略/统计面/运维面）且与「禁并存」纪律（§15.2）同族（第二分配器 = 双行为面）。列 **M2+ 基准准入候选**：场景基准显示分配占帧 >5% 才立项评估。
- Lua 侧上限对齐：scripting-lua §6 表（内存/指令/错误/死循环/隔离五维）是脚本域对应物，本表补 C++ 域——两表合为全进程内存纪律。

## 5. 基准设施（#5 后半）

**三形态**：

| 形态 | 内容 | 归属 | 门槛用途 |
|---|---|---|---|
| a 微基准 | 单机制：zstd 阈值（sdk-contract :395 欠账）/RNG 子流/定时器轮/发送环吞吐 | ctest 注册（tests/） | PR 级回归可见（数字劣化即红） |
| b 场景基准 | N 场景 × M 实体 × 合成 intent 流（确定性 RNG 派生——battle-determinism §4 同源，负载可复现、结果可比） | apps/bench（运维工具，apps→modules 单向；BW server/tools、KBE load nonexistent 自建同型） | M2+ 准入基准（before/after 对比）、容量验收（§1 画像跑通） |
| c 录制回放 | 线上采样的 intent 序列 = **battle-determinism §5 同格式**——录制既是回放验证负载也是压测负载（真实包型/真实节奏） | 与 battle-determinism P3 重放器同批 | 真实负载校准（合成流标定不了的包型分布） |

- **「M2+ 每项先有基准」的落点**（net-abstraction §5.6 纪律的设施化）：io_uring 切换、bundle 攒包、分配器评估等 M2+ 项的准入 = 形态 b 的同负载 before/after + 显著性判据（尾延迟 p99 改善 ≥X% 才合入）；基准拓扑（场景数/实体分布/包型）进配置入库。
- **环境纪律**：基准机配置声明（CPU 型号/内核/NIC/关 NUMA 平衡）；单进程跑（分布式压测 P3 编队后另立）；**对照基线 golden 化入库**（防「跟上次本地跑比」的漂移——与契约 golden 同哲学）。
- **系统级验收指标集**（各设计散落验收的汇总 + 新增；clock-and-time §10 P3 引用的「容量批 §5 基准」即本表）：

| 指标 | 目标 | 出处 |
|---|---|---|
| 属性变更→客户端 p99 / INSTANT | ≤150ms（NORM）/ ≤1 tick | attribute-sync §11 |
| 单客户端稳态带宽 | ≤ 预算 80%（100 人同屏） | attribute-sync §11 |
| 跳拍率 / dropped_ticks | 稳态 <0.1% 帧；连续跳拍告警 | clock-and-time §4/§10（新增量化） |
| 水位 soft+ 触发率 | 稳态 <1% 会话·min（触发即负载异常信号） | net-abstraction §4.2（新增量化） |
| violation_score 断开率 | 稳态 <0.01% 会话·min | net-abstraction §4.3（新增量化） |
| 重连恢复 / 崩溃回档 | ≤1 RTT+快照(≤50KB) / ≤2s journal 零丢失 | attribute-sync §11 |
| 帧耗时分位 | tick 段 p99 ≤70ms（§2 表合计），连续 3 帧 >80ms 过载告警 | clock-and-time §4（新增量化） |
| 录制体积 / hash 链分歧率 | ≤5MB/关键战斗 / 复算分歧 = 0（版本内） | battle-determinism §5/§8（接入点） |
| RSS | 稳态 ≤4GB，告警 6GB | §4（本表） |

## 6. 分期落地

- **P1**：容量参数集落 config（CCU/池大小/预算表常量/告警线）+ 微基准首批（zstd 阈值基准化——sdk-contract :395 欠账兑现；发送环；RNG）+ 跳拍/水位触发计数器进 MetricRegistry（指标先行，基准后到）。
- **P2**：场景基准 v1（apps/bench：合成 intent 流 + 帧耗时分位/内存/带宽采样进 MetricRegistry）+ 对象池三类 + 帧预算表常量第一轮校准。
- **P3**：录制回放负载接入（battle-determinism P3 重放器同批）+ 容量验收测试（5000 CCU 画像跑通 §5 指标集全表）+ 分配器 M2+ 评估点（若基准立项）。

## 7. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| clock-and-time | tick 10Hz 口径源；跳拍率/gap_ticks 指标化；§10 P3 的「容量批 §5」= 本文 §5 指标表 |
| attribute-sync | §11 验收表并入本文指标集；§5.1/§10 预算/截断机制沿用；§8.2 摊写定量（journal 缓冲预算） |
| net-abstraction | §5.6 M2+ 准入基准的设施化（本文 §5 形态 b）；§4.2/§4.3 水位/限流触发率指标化；§5.8 环容量入内存表 |
| scripting-lua | §5.2 钩子预算入帧预算表；§6 Lua 状态上限数值化（64MB/状态）与 C++ 表并列 |
| sdk-contract | §10.6 量化口径统一（20Hz 旧口径 → 10Hz）；:395 zstd 阈值基准化欠账在本文 P1 兑现 |
| battle-determinism | §4 确定性 RNG = 基准负载可复现的来源；§5 录制格式 = 回放负载格式；§8 P3 指标（分歧率/体积）接入 |
| architecture-review | 16.8.3 归属判据的执行：apps/bench（聚合工具）↔ modules 检测原语两截；基准 golden 化与契约 golden 同哲学 |

---

*基线：apollo main @ e0b360be（散落数值出处本会话实读：sdk-contract :395/:476/:500、attribute-sync §5.1/§11、net-abstraction §3/§4.1/§4.2/§4.3/§5.6/§5.8、scripting-lua §5.2/§6、clock-and-time §4/§10、battle-determinism §4/§5/§8）。全部估算值标注「初始/量级口径」，基准校准前不作为硬验收——与 sdk-contract §10.6「估算口径，非基准」同一诚实纪律。缺口登记：design-gap-inventory #5/#7；本件落盘后回填状态列。*
