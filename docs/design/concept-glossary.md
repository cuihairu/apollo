# Apollo 核心术语表（concept-glossary）

> 状态：术语权威载体（登记性文档，随裁决滚动增补）。定位：把**通用架构概念**整理成一张表——每个词条给三件事：**一般含义 / 出现在哪些引擎 / apollo 的对应物与立场**（带权威载体指针）。后续设计与分析文档引用本表词条名，不再各自从零定义；新概念**先入本表再落文档**。口径日期 2026-09-30。

---

## 0. 用法与裁决记录

**引用用法**
- 引用格式：「词条名（concept-glossary）」；跨引擎对照以本表为准，不重复贴行号。
- 收录判据：多家引擎/业界共有的**通用概念**，或 apollo 自造且易混的词；机制参数、实现细节归各设计文档，本表只定边界。
- 表内跨引擎行号为各仓工作副本实读记录（双轨口径同 deep-dive §0）；apollo 载体指针随设计文档滚动，以 `docs/design/*` 为准。

**术语裁决记录（2026-09-30，用户指令）**
1. **Battle → 副本定名**（两则）：① docs/25「Battle Service」的真实语义是**副本**而非「战斗」——该名**作废**，后续文档一律写「**副本**」（英文/代码标识 = **instance**；别名 dungeon；塔防/竞技语境口语 = room）。② **「battle/战斗」一词保留给战斗逻辑域**（战斗验证/战斗结算——见「战斗验证服务」词条），不再用于任何空间/实例命名。对应 KBEngine `Space` 概念；已删 docs/25 按 `git show 1e37073d^:docs/25-Battle_Service_Design.md` 可溯。
2. **不引注册中心**：不引入 etcd/consul 类外部注册中心；进程发现唯一口径 = **G-1 双层**（machined 守护 + UDP 广播，net-abstraction §7）。注册中心来源文档 docs/05 §2.3 随裁决整档删除（git 可溯），36 号 #13 行同步改写。
3. **ghost 不做**（决策 #9，2026-09-29 登记）：跨 Zone 迁移走显式 handoff；远程可见走 RO_MIRROR 只读镜像。

---

## 1. 术语总表

### 1.1 世界与实例

