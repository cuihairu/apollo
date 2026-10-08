# Gateway 必要性与前端连接层拓扑调研（gateway-topology-survey）

> 状态：研究件（2026-10-08）。定位：回答「apollo 要不要独立 gateway-app（连接面进程）」——有/无两种拓扑的职责对照、九框架先例取舍、apollo 取路建议。**建议件不代拍**：gateway-app 的落地节奏与形态定案挂 net M1 拍板链（improvement-plan P3-4 批记），本件只供「要不要」的论据，「何时/怎么」不裁。证据分级沿用 docs/36 §0：A=本地源码/官方文档原文实读（本轮带路径行号或 URL），B=本仓文档，C=官方文档转述，D=社区存疑。与 docs/36 分工：36 号是十对象八面深挖（A 级本地源码实读），本件只取「前端连接层 vs 后端 RPC」一轴——五家复用 36 号已证事实并补本轮实读引注，Pitaya/Colyseus/Nakama/Orleans 四家为首采。

---

## 0. 结论先行

**apollo 留独立 gateway-app。** 后端互联不学 Pomelo/Pitaya 的消息中间件全 RPC（nats），照既定口径自行开发进程间总线（architecture-review §15.2「禁第二套 IPC」+ 2026-09-30 用户裁决不引 etcd/consul，net-abstraction §7 G-1 已落档）。

留的四条理由（§1 逐条展开）：

1. **连接稳定域与场景计算域分离**——Zone 重建（§7 重建口径）/换幕/迁移期间客户端 TCP 不断，session-and-online-directory §6 gateway 行的 resume 语义（token 绑 manager 域会话、不绑 gateway 实例、不绑 Zone）以「有一个不随 Zone 生死的连接终点」为物理前提；
2. **公网面与风控单点**——token 验签、上行限流、violation_score、加密握手集中一处（login-flow §1 已裁 gateway 不触账号库、不做准入），Zone 进程走内网，公网暴露面收敛到一个专职进程；
3. **协议族收口**——wss 兜底形态归 gateway（net-abstraction §5.9：TCP+CryptoFilter 主路径 / wss 浏览器兜底，「P3 随 gateway-app 定案」），Zone 只讲一种内部协议；
4. **场景单写者纪律**——Zone 的帧预算不被会话层 IO 热点侵占（B 模型实测 scene 局部 5.90ms/100ms 帧，capacity-and-benchmark §2.1；多景并行化后占用上升，余量要留给场景面）。

代价如实记（§1.3 反面证据一并记）：一次内部转发跳（同机零成本、跨机一次 NIC 往返）+ 每字节经 gateway 转发的带宽份额 + 一个专职进程的 net 环内存预算（capacity §4：每会话 64KB×2×5000 ≈ 640MB 量级）——换取的是上述四条。

## 1. gateway 必要性验证

### 1.1 有/无两种拓扑的职责对照

| 职责 | 有 gateway（apollo 现架构） | 直连 Zone（替代形态=KBE baseapp 型） |
|---|---|---|
| 连接保持/L2 会话（seq/ack/心跳/resume/水位，net-abstraction §3） | gateway 集中持有全套（term-contract.md:47「四通道会话」） | 每 Zone 进程自带全套——L2 与场景线程同进程（IO 线程/场景线程分离在进程内仍成立，但热点抢核） |
| 认证 | token 本地验签（login-flow §3——gateway 持 K_login 验证密钥，不触账号库） | 每 Zone 持验证密钥；或 Zone 回问 manager（存量 gateway 的错位实现先例，login-flow §10 已判迁移） |
| 路由 | 按 zone 透传（term-contract.md:47「按 zone 透传」；ResolveRoute 问 manager 定位） | 不需要跨进程路由——但玩家换 Zone（换幕跨进程/副本重开）= 客户端重连 + 全量握手 + 快照重传（attribute-sync §11 快照上限 50KB） |
| 分发/广播 | gateway 扇出下行 | Zone 直发（少一跳，同机部署下扇出成本更低） |
| 加密/协议族 | CryptoFilter 主路径 + wss 兜底，收口一处（§5.9） | 每 Zone 双协议栈（TCP 自定义 + wss），或砍掉 wss 牺牲页端 |
| 故障域——连接进程死 | gateway 死 → §6 gateway 行：该 gateway_id 反查全部条目转 Suspended（§4 窗口），客户端重连**任意** gateway 实例 resume | 等价物不存在——入口即权威进程，权威死=入口死 |
| 故障域——场景进程死 | Zone 死 → §6 Zone 行窗口处置，**客户端连接不断**（gateway 保持 TCP，Zone 重建后 Session 重挂 Anchor，object-model.md:129 对象轴） | Zone 死 = TCP 全断；重连风暴落 Zone 自己——而 §7 重建完成前 Zone 无法应答 resume 握手，窗口期客户端无处落脚 |
| 扩缩容 | gateway 集群增减实例，客户端无感（resume 不绑实例） | Zone 增减 = 其上全部客户端重连 |

