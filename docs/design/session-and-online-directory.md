# Apollo 会话与在线目录（session-and-online-directory）

> 状态：设计稿（评审中）。定位：**玩家在线状态的权威登记与查询面**——谁在线、在哪条线/哪个 Zone/哪个副本、重复登录与顶号裁决、掉线保活窗口、跨进程玩家寻址的数据源（RouteResolver「宿主定位」职责的供数方）。缺口登记 design-gap-inventory #12（2026-09-30 立项同日落盘）。互引：net-abstraction §7（G-1 编队/manager 域/共享模型四类通道表）、§3（L2 resume）、§5.7（InterServerLink/authority_epoch）、§5.9（login_token）、attribute-sync §8.2（Redis 边界）、sdk-contract §11（internal 域）、scripting-lua §7.5（GM 查询面）、clock-and-time（窗口计时）、capacity-and-benchmark（容量与对账预算）；登录链路全貌（#13）、入站对接（#14）、Bots 压测（#15）为下游消费方。

---

## 执行摘要

1. **在线目录 = manager 域的进程内存权威态，不进 Redis、不落 DB、无 journal**——「全局仲裁态集中不共享」通道族（net-abstraction §7 四类通道表末行）的直接落点：顶号与准入裁决必须单点串行（两个进程各自认领同一账号 = 双权威，违反单写者的进程间推论）；在线态是易失态（进程重启即失效），持久化只有 DB 账号位（最后在线时间，storage.xml 存档列）。容量 trivial（5000 CCU × ~200B ≈ 1MB，capacity §3 口径）。
2. **写路径 = 事件上报 + 周期对账**：登记锚点 = 准入通过、Zone 接受会话实体时（准入前的账号鉴权/token/选服排队归 #13——本设计只定目录的数据面与登记锚点）；增量事件源三——Zone 会话增删、gateway 连接生死（L2 心跳超时 / resume TTL 过期）、manager 自身裁决（顶号/窗口超时）。防漂移 = 周期对账（目录 ↔ 每 Zone 会话集 hash 比对），失配走快照重置——owner-订阅模型的标准恢复路径，不逐条修补。
3. **顶号 = 新顶旧，框架语义，裁决权在 manager**（目录 owner 单点）：新登录命中已有 Online/Suspended 条目 → 同一裁决临界区内旧条目终结、新条目写入 → 异步下发踢除（旧 Zone 销毁/转托管会话实体 + 旧 gateway close(kicked)）。与 resume 不冲突：resume 是**同一会话的连接级恢复**（L2 resume token，net-abstraction §3），不是新登录；窗口内被顶同样成立（Suspended 非免死金牌）。KBE 把裁决留给脚本层（引擎无内建）——apollo 升格为框架语义，理由：顶号与目录/准入强耦合，归框架防每个业务重写一份裁决状态机。
4. **掉线保活窗口（Suspended）与 resume token TTL 同源一个值**——不设两套窗口、两个计时器族：窗口内实体留 Zone（业务可见掉线态，PlayerAnchor `Disconnected` 的目标语义化），窗口满 → 目录删条目 + Zone 侧终结（存档走 attribute-sync §8 write-behind，无特殊处理）。计时 = manager 侧定时器（deadline = tick 号，clock-and-time 口径）——依赖定时器轮组件（architecture-review §23-④ 未动工、C-80 同族，登记随代码批）。
5. **查询面三消费方**：① RouteResolver 宿主定位 = 目录 delta 的**进程内只读镜像**（各进程经 InterServerLink 订阅，seq 续传）+ ServerID 分段先验分流（entity_id 内嵌类型/实例段，36号 #15）+ route_version/authority_epoch 校验兜底（§5.7——镜像过期不致错误路由，调用失败走 re-resolve）；② GM/观测 = control 通道 admin 查 manager（集中真相）；③ 全服广播 = 本地镜像按 zone 分发。高频好友在线走镜像不查 manager。镜像一致性声明：**投影最终一致（秒级），裁决与权威读只在 manager**——读旧镜像的最坏后果 = 一次失败调用 + re-resolve，不破坏单写者。
6. **崩溃恢复 = 全量重报重建**：manager 死 → machined 拉起（G-1）→ 恢复相位排他（manager 域，拒新直至收敛）→ 各 Zone/gateway 全量重报现存会话 → 收敛开放。目录是索引不是权威数据持有者（连接在 gateway、实体在 Zone），重建不丢服务；无 journal——5000 条全量重报 < 1s，重建成本低于 journal 维护成本。
7. **消息面 = internal 域事件族**（sdk-contract §11 id 900+ 分段；**client 域零新增**——域纪律照旧）：`SessionUp/SessionDown/SessionMoved/SessionKicked` 事件族 + `DirectorySnapshotRequest/Reply` 对账族；信封 = InternalMessageEnvelope（net-abstraction §7），invoke_mode：事件 OneWay、对账快照 RequestReply（低频控制面，合规）。被顶客户端的 kicked 原因码进 errors.xml（随代码批，契约文件本轮冻结不动）。
8. **现状衔接（存量不推翻）**：`modules/game/session`（六文件 302 行：PlayerAnchor 六态 + SessionBinding + WorldAssignment + AnchorManager + SessionLocator 双哈希）= **进程内锚点域已存在**，唯一消费方 base-app（base_server.cpp:131-167 bindSession/unbindSession/assignWorld）+ 直测 tests/test_base_anchor.cpp。定位：目录条目字段 = `SessionBinding + WorldAssignment` 的进程间扩展；SessionLocator::bind（session_locator.cpp:9-11 顶掉旧索引）是顶号的进程内雏形（无踢除通知面）；该域现挂 base-app（BW 形态存量 app，现行拓扑无 base 侧——docs/index）——目标落点 = Zone 会话面 + manager 目录面，迁移归代码批（登记，源码冻结不动）。