| 术语 | 一般含义 | 出现的引擎 | apollo 对应与立场（权威载体） |
|---|---|---|---|
| **场景**（Scene / Space） | 世界中一块有归属的逻辑单元（地图、分区、副本皆是）：实体容器、AOI 边界与地理数据的载体 | BW = `Space`（cellapp/space.hpp「represent a space」；cellappmgr/space.hpp「manage the different spaces」——**场景侧进程与管理进程各持 Space 定义**）；KBE = `Space`（cellapp/space.h `class Space : public Entity`——空间本身是实体；cellappmgr/space.h `spaceID_+Cells+geomappingPath`）；skynet 无世界单元概念 | **scene**——`AOIEntity::sceneId` 隔离副本与分段可见（attribute-sync §4.3），由场景线程承载（scripting-lua §2 单写者纪律） |
| **副本**（英文定名 **instance**；别名 dungeon、room——历史错名「Battle」已废，见 §0 裁决 1） | 一次玩法开启的**独立世界实例**：自带场景、规则与参战实体，生命周期随玩法起止——语义是「实例化的场景」，不是「战斗」 | KBE = `Space` 实例（space 即场景/副本的统一概念——用户 2026-09-30 指认）；BW = Space 实例（cellappmgr 按 space 分配 cellapp，**无独立进程**）；skynet 无 | **副本实例**（instance，短生命周期、tick 20-50ms 可配、结果单向落回——36号 #16「自创+借鉴」，三家无独立进程，deep-dive §16 负空间）；塔防/竞技的「**房间（room）**」= 副本的口语形态，一个房间一个副本实例（docs/30）；小玩法可不 offload 留 Zone |
| **Zone** | （apollo 特有词）**逻辑服进程**：承载 **1..N 个 scene** 的运行与实体权威，是故障域与扩缩的粒度（§2.1；「场景实例粒度」旧表述 2026-10-03 修正）；**不拆 cell/base 两族进程**——Base/Cell 是对象职责分层不是进程分层（player-object-model §0） | 对照：BW/KBE = cellapp（空间侧）+ baseapp（会话侧）两族进程；EQEmu = zone 一进程；skynet = 单节点多 service | 36号 #9「场景实例粒度（Zone）而非无缝 cell」；网关透传/Zone 解码的装配分工（sdk-contract §10.6）；「instance 进程」= 给单个 scene 单开进程的 offload 形态（36号 #16） |
| **玩家对象模型**（Proxy/Base/Cell） | 玩家对象的**职责三分层**：Proxy/Session（连接承载）、Base/PlayerAnchor（长期逻辑+断线保留+跨场景锚点，驻 Home Zone 不动）、Cell/Avatar（场景内空间权威，进 scene 生出 scene 死）——**对象分层 ≠ 进程分层**，三层同驻 Zone | KBE/BW = 三层且跨 baseapp/cellapp 两族进程（base-cell-proxy-model 参考件）；TrinityCore = 单进程内对象 | **player-object-model.md（2026-10-03 落盘，gap #18）**：A 三层保留 + Cell 精简（砍 ghost/迁移、精简 witness、断线升 Base、换幕=Cell 销毁重建） |
| **战斗验证服务**（battle verification / 影子复算） | 客户端权威战斗（帧同步/客户端演算）下，**服务端独立运行同一份战斗逻辑复算验证**——注册为服务、RPC 交互：输入流/战报上行、验证与结算结果下行，用于反作弊 | 行业通型但非引擎内建：JS 双端（客户端 JS + Node 验证服）、C# 双端（Unity + .NET，可热更）皆社区/自研实践；BW/KBE/skynet 无内建（均为状态同步、服务端权威，无需）——36号 §2 各节 | apollo **设计已立**（docs/design/battle-verification-service.md，2026-09-30；缺口 #17 立项即落盘）：独立 verifier 服务注册进 G-1（InterServerLink + internal 域三消息族），上行 battle-determinism §5 四元组 + 客户端 hash 申报、下行 verdict 四值 + **权威结算载荷（复算产出，客户端申报仅对照）**；验证两档 = 终局 hash 对照常开 / 逐 tick 复算按需；语言面 = **Lua 双端共享**（客户端 xLua/服务端 scripting-lua，`combat_bundle_hash`+VM 版本线双锚；确定性子集 = battle-determinism §2 全量继承 + 跨端增量三件）——与 C#/JS 方案同型；存在前提 = 客户端权威战斗（服务端权威不需要，§0 写死）；命名纪律 = battle 词专属此域（§0 裁决 1-②） |
| **无缝世界**（Seamless） | 大世界跨进程连续：实体在逻辑分区（cell）间自由漫游，边界对玩家不可见 | BW = 无缝世界（代价：ghost 双写/边界协商/跨进程调试不可单步——36号 #9 引已删 BigWorld架构深度解析 §五）；KBE = 同（ghost + entity migration） | **否决，采副本/分线制**（36号 #9）——5000+ CCU SLO 下场景实例的故障域与扩缩已够用；**无无缝地图仍是 MMO**：判据是大规模同时在线+持久世界交互，非地图连贯（WoW/FF14 分区+副本+分线照旧是 MMO 代名词）——定位句见 player-object-model §5：副本/分线制轻量 MMO 服务器引擎 |
| **线**（line / 分线） | 同一地图的**并行场景实例**——「几线」= 同 map 的第几个实例；中文 MMO 圈通称 | KBE = 引擎无 line 一级概念（源码/配置零命中——同图多 `Space` 实例即多线，Spaces 管理在脚本层）；BW = 无缝世界不分线；行业（国产 MMO）口语通称 | **线非一级概念**：由 scene_id/instance_id 承载（在线目录条目记所在线——session-and-online-directory §1）；「换线」= 显式 handoff / 重进另一实例（决策 #9 推论），不做无缝 |

