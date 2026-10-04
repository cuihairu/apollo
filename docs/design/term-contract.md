# Apollo 术语契约（term-contract）

> 状态：**契约 v1.0（2026-10-03 定稿，用户审定）**。定位：全项目**命名契约**——每个术语钉死「中文名 / 英文名 / 代码标识」三件套 + **其他引擎含义对照** + 禁用名。与 concept-glossary 分工：glossary 管含义辨析与证据（为什么这么叫），本契约管命名与拼写（叫什么、怎么写）——**拼写真相源以本契约为准**。术语来源：concept-glossary 全部词条 + §0 三裁决 + 玩家对象模型（gap #18）+ 2026-10-03 讨论定稿。

---

## 0. 契约规则

1. **三件套唯一**：中文名（文档叙述）/ 英文名（文档与注释词根）/ 代码标识（struct、module、文件、契约 XML 字段的实际拼写）一一对应。改名 = 契约变更，须在 §3 版本记录留痕。
2. **他家名字不入户**：BW/KBE/skynet/UE 等的机制名（baseapp、cellapp、Space、Mailbox…）只作对照出现，引用时必须写成「BW 的 cellapp」形式；**不得作 apollo 代码标识或裸用**。
3. **禁用名（§2）在文档与代码中零出现**——历史件与引用他家机制的对照段除外。
4. **新术语流程**：先入 concept-glossary（含义辨析）→ 再入本契约（命名钉死）→ 才能进设计文档/代码。
5. 对外产品文案另守 `~/workspaces/standards/产品文案用词规范.md`（禁「自行开发/遥遥领先」等——用户 2026-10-03 禁令），本契约管技术命名，不与混用。

## 1. 术语契约总表

### 1.1 世界与空间域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 场景 | scene | `Scene` / `scene_id` | 世界中一块有归属的逻辑单元：实体容器、AOI 边界、地理数据载体 | BW/KBE = `Space`（KBE 中空间本身是实体）；skynet 无 | 禁作 `Space`；「地图」≠ 场景（地图是资产） |
| 副本 | instance | `Instance` / `instance_id` | 一次玩法开启的场景实例：生命周期随玩法起止 | KBE = `Space` 实例；BW = Space 实例（无独立进程）；WoW = instance（副本本义） | 禁：**Battle（已废错名）**、dungeon/room 作代码标识 |
| 房间 | room（口语） | —（不入代码） | 副本在塔防/竞技语境的口语 | — | 仅口语 |
| 分线 | line（口语） | —（由 `scene_id` 承载） | 同一地图的并行场景实例 | KBE = 多 `Space` 实例；BW 无缝不分线 | 禁作一级概念/代码标识 |
| Zone | Zone | `Zone` | 逻辑服进程：承载 1..N 个 scene，故障域与扩缩粒度 | EQEmu = zone 一进程；BW/KBE = cellapp+baseapp 两族 | 禁：cellapp/baseapp、场景实例粒度（旧表述已修正） |
| Home Zone | HomeZone | `home_zone_id` | Base 驻地：登录时分配，永不随场景切换迁移 | 无对应（BW baseapp 归属的进程版） | — |
| 换幕 | scene change | — | Cell 销毁重建（进副本/换线/跨场景） | 无对应（BW/KBE 是 entity migration——被否决） | 禁：迁移/migration（handoff 另指显式交接，不混用） |
| 无缝世界 | seamless | — | 跨进程连续大世界（边界不可见） | BW/KBE = ghost 双写 | apollo 否决（36号 #9） |
| 地图资产 | map asset | `map_id` | 地形/碰撞/寻路的离线烘焙资产（gap #16） | BW = chunk 体系；KBE = `res/spaces/` | 禁与「场景」混用 |

### 1.2 玩家对象域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 会话 | Proxy / Session | `Proxy` / `Session` | 连接承载：四通道会话、resume、顶号挂载 | KBE = `Proxy` 实体 | — |
| 玩家锚 | Base / PlayerAnchor | `PlayerAnchor` | 长期逻辑归属：断线保留、跨场景锚点，驻 Home Zone | KBE/BW = Base 实体（baseapp） | 代码标识不用 `BaseEntity`（免与他家混淆） |
| 场景化身 | Cell / Avatar | `Avatar` | 场景内空间权威：移动/战斗/AOI，进 scene 生出 scene 死 | KBE/BW = Cell 实体（cellapp） | 代码标识不用 `CellEntity` |
| 易失态 | volatile state | — | L3 不落档状态（HP/位置/临时 buff），换幕不带走 | BW = volatile 更新序号 | — |
| 玩家对象模型 | player object model | — | Proxy/Base/Cell 三层职责分层，同驻 Zone（对象分层≠进程分层） | KBE/BW = 三层且跨进程 | — |