## 0. 为什么是缺口（负空间与先例）

- **负空间**：design/ 九份 grep「顶号|重复登录|在线状态|online」零命中（gap #12 证据，2026-09-30 实测）；既有件只覆盖「连接在不在」（L2 心跳/resume）与「实体归谁」（ServerID/sceneId），**「玩家在哪个进程上」没有全局登记**。
- **KBE**：引擎无在线目录——在线 = baseapp 实体在内存；分配归 baseappmgr；**重复登录裁决在 assets 脚本层**（引擎无内建语义；assets 仓库本机无，sdk_templates 占位已核，gap #12 证据）。「线」也非引擎一级概念（kbe 引擎源码/配置零命中 `line`——同图多 Space 实例即多线，Spaces 管理在脚本层）。
- **BW**：在线目录**分散在 mgr**——baseappmgr 持 base 分配表 + `(addr, load)` 对上报 loginapp 做登录分流（baseappmgr.cpp:588-599/:1117）；顶号 = baseapp 销毁旧 base 实体族（会话侧自处）。
- **为什么目录不能每 Zone 自管**：寻址要跨 Zone 查询（无全局索引 = O(N) 广播或靠猜）；顶号要全局串行（两个 Zone 各自认领同一账号 = 双权威）。两难一并解于「集中不共享」——这就是本设计归 manager 域的论证。
- **既有件与缺口对照**（防重复建设）：

| 既有件 | 覆盖 | 缺（本设计补） |
|---|---|---|
| manager 最轻分配/准入闸门（net-abstraction §7） | 落点选择、过载拒登 | 登记后的**持续跟踪**与查询面 |
| L2 会话 resume（net-abstraction §3） | 同一连接断线恢复 | **跨进程可见性**（别的进程不知道这条会话） |
| login_token（net-abstraction §5.9） | 一次性入场凭证 | 入场后的**在线事实** |
| ServerID 分段（36号 #15）/sceneId（attribute-sync §4.3） | id 自带来源、场景隔离 | id → **当前宿主进程**的映射 |
| modules/game/session 锚点域（本节 §现状） | 进程内 player↔session↔world | 进程间聚合与裁决 |

## 1. 数据模型

**目录条目**（owner = manager 域，进程内存）：