### 1.2 进程与编队

| 术语 | 一般含义 | 出现的引擎 | apollo 对应与立场 |
|---|---|---|---|
| **服务发现** | 进程启动后如何知道彼此存在与死活 | BW = bwmachined 守护 + machine_guard 生死广播（machine_guard.hpp:496-497/:609-613）；KBE = machine UDP 广播探测（machine.cpp:646-670——上游注释自认同网段失效，`<addresses>` 手工 workaround 见 kbengine_defaults.xml:814-833）；skynet = 静态 cluster 节点表（clusterd.lua:88-108 loadconfig，按需 TCP）；微服务业界 = etcd/consul 注册中心 | **G-1 双层**：单机 machined 守护 + 跨机 UDP 广播，**不引 etcd/consul**（2026-09-30 裁决——net-abstraction §7 G-1；深潜证据 deep-dive §13） |
| **注册中心**（Registry） | 外部强一致 KV：集中存进程表、租约心跳、客户端拉取——微服务标配 | 业界（etcd / Consul / ZooKeeper）；**KBE 语境的「注册中心」多指 machine/mgr 这类引擎内管理组件，不是外部服务**（docs/qa q28 辨析） | **不引入**（§0 裁决 2）——原 docs/05 §2.3 设计（Redis/etcd + 心跳 5s/30s）已删 git 可溯 |
| **守护进程**（machined） | 每机一个的本地监工：拉起、重启、上报本机进程生死 | BW = bwmachined（server/tools/bwmachined + machine_guard 协议）；KBE = machine（组件表 + 超时清理）；skynet 无 | **machined 式守护**（G-1 左半：本机进程生死/拉起/崩溃上报；拉起策略 = 编队配置） |
| **热备与接管**（backup / reviver） | 主进程死、备进程按已知位点升为权威 | BW = backup_sender 分帧热备 + 一致性哈希备份链 + reviver 独立进程（server/reviver/）；KBE = **无进程级容灾**（宕机 = 最后归档窗口，C-51）；skynet = 无 | **G-2 最小骨架**：backup-hash 链 + reviver 两件先行；热备流 = PersistJournal 只读镜像，备机 ack 的 journal 位点即接管起点（net-abstraction §7 G-2） |
| **恢复相位**（recovery） | 拓扑剧变期间的**排他窗口**：拒绝新请求直至收敛，防恢复中拓扑再变 | BW = cellappmgr `startRecovery()`（cellappmgr.cpp:281/:1411）+ 恢复期 `Denying…` 拒新（:1300）；KBE/skynet 无此层 | **manager 域排他恢复相位**：machined 死亡事件 → 拒新场景/新进程 → base 死走 reviver、cell 死在幸存进程重建（net-abstraction §7） |
| **负载与准入**（load balancing / admission） | 落点选择（挑最轻）+ 过载保护（拒登/排队），一份指标两用 | BW = `minAppLoad()` 最轻分配（baseappmgr.cpp:588-599）+ LoginConditions 过载准入（:947-954）+ cell 再平衡族（cellappmgr.hpp:305/:307）；skynet = MQ 溢出告警（skynet_mq.c:19 MQ_OVERLOAD 1024） | **manager 域**：指标同源 G-5（观测与调度同一份数据）、新负载往轻处走 + 准入闸门；自动 cell 迁移推迟 M2+（net-abstraction §7） |
| **在线目录**（online directory） | 谁在线、在哪条线/哪个进程的全局登记与查询——跨进程寻址与顶号裁决的数据底座 | BW = 分散在 mgr（baseappmgr 持 base 分配表 + (addr, load) 上报 loginapp 分流——baseappmgr.cpp:588-599/:1117）；KBE = **无**（在线 = baseapp 实体在内存，重复登录裁决在脚本层）；微服务业界 = 注册中心里的会话表（apollo 不引，见 §2.3） | **manager 域集中权威 + 事件投影镜像**（session-and-online-directory，2026-09-30 落盘）：进程内存权威态（不进 Redis 不落 DB 无 journal）、事件上报 + 周期对账、查询走镜像（RouteResolver/GM/广播三消费方）；「全局仲裁态集中不共享」通道族（net-abstraction §7）的落地件 |
| **登录链**（login flow） | 从客户端启动到进入世界的完整链路：账号鉴权 → 选服/准入 → 入场凭证签发 → 游戏连接建立——流程横跨多进程，凭证与裁决点是设计核心 | BW = loginapp 独立进程（鉴权 + mgr 指派 baseapp，36号 §2.1）；KBE = loginapp 独立进程 + clientsdk_downloader SDK 下发（deep-dive §4）；skynet 无内建（业务自写） | **两阶段连接**（login-flow，2026-10-01 落盘，缺口 #13 CLOSED）：登录连接 client↔login-app 短连接（匿名握手 + AEAD 传凭据）；游戏连接 client↔gateway（ClientHello 带 login_token）——「拓扑入口 = gateway」指游戏会话面；token = HMAC-SHA256 自包含 + **manager 准入临界区一次性核销**（准入/选服/顶号预裁归 manager 单点）；鉴权/账号域归 login-app；三凭证辨析见 §2.5 |
| **入站对接**（interfaces） | 外部第三方**主动进来**的 HTTP 面：渠道支付回调、账号绑定回调——承载进程、验签、幂等、向游戏内投递的接线（与出站 HTTP 是同渠道两方向） | KBE = `tools/server/interfaces` 独立进程（HTTP 收回调 → 查询/投递内部，deep-dive §4）；BW = 无独立对接进程（billing 记录直进 db 层 mysql_billing_system，36号 问11）；skynet 无 | **独立 interfaces 进程**（inbound-interfaces，2026-10-01 落盘，缺口 #14 CLOSED）：薄监听层（无模板/会话/静态文件——Drogon 裁决维持不复议）；鉴权 = HMAC 签名主 + IP 白名单辅（mTLS 不进路线图）；投递两段式（先持久后处理 + 查在线目录定 Zone，tick 边界消费）；幂等键 = 渠道订单号；入站限定三类白名单（回调/健康检查/预留运维——后台 UI/报表/GM 不入此面） |