对照表的核心差异一条：**直连拓扑里「入口进程」与「权威进程」是同一个，于是权威的每一次重建/迁移都变成连接事件**。有 gateway 拓扑把两件事拆开——这正是 §6 把死亡行按死者角色分两行（gateway 行/Zone 行）的前提；没有 gateway，§6 只剩一行且恢复语义退化为「客户端重连风暴 + 重连风暴本身压垮正在重建的 Zone」。

### 1.2 apollo 场景数据论证

用 capacity-and-benchmark 既定口径的数字过一遍两种拓扑：

- **连接规模与内存**：5000 CCU/进程（capacity §1）。有 gateway：gateway 集中 5000 条会话 ≈ 640MB net 环预算（§4 表）+ 专职 IO 线程——一个进程的容量份额。直连：摊到 N 个 Zone，每 Zone ~5000/N 条——单看数字直连更省「一个进程」。但 Zone 进程的部署粒度受场景负载驱动（B 模型 10 场景进程族），Zone 数量随玩法扩缩，客户端连接分布跟着玩法走——**洪峰容量的规划单位从「连接面」变成「玩法面」，login-app 洪峰（重连风暴，login-flow §9 容量项）无处隔离**。
- **下行带宽**：全服出口 160MB/s 打满 ≈ 1Gbps（capacity §3）。有 gateway：zone→gateway 段与 gateway→client 段各摊一份，跨机部署时两段在不同 NIC 上；直连：一段。**这是直连的真实带宽优势**——但同机部署（编队常见形态）下 gateway↔zone 走 lo/本机 NIC，代价趋零；跨机部署时代价 = 每字节一次额外内网传输，内网带宽远大于公网出口，瓶颈段不变。
- **事件放大**：B 模型 ~19543 广播事件/帧 ≈ 195k 事件/s（capacity §2.1）。这个量级的扇出面无论落在 gateway 还是 Zone 都是同一个 cost center（attribute-sync per-viewer 预算在 collect/flush 段已定），gateway 不放大也不缩小——透传不重组（term-contract.md:47）。
- **恢复期落脚点**：§7 口径目录无 journal 易失索引重建 <1s@5000 条——但这是 manager 侧；Zone 重建（场景重开 + Anchor 从 journal/DB 重放）是秒级。窗口内 resume 握手需要一个活着应答的端点：有 gateway = gateway 应答（Suspended 条目 + 重挂排队）；直连 = 无端点可应答。**这一条不依赖任何性能假设，是结构性的。**
- **场景线程纪律**：net-abstraction §3 的线程模型（IO 线程搬运字节、场景线程单写者）在两种拓扑下都成立；差异是抢核面——直连把 5000/N 条会话的加密/解帧/限流 CPU 放进 Zone 进程，与 simulate/recalc/collect（§2 帧预算表 ~55ms/100ms 已分配）共享 CPU 预算。当前桩口径下余量大（B 模型 5.9%），但余量的规划用途是玩法复杂化与多景并行（capacity §2.1 结论方向），不是会话层热点。

结论：**留**。四条结构性理由（§0）无一依赖性能数字；性能账上有得有失、跨机部署才有实质带宽代价，恢复期落脚点与故障域拆分是决定性权重。

### 1.3 反面证据如实记（直连派不是输家）

九框架里直连/一体派占多数（BW/KBE/TrinityCore/Nakama/Colyseus/Orleans，§2），先例数量本身是对称的证据，必须交代 apollo 为什么站在少数侧：