| 字段 | 类型 | 说明 |
|---|---|---|
| `account_id` | key | 账号域 id（login-app 签发 token 的主体） |
| `player_entity_id` | u64 | ServerID 分段（36号 #15）——RouteResolver 先验分流用 |
| `session_id` | u64 | 连接域 id（§5.9 HKDF info 同源） |
| `gateway_id` | u32 | 接入网关——gateway 死亡反查处置用 |
| `zone_id` | u32 | 宿主 Zone 进程——Zone 死亡反查处置用 |
| `world_assignment` | 结构 | **复用 modules/game/session WorldAssignment**（world_id/map_id/instance_id/space_id/route_version）——所在线/副本的定位四件套 |
| `state` | enum | `Online / Suspended / Leaving`（见状态机） |
| `anchor_epoch` | u64 | 同账号第 N 次会话——**幂等键 + 竞争裁决键**（§3/§4） |
| `deadline_tick` | u64 | Suspended 窗口截止（clock-and-time tick 号） |

**反查索引**：by_entity（寻址）、by_zone / by_gateway（进程死亡批量处置）。全部内存哈希，无落盘。

**条目状态机**：

```
        bindSession(准入通过+Zone接受)          连接死(心跳超时/gateway死)
 (无) ─────────────────────────► Online ─────────────────────────► Suspended
                                    │  ▲                                │
                                    │  └────── resume 成功 ─────────────┘
                                    │（同一 session_id + anchor_epoch）
              显式登出/迁移           │              窗口满(deadline_tick)
        Online ──────────► Leaving ─┴─► Removed ◄───────────────────────┘
        任意态 + 新登录命中 ──────────► Removed(旧, kicked) + 新条目（§3 顶号）
```

- `Leaving` = 显式登出/跨 Zone handoff 的中间态（§7 与 todo 批次 3 显式 handoff 衔接）；`Suspended` = 掉线保活窗口（§4）。

## 2. Owner 与写路径

- **owner = manager 域**（与准入闸门、恢复相位同进程同域——裁决串行点天然成立；现行拓扑即 docs/index 的 World/manager）。
- **三个事件源**：① Zone 会话增删（SessionUp/SessionDown，实体创建/销毁时上报）；② gateway 连接生死（L2 心跳超时、resume TTL 过期、gateway 进程死亡事件——G-1 编队事件）；③ manager 自身裁决（顶号踢除、窗口超时）。
- **登记锚点（时序）**：准入通过（manager 定落点 Zone，最轻分配）→ 下发 Zone → Zone 创建会话实体 → `SessionUp` 上报 → **目录此刻起可见**。准入失败/Zone 拒收 → 无条目（不留幽灵）。边界：准入之前的链路（账号鉴权/token 签发/选服/排队）归 #13。
- **对账（防漂移）**：周期（默认 30s，P3 校准）manager ↔ 每 Zone 会话集 hash 比对；失配 → 该 Zone 走快照重置（DirectorySnapshotRequest/Reply 全量对表）——与 net-abstraction §7「同步三层含义」一致：事件丢 = 传输层续传的事，对账失配 = 状态层快照重置的事，互不兜底。
- **幂等与乱序**：事件携带 `(account_id, anchor_epoch)`；旧 epoch 迟到事件丢弃；重放无副作用（set 语义天然幂等，attribute-sync §8.2 同论证）。

## 3. 顶号与重复登录（框架语义）

**裁决：新顶旧。** 同账号新登录（全新 ClientHello + 新 login_token，非 resume）命中目录已有条目（Online/Suspended/Leaving 皆然）：

1. manager 在**同一裁决临界区**（串行 tick 处理，无锁竞争——集中不共享的收益）：旧条目 → Removed、新条目写入（新 anchor_epoch）。
2. 异步下发踢除：旧 Zone 收 `SessionKicked`（销毁会话实体或转托管——业务策略，框架给事件不给策略）；旧 gateway 收 close(kicked)。
3. 竞态窗口：旧客户端的 resume/上行在踢除指令在途时后至——验证 `anchor_epoch` 失效即拒（token 与 epoch 双锚，§5.9 的 session_id 域再绑一层 epoch）。
4. 客户端契约：被顶端收 kicked 原因码（errors.xml 登记，随代码批）。

