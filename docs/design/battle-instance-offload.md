# Battle Instance Offload 前置设计（副本进程形态）

> 状态：**前置设计稿（2026-10-07）；三拍板点已拍板 2026-10-10（ADR-012：按需 spawn 请求协议 / 内存桩过渡 / 崩溃即作废）**。定位：36 号决策表 #16「Battle 独立实例 + Replay/Inspector 适配缝」的进程形态骨架——仿 net-abstraction §7「P3 前置设计」体裁：钉形态、钉依赖序、钉拍板点，不写实现细节。术语以 term-contract §1.3 为准（**副本进程** instance process：单 scene 单开进程的 offload 形态）。

---

## 1. 现状声明（显式）

- 战斗运行时**在进程内**：P2-2 交付的 `BattleRuntime` 五段状态机经 `Instance::attach_battle` 挂接（world→battle 同进程链接），P2-4 `AnchorRewardSink` 同进程落账。**跨进程战斗为零**——这是分期不是遗漏。
- 确定性前提**已交付**：battle-determinism 四约束（判定域/浮点/迭代序/随机子流）+ ReplayTuple/事件 hash 链（P2-2）——「跨机实例同 binary 即同确定域」（battle-determinism #16）已具备；实例卸载形态（Matchmaker/Instance Manager 骨架，docs/25 §3.1-3.2 可溯）归本批设计。
- 跨进程传输**未落地**：M1 自研内核随 P3-2 批重估（todo 契约差异清欠行）；nng 已退役（term-contract §2）。

## 2. 目标形态（36 号 #16 口径）

大型团战 offload 到**短生命周期副本进程**：故障域与 tick 率独立可调（25 §3.2/§3.3/§8）；结果经 Result Dispatcher **单向落回**（25 §3.4）；Gate↔Battle 不直连（25 §5）；关键帧记录留确定性回放之缝（25 §6）。适配缝三件（36 号）：① 契约 battle 专用通道（sdk-contract §2.3 之外扩展）② 实例 tick 20-50ms 独立可配 ③ Replay 关键帧记录。**现状不实现 lockstep**（36 号 #18：只留缝）。

## 3. 组件与职责（docs/25 §3.1-3.2 骨架对应物，不取全家桶）

| docs/25 件 | apollo 对应物 | 现状 |
|---|---|---|
| Matchmaker / Scheduler | **落点 = manager 域「分配与准入」同源**（net-abstraction §7：负载即观测——帧耗时/实体数/队列水位经 control 通道上报，观测与调度消费同一份数据，不做第二套负载统计） | 未落地（manager 域主体后续批） |
| Battle Instance Manager | **实例生命周期**：spawn / teardown / 崩溃处置——spawn 复用 machined 监督面（批 F 已交付）；teardown = 结算后自灭（短生命周期） | 监督面已交付，按需拉起协议未定（拍板 1） |
| ECS + Battle Pipeline | `BattleRuntime` 五段管线（P2-2） | 已交付（进程内形态） |
| Result Dispatcher | 结算单向落回：奖励语义沿 `IRewardSink`（P2-4）跨进程落到 PlayerAnchor 所在 Zone——battle 只出不进不回写（单向解耦，25 §3.4/§5） | 语义已备（同进程版），跨进程未落地 |
| Persistence | 战斗结算落档沿 write-behind journal（attribute-sync §8.2 单写者纪律）——实例进程**不直写 DB** | 纪律已立 |

## 4. 生命周期协议 ——【拍板 1 已拍板：(b) 按需 spawn 请求协议，2026-10-10 / ADR-012】

实例进程怎么生（machined 现状：UDP 发现/心跳/死亡目录 + roster 静态监督，无按需请求通道）：

- **(a) 静态预池**：副本进程入 machined roster 常驻预启，需求方经目录取空闲实例。——machined 零改动；代价：空闲占用 + 池大小靠配置。
- **(b) 按需 spawn 请求协议**：需求方（Zone/manager）→ machined 发起拉起请求，实例就绪后经目录可见。——语义最准；代价：machined 需新增控制面（UDP 请求/应答报文，或 §7 control 通道进程间延伸——**控制面形态需与 manager 域消费端统一设计**，避免两套控制协议）。

