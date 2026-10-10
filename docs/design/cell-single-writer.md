# Cell 单写者收口（RPC 受理与模拟线程的并发形态）

> 状态：**前置设计稿（2026-10-10）；六拍板点全裁（ADR-015 主案 (a) 队列化异步受理 + ADR-016 配套：②水位两级/③不设快路径/④Manager mutex 保留/⑤停机 G-3 镜像 + ADR-017 应答接线：ack-on-accept 现行/延时应答随 M1/阻塞适配否决；ADR-015 的 (c) 维持否决、(b) 经拍板⑥ 彻底否决）**。定位：todo P3-2 批 B「cell-app 单写者收口先行」的决策面展开——钉竞争实证、钉方案三岔语义、钉拍板点，不写实现细节。体裁仿 net-abstraction §7「P3 前置设计」/battle-instance-offload。术语以 term-contract §1.3 为准（cell-app 原型名，ADR-014 不更名）；权威并发口径 = ADR-009 + attribute-sync §10.2 单写者纪律注记 + concurrency.md §1.1 后况勘误（本件为其「方案分叉列拍板项」的直接展开）。

---

## 1. 现状声明（A 级实读，2026-10-10）

**竞争本体：World/Scene 图零锁双写者。**

- 模拟写者：`gameThread_`（cell_server.cpp:142）→ `worldHost_->tick()` → `CellWorldService::tick` → `world_.tick(...)`（cell_server.cpp:56）——`Scene::tick` 遍历 `entities_`（scene.cpp:153，`std::unordered_map`）。
- RPC 写者：`protocol::RepSocket` 自持 worker 线程（socket.cpp:124 `workerLoop`）回调 `setRequestHandler`（cell_server.cpp:98）→ `handleCellCreateEntity/handleCellDestroyEntity/handleCellEntityMove/handleCellCrossBorder/handleCombatSkillCast` 直触 `world_`——`find_scene`（:176）、`spawn_entity`（:191）、`despawn_entity`（:210-211）、`aoi().move`（:225-231）、`find_session`+`execute_scene_transfer`（:253-287）。二线程之间零保护。
- 生产未触发仅因负载空洞（concurrency §1.1 已录）——竞争是结构性的，不是概率性的。
- 调用线程归属图（二次摸底收口）：**WorldSessionManager** 全方法 mutex（world_session_manager.cpp:8-159）但只护会话表不护 world 树；**AnchorManager** 有 mutex（anchor_manager.hpp:26）跨 zone-app 受理线 + autoSaveThread_（snapshot base_server.cpp:496）；**SessionLocator** 单线程（zone-app 主线专用）。锁已就位的两件不是本件竞争面——但形态归属随拍板①统一（见 §5 拍板 4）。
- RPC 消费方现况：真进程消费 = gateway 转发路径（半成品壳，ADR-011 处方挂账）+ 协议单测；调用面已是 `sendRequestAsync` 回调式（socket.hpp:96-100）——**应答异步化不破坏调用方形态**。
- 未来对接：net M1 接收端（InterServerLink，ADR-011）与 gateway ingress（P3-2）的接收面天然是「收包入队」形态——本拍板定义的受理面就是它们的进程内骨架。

## 2. 目标形态（权威口径）

ADR-009（并发所有权）：模拟域单线程驱动、跨线程面收敛最小集。attribute-sync §10.2 单写者纪律注记：**「实体归属其 scene/空间线程，跨线程访问一律投递意图/消息」**；写语义要求「一批变更在固定边界内可见、有序、可序号化」（§10.1 G-7 论证之二）。即：模拟态的一切变更发生在 tick 边界、由模拟线程独占执行；其它线程只投递意图、收取结果。

## 3. 方案三岔展开

### (a) 队列化异步受理

RPC worker 只做解码+入队（意图信封），`gameThread_` 在 tick 边界消费、执行、发回应答。

- **语义**：全部变更 tick 对齐；应答延迟 ≤1 tick（10Hz = 100ms 上界）；调用方回调式，无语义破坏。
- **代价**：pending reply 关联面新增（应答须从 tick 边界发回，需保存关联键——header.sessionId 可承载）；队列水位面要定义（对齐 net-abstraction §5.6 背压水位口径：拒绝/丢旧/水位告警）；停机时序适配 G-3 五阶段（先断流 → 队列 drain → gameThread join）。
- **与既有口径关系**：与 G-1 收尾批先例同构——死亡事件、镜像包、恢复重报全部「收包入队、主循环消费」；本拍板把 RPC 请求归一为同一形态，进程内一跳同型化。

### (b) 队列化 + 同步等待

worker 入队后阻塞（condvar/future）等 gameThread 处理完，取结果即回——同步应答语义保住。