- **为什么新顶旧**：主动重新登录 = 玩家明确意图；与 resume 分工干净（resume = 同会话恢复，顶号 = 新会话替代）；「旧踢新」「排队」把裁决复杂度转嫁给业务且都要全局串行点，收益为零。KBE 脚本层默认行为同型（销毁旧 avatar 建 new），BW baseapp 同族。
- **为什么归框架不归脚本**：顶号裁决与目录/准入/epoch 强耦合，归框架（本设计）防每个业务重写一份裁决状态机——与 KBE「引擎不管」的差异是刻意加强项（GM 面 scripting-lua §7.5 同型论证）。

## 4. 掉线保活窗口（Suspended）

- **窗口与 resume token TTL 同源一个值**（一个配置项、一个计时器族）——两套窗口必漂移，漂移即「token 有效但实体已销毁」或反之。默认建议 30s（P3 容量批校准；resume TTL 数值本设计不定，net-abstraction §3 既有口径）。
- 窗口内：实体留 Zone（业务可见掉线态——`AnchorState::Disconnected` 的目标语义），目录 Suspended；**resume 成功 → Online（目录只改状态列，不动实体）**；窗口满（`deadline_tick` 到）→ 目录删条目 + Zone 收 SessionDown（终结/转托管业务定，存档走 write-behind 无特殊处理）。
- 计时归属：manager 侧定时器（deadline = tick 号）；**依赖定时器轮组件**（architecture-review §23-④ 未动工、C-80 登记同批）——代码批依赖，非设计缺口。

## 5. 查询面（三消费方）

| 消费方 | 形态 | 路径 |
|---|---|---|
| **RouteResolver 宿主定位**（remote-entity-call-design 四件套之一，其数据源即本设计） | 进程内**只读镜像**（订阅目录 delta，InterServerLink 投影 + seq 续传） | 先 ServerID 分段先验分流（id 内嵌类型/实例段）→ 镜像查 zone_id → route_version/authority_epoch 校验（§5.7）→ 失配/未命中 re-resolve（RequestReply 低频控制面） |
| **GM/观测查询**（scripting-lua §7.5 / observability） | 集中真相 | control 通道 admin → manager：在线列表/单玩家轨迹/按 Zone 统计 |
| **全服广播/好友在线** | 镜像投影 | 业务订阅 SessionUp/Down 流（events 语义）；广播按镜像 zone 分发——不扫 manager |

- **一致性声明**：镜像 = 投影（最终一致，秒级）；**裁决与权威读只在 manager**。读旧镜像最坏后果 = 一次失败调用 + re-resolve——与 RO_MIRROR 同哲学（net-abstraction §7：共享 = 订阅 owner 的投影）。

## 6. 崩溃恢复（按死者角色）

| 死者 | 处置 | 依据 |
|---|---|---|
| **manager** | machined 拉起（G-1）→ 恢复相位排他（拒新）→ 各 Zone/gateway **全量重报**现存会话 → 收敛开放。目录无 journal（易失索引，重建 < 1s @ 5000 条） | manager 域恢复相位（net-abstraction §7）；「两层恢复独立成立」（客户端 resume 与目录重建谁先完成谁先服务） |
| **gateway** | 该 `gateway_id` 反查 → 全部条目转 Suspended（走 §4 窗口）；客户端重连（任意 gateway 实例）→ resume（token 绑 manager 域会话语义，不绑 gateway 实例）或新登录 | L2 resume（net-abstraction §3） |
| **Zone** | 该 `zone_id` 反查 → 条目依 §7 恢复相位处置（reviver/重建）；重建完成 SessionUp 重报；窗口内未恢复的会话按掉线超时走 | G-2/恢复相位（net-abstraction §7） |

## 7. 与 Redis/DB 的边界

- **不进 Redis**：目录是**仲裁态**不是共享热数据（attribute-sync §8.2 定位：Redis 只做后者）；顶号/准入要单点串行，Redis 共享表把裁决拆成分布式竞争——双写一致性成本 > 收益，且违反「集中不共享」（net-abstraction §7 四类通道表）。好友在线的高频读由镜像承接（§5），不需要 Redis。
- **DB 面**：在线态不落库；账号位（最后在线时间/最后 Zone）= 存档列提升（`column: true`，storage.xml——归 #13/批次 2 语句集）。
- **journal**：目录无 journal（§6 重建论证）。

## 8. 消息面（internal 域事件族）