- **BW/KBE 的 baseapp 是「入口+实体宿主」一体**——proxy 实体驻留 baseapp，客户端 channel 本来就落它，权威拆出去的 cellapp 从不见客户端。一体不是它们的妥协，是它们的本质形态。apollo 把实体权威拆给 Zone（场景单写者 + Home Zone 驻 PlayerAnchor，object-model.md:129）之后，入口与权威天然两件——**gateway 不是「额外加的进程」，是拆分的另一半**。
- **Orleans 官方推荐 co-hosting 免网关**（learn 官方 client 页本轮实读：co-hosted client "doesn't need a separate gateway. This avoids a network hop and a serialization/deserialization round trip"）——但同一页给出成立前提："Client code is often very *thin* (e.g., translating incoming HTTP requests into grain calls)"。游戏客户端连接不是 thin 转发：L2 四通道 + resume + 水位 + 限流是全套有状态会话层，thin 论证不成立。Orleans 的外部客户端形态（frontend web 服务器做 "connector or conduit"）恰好说明：**连接面重的时候，它就要成为专职的东西**。
- **skynet 证明「可以没有强制拓扑」**——gate 是可选服务，单节点可以完全不用。这是库与框架的定位差（skynet 给原语不给拓扑），不构成对 apollo 拓扑的反驳；apollo 的四档目标（轻在线/房间制/普通 MMO/BigWorld 增强态，kbe-reference-principles §2）要求拓扑有约定，不是要求每个档位都长一样。

### 1.4 结论与角色收窄

留 gateway-app，且角色按既有设计收窄——只做连接面三件：token 验签、四通道会话承载、按 zone 透传（term-contract.md:47）；不做准入（login-flow §1 已裁：准入归 manager、鉴权归 login-app）、不做业务逻辑、不做文件服务（契约包指针制，login-flow §6）。gateway 越薄，§1.1 对照表的直连派「少一跳」论据越有说服力时 gateway 越无害——薄透传的一跳是转发成本不是逻辑成本。

## 2. 九框架对照：前端连接层 vs 后端 RPC

### 2.1 总表

| 框架 | 语言/血统 | 前端连接层 | 后端互联/RPC | 服务发现 | 证据级 |
|---|---|---|---|---|---|
| BigWorld | C++（开源 14.4.1） | **无独立网关**——loginapp 短连接指派 baseapp，客户端直连 baseapp（入口+proxy 实体宿主一体） | Mercury 自研（TCP/UDP 双模 + filter 双族 + 窗口重传） | bwmachined 守护 + machine_guard 协议 | A（本地源码，docs/36 §2.1 + 本轮 server 目录清单） |
| KBEngine | C++ | loginapp（独立登录进程，含 SDK 下发）→baseapp（入口+实体宿主一体） | 自研 bundle/channel + EntityCall | machine 进程 UDP 广播应答 | A（loginapp.cpp:1185-1186 本轮实读 + docs/36 §2.2） |
| skynet | C + Lua | gate service（可选，拓扑自组）| cluster.call（节点间 RPC，节点表手工配置） | 无自动发现（cluster.conf 手工） | A（service/gate.lua、lualib/skynet/cluster.lua 本轮实读） |
| Pomelo | Node.js（2012–2023.09 归档） | **connector 专职前端**（hybridconnector/sioconnector） | 自研 RPC：proxy + remote + mailbox/mail station + master 事件 | master server（订阅 server changing events） | C（官方 wiki 本轮实读） |
| Pitaya | Go（Pomelo 血统后继） | **connector 专职前端**（frontend） | **nats 为主 RPC 传输**（cluster 示例），另有 gRPC 形态（cluster_grpc 示例） | etcd | C（官方 README 本轮实读） |
| TrinityCore | C++（WoW 模拟器） | **无网关**——authserver（登录）+worldserver（游戏直连）两进程 | 无后端互联（单世界单体） | 无 | A（3.3.5 源码树本轮实读） |
| Colyseus | Node.js/TS | **room 进程即前后端一体**；matchmaking 后直连 host 进程；NGINX 动态端口代理 | 无进程间 RPC（room 之间不通信） | Redis（RedisPresence + RedisDriver，matchmaking registry） | C（官方 scalability 页本轮实读） |
| Nakama | Go | **无独立网关**——单体服务器终结 socket（WebSockets/rUDP）+ 管理面 gRPC/HTTP | 单体内子系统；企业版集群 gossip+CRDT + 跨节点消息路由 | 内建（企业版） | C（官方 architecture 页本轮实读） |
| Orleans | .NET | **无专职网关进程**——外部客户端连 silo 端点（多网关列表按成员表刷新）；官方推荐 co-hosting 完全免网关 | 自研 silo 间消息（TCP + grain directory） | 成员表（membership table） | A（官方 client 页 + GatewayOptions 本轮实读） |