### 1.3 实体与同步

| 术语 | 一般含义 | 出现的引擎 | apollo 对应与立场 |
|---|---|---|---|
| **实体标识**（EntityID） | 世界内实体的唯一 id；能否内嵌来源信息决定可追溯性 | BW = EntityID=int32（basictypes.hpp:104）/ DatabaseID=int64（:191）/ UniqueID 128 位点分（unique_id.hpp:17-25）；KBE = `ENTITY_ID` int32（platform.h:342） | **64 位 ServerID 分段（Region/Group/Type/Instance）——自创设计**（36号 #15 证伪修正：BW 无分段对应物；原 05 §2.2 已删 git 可溯） |
| **ghost** | 实体在邻进程的**双向投影**：跨边界可见 + 调用转发，无缝世界的运行基础 | BW = ghost/real 双侧（entity_ghost_maintainer、buffered_ghost_message 缓冲队列族——deep-dive §9）；KBE = ghost（cellapp/ghost_manager.*）+ entity migration | **不做 ghost**（决策 #9）；对应机制 = **RO_MIRROR 远程只读镜像**（attribute-sync §4.4——单向只读，调用转发权威侧）；跨 Zone 迁移走显式 handoff（todo 批次 3） |
| **属性同步**（property replication） | 权威侧属性变更如何到客户端：脏打包、可见域、按观察者补洞 | BW = properties + witness 双序号（witness.cpp:2470-2516 dumpAoI 实证）；KBE = `.def` flags + dirty 打包 + DetailLevels；skynet 无内置 | 契约 `sync`/`predict` 位 + 8 位可见域掩码 + per-viewer acked_seq 水位（attribute-sync §5/§10） |
| **AOI** | 决定「谁该看到什么」的邻域计算与分发 | BW = cellapp witness/aoiMap_（随 CellApp 绑死）；KBE = `coordinate_node.*` 十字链（进程内 O(1) 增删，同样锁在 cellapp）；skynet 无内置 | **独立 AOI 服务**（网格+四叉树+shard，sceneId 隔离——可独立扩缩、Zone 卡顿不拖 AOI；36号 #10） |
| **可见域分档**（condition / LOD） | 按身份/距离/档位决定字段子集与更新频率 | UE = `COND_*` 条件位（C++ 宏，改协议须重编）；BW = AoIUpdateScheme 权重函数 + LOD 事件序号数组（aoi_update_schemes.hpp:18-56、entity_cache.hpp:189）；KBE = DetailLevels 三档半径+滞回（detaillevel.h:21-23） | 8 位 `SYNC_*` 掩码（SELF/TEAM/GUILD/AOI/WORLD 五域）——**条件做成契约字段而非宏**（sdk-contract §2.3 `sync=`，36号 #11） |