| 消息 | 方向 | invoke_mode | 载荷 |
|---|---|---|---|
| `SessionUp` | Zone → manager | OneWay | account_id/player_entity_id/session_id/gateway_id/zone_id/world_assignment/anchor_epoch |
| `SessionDown` | Zone → manager | OneWay | account_id/anchor_epoch/reason |
| `SessionMoved` | Zone → manager | OneWay | account_id/anchor_epoch/new world_assignment（跨 Zone handoff/todo 批次 3） |
| `SessionKicked` | manager → Zone | OneWay | account_id/old anchor_epoch/reason |
| `DirectorySnapshotRequest/Reply` | manager ↔ Zone | RequestReply | 对账失配时的全量会话集（低频控制面） |

- 信封 = `InternalMessageEnvelope`（net-abstraction §7）；id 进 sdk-contract §11 internal 域分段（900+）——**随代码批进契约**（契约文件冻结，本轮只设计）；client 域零新增（域纪律）；跨域禁令照旧。
- **下行通知（踢除 close 帧）**走客户端 control 通道（net-abstraction §4.1 绝不丢语义）。

## 9. 分期落地

- **P2（单进程/Compact）**：目录退化 = manager 域进程内表——`modules/game/session` SessionLocator 的扩展消费（现存 mutex 组件归 owning 线程化，随代码批）；顶号在进程内即闭环；docs/30 Compact 形态同（manager 内嵌）。**此形态零新增进程、零新增协议**。
- **P3（多进程全量）**：InterServerLink 投影/恢复相位/对账/事件族进契约。
- **依赖**：G-1 编队事件（gateway/Zone 死亡检测）、#13 登录链路（上游）、定时器轮组件（窗口计时——architecture-review §23-④/C-80 同批）、#15 Bots（验收对象：顶号风暴/断线重连压测互为验收）。
- **验收建议**：① 顶号并发（同账号 N 端并发登录）零双权威残留（目录终态唯一条目）；② manager kill -9 → 目录收敛 < 5s 且期间零错路由（镜像 re-resolve 兜底可证）；③ 镜像路由失配率（capacity 指标族新增项）。

## 10. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| net-abstraction | §7 共享模型四类通道表「会话」行与「全局仲裁态」行的落地件；§3 resume = 窗口同源；§5.7 epoch = 竞争裁决兜底；§5.9 login_token = 入场凭证（本设计管入场之后）；manager 域 = 目录 owner |
| attribute-sync | §8.2 Redis 边界（目录不进）；§8 write-behind（窗口满存档）；§4.3 sceneId（world_assignment 定位粒度） |
| sdk-contract | §11 internal 域事件族 + errors.xml kicked 码（随代码批）；client 域零新增 |
| scripting-lua | §7.5 GM 在线查询消费本目录（admin 单入口 → control → manager）；好友在线走镜像订阅 |
| clock-and-time | 窗口 deadline = tick 号；跨进程比较只用 epoch/seq（anchor_epoch 遵守） |
| capacity-and-benchmark | 目录容量（~1MB trivial）；对账/全量重报进帧预算（恢复相位分批）；镜像失配率指标 |
| architecture-review | 存量 modules/game/session 定位（§现状——迁移登记代码影响项）；remote-entity-call-design「缺口③宿主定位无数据源」由本设计闭环 |
| design-gap-inventory | #12 CLOSED；#13/#14/#15 的前置（登录链路/入站/Bots 均消费目录） |

---

*基线：apollo main @ 1e701d51（文档态；源码冻结未动）。存量实读（2026-09-30）：modules/game/session 六文件 302 行（player_anchor.hpp:12-30 AnchorState/SessionBinding、world_assignment.hpp:7-17、session_locator.cpp:5-66 含 :9-11 顶号雏形）+ apps/base-app/src/base_server.cpp:106-167（唯一消费方 bindSession/unbindSession/assignWorld）+ tests/test_base_anchor.cpp（100 行直测）；KBE/BW 先例证据 = gap #12 登记（baseappmgr.cpp:588-599/:1117、KBE 脚本层顶号/assets 不在本机已核）。设计裁决八条见执行摘要；窗口默认值与对账周期为初始建议，P3 容量批校准。*