### 2.2 逐家：形态 / 为什么 / 适用 / 不适用

**BigWorld**（A 级，docs/36 §2.1 已证 + 本轮 `programming/bigworld/server/` 目录清单复核）

- 形态：loginapp 只做登录与指派（baseappmgr `minAppLoad()` 取最轻——baseappmgr.cpp:588-599，loginapp :1117 消费 (addr, load) 分流，docs/36 已证），客户端 thereafter 直连 baseapp；server 目录族 loginapp/baseapp/cellapp/baseappmgr/cellappmgr/dbapp(mgr)/reviver，无 gateway 进程。
- 为什么：baseapp 本就是 proxy 实体宿主与客户端 channel 端点——入口与权威一体，拆分反而多一跳；Mercury 让全部进程共一套网络语义（net-abstraction §5.7「服务器代码全员直用 Mercury」）。
- 适用：无缝大世界 MMO（cell 无缝迁移把「连接跟着实体走」全部交给 baseapp proxy 段消化）。
- 不适用：入口进程要独立扩缩/独立故障域的形态——baseapp 重启 = 其上全部客户端重连（BW 用热备/接管缓解，G-2 全套先例，apollo P3 骨架取两件）。

**KBEngine**（A 级）

- 形态：loginapp 独立登录进程（鉴权 + clientsdk_downloader SDK 下发，login-flow §0 已引）；登录成功把 baseapp 地址发给客户端——`loginapp.cpp:1185-1186`（本轮实读）：「客户端得到baseapp地址的同时也会返回这个账号名称 / 客户端登陆baseapp应该使用这个账号名称登陆」；此后客户端直连 baseapp，cellapp 不见客户端（baseapp 目录含 entity/forwarder/backuper/data_download 全套——proxy+持久+分发一体，本轮目录清单）。
- 为什么：与 BW 同构的入口-权威一体（KBE 即 BW 血统简化）；loginapp 独立只解决「账号域与游戏域分离」（apollo login-flow 同判）。
- 适用：MMO 中量级；KBE 无进程级容灾（architecture-review C-51）——直连形态下连接恢复全靠客户端。
- 不适用：需要连接面独立恢复语义的形态（KBE 没有 §6 gateway 行的对应物；apollo §6 两行语义是 KBE 直连形态没有的增量）。

**skynet**（A 级）

- 形态：gate 是可选服务（`service/gate.lua` 本轮实读：`connection = {} -- fd -> connection` 表 + `skynet.register_protocol { name = "client", id = PTYPE_CLIENT }`——gate 持连接、把客户端消息按 PTYPE_CLIENT 投给 agent 服务）；节点间 cluster.call（`lualib/skynet/cluster.lua` 本轮实读：clusterd per-node sender）；节点表手工配置，无自动发现。
- 为什么：skynet 是库不是框架——只给原语（gate/cluster/watchdog），拓扑由用户拼；单节点可完全不用 gate，多节点 cluster 才登场。
- 适用：有自组拓扑能力的团队；卡牌/SLG/休闲等中低连接密度；skynet debug_console/monitor 是 apollo P3 观测工具先例（net-abstraction §7 已引）。
- 不适用：要开箱即用编队/发现/恢复语义的团队——machined 式监督、死亡通知、恢复相位全要自长（apollo G-1 取 BW 守护先例正是补这个）。

**Pomelo**（C 级，官方 wiki 本轮实读）