### 1.3 进程与编队域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 登录进程 | login-app | `LoginApp` | 账号鉴权、token 签发、契约包指针（短连接） | BW/KBE = loginapp | — |
| 管理进程 | manager | `ManagerApp` | 准入/顶号预裁/落点/核销（单点串行） | BW = baseappmgr+cellappmgr；KBE = 双 mgr | 禁：mgr 缩写作标识 |
| 网关进程 | gateway-app | `GatewayApp` | token 验签、四通道会话、按 zone 透传 | KBE = baseapp 代理段；BW = 无独立 | — |
| 副本进程 | instance process | — | 单 scene 单开进程的 offload 形态（36号 #16） | 三家无独立进程 | — |
| 守护进程 | machined | `Machined` | 每机本地监工：拉起/重启/生死上报（G-1 左半） | BW = bwmachined；KBE = machine | — |
| 对接进程 | interfaces | `InterfacesApp` | 第三方入站 HTTP 回调面 | KBE = `tools/server/interfaces` | — |
| 日志收集进程 | logger | `LoggerApp` | 日志双出口（本地文件真相源+push） | BW/KBE = logger 工具 | — |
| 战斗验证进程 | verifier | `VerifierApp` | 客户端权威战斗的服务端复算对账（gap #17） | 行业自行开发通型，三家无内建 | — |
| 服务发现 | service discovery | G-1 | 单机守护+UDP 广播双层 | BW/KBE = machined 同型 | 禁：注册中心 |
| 注册中心 | registry | — | 外部强一致 KV（etcd/consul） | 微服务标配 | **不引入**（裁决 2）；KBE 语境同词异义（引擎内管理组件） |
| 热备与接管 | backup / reviver | G-2 | backup-hash 链+reviver | BW = backup_sender+reviver；KBE 无 | — |
| 恢复相位 | recovery phase | — | 拓扑剧变期排他窗口：拒新至收敛 | BW = cellappmgr `startRecovery()` | — |
| 在线目录 | online directory | `OnlineDirectory` | 谁在线/在哪的全局登记查询（manager 权威+镜像） | BW 分散 mgr；KBE 无 | — |
| 顶号 | duplicate login | — | 新会话顶替旧会话（≠断线重连） | 行业通型 | — |
| 掉线保活窗口 | grace window | `Suspended` 态 | 断线实体保留等待重连的时间窗 | 行业通型（脚本层近似） | — |
| 准入 | admission | — | 负载闸门+落点+核销 | BW = LoginConditions | — |

### 1.4 连接与协议域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 四通道 | four channels | `movement`/`attributes`/`events`/`control` | 客户端会话面四通道，各带独立 seq | BW = witness 双序号同构 | — |
| 帧层级 | frame layers | L0/L1/L2 | 传输/帧格式/会话三层 | BW = Mercury | — |
| 过滤器链 | frame filter | `FrameFilter`/`CryptoFilter` | 帧层可插管线（加密/压缩） | BW = Mercury filter 栈 | — |
| 远程实体调用 | remote entity call | `RemoteEntityCall` | 跨进程实体方法调用（OneWay 默认/RequestReply 限控制面） | BW = Mailbox；KBE = EntityCall；skynet = `skynet.call` | 禁：Mailbox/EntityCall 作标识 |
| 进程间连接设施 | inter-server link | `InterServerLink` | 拨号/心跳/重连/背压公共底座 | BW = Mercury 连接器族 | — |
| 入场券 | login token | `login_token` | 一次性入场凭证（HMAC 自包含+核销） | KBE/BW = ticket 类 | — |
| 帧密钥 | session key | `session_key` | 每连接协商的帧加密密钥（X25519+HKDF） | — | — |
| 恢复凭证 | resume token | `resume_token` | 断线恢复会话的凭证（与保活窗口同源 TTL） | — | — |
| 意图上行 | intent | — | 客户端只发「想做什么」，服务端定夺 | 对照 lockstep（被否决，36号 #18 只留适配缝） | — |