### 1.4 调用与连接

| 术语 | 一般含义 | 出现的引擎 | apollo 对应与立场 |
|---|---|---|---|
| **远程实体调用** | 跨进程调用另一进程上的实体方法 | BW = Mailbox（异步单向）；KBE = EntityCall（异步单向，结果用反向调用）；skynet = `skynet.call`（同步阻塞当前协程，lualib/skynet.lua:227） | **RemoteEntityCall 四件套**：OneWay 默认、RequestReply 只限控制面（同步返回不进热路径——net-abstraction §7、architecture/remote-entity-call-design.md） |
| **进程间连接设施** | 稳定连接公共底座：拨号、心跳、重连、断连语义——业务不自写重试 | BW = Mercury（TCPConnectionOpener 状态机；LoggerEndpoint 各消费者自写重连 = 反面教材，logger_endpoint.cpp:672-703）；KBE = Network 模块 Channel 族；skynet = cluster 按需 TCP | **InterServerLink**：连接器 / 统一重连（指数退避 + 编队事件联动）/ in-flight 按 invoke_mode 分 / authority_epoch 区分对端重启（net-abstraction §5.7） |
| **心跳与卡死检测** | 两件事：传输层「还活着吗」与逻辑层「还在干活吗」——不可混用 | skynet = monitor 版本号比对（不变 = 报警不杀，skynet_monitor.c:31-45）；KBE = machine 组件超时清理（进程级）；BW = machine_guard 生死 + 连接层保活 | 分两层：连接级心跳归 net-abstraction §3/§5.7；逻辑卡死归 **G-5 检测原语**（per-scene 心跳版本号/队列水位/实体计数/帧耗时，control 通道上行——net-abstraction §7） |
| **顶号**（duplicate login / kick） | 同账号新登录**顶替**旧会话：旧连接被踢、旧实体终结，新会话成立——与断线重连（resume）是两件事 | KBE = 引擎无内建、assets 脚本层自决（销毁旧 avatar）；BW = baseapp 销毁旧 base 实体族；行业通型（端游/手游皆有） | **框架语义：新顶旧**，裁决权在 manager（目录 owner 单点串行）——同一临界区旧条目终结+新条目写入，异步下发踢除；`anchor_epoch` 锚竞争裁决（resume/迟到上行验 epoch 失效即拒）；与 resume 分工：resume = 同会话连接级恢复（net-abstraction §3），顶号 = 新会话替代（session-and-online-directory §3） |
| **掉线保活窗口**（grace window） | 断线后实体保留、等待重连的时间窗——窗口内不销毁不落档，窗口满按离线处置 | 行业通型（KBE/BW 以脚本层定时器与 base 生命周期近似实现，引擎无专门语义） | **Suspended 态**：窗口与 resume token TTL **同源一个值**（不设两套）；窗口内实体留 Zone、resume 成功回 Online；窗口满 → 目录删条目 + Zone 终结（存档走 write-behind）；计时 = manager 定时器 deadline=tick 号（session-and-online-directory §4） |