- **语义**：变更仍 tick 对齐（gameThread 在边界消费）；应答即时。
- **代价**：**队头阻塞**——RepSocket workerLoop 单线程，handler 返回前不收下一请求；gameThread tick 慢（50ms+）时全部后续请求堆积，背压传导到 nng 缓冲直至发送方超时。到达率 × tick 时长的乘积一旦越界，RPC 吞吐崩塌。
- **关联面**：condvar 等待面本身是跨线程面；停机 join 顺序要 handler 先断流再停 gameThread（G-3 在 cell-app 的镜像序），否则 join 挂死。

### (c) 最小互斥回退

`world_` 加一把 mutex（或 Scene 粒度锁），两线程照旧直触。

- **语义**：零结构调整，diff 最小；与 WorldSessionManager/AnchorManager 现状形态一致。
- **实质否决面（本件反证）**：handler 在 **tick 中途** 变更容器——`Scene::tick` 正在遍历 `entities_`（unordered_map）时 worker 线程 `spawn_entity`/`despawn_entity`（scene.cpp:43-56）→ 迭代器失效 = UB 崩溃，即使不崩也破坏迭代序 = **battle-determinism 四约束之「迭代序」的实质威胁**（确定性域内实体 tick 序被 RPC 插序打乱）。要救 (c) 只能把锁粒度做成「tick 边界全锁」——那已是 (b) 的语义，绕回队头阻塞。
- **结论**：(c) 不是候选，是反面对照——锁面进入模拟域热路径且不解决确定性，违反 ADR-009「模拟逻辑零锁」决策本体。

## 4. 诚实对比（三岔风险对表）

| 维度 | (a) 异步受理 | (b) 同步等待 | (c) 互斥回退 |
|---|---|---|---|
| 单写者（ADR-009） | ✓ | ✓ | ✗（锁进模拟域） |
| 确定性迭代序 | ✓（tick 边界变更） | ✓ | ✗（tick 中途突变） |
| 应答延迟 | ≤1 tick | 即时 | 即时 |
| 吞吐塌方风险 | 无（worker 永不阻塞，水位拒收） | 有（队头阻塞×tick 时长） | 无（但竞争热路径） |
| 实现代价 | pending reply 关联 + 水位 | condvar + 停机序 | 最小 diff（假解） |
| net M1 接收端复用 | 直接复用（受理面=接收骨架） | 重做（同步面废弃） | 重做 |
| 停机时序（G-3） | 断流→drain→join | 断流→（小心 join 序）→join | 无新增 |

## 5. 拍板点（本件不代拍，候选倾向已记）

1. **主案三岔**——【已拍板 2026-10-10 / ADR-015：采 **(a) 队列化异步受理**】§10.2 纪律注记原文即 (a) 的直系口径；G-7 论证「固定边界内可见、有序、可序号化」= tick 边界受理；调用面已回调式，延迟增量无语义破坏；受理面即 M1 接收端骨架（(b) 的同步面在 M1 下废弃重做）。(b) 保留为 (a) 水位语义不足时的升级案，(c) 否决。（(b) 升级案后经拍板⑥ 彻底否决，见 ADR-017 决策 2）
2. **队列水位与过载语义**——【已裁 2026-10-10 / ADR-016：水位**两级**（高水位告警 + 顶格拒收回过载错误包），不丢旧不静默；过载响应=回调错误码；数值=tick 预算锚定】决策面前置设计 docs/design/queue-watermark.md（水位形态三岔 + 过载响应 + 预算锚定）。
3. **只读快路径**——【已裁 2026-10-10 / ADR-016：**不设快路径**】全量入队，受理面单形态；PING 延迟 1 tick 无害，多一条快路径就多一个并发形态要审。
4. **Manager 三件归并处置**——【已裁 2026-10-10 / ADR-016：mutex **保留**】WorldSessionManager/AnchorManager 已是「跨线程最小集」形态，数据面小、竞争烈度低；迁移收益不抵改形风险；SessionLocator 不动；单写者收口只指 World/Scene 图。与二次摸底「锁必须保留除非持久化线改形」一致。
5. **停机时序**——【已裁 2026-10-10 / ADR-016：G-3 五阶段 cell-app 镜像】停受理（断流回维护中，zone-app `dispatchRequest` 维护闸同型）→ 队列 drain → gameThread join → worldHost stop。
6. **应答接线（传输层卡点）**——【已裁 2026-10-10 / ADR-017：**ack-on-accept 现行 + 延时应答随 M1 升级；(iii) 阻塞适配否决**】卡点本体：ADR-015 (a) 要求「worker 只做解码+入队，gameThread_ 在 tick 边界消费、执行、**发回应答**」（延时应答），但现行传输 `RepSocket`（nng_rep0）`workerLoop` 严格 lockstep（`nng_recvmsg` → `handler_(data)` 同步返回 → `socket_.send(response)` → 下一轮 recv，socket.cpp:139-172/socket.hpp:119）——应答必须由 recv 同一线程同步产出，延时应答不可实现；且 :165 空应答跳发送（现行变更类回 `{}`）在真实 REP 下欠应答即卡死，全仓走桩分支（:431-488）正因如此。裁定要点：worker 解码+校验+入队（意图信封，ADR-016 水位两级闸）后**受理点立即回执**（变更类空 ack = 现行返回值逐字节同现状；过载回过载错误包）；gameThread_ tick 边界消费执行、结果不落应答；PING 的 Pong 受理点回（纯 wire echo 无 world 访问，非③禁令所指快路径）；延时应答（pending-reply 关联面 + gameThread_ 发送路径）随 M1 InterServerLink 升级，受理面骨架零废弃。(iii) 阻塞适配 = (b) 换皮，反证三条（队列深度恒 ≤1 水位机死码 / 吞吐塌至 tick 率 / 与 ADR-015 弃 (b) 原判冲突）——否决。三候选倾向分析（(i) 等 M1 / (ii) ack-on-accept / (iii) 阻塞适配）与倾向口径勘误（原「(i) 为正本」针对终局接线，ack-on-accept 是 (i) 的前置阶段非绕路）详见 ADR-017。