- 形态：connector 专职前端 + backend 服务器池 + master 管理进程。RPC 自研三层（官方 wiki「RPC-Framework」页原文，本轮实读）：proxy component（"builds client stubs and mounts them on `app.rpc`"）+ remote component（服务端装载）；mailbox（"a mail box corresponds to a remote server"，懒连接——"establish the connection to remote server when the client first initiates an RPC invocation"）+ mail station（"maintains all the mail box instances for current server"按 id 转发）；proxy 启动即订阅 master（"subscribe to master server for server changing events"）。connector 自定义面（官方 wiki「Understanding-Connector」页，本轮实读）：hybridconnector/sioconnector 两实现、listen/kick/encode/decode 契约。
- 为什么：Node.js 单线程——「保持万级连接」与「跑房间逻辑」两种事件循环负载装不进一个进程，多进程必然而前端必须专职。
- 定位自述（README，本轮实读）："mobile, social, web, MMO rpg(middle size)"——middle size 是官方自我限定。
- 教训注记：2023-09-25 归档（repo banner，本轮实读）。前后端拆分 + 自研 RPC + master 单点的运维面是中小团队的负担；后继者 Pitaya 换 Go + 外部中间件（§2.3）。

**TrinityCore**（A 级）

- 形态：两进程——authserver（登录/账号）+ worldserver（游戏世界，客户端直连）；`src/server/{authserver, worldserver, database, game, scripts, shared}`（3.3.5 源码树，本轮实读）；无后端互联——单世界单体，进程内 packet 处理。
- 为什么：模拟器传统——世界服单进程权威，登录与世界分进程只因协议不同（SRP6 vs 游戏包），不是拓扑拆分。
- 适用：单世界 MMO 模拟器（数百-数千 CCU/世界）；多 realm = 多套完整进程组（authserver 可共享）。
- 不适用：多进程水平扩展——TC 没有「一个世界拆多进程」的形态，横向扩容=复制整套。

**Colyseus**（C 级，官方 docs 本轮实读）

- 形态：room 进程即前后端一体——每个 Node 进程跑 N 个 room，matchmaking 拿座位预留后客户端直连 host 进程；跨进程扩缩靠 Redis（官方 scalability 页原文："Redis is required for scaling"；RedisPresence + RedisDriver pub/sub）；NGINX 按端口动态代理（`location` 正则取端口号 → `proxy_pass http://127.0.0.1:$PORT`）或 Traefik 服务发现。
- 为什么：房间制的极端形态——room 就是运行与隔离单位，房间之间不通信，进程间 RPC 问题整个不存在；代理与 Redis 只解决「找到房间的进程」。
- 适用：休闲/回合/小房间实时（官方 repo 自述 "realtime and turn-based games, matchmaking"，本轮实读）；每房间人数小、房间间强隔离。
- 不适用：跨房间交互（世界聊天/大厅/跨房玩法要自搭）、大 AOI 高频同步（Node 单线程 + 无 per-viewer 预算体系）。

**Nakama**（C 级，官方 docs 本轮实读）

- 形态：单体——官方 architecture 页原文："a monolithic stateful server that exposes real-time and non-real-time APIs from multiple subsystems"；socket 面 "runs on WebSockets and rUDP"（协议缓冲或 JSON），管理面 "runs on gRPC and HTTP"；runtime 模块（Go/Lua/TypeScript）进程内跑 authoritative match handler；企业版集群 gossip + CRDT 服务发现 + "tracks the whole cluster's set of socket connections to clients and routes incoming messages to the right nodes"。
- 为什么：BaaS 形态——鉴权/存储/排行/聊天是通用子系统，单二进制运维最简；实时对战作为子系统内嵌而非独立拓扑层。
- 适用：中小型通用后端（休闲/社交/回合）+ 中低频实时；对外不用拼 topology。
- 不适用：帧级实时场景同步——单线程 per-match handler、无 AOI/per-viewer 体系、消息路由按节点而非按会话通道（与四通道 QoS 需求错位）。

**Orleans**（A 级，官方 docs 本轮实读）