### 1.5 节拍与数据

| 术语 | 一般含义 | 出现的引擎 | apollo 对应与立场 |
|---|---|---|---|
| **tick / 单写者** | 固定节拍、单线程全序推进世界——换来无锁与确定性 | BW = 定 tick 默认 10Hz（server_app_config.cpp:28 DEFAULT_GAME_UPDATE_HERTZ）；KBE = 定 tick 默认 10Hz（kbengine_defaults.xml:5 gameUpdateHertz）；skynet = 事件驱动无 tick（省空转但要自建节拍） | **场景线程单写者 + 主循环 10Hz**（clock-and-time/capacity 定案；docs/05 §3.3 与 36号 #8 的「20Hz」为旧口径） |
| **意图上行**（intent-only） | 客户端只发「想做什么」，服务端校验并定夺 | 对照物 = lockstep（客户端输入进判定、帧同步）——三框架源码均无内建 lockstep（deep-dive §18 负空间） | sdk-contract §2.3「intent only, 服务端定夺」；lockstep 取舍 = 36号 #18（只留适配缝、不进大世界协议） |
| **write-behind journal** | 状态变更先落追加日志、后刷快照，崩溃可重放 | BW = baseapp 周期备份（backup_sender 族，快照流无 journal）；KBE = Archiver 平滑刷库（archiver.cpp:20-70，按 tick 头部区段）；skynet 无 | attribute-sync §8.2——journal 同时是 G-2 备份镜像流与崩溃恢复位点（三用一位，不另起备份协议） |
| **契约**（contract） | 一份声明（属性/消息/白名单）多端生成的单一真相源 | BW/KBE = `.def`（两家皆 XML 但**无 schema 层**，错拼标签静默缺省——entity_description.cpp:184-190 / entitydef.cpp:188-210）；UE = 编译期宏；EQEmu/TC = 源码 opcode 表 | XML + XSD 形式校验 + 生成器（sdk-contract §2、xml-generation）——生成器不进运行时链接图（36号 #4） |

---

## 2. 重点辨析（易混词对）

### 2.1 scene vs 副本 vs Zone——三个粒度各管什么

- **scene（场景）** = 数据与逻辑单位：实体容器、AOI 边界（sceneId）。
- **副本** = scene 的**生命周期形态**：玩法开启时实例化、结束时销毁；可放独立进程（apollo 的副本实例进程），在 KBE 里就是又一个 `Space` 实体（space 本身就是可脚本实体）。
- **Zone** = **进程单位**：承载一个或多个 scene/副本的逻辑服进程，是故障域与扩缩的粒度。
- 一句话：**scene 是「一块世界」，副本是「一块世界的一次运行」，Zone 是「跑世界的机器格子」**。
- 玩家对象同轴收口（2026-10-03）：**锚在 Base（Home Zone 不动），空间态在 Cell（随 scene 生死）**——三层职责与换幕/断线/重连规则见 player-object-model（gap #18）；**对象分层 ≠ 进程分层**，三层同驻 Zone。

### 2.2 ghost vs RO_MIRROR——名字不同因为立场不同