## 6. 拍板后路径

受理面批（意图信封 + 队列 + tick 消费 + 受理点回执，cell-app）——**已交付（2026-10-10，90d8ebb7）**：意图信封 + AcceptanceQueue（有界 FIFO + 两级水位 + 顶格拒收）+ 受理点回执（ack-on-accept）+ tick 边界 drain + 停机 G-3 尾扫，cell_acceptance_tests 六组全绿；应答接线按 ADR-017（ack-on-accept 现行）。水位与过载批（对齐 §5.6 语义）随受理面批同批落地。Manager 归并复核批（若拍板 4 有改）→ net M1 接收端接线（受理面即骨架，M1 只换传输底座，新增延时应答升级增量：pending-reply 关联面 + gameThread_ 发送路径）。battle-instance-offload 的 spawn 请求协议控制面复用同一受理形态（ADR-012）。

## 7. 与其余设计的交集

- **attribute-sync §10.1/§10.2**：G-7 调度范式论证 + 单写者纪律注记——本件的权威口径源。
- **concurrency.md §1.1**：竞争实证的后况勘误——本件是其「方案分叉」的展开与收口载体。
- **battle-determinism**：四约束之迭代序——方案 (c) 的实质否决面。
- **net-abstraction §5.6**：M1 水位/背压口径——拍板 2 的对齐基准；§5.7 InterServerLink——受理面的未来传输底座。
- **ADR-009/ADR-011/ADR-012**：并发所有权决策本体；gateway surface 与 M1 的时机；offload 控制面同型复用。
- **session-and-online-directory §6**：恢复相位在 cell-app 侧的重报受理——同样走「收包入队、主循环消费」先例（G-1 增量③）。

## 8. 证据来源清单（A 级实读，2026-10-10）

- `apps/cell-app/src/cell_server.cpp`：:56（tick 写者）、:98-127（handler 注册）、:142（gameThread_）、:176/:191/:210-211/:225-231/:253-287（worker 直触 world_）、:344（gameLoop tick）
- `modules/protocol/include/apollo/protocol/socket.hpp`：:116-138（RepSocket）、:96-100（sendRequestAsync 回调式）、:119（RequestHandler 同步返回 `std::vector<uint8_t>`）；`src/socket.cpp`：:124/:139（workerThread_/workerLoop）、:139-172（recv→handler→send 严格 lockstep）
- `modules/game/world/src/scene.cpp`：:43-56（spawn/despawn）、:153（tick 遍历 entities_）；`include/.../scene.hpp`：:85-87（unordered_map 容器）
- `modules/game/world/src/world_session_manager.cpp`：:8-159（全方法 mutex）
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`：:26（mutex）
- `apps/zone-app/src/base_server.cpp`：:231（setRequestHandler→dispatchRequest）、:299-340（维护闸同型）、:496（autoSaveThread_ snapshot）
- `docs/rearchitecture/concurrency.md` §1.1、`docs/design/attribute-sync.md` §10.1/§10.2、`docs/architecture/adr.md` ADR-009/ADR-011/ADR-012/ADR-015/ADR-016/ADR-017

*基线：apollo main @ 8cab82d5。本件为前置设计——六拍板点全裁（ADR-015/ADR-016/ADR-017，2026-10-10）；受理面批已落地（2026-10-10，90d8ebb7）。*