- 形态：grain 虚拟 actor 集群；外部客户端连 silo 端点——GatewayOptions 的 "list of active gateways"（"The list of active gateways will be refreshed every minute by default"）按成员表刷新，客户端在多网关列表上择一；官方**推荐 co-hosting**（client 与 silo 同进程）："doesn't need a separate gateway. This avoids a network hop and a serialization/deserialization round trip"；外部客户端形态 = frontend web 服务器 "acts as a connector or conduit to the cluster"。
- 为什么：云原生设计——连接面被假定为 thin（HTTP 请求转 grain 调用），游戏逻辑全在 grain；网关从「专职角色」退化为「任意 silo 的一个端点角色」（Orleans 1.x 专职网关节点属历史形态——本轮未获可引原文，C/D 级演化注记，不作论据）。
- 适用：回合制/策略/大厅服务——grain 粒度状态、请求-响应为主、能接受 observer best-effort 单向通知。
- 不适用：高频下行广播流——observer "sent as one-way, best-effort messages"（官方原文），无 QoS 通道/背压/水位体系；游戏会话层需求与 grain 调用语义错位。

### 2.3 Pitaya 特写（点名项）：后端全 RPC 的取舍

**架构事实**（官方 README，本轮实读）：Go；connector 为 frontend、room 为 backend（官方示例 "a frontend connector and a backend room"）；服务发现 etcd（"(optional, used for service discovery)"）；RPC 走 nats（"(optional, used for sending and receiving rpc)"），另有 gRPC 形态（cluster_grpc 示例并列——**README 未用 "default" 一词，nats 为主传输的口径按官方示例组织与社区通行说法，C 级如实注记**）；血统：致谢 pomelo "inspiration on the distributed design and protocol"、nano "the framework pitaya is based on"；自述定位 "an simple, fast and lightweight game server framework with clustering support… for distributed multiplayer games and server-side applications"。

**后端全 RPC 的取舍**（本轮分析，标注为分析非引用）：

- 收益：
  1. 后端进程零连接态——任意伸缩、滚动发布客户端不断线；会话状态在 connector 或可序列化到后端，故障半径小；
  2. 前端薄——connector 纯转发可复用，业务迭代不碰连接层；
  3. 服务间调用统一为 RPC 语义（含 server-to-server），业务不感知拓扑；「谁在哪」从路由问题变成 etcd 查询问题；
  4. nats 把 fanout/订阅语义白拿——广播类消息（聊天/匹配通知）由中间件承担。
- 代价：
  1. 每条客户端消息两跳以上（client→connector→nats→backend→原路返回），每段序列化——movement 类高频小帧的 CPU/延迟税最重；
  2. 两个外部件的部署/监控/故障域（nats + etcd）——中小团队运维面显著扩大（Pomelo 归档教训的镜像：换了一组中间件，没有消掉运维面）；
  3. **语义错位是根本的一条**：消息中间件的交付语义（subject 粒度、at-least-once、无 per-session seq/ack/水位）与游戏会话语义（per-channel 可靠性分级、重连续传、背压）不同层——四通道那套仍要自己长，nats 只解决了传输不解决会话；
  4. 恢复语义薄：connector 重启 = 其上全部连接重建，etcd 感知到的是服务上下线而不是「会话如何恢复」。
- 适合：房间制/休闲/社交/中量级 MMO 的房间-大厅-聊天-匹配面——消息频率中低（人均条/秒级）、会话状态可丢可重建、内部跳延迟容忍 10ms 级；TFG 血统即手游社交对战（与 Pomelo 自述 middle size 同源）。
- 不适合：AOI 密集广播（apollo B 模型 195k 事件/s/进程的量级，经 nats 转发的吞吐与语义都不合）、确定性帧同步（battle-determinism 域）、无缝世界实体迁移。
- **与 apollo 纪律对照**：apollo 2026-09-30 用户裁决不引 etcd/consul（游戏服拓扑小、变更低频，machined 守护 + UDP 广播最终一致够用——net-abstraction §7 G-1 落档）；nats 同判——进程间总线自行开发（architecture-review §15.2 禁第二套 IPC；§5.6 M1 演进阶梯）。Pitaya 的外部件成本恰是裁决理由的实证镜像：它把「自研 RPC」的复杂度换成了「中间件运维」的复杂度，apollo 的裁决是第三条路——自研总线、语义按 §5.2 吸收清单抄 Aeron/Mercury，不引外部件也不重造消息队列。