**裁决**：(b) 但控制面复用 §7 编队事件同型（Query/Advertise 已立「发现面」先例——拉起请求/就绪应答是同一扩展方向）；(a) 作为 (b) 就绪前的过渡。**相容注记（2026-10-10 / ADR-017）**：受理面应答接线裁定 ack-on-accept 后，「就绪应答」不是同步 RPC 回包——拉起请求受理点回执（ack），就绪信号走目录 Advertise（实例就绪后经目录可见即本拍板原文）；M1 延时应答落地也不改变就绪信号路径。

## 5. 数据面与依赖序 ——【拍板 2 已拍板：(b) 内存桩过渡，2026-10-10 / ADR-012】

- **battle feed = internal 域信封消息**（net-abstraction §7「信封与契约合流」：`InternalMessageEnvelope` = sdk-contract §11 id 900+，由生成器产出，不手写第二份协议）；battle 专用通道（36 号适配缝①）随契约批入库——**契约入库跟实现走**（P2-3 纪律：协议随实现入库，不预支）。
- **传输 = M1 自研内核**（§5.6/§5.7 InterServerLink 连接语义）——P3-2 批。依赖序两案：
  - (a) **M1 先行**：offload 批排在 P3-2 后，直接落真传输；
  - (b) **内存桩过渡**：offload 先以同机管道（pipe/shmem SPSC，§7 同机镜像流候选形态）落生命周期与协议骨架，M1 就绪后换底座。
- **裁决**：(b)——生命周期/协议/故障处置的设计风险与传输解耦，先消账；代价：一次底座替换。

## 6. 确定性承载（已交付，无新设计）

跨机实例同 binary 即同确定域（battle-determinism §1 四约束）；ReplayTuple + 事件 hash 链 = 回放/Inspector 缝（25 §6）；实例 tick 与 Zone 主 tick **时间轴独立**（clock-and-time.md：tick 号 = 回放时间轴——副本进程自持 tick 节拍，20-50ms 可配，36 号适配缝②）。

## 7. 故障域 ——【拍板 3 已拍板：(a) 崩溃即作废，2026-10-10 / ADR-012】

实例崩溃检测 = machined 死亡事件（G-1 目录面，已交付）；进程重启 = 监督面退避重启（批 F，已交付）。**战斗态裁决**：
- (a) **崩溃即作废**：团战重开，玩家侧按错误面提示（最简；短生命周期语义下损失窗口小）；
- (b) **关键帧重放恢复**：备机/重生实例从最近关键帧 + 输入重放恢复（25 §6 回放缝的容灾消费；代价：关键帧落盘面 + 重放协议）。

**裁决**：(a) 先行，(b) 留缝（ReplayTuple 已是关键帧素材，协议面后补）。

## 8. 依赖与分期小结

1. **已交付**：确定性运行时（P2-2）/ 长期态+奖励单向口（P2-4）/ machined 目录+发现+监督（P3-1 批 B-F）。
2. **三拍板点已拍板**（2026-10-10 / ADR-012）：§4 按需 spawn 请求协议、§5 内存桩过渡、§7 崩溃即作废。
3. **拍板后路径**：协议消息契约批（internal 域）→ offload 骨架批（生命周期+落回，内存桩过渡）→ M1 就绪后换底座 → manager 域落点接入（Matchmaker 职责归位）。

## 9. 与其余设计的交集

- net-abstraction §7：控制面复用、信封合流、负载同源、同机镜像流候选——**总纲**。
- battle-determinism.md：四约束与回放格式——确定性前提，本文 §6 全盘引用不重复定义。
- battle-verification-service.md：VerifierApp 客户端权威对账——另一条战斗验证线，与 offload 正交（可共呼吸同一 hash 链）。
- session-and-online-directory.md：会话/在线目录 = manager 域另一职责件，RouteResolver 宿主定位供落回路由。
- term-contract §1.3：副本进程行（本设计的命名锚）。