ghost 服务于**无缝世界**：邻进程双向投影、双写、调用转发，是 apollo 决策 #9 否决的形态；RO_MIRROR 是 apollo 保留的**单向只读镜像**：热备流与只读视图用，写永远只在 owner，调用转发权威侧——「共享 = 订阅 owner 的投影」（net-abstraction §7 共享模型）在镜像类信息上的落点。所以 apollo 不是「把 ghost 改名」，而是**换了同步模型**。

### 2.3 服务发现 vs 注册中心——事实获得 vs 外部台账

发现回答「谁活着、在哪」（apollo：守护 + 广播，最终一致）；注册中心是外部强一致台账（etcd/consul，租约 + watch）。裁决不引的依据：进程拓扑小、变更低频，强一致换不来对等收益（net-abstraction §7 G-1）；广播跨网段的缺口由**单机守护**补——机器级生死与拉起本来就不该靠广播（deep-dive §13 裁决后读法）。注意 KBE 语境里的「注册中心」多指 machine/mgr 这类**引擎内**管理组件（qa q28），与微服务注册中心不是一回事。

### 2.4 心跳的两个层次

连接心跳（字节层：RTO/超时 → 断连重连 resume）与逻辑心跳（tick 层：版本号不变 = 卡死）不可混用——前者由传输/会话层处置（可杀连接），后者只报警不杀（skynet monitor 同型）。apollo 两层各有归属（§1.4 末行），检测与恢复也是两件事：G-5 只出信号，处置归 manager 恢复相位。

### 2.5 login_token vs session_key vs resume token——三层凭证各管一段（2026-10-01，随 login-flow 落盘）

| 凭证 | 域 | 管什么 | 签发/协商方 | 寿命 |
|---|---|---|---|---|
| **login_token** | 账号域 | 一次性**入场券**：证明「鉴权已过、准入已批」——gateway 本地验签、manager 临界区核销 | login-app 签发（HMAC-SHA256 自包含） | 短 TTL（60s 建议）+ 一次性（nonce 核销） |
| **session_key** | 连接域 | **帧加密密钥**：ClientHello/ServerHello 握手协商出，喂给 CryptoFilter 加密全帧 | 每连接双方协商（X25519+HKDF，net-abstraction §5.9） | 每连接新——resume 重连也重走 ECDH，**不恢复旧密钥** |
| **resume token** | 会话域 | **断线恢复凭证**：证明「我是刚才那条会话」——重连时恢复 L2 seq/队列状态 | 游戏连接建立时服务端发（L2） | TTL 与掉线保活窗口**同源一个值**（session-and-online-directory §4） |

三层互不通用、互不派生（密钥层次 net-abstraction §5.9 :317 的凭证面）：偷到 resume token 的人造不出 session_key（要重走握手），持旧 session_key 重连不被接受（nonce 新鲜性），login_token 用第二次即拒（nonce 已核销）——每层的失守都封死在层内。

---

## 3. 增补纪律与基线

- 新概念**先入本表再落文档**；文档引用词条名，不重复定义。裁决改口径时同步改本表（沿革注写在 §0）。
- 已删文档（docs/00、docs/05、docs/17、docs/25、BigWorld架构深度解析）一律按「git 历史可溯」读，本表不复述其内容、只留指针。
- 跨引擎行号为本地三仓工作副本实读（双轨口径见 deep-dive §0）；三家之外的引擎（UE/Unity/EQEmu/WoW 生态）只写到机制名、不引行号（源码不在本机，36号 §0 分级口径）。

---

*基线：apollo main（本文件随提交见 git 记录）；术语裁决 2 条（Battle→副本定名、不引注册中心）为用户 2026-09-30 当日指令，ghost 否决为决策 #9（2026-09-29 登记）。跨引擎源证沿用 36号 §1 决策追溯表与 deep-dive §1-§18 实读记录 + 本轮实读（BW cellapp/space.hpp、cellappmgr/space.hpp；KBE cellapp/space.h、cellappmgr/space.h、platform.h:342——2026-09-30）。零源码改动。*