## 3. 归纳：前端连接层三形态 × 后端互联两族

**前端连接层三种形态**：

| 形态 | 成员 | 本质 |
|---|---|---|
| ①专职前端进程（拆分派） | Pomelo connector、Pitaya connector、apollo gateway-app | 连接面与权威面拆开——权威重建/迁移不撕连接 |
| ②入口+权威一体（一体派） | BW baseapp、KBE baseapp、Nakama 单体、Colyseus room | 权威进程兼连接面——少一跳，代价是权威事件=连接事件 |
| ③无前端概念（极简派） | TrinityCore worldserver、skynet 自组 | 拓扑留给用户/单体到底 |

（Orleans 跨②③：co-host 推荐=②，外部客户端多网关列表=①的弱化形态。）

**规律一条**：前端形态与「权威拆没拆」强相关——BW/KBE 权威在 baseapp 所以入口在 baseapp；apollo 权威在 Zone（场景单写者）所以入口独立；Colyseus/Nakama 权威即进程所以没有第二进程可拆。**不是先例选了拓扑，是权威布局决定了拓扑**——这也是 §1.4 角色收窄论证的底部逻辑。

**后端互联两族**：

| 族 | 成员 | 判据 |
|---|---|---|
| 自研 RPC | BW Mercury、KBE、skynet cluster、Pomelo、Orleans | 游戏服主流——要 mailbox/EntityCall 语义与热路径开销控制 |
| 消息中间件 | Pitaya（nats） | 通用分布式取舍——把发现/订阅/fanout 外包，代价是语义错位与运维面 |

gRPC 类只出现在非热路径（Nakama 管理面、Pitaya 可选形态）——热路径无一例外自研或中间件，没有 gRPC 直进游戏热路径的先例。

## 4. apollo 取哪条路 + 理由（建议件，不代拍）

**取：独立 gateway-app（连接面）+ Zone 不见客户端 + 自研进程间总线（net M1）+ login-app 独立登录短连接（login-flow 已裁）。**

- **为什么是①拆分派而不是②一体派**：apollo 的权威布局已定（object-model.md:129 对象轴 Connection（gateway）→ Session（可失可重挂）→ PlayerAnchor（Home Zone，不动））——Session owner 就是 gateway，Anchor 迁移语义（不迁移）与换幕销毁重建都以「连接终点 ≠ 权威终点」为前提。跟着 §1.1 表走：§6 两行死亡语义、§4 窗口、resume 重挂，整套会话恢复设计在直连形态下无处安放。四档目标（轻在线/房间制/普通 MMO/BigWorld 增强态）里，轻在线/房间制档位可以 gateway/Zone 同机共置（同机一跳代价趋零，§1.2），不必为它们放弃普通 MMO 档的恢复语义。
- **为什么后端不取中间件族**：§2.3 已析——语义错位（四通道/resume/水位仍要自长）+ 外部件运维面 + 热路径序列化税；apollo 的裁决链（§15.2 禁并存 + 2026-09-30 不引 etcd/consul + §5.6 M1 自研总线）与九框架主流（自研 RPC 六家）同侧。
- **与 Pomelo/Pitaya connector 的同形不同因**：拓扑形态相同（专职前端），理由不同——它们因 Node 单线程负载必拆，apollo 因场景单写者纪律与恢复语义拆。同形不同因意味着可学的是形态不是它们的实现动机。
- **与 Orleans 的镜像**：Orleans 教「连接面薄到可以 co-host 就别拆」；apollo 的连接面不薄（全套会话层），所以拆。两条不矛盾，判据同一条：连接面是不是 thin。

**拍板边界（不代拍）**：gateway-app 现状是半成品壳——rearchitecture/architecture.md:15（"骨架完整；客户端 ingress 未接线（NullClientIngressServer 空转）；sendToClient placeholder；resolveRoute 落点端口错配 9002 vs 9003"）+ improvement-plan.md:172 摸底三因批记（启动期全后端硬依赖/默认端口错位/ChatApp 幽灵依赖，挂 net M1 拍板链）。本件只供「要不要独立 gateway」的论据——结论「留」；「何时修、按最小修复处方还是等 net M1 直接接线」归拍板链，本件不裁。