### 1.5 同步与数据域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 属性同步 | property replication | — | 权威侧变更到客户端：脏打包+可见域+补洞 | BW = witness；KBE = .def flags | — |
| 邻域可见 | AOI | `AOI` / `scene_id` 隔离 | 「谁该看到什么」的计算与分发（独立服务） | BW = aoiMap_；KBE = 十字链 | — |
| 观察者集 | viewer set | `ViewerState` | per-viewer 进度水位+广播目标集 | BW = witness 实读同构 | 禁：witness 作代码标识（他家名） |
| 只读镜像 | read-only mirror | `RO_MIRROR` | 单向只读投影，写只在 owner | BW/KBE = ghost 双向投影（被否决） | 禁：ghost 作机制名 |
| 可见域分档 | sync mask | `SYNC_*`（SELF/TEAM/GUILD/AOI/WORLD） | 按身份/距离定字段子集与频率 | UE = `COND_*` 宏；KBE = DetailLevels | — |
| 先记日志 | write-behind journal | `PersistJournal` | 先落追加日志后刷快照（兼热备流/恢复位点） | BW = 快照流无 journal；KBE = Archiver | — |
| 契约 | contract | `entities.xml` 等 | 一份声明多端生成的单一真相源 | BW/KBE = `.def`（无 schema 层） | — |
| 契约哈希 | schema hash | `schema_hash`/`client_hash` | 契约版本指纹（按域双份） | — | — |
| 节拍 | tick | `tick` 号 | 固定节拍全序推进（场景线程单写者 10Hz） | BW/KBE = 10Hz；skynet 事件驱动 | — |

### 1.6 玩法域

| 中文名 | 英文名 | 代码标识 | 定义一句 | 其他引擎含义对照 | 禁用/别名 |
|---|---|---|---|---|---|
| 战斗验证服务 | battle verification | `VerifierApp` | 客户端权威战斗的服务端复算对账 | 行业 JS/C# 双端自行开发通型 | 「battle/战斗」一词**专属此域** |
| 战斗 | battle | — | 战斗逻辑域（验证/结算） | — | 禁用于空间/实例命名（§2） |

## 2. 禁用名总表

| 禁用名 | 类型 | 原因 | 替代 |
|---|---|---|---|
| **Battle**（空间/实例义） | 历史错名（裁决 1） | docs/25 语义实为副本非战斗 | 副本 / `instance` |
| **ghost**（机制名） | 否决机制（决策 #9） | 无缝世界代价，已砍 | `RO_MIRROR` 只读镜像 |
| **cellapp / baseapp** | 他家进程名 | apollo 不拆两族进程 | `Zone` |
| **BaseEntity / CellEntity** | 他家对象名 | 对象分层≠进程分层，免混淆 | `PlayerAnchor` / `Avatar` |
| **Space** | 他家空间名 | KBE/BW 名 | `scene` |
| **Mailbox / EntityCall** | 他家调用名 | — | `RemoteEntityCall` |
| **witness**（代码标识） | 他家名 | — | `ViewerState` / viewer set |
| **注册中心（etcd/consul）引入** | 否决引入（裁决 2） | 拓扑小、变更低频，强一致无对等收益 | G-1 双层 |
| **nng** | 退役（36号 #2） | 自持四层 | InterServerLink |
| **lockstep**（作大世界协议） | 取舍（36号 #18） | 只留适配缝 | 意图上行 |
| **房间 / 线 / 分线**（作代码标识） | 口语 | — | `instance` / `scene_id` |
| **场景实例粒度**（描述 Zone） | 歧义表述 | 与「承载 1..N scene」自相抵触（2026-10-03 修正） | 承载 1..N scene 的逻辑服进程 |
| **迁移 / migration**（场景切换义） | 否决机制联想 | ghost 语义残留 | 换幕 / scene change |
| **自行开发、遥遥领先**等 | 文案禁词（用户禁令） | 产品文案用词规范 | 基于 XX 构建 / 自写 / 封装 |

## 3. 版本记录

| 版本 | 日期 | 变更 |
|---|---|---|
| v1.0 | 2026-10-03 | 初版：收编 concept-glossary 全部词条（1.1-1.5 五组）+ §0 三裁决 + 玩家对象模型三件套（gap #18）+ 讨论定稿（换幕/易失态/Home Zone）+ 产品文案禁词行；禁用名表首立。**同日用户审定**：① 玩家空间对象代码标识 = `Avatar`（Base 层 = `PlayerAnchor`）② 换幕/房间分线两口径无异议——v1.0 定稿 |