## 5. 与其余文档的交集

| 关联文档 | 落点 |
|---|---|
| docs/36-MMO_Frameworks_Comparative_Analysis.md | 五家（BigWorld/KBEngine/skynet/Pomelo/WoW-TC）A/B/C 级证据的母本——本件复用其 §2.1/§2.2/§2.4/§2.9 已证事实；本件增量=前端连接轴 + Pitaya/Colyseus/Nakama/Orleans 四家首采 |
| net-abstraction | §5.4 GATE 变体→gateway-app；§5.9 wss 兜底随 gateway-app 定案（理由 3）；§5.6 M1 进程间总线（后端互联裁决）；§5.7 InterServerLink=zone↔gateway 链路底座 |
| login-flow | §1 两阶段连接/登录不经 gateway 反方案记录；§3 token 验签归 gateway（职责收窄）；§10 存量 session_admission_service RPC 回问错位 |
| session-and-online-directory | §6 gateway 行/Zone 行——理由 1 的语义本体；§4 掉线窗口 |
| term-contract | :47 网关进程行（token 验签、四通道会话、按 zone 透传）——职责收窄口径的既有定义 |
| object-model | :129 进程轴/对象轴（Session owner=gateway、PlayerAnchor 不迁移）——权威布局决定拓扑（§3 规律）的依据 |
| capacity-and-benchmark | §1/§2.1/§3/§4 数字供数（5000 CCU/1Gbps/195k 事件/s/640MB 环预算） |
| improvement-plan | P3-4 批记 gateway 摸底三因（现状半成品的出处）；P3-2 gateway surface（ingress 接线挂点） |

## 6. 证据来源清单（本轮实读，2026-10-08）

**本地源码（A 级）**：

- KBEngine：`~/workspaces/kbengine/kbe/src/server/loginapp/loginapp.cpp:1185-1186`（baseapp 地址下发注释）、`kbe/src/server/baseapp/` 目录清单（entity/forwarder/backuper/data_download 一体面）
- skynet：`~/workspaces/skynet/service/gate.lua`（fd→connection 表 + PTYPE_CLIENT 注册）、`~/workspaces/skynet/lualib/skynet/cluster.lua`（clusterd sender）
- BigWorld：`~/workspaces/BigWorld/programming/bigworld/server/` 目录清单（loginapp/baseapp/cellapp/mgr 族/reviver，无 gateway）；行级证据复用 docs/36 §2.1（baseappmgr.cpp:588-599/:1117 等）

**官方文档（C 级，URL 均为本轮实读）**：

- Pitaya README：https://raw.githubusercontent.com/topfreegames/pitaya/master/README.md
- Pomelo repo（归档 banner 2023-09-25）/wiki（RPC-Framework、Understanding-Connector、Frontend Server 页面组织）：https://github.com/NetEase/pomelo
- TrinityCore 3.3.5 源码树：https://github.com/TrinityCore/TrinityCore/tree/3.3.5/src/server
- Colyseus scalability：https://docs.colyseus.io/scalability
- Nakama architecture：https://heroiclabs.com/docs/nakama/getting-started/architecture/
- Orleans clients：https://learn.microsoft.com/en-us/dotnet/orleans/host/client ；GatewayOptions：https://learn.microsoft.com/en-us/dotnet/api/orleans.configuration.gatewayoptions 及其源码 GatewayOptions.cs（dotnet/orleans）

**复用本仓（B 级）**：docs/36、net-abstraction、login-flow、session-and-online-directory、term-contract、object-model、capacity-and-benchmark、architecture-review §15.2/§25、improvement-plan P3-2/P3-4（引注见文内）。

---

*基线：apollo main @ 8a2c919a。外部文档与本地源码均为 2026-10-08 本轮实读；「nats 为主传输」按官方示例组织口径（README 未用 default 一词，C 级如实注记）；Orleans 1.x 专职网关节点为历史口径（本轮未获可引原文，C/D 级，仅作演化注记不作论据）。§2.3/§3 的取舍分析与规律归纳为本件分析，标注非引用。本件为建议件——gateway 拍板归 net M1 链（improvement-plan P3-4），不代拍。*
