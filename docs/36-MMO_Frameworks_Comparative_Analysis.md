# 36. 常见 MMO 服务器框架对比分析（Apollo 视角）

> 状态：分析文档（对比研究）。覆盖 BigWorld、KBEngine、skynet、Pomelo、NoahGameFrame、Unity Netcode、UE Replication、EQEmu、WoW 生态（TrinityCore/CMaNGOS 近似）、Space Engineers（资料不足标注）十个对象。每维度对照 Apollo 的设计选择（docs/00、docs/04、docs/05 与 docs/design/*），给出取舍理由。纯文档工作，零源码改动。

## 0. 证据分级与口径

| 级别 | 含义 | 标注方式 |
|---|---|---|
| A | 本地源码实读（BigWorld 官方 14.4.1 包 `~/workspaces/BigWorld`、KBEngine 浅克隆 `~/workspaces/kbengine`、skynet `~/workspaces/skynet`） | `路径:行号` |
| B | 本仓库文档（docs/00、04、05、17、25、BigWorld 深度解析、analysis/ioc-review、design/*） | `docs/… §…` |
| C | 官方网络文档/公告（归档仓库、厂商文档页） | Markdown 链接 |
| D | 社区资料/招聘口径/论坛，可靠性较弱 | 文末加「（D）」或明示存疑 |

**假设与口径（预先声明）**：
1. 任务书中的 "NoahWolf" 解释为 **NoahGameFrame**（ketoo），无 "NoahWolf" 这一框架的公开记录（D）。
2. "Space Engineers MSE" 在公开网络与本机均未查证到对应工程（疑为 MES——Modular Encounters Systems——模组框架的误记，或 Keen 内部代号）；本机 `~/workspaces/SSEngine` 经盘点为通用 C++ 支撑库（线程/内存池/DB 门面，MIT，见其 README.md），**不是** Space Engineers 引擎。Space Engineers 一节按「资料不足」处理，仅给出可查证的 1.187 Multiplayer Overhaul 公开材料。
3. WoW 服务端细节暴雪从未官方公开，WoW 一节以 **TrinityCore/CMaNGOS 公开实现 + 社区文档**为近似依据（TrinityCore 源码未实读，全节 D 级偏多，逐处标注）。
4. docs/17、docs/25 为早期设计稿，其中 Transport=NNG 的表述已被 ioc-review 的网络自建决策取代；本文引用两文档时只取其**结构设计**（编排/AOI/Battle 分层、Pipeline、回放），不引用其传输选型。
5. KBEngine 行号基于浅克隆工作副本；BigWorld 行号基于官方 14.4.1 包；两者与 ioc-review §16 的已核证据一致。

---

## 1. Apollo 决策追溯表

| # | Apollo 关键设计点 | 来源框架/理念 | 采纳/改造/否决 | 理由（落到对方具体做法） |
|---|---|---|---|---|
| 1 | 网络四层自建（L0 poll-Reactor / L1 帧定界 magic+seq+CRC32C / L2 会话 / L3 GameConnection） | BigWorld Mercury（`lib/network/udp_channel.hpp:81/:139-140` filter 栈）；skynet gate 的帧封装 | 改造 | Mercury 把 filter 做成可插栈的字节↔字节边界（加密/压缩/包调制），证明分层过滤在引擎内可行；但 Mercury 绑定 UDP+聚合包，Apollo 要 TCP/WebSocket 双族，故只取 filter 位置语义，帧格式自定（net-abstraction §3、§5.5） |
| 2 | nng 退役，中间件只留语义蓝本 | skynet harbor→cluster 两代 IPC 更替（ioc-review §16 实证） | 否决第三方消息层 | skynet 自己都把 harbor 换成了 cluster，证明自研进程内消息总线的代际更替是常态；引入 nng 意味着把更替风险外包给一个社区项目，不如自持四层（net-abstraction 摘要 3/6 已闭） |
| 3 | 契约 = XML+XSD（xs:key/keyref/enumeration），一行一属性紧凑风格 | KBEngine `.def`（一端声明、多端生成的精髓）；BigWorld `.def` 同构（`entity_description.cpp:184-190`，C-50） | 采纳+补强 | 两家的 .def 本就是 XML 但都**只解析、无 schema 层**，错拼标签静默缺省；Apollo 的增量恰好是 XSD 形式校验——策划可读、diff 友好、校验器零开发（sdk-contract §2，ioc-review §15.3） |
| 4 | `sdks/{contract,gen,unity,cocos,laya,cpp}` 布局，生成器独立二进制 | KBEngine `sdks/` 目录 + `kbe/tools/xlsx2py` 在 src 之外的先例 | 采纳 | 生成器不进运行时链接图，失败即阻断发版而非带病上线（xml-generation §4） |
| 5 | 契约与存储解耦（`storage.xml` 服务端私有，两段式） | **否决** KBEngine `.def` 属性/存储耦合 | 否决对方耦合 | KBE 把持久化绑进 .def 的直接后果：复杂类型 blob 化、只会 base 快照、无日志、无查询面（attribute-sync §8 五条短板）；Apollo 一条原则——契约答「线上怎么传」，存储答「怎么落」（sdk-contract §7） |
| 6 | write-behind journal + 列提升（先日志后快照） | BW BaseApp 周期备份（`baseapp/backup_sender.cpp`、`write_to_db_reply.cpp`）；KBE Archiver 平滑刷库（`archiver.cpp:20-70`） | 改造 | KBE 的平滑算法（每 tick 取 `size*archiveIndex_/periodInTicks` 头部区段）被 Apollo 改写为「每 tick 取脏实体数/目标刷库 tick 数的头部区段」——脏实体突发时不再摊不平（attribute-sync §8.2 同步修订） |
| 7 | per-viewer acked_seq ChangeHistory + dual-axis markers | BW witness/volatile+event 双序号（`witness.cpp:2470-2516` dumpAoI 实证） | 改造 | BW 证明「按观察者记水位」是对的，但 witness 绑死 CellApp 与实体缓存；Apollo 把水位泛化为每观察者 acked_seq + 时间/事件双轴标记，AOI 独立服务也能用（attribute-sync §10） |
| 8 | 20Hz 单逻辑主线程 + 网络/DB 线程池 MPSC | BW/KBE 定 tick（KBE 10Hz，Archiver 按 tick 计周期）；skynet 单 service 串行 | 混合 | 定 tick 才有稳定刷库/差分节拍（KBE 实证）；单 service 串行才有无锁全序（skynet 实证）；Apollo 两者都要：主线程全序 + 池化旁路 IO（05 §3.3、attribute-sync §10.1） |
| 9 | 场景实例粒度（Zone）而非无缝 cell | **否决** BW 无缝世界 | 否决 | BW 自己承认的成本：ghost 双写、边界协商、跨进程调试不可单步（docs/BigWorld架构深度解析.md §五缺点 1-4）；Apollo 目标是副本/分线型 MMORPG（5000+ CCU SLO），场景实例的故障域与扩缩已经够用（docs/00、17 §3） |
| 10 | AOI 独立服务（网格+四叉树+shard，可独立扩缩） | 各家 cell 内 AOI（BW cellapp、KBE `cellapp/coordinate_node.*` 十字链）+ 独立服务化思想 | 改造 | KBE 十字链是「进程内、随实体移动 O(1) 增删邻居」的好结构，但它锁死在 cellapp 里；Apollo 把 AOI 计算从逻辑进程剥离成 shard 服务，Zone 卡顿不拖 AOI（17 §5.1/§5.4） |
| 11 | 8 位 SYNC_* bitmask 可见域 + 50ms 批差分 | UE `COND_*` 条件位思想（[Conditional Property Replication](https://dev.epicgames.com/documentation/unreal-engine/conditional-property-replication?application_version=4.27)） | 改造 | UE 的条件位是 **C++ 宏**（编译期、改协议必须重编译）；Apollo 把条件做成契约字段 `sync=`（sdk-contract §2.3），8 位掩码批量差分（05 §6.3/§6.4），运行时可配、多端同源 |
| 12 | Lua 5.4 + sol2 白名单脚本 | skynet 的 C/Lua actor 生态；KBEngine cellapp 内嵌 Python | 改造 | KBE Python 全功能脚本的攻击面与热更失控风险大；skynet 证明 Lua 足以承载业务且可热更；Apollo 再收紧：脚本可写面由契约生成白名单（predict 位交集，sdk-contract §5） |
| 13 | Consul/Etcd 服务注册 + 心跳 5s/30s | **否决** KBEngine machine UDP 广播发现（`kbe/src/server/machine`） | 否决对方机制 | UDP 广播只能发现同网段进程，跨机房/容器网络即失效；registry 心跳在 K8s/云环境是标配（05 §2.3） |
| 14 | Prometheus/Kafka/ClickHouse 可观测栈 | BW `server/tools/{message_logger,bw_profile,…}`；KBE `tools/logger` 独立进程；skynet monitor+debug_console+logger 三板斧（`skynet_start.c:209-211` 启动序列） | 改造 | 三家都把「日志/剖析是一等公民」做进了引擎自带组件，方向对、形态旧——自建轮子不接入生态；Apollo 只学定位不学实现，直接接外部标准栈（docs/00） |
| 15 | ServerID 64 位（Region/Group/Type/Instance 分段） | BW 64 位唯一 ID（docs/BigWorld架构深度解析.md §二：type|serverId|index 分段） | 采纳+改段 | 分段 ID 使实体可追溯来源进程；Apollo 按部署语义重划段位（05 §2.2） |
| 16 | Battle 独立实例 + Replay/Inspector 适配缝 | 混合同步需求的通用解（见 §4 问 10）；BW 把战斗算在 cell 内、无独立实例 | 自创+借鉴 | 大型团战 offload 到短生命周期实例进程，故障域与 tick 率独立可调（25 §3.2/§3.3/§8），关键帧记录留确定性回放之缝（25 §6） |
| 17 | MySQL 8.0 + Redis + ClickHouse；PostgreSQL 留缝、MongoDB 不留缝 | BW db_storage_mysql/db_storage_xml 双后端、KBE MySQL-only（`kbe/src/lib/db_mysql`） | 对照自定 | 两家都把 DB 抽象成可插后端（BW 实证），但 KBE 实际只有 MySQL 一条腿（目录实证）；Apollo 用 storage.xml「语句即数据」天然留出 SQL 方言缝（见 §4 问 11） |
| 18 | 帧同步只留适配缝不做实现 | 各框架均无内建 lockstep（见 §4 问 10） | 暂缓 | MMORPG 上行 intent-only（服务端定夺）与 lockstep 确定性直接冲突；如需战斗帧同步，走 Battle 实例单独确定性运行时，不污染大世界协议（25 §3.3/§6） |
| 19 | 编解码**反射为默认、代码为两端增强（v3，2026-09-29 同日修正）**：框架固定消息族（AttrDelta/AttrBatch/movement/heartbeat 等，schema **不随契约变**）强类型代码一次生成内建框架、守服务端热路径；业务消息走 descriptor.bin 反射 → sol2 桥 → Lua handler（contract.lua 承载 attr 表/路由/白名单），**服务端契约变更零重编**；客户端按代码热更能力分档——有热更管线的端（Unity/HybridCLR、Cocos/Laya TS 脚本）走 protoc/pbjs 生成代码（编译期类型检查+热更兼得），bin 为兜底/工具/新端通道 | BW（生成代码零开销但改契约全端重编）与 KBE（生成代码编进包不能热更，拿 importClientMessages 运行时协商补演进——§2.2 ④ 登记的病）的**真实分叉**——都不照抄：取 protobuf FileDescriptorSet 自描述中间态 + 装载期一致性闸（bin↔hash 锚定、bin↔Lua 路由逐条对齐启动红），严谨性从编译期改装载期承接 | 对照自定（v3 反射默认） | 演进链路两个最慢环节（服务端 C++ 重编、端侧商店发版）**同时**与协议解耦；反射税只落低频业务消息（万条/秒 × µs 级解码，可忽略），消息两分法保热路径零反射；.bin/contract.lua/端代码皆构建期定型、运行期只读（不落「运行时解释」批判面）；单一契约源同批吐全部产物、manifest 逐文件 hash 挡混搭；业务消息反射的**解码位置为装配选择非契约分叉**（C++ 反射+sol2 桥默认 / 字节透传+Lua 侧 lua-protobuf 备选——两案同源同批同 bin，切换换装配零契约改动，sdk-contract §10.6 附节） |

---

## 2. 框架分述

每节按固定八面：进程拓扑 / 并发模型 / AOI / 同步与属性复制 / 脚本集成 / 扩展机制 / 典型生产规模 / 失败教训。

### 2.1 BigWorld（官方 14.4.1，本地实读，A 级）

- **进程拓扑**：bwmachined（守护，启停监控）→ loginapp（认证+分配）→ baseappmgr/cellappmgr（管理面）→ cellapp（空间逻辑）×N、baseapp（玩家代理+持久化）×N、dbapp 相关扩展（`programming/bigworld/server/{cellapp,baseapp,cellappmgr,baseappmgr,loginapp}` 目录清点；`server/tools/bwmachined`）。客户端先连 loginapp，被指派给 baseapp，baseapp 再把实体"投递"进 cell 空间。
- **并发模型**：多进程为主，进程内单主线程 tick + TimeQueue 定时器（`lib/cstdmf/time_queue.hpp:60/:74`，自带单测 `test_time_queue.cpp`；ioc-review §16.2 引证）。跨进程全靠 Mercury 消息。
- **AOI**：cellapp 内 witness/实体缓存 + ghost 机制；AOI 更新方案可按实体类型配置（`cellapp/aoi_update_schemes.hpp:18/:58/:75`——`AoIUpdateScheme` 按距离调整更新密度）；观测者侧 dumpAoI 输出 volatile 129/130 + event 45/45 双序号（`cellapp/witness.cpp:2470-2516`）。
- **同步/属性复制**：属性级 detail level 档位（`cellapp/entity_cache.hpp:89-90/:186`，`DetailLevel` 为 uint8 档号），距离越远档位越低、字段与频率随档位收缩；跨 cell 用 ghost 影子复制（`cellapp/buffered_ghost_message*.cpp/hpp` 全套文件）。
- **脚本集成**：cell/base 实体逻辑 Python 内嵌（.def 声明暴露方法）。
- **扩展机制**：.def 数据驱动 + Mercury filter 栈（加密/压缩可插）+ server/tools 独立工具链。
- **典型生产规模**：《坦克世界》公开报道单服务器 19 万同时在线（吉尼斯口径）[（C/D）](https://worldoftanks.eu/en/news/general-news/wargaming-acquires-bigworld)；引擎宣传口径支撑大型无缝世界。
- **失败教训**：① 2012-08-07 Wargaming 以 4500 万美元收购 BigWorld Pty——技术闭源私有化，2014 后停止对外授权，中间件公司被单一大客户买断是引擎厂商的极端结局（[GamesIndustry](https://www.gamesindustry.biz/companies/bigworld-technology)、[Wikipedia: Riot Sydney](https://en.wikipedia.org/wiki/Riot_Sydney)）；② ghost/边界一致性的复杂度与调试成本是其公开共识短板（docs/BigWorld架构深度解析.md §五）。

### 2.2 KBEngine（本地浅克隆实读，A 级为主）

- **进程拓扑**：machine（进程管理/广播发现）→ loginapp → baseappmgr/cellappmgr → cellapp ×N、baseapp ×N、dbmgr（集中 DB 门面）、interfaces（第三方账号对接）（`kbe/src/server/` 清点：`baseapp baseappmgr cellapp cellappmgr dbmgr loginapp machine tools`）。
- **并发模型**：多进程，进程内单逻辑线程；tick 节拍驱动（10Hz 口径，Archiver 周期按 tick 数计：`kbe/src/server/baseapp/archiver.cpp:20-70`，`size * archiveIndex_ / periodInTicks` 头部区段平滑刷库）；定时器 `lib/common/timer.h:101/:108`。
- **AOI**：cellapp 内**十字链**（x/z 两条有序坐标链）——`kbe/src/server/cellapp/coordinate_node.{h,cpp,inl}`、`coordinate_system.h`、`entity_coordinate_node.*`；实体移动时在链上增删邻居，配 viewspace 半径。
- **同步/属性复制**：.def 声明属性 flags（ALL_CLIENTS 等可见域）+ cell/base 两类空间；服务端按 dirty 打包下发，客户端 SDK 侧 onData 回调应用（SDK 仓库不在本机，客户端细节标注为 C 级未实读）。
- **脚本集成**：cellapp/baseapp 内嵌 Python（.def 暴露的方法/属性）；Bot 工具（`tools/bots`）。
- **扩展机制**：.def→多端 SDK 生成（C++/Unity/Cocos 等，本仓库 sdk-contract §2.1 已析）；`kbe/tools/xlsx2py` 策划表→py。
- **典型生产规模**：中小团队手游/页游广泛使用，无可靠公开 CCU 数据（D）。
- **失败教训**：① 存储是结构性短板——.def 耦合持久化导致 blob 化/无日志/无查询面（attribute-sync §8）；② machine UDP 广播发现在云网络环境失效（本仓库 13 号追溯行）；③ 项目维护节奏近年放缓，社区活跃度依赖中文圈（D）；④ **内外契约不分离**——`*_interface.h` 单文件混布三类受众消息（对客户端 `hello`/`loginBaseapp`/`onClientActiveTick` 带 EXPOSED 标记，`baseapp_interface.h:99-157`；对 dbmgr `onDbmgrInitCompleted` :90；对 cellapp `onMigrationCellappStart/End` :326-331），消息 ID 共享进程内单一自增分配表（`message_handler.cpp:139`，EXPOSED 只是导出标记非 ID 空间隔离，`baseapp_interface_macros.h:27-36`）——内部消息增删推动 ID 排布与客户端 SDK 重生成；`importClientMessages` 连接期动态下发消息表（`baseapp.cpp:4859→4891`），用运行时协商掩盖契约不分。BigWorld 同病异形：.def 单文件混 Client/Base 方法段（ioc-review §16.2），Mercury `InterfaceMinder::add` 按 `elements_.size()` 顺序分配、暴露方法经 `ExposedMethodMessageRange` 在同一张 0-254 表占保留段（`interface_minder.cpp:38/:53-70`、`method_description.hpp:31`）——单一 ID 分配表不分受众。Apollo 规避（sdk-contract §11）：单契约源 + msg `domain` 属性、msg id 按域分段独立排布、schema_hash 按域双份（client_hash 只覆盖 client 域——握手稳定、bin 不重发）、apollo_gen 按域投影（客户端 bin 只含 client 域）、XSD 跨域引用禁令；域分段逻辑为硬规则，物理组织（单文件/include 聚合）不设强制。

### 2.3 skynet（本地实读，A 级）

- **进程拓扑**：**无预设游戏拓扑**——单进程内成百上千 service（actor），跨进程靠 gate/多 skynet 节点 + cluster 通信；游戏业务的 login/gate/scene 全是业务层 service 角色约定，非引擎概念。
- **并发模型**：多 worker 线程从全局队列取 service 消息，**单 service 内严格串行**（`skynet-src/skynet_start.c:156-167`，`thread_worker` → `skynet_context_message_dispatch`）；无统一 tick，纯事件驱动；MQ 溢出（默认 1024）报错、monitor 监控死循环（ioc-review §16 实证）。
- **AOI**：引擎不内置；社区以 service 实现九宫格/十字链自建（如 sproto 生态项目，D）。
- **同步/属性复制**：不内置；消息即一切（sproto/自定协议）。
- **脚本集成**：Lua 一等公民，业务 service 全 Lua；云风著《Skynet 框架设计与实现》。
- **扩展机制**：C 服务（.so）动态注册 + Lua 层封装；两代 IPC（harbor→cluster）证明其扩展路线是「自研而非引中间件」。
- **典型生产规模**：简悦/灵犀互娱《陌陌争霸》《心动庄园》；陌陌 MMO 项目上线前咨询云风（[codingnow 博客](https://blog.codingnow.com)）；顺网科技年报披露 skynet 引擎游戏（[年报](https://vip.stock.finance.sina.com.cn)）；三七互娱等招聘要求 skynet（D 级口径，多个来源）（C/D）。
- **失败教训**：① 起源即教训——原框架 Erlang 写、性能不达标，云风 2012-07 用 C/Lua 重写（[gameres 访谈](https://www.gameres.com/867001.html)）；② sproto 无内建版本字段，协议演进靠人工纪律（ioc-review §16 实证）；③ 无内置存储/日志管线，业务自建面大——是「框架」而非「引擎」的代价。

### 2.4 Pomelo（NetEase，网络资料，C 级）

- **进程拓扑**：master + server 目录约定：frontend（connector/gate）/backend（area/chat/…），master 负责进程管理与 RPC 路由表。
- **并发模型**：Node.js 事件循环单线程/进程，多进程水平扩展。
- **AOI**：不内置（area 服务示例里有简单的广播域，D）。
- **同步/属性复制**：不内置属性复制；提供 **filter 链**（before/after handler filter）做横切处理，channel.broadcast 广播；同步模型业务自建（[归档仓库](https://github.com/NetEase/pomelo)）。
- **脚本集成**：JS 即脚本（Node.js 本体），无独立热更设计。
- **扩展机制**：组件/插件体系（component/plugin）、protobuf 协议 + route 字符串。
- **典型生产规模**：官方示例 Lord of Pomelo 为 demo 规模；无可靠生产 CCU 公开数据（D）。
- **失败教训**：**2023-09-25 官方归档转只读**，npm 最后一版 2019-11-08（[npm](https://www.npmjs.com/package/pomelo)）；社区续命 fork pinus（[pinus](https://github.com/node-pinus/pinus)）。教训：绑定单一语言运行时的框架随该运行时的生命周期沉浮。

### 2.5 NoahGameFrame（网络资料，C 级，资料有限）

- **进程拓扑**：分布式插件框架，多 server 进程（自述 actor library + network library）（[GitHub](https://github.com/ketoo/NoahGameFrame)）。
- **并发模型**：多线程 + actor 库（自述口径，未实读源码）。
- **AOI / 同步**：自称支持实时多人，AOI/复制细节公开资料有限（D）。
- **脚本/扩展**：插件（动态库）驱动，属性/事件驱动组件模型。
- **典型生产规模**：约 4.1k star；无可靠生产案例公开数据（D）。
- **失败教训**：维护趋缓（近年无实质提交，D）；文档薄弱，社区以中文圈为主。

### 2.6 Unity Netcode for GameObjects（官方文档，C 级）

- **拓扑/并发**：**客户端引擎网络层**，非服务器框架——host/dedicated server 单进程承载（官方[文档](https://docs.unity3d.com/Packages/com.unity.netcode.gameobjects@latest)）；MMO 级拆分需自行叠后端。
- **AOI**：无内建 AOI；提供 Object spawn/visibility 回调自建（C）。
- **同步**：服务器权威 `NetworkVariable<T>`（默认仅服务器/Owner 可写，delta 下发 + OnValueChanged）；`ServerRpc`/`ClientRpc`（C）。
- **脚本/扩展**：C# 组件模型；Transport 层可替换。
- **典型规模**：小房间制（十人级）为主（C/D）；MMO 场景非设计目标。
- **失败教训**：前身 MLAPI 被收编为 NGO 后 API 大改、版本迁移成本高（社区共识，D）；对服务器侧进程拓扑/持久化完全不管——用它做 MMO 等于只买了客户端同步件。

### 2.7 Unreal Replication（官方文档，C 级）

- **拓扑/并发**：单 authoritative server 进程（GameThread 驱动复制收集），多实例靠进程级开多个 dedicated server。
- **AOI**：**relevancy** 机制——`NetCullDistanceSquared` 距离球 + `bAlwaysRelevant`/`bOnlyRelevantToOwner` 例外位（[Unreal Python 文档](https://dev.epicgames.com/documentation/unreal-engine)口径、[Neukirchen Compendium](https://cedric-neukirchen.net/docs/multiplayer-compendium/replication)）；带宽按 `NetUpdateFrequency`/`NetPriority` 配额分配。
- **同步**：属性复制（`DOREPLIFETIME` + `COND_*` 条件：COND_OwnerOnly/SkipOwner/InitialOnly…）+ dormancy（`DORM_Never/DORM_Initial`，休眠 actor 整体出复制列表）（[Dormancy](https://dev.epicgames.com/documentation/unreal-engine/actor-network-dormancy-in-unreal-engine)、[Conditional Replication](https://dev.epicgames.com/documentation/unreal-engine/conditional-property-replication?application_version=4.27)、[Replicate Actor Properties](https://dev.epicgames.com/documentation/unreal-engine/replicate-actor-properties-in-unreal-engine)）。
- **脚本**：Blueprint 不可热更（运行中改蓝图逻辑需重启/重新加载关卡，C）。
- **扩展**：ReplicationGraph（大型场景复制图）为官方性能出路（社区口径 D）。
- **典型规模**：开放世界多人（数十人/实例）成熟；MMO 级需自建分区后端（C/D）。
- **失败教训**：属性复制默认全量对比逐连接打包，大型世界必须上 ReplicationGraph，否则 CPU/带宽双瓶颈（社区共识，D）；蓝图非热更与 MMO 常驻运维相性差。

### 2.8 EQEmu（网络资料 + 论坛口径，C/D 级）

- **进程拓扑**：login server（认证）→ world（中枢：角色选择、zone 进程管理）→ **zone 进程 ×N**（`eqlaunch` 拉起，`./eqlaunch first/second/zone`）+ ucs（聊天）+ queryserv（DB 查询卸载）（[EQEmulator 论坛启动序列](https://www.eqemulator.org/forums)）；zone 配置与动态数据经**共享内存**分发（论坛多处「shared memory error after reboot」佐证）。
- **并发模型**：多进程（每 zone 独立进程），zone 内单线程 tick。
- **AOI**：EQ 原版按 zone 边界切世界（zone 即加载单元），无细粒度 AOI（公开口径）。
- **同步**：服务器权威 spawn/移动/属性包（原版协议近似）。
- **脚本**：**Perl + Lua 双语 quest 脚本**（`zone/embparser.cpp` 嵌入式 parser，脚本注册 NPC 事件），社区正把 Perl 资产迁 Lua（[论坛](https://www.eqemulator.org/forums)）。
- **扩展**： quest 脚本 + DB 表驱动（内容全在 MySQL，见 §4 问 11）。
- **典型规模**：复刻社区服，单服数十至数百 CCU（D）。
- **失败教训**：共享内存方案的运维脆弱性（重启后 segment 失效类问题常见于论坛，D）；zone 粒度切世界导致 EQ 式"加载画面"体验——**反面印证**了无缝世界与副本制之间的折中路线。

### 2.9 WoW 生态（TrinityCore/CMaNGOS 近似，D 级为主，存疑标注）

- **拓扑**（公开模拟器实现口径）：authserver（认证）+ worldserver（全部世界逻辑）双进程（[TrinityCore](https://github.com/TrinityCore/TrinityCore)）。
- **并发**：worldserver 内 **per-map 多线程**（每张地图一个更新上下文，线程池执行；Lua 脚本绑定 per-map 状态的社区讨论佐证 [talk.trinitycore.org](https://talk.trinitycore.org)；源码未实读，存疑）。
- **AOI**：网格 cell 划分（grid/cell 二级）+ 视距（约 90 码 VISIBILITY_* 常量口径，社区资料，存疑）；玩家进出 cell 触发可见性事件。
- **同步**：服务器权威移动/属性包（SMSG/opcode 体系）；opcode 硬编码于源码表。
- **脚本**：C++ 脚本模块（TrinityCore）+ SQLite? 不——脚本内容 DB 表驱动；CMaNGOS 同构。第三方 Lua 引擎为社区扩展（[论坛讨论](https://talk.trinitycore.org)，D）。
- **扩展**：DB 表驱动内容（creature/quest/loot 全在 MySQL），opcode 编译期硬编码（D）。
- **典型规模**：暴雪真实架构不公开；模拟器单服数百至千人级（D）。**WoT 类比**：WoW 官方同时在线峰值公开口径达千万级（全网多服，非单服，D）。
- **失败教训**：① 单 map 内全序串行是理论上限（per-map 线程无法再细分，社区共识 D）；② 私服生态的法律风险与代码质量参差是工程教训而非架构教训；③ 本节全部为近似依据——暴雪内部资料缺失，引证强度最低。

### 2.10 Space Engineers（MSE）——资料不足专条

任务书所指 "MSE" 在公开网络检索不到对应工程（多组关键词组合均空手而归）；最接近的公开材料：
- 2018-07 Update 1.187 "Major Overhaul of Multiplayer"（[Marek Rosa 官方博客](https://blog.marekrosa.org/2018/07/space-engineers-multiplayer-overhaul)）：服务器权威化、专用服务器优化、Safe Zones——从 P2P host 向 dedicated server 权威模型的迁移。
- 官方 Wiki 多人页与专用服务器页（[wiki](https://spaceengineers.wiki.gg/wiki/Multiplayer)）。
- "MES (Modular Encounters Systems)" 是 Space Engineers 的 NPC 遭遇模组框架，与服务器框架无关（命名近似，疑为任务书笔误来源）。

**结论**：物理沙盒的**服务器权威物理同步**（防作弊同步刚体状态）与 MMO 属性同步是两个问题域；Keen 的 1.187 重构证明"从客户端信任迁到服务器权威"是物理游戏的真实痛点，但公开资料不足以支撑八面分析。按任务书允许，如实标注资料不足，不进对比表主列（仅占位备注）。

---

## 3. 多维对比表

| 维度 | BigWorld | KBEngine | skynet | Pomelo | NoahFrame | Unity NGO | UE Replication | EQEmu | WoW/TC 生态 | Apollo |
|---|---|---|---|---|---|---|---|---|---|---|
| **并发模型** | 多进程+单线程 tick/进程+TimeQueue | 多进程+单逻辑线程 10Hz | 单进程多线程+actor 串行 | 多进程 Node 事件循环 | 多线程 actor | Unity 主循环单线程 | GameThread 单线程复制 | 多进程 zone 单线程 | per-map 多线程（存疑） | 单 Zone 20Hz 全序+MPSC 池（05 §3.3） |
| **进程拆分** | cellapp/baseapp/loginapp/machined | 同 BW 系谱+dbmgr | 无预设（service 约定） | connector/area/master | 多 server 插件进程 | 无（单进程） | 单 authoritative 进程 | world/zone/ucs/queryserv | authserver+worldserver | Gate/World/Orchestrator/Zone/AOI/Battle/DataProxy/Social/LogAgent（00） |
| **AOI** | witness+ghost+detail 档 | 十字链 cellapp 内 | 不内置 | 不内置 | 有限公开资料 | 无（回调自建） | relevancy 距离球+优先级 | zone 边界即 AOI | grid/cell+视距（存疑） | 独立 AOI 服务：网格+四叉树+shard（17 §5） |
| **状态同步** | 属性 filter/detail level/双序号 | .def flags+dirty 打包 | 不内置（消息自建） | 不内置（filter 链） | 有限公开资料 | NetworkVariable 权威 delta | COND_*+dormancy+频率配额 | 权威 spawn/移动包 | opcode 权威包（存疑） | 8 位 SYNC_*+50ms 批差分+acked_seq 水位（05 §6、attribute-sync §10） |
| **脚本热更** | Python（cell/base） | Python+工具链 | Lua 一等公民+热更 | JS 本体 | 插件动态库 | C#（IL 热更社区方案） | Blueprint 不可热更 | Perl/Lua quest | C++/社区 Lua（存疑） | Lua 5.4+sol2 白名单（契约生成） |
| **存储** | MySQL+XML 双后端（`db_storage_mysql/xml`） | MySQL-only（`lib/db_mysql`） | 无内置 | 社区 dao（MySQL/Redis） | 自带简单存储（D） | 不涉及 | 不涉及 | MySQL+共享内存 | MySQL（官方依赖） | MySQL8+Redis+ClickHouse，journal+快照两段式（00、attribute-sync §8） |
| **可观测性** | message_logger/bw_profile 工具链 | logger 独立进程 | monitor+debug_console+logger | log4js 插件（C） | 基础日志（D） | 引擎外自建 | 引擎外自建 | logsys（D） | GM 命令+日志（D） | LogAgent→Kafka→ClickHouse+运行时 ConsoleEvent（00） |
| **社区/许可证** | 闭源被收购（4500 万美元，2012） | 开源 MIT，中文社区为主，节奏放缓 | MIT，云风+国内生态活跃 | **已归档**（2023-09） | MIT，趋缓 | Unity 官方维护（随 Unity 商业政策） | Epic 官方维护（5% 分成模式） | 开源社区（C/D） | GPL 系私服生态（D） | 自研（本仓库） |

---

## 4. 设计取舍深挖（十一问）

### 问 1：逻辑线程模型——单线程逻辑 vs 多线程 vs 多进程

- **skynet**：单进程多 worker 线程 + actor。取舍点：worker 抢全局队列（`skynet_start.c:156-167`），单 service 内串行——逻辑无锁，代价是 service 阻塞即饿队列，隔离手段=MQ 溢出告警（1024）+monitor 抓死循环（A 级实证）。卡顿隔离粒度=service（消息队列）。
- **BigWorld/KBEngine**：多进程单线程 tick。隔离粒度=进程（cellapp 崩溃只死一个区域）；代价=一切跨实体交互都是 RPC。BW 的 TimeQueue（`time_queue.hpp:60/:74`）与 KBE 的 timer（`timer.h:101/:108`）都是单线程定时器轮，实证两家把「定时」也收进逻辑线程。
- **TrinityCore**：per-map 多线程——地图是并行单位、map 内仍全序（存疑口径）。卡顿隔离=map。
- **EQEmu**：极致多进程：一 zone 一进程（eqlaunch 启动序列实证），隔离最彻底，代价是内存/句柄开销与跨 zone 通信最重。
- **UE/NGO**：单线程游戏循环+复制收集，隔离=实例进程数（C）。
- **Apollo 取舍**：ZoneServer 内 20Hz 单逻辑主线程全序 + 网络/DB 线程池 MPSC 旁路（05 §3.3）。**不照抄** TC 的 per-map 多线程：per-viewer acked_seq ChangeHistory 依赖全服单序（attribute-sync §10.1），多逻辑线程需要把水位表分片，收益不抵复杂度；**不照抄** skynet 纯事件：刷库/差分/心跳需要稳定节拍（同 §10.1 tick 论证）。卡顿隔离=场景实例（17 §4.3 缓冲 AOI 事件按帧处理）+Battle 独立实例（25 §3.2）。

### 问 2：cell/base 拆分——为什么拆、代价、边界迁移

- **BigWorld**：cellapp（空间逻辑+AOI+ghost）/baseapp（玩家代理+持久化+备份）职责拆分——DB IO 永不占 cell tick（docs/BigWorld架构深度解析.md §四）。边界迁移：`Entity::migrate()`（`cellapp/entity.hpp:283`）+ `migratedAll()`（:284，等全部 ghost 确认迁毕）+ `buffered_ghost_message*` 全家（迁移窗口期消息缓冲）。代价由其文档自证：ghost 双写、边界协商、跨进程调试不可单步（架构深度解析 §五缺点 1-4）。
- **KBEngine**：同构拆分（`kbe/src/server/{cellapp,baseapp}`），dbmgr 第三拆（DB 门面独立进程）；迁移机制细节本机浅克隆未深读，不展开（如实标注）。
- **Apollo 取舍**：**不拆 cell/base，改拆 Zone/DataProxy**。拆的理由 BW 已证明（IO 与逻辑分离、故障域分离），但 Apollo 的世界是实例制——Zone 就是一个场景进程，无跨进程空间边界，ghost 机制整块不需要；持久化独立成 DataProxy 服务（00 拓扑），存储协议与游戏协议彻底分家（sdk-contract §7 的两段式）。跨 Zone 迁移=场景级 FSM（17 §3.1 TransferPlayer、生命周期 Create→Recycle），以「整场景」为迁移单位，不存在实体级边界协商。

### 问 3：属性同步实现方式（粒度/带宽策略）

- **BigWorld**：属性级 **detail level 档位**（`entity_cache.hpp:89-90/:186`，uint8 档号）——距离决定档位，档位决定字段子集与频率；`AoIUpdateScheme` 按距离动态调密度（`aoi_update_schemes.hpp:75` `apply(scheme, distance)`）；witness 双序号（volatile 移动流+event 离散流，`witness.cpp:2470-2516`）保证按观察者补洞。带宽策略=**空间驱动的分级降载**。
- **KBEngine**：.def 属性 flags 声明可见域，服务端 dirty 打包下发；客户端 SDK onData 逐属性应用（SDK 未实读，C 级）。带宽策略=**声明式可见域**，无分档。
- **Pomelo**：无属性复制；filter 链（before/after）是请求管线横切，不是同步机制（C）。
- **UE**：三件套——relevancy（`NetCullDistanceSquared` 距离球）×频率（`NetUpdateFrequency`+`NetPriority` 带宽配额）×条件（`COND_OwnerOnly/SkipOwner/…`）+ dormancy 整体休眠（三份官方文档，链接见 §2.7）。带宽策略=**条件位+配额**，最精细但全在 C++ 宏里。
- **Unity NGO**：NetworkVariable 权威 delta（C）——粒度=变量，无条件位、无分档。
- **Apollo 取舍**：8 位 SYNC_* bitmask 声明 SELF/TEAM/GUILD/AOI/WORLD 五域（05 §6.3）+ 50ms 批差分（05 §6.4）+ per-viewer acked_seq 水位补洞（attribute-sync §10）。**学 UE 的条件位**但放进契约字段而非宏（sdk-contract §2.3 `sync=`）；**学 BW 的距离分档**思想，落成 dual-axis markers 近密远疏（attribute-sync §10）；**不学 KBE 的单档可见域**——五域掩码是 KBE flags 的超集，且与契约同源。

### 问 4：契约如何定、版本演进

- **BigWorld/KBEngine**：.def XML→解析生成（BW `entity_description.cpp:184-190`、KBE `entitydef.cpp:188-210`，C-50：本就是 XML 但无 schema 层）；演进=改文件重启，无版本握手。
- **Pomelo**：protobuf+route 字符串硬编码（C）；演进靠人工同步，无校验。
- **skynet**：sproto 无内建版本字段（A 级实证，ioc-review §16）；社区惯例「加字段靠默认值兼容」。
- **UE**：编译期宏（DOREPLIFETIME/UHT 生成）——改协议必须重编译（C）。
- **NGO**：运行时注册 NetworkVariable，无独立契约文件（C）。
- **EQEmu/TC**：opcode 硬编码于源码表（D）。
- **Apollo 取舍**：XML+XSD 契约（xs:key/keyref/enumeration）+ 生成器 + `schema_hash` 握手（服务器同时支持 N/N-1）（sdk-contract §2.3/§6、attribute-sync §7.2）。学 KBE「一端声明多端生成」的精髓；补两家都没有的 XSD 校验；hash 而非手工版本号防漏改。演进纪律：增字段兼容、改语义升版、删除双端同周期（sdk-contract §6）。

### 问 5：数据落库时机、缓存层、断线重连

- **BigWorld**：baseapp 周期性备份链（`baseapp/backup_sender.cpp`、`offloaded_backups.cpp`、`write_to_db_reply.cpp` 文件实证）+ 关机快照；DB 后端双插（`lib/db_storage_mysql`/`db_storage_xml`）。缓存层=baseapp 内存即权威，无独立缓存进程。
- **KBEngine**：Archiver 平滑刷库（`archiver.cpp:20-70`：每 tick 取头部区段，300s 周期口径；`shouldAutoArchive` 支持 KBE_NEXT_ONLY 单次写后停档）；dbmgr 集中写（`buffered_dbtasks.cpp`）。断线重连=baseapp 端实体保活窗口后重投（公开口径 C/D）。
- **EQEmu**：zone 周期性 char 存盘（论坛口径 D）。
- **TC/WoW 生态**：worldserver 直写 MySQL，无 write-behind 抽象（D）。
- **skynet/Pomelo/NGO/UE**：不内置（前者树内无 DB 组件，实读清点；后两者客户端引擎不涉及）。
- **Apollo 取舍**：write-behind journal（先追加日志后快照，两段式）+ 列提升机制（attribute-sync §8/§8.2）；KBE 平滑算法改造为「每 tick 取脏实体数/目标刷库 tick 数的头部区段」防突发摊不平；Redis 在 DataProxy 之后做会话/热数据（00）。断线重连=enter-view 全量快照 → dwell 增量 → leave 清理三段式（attribute-sync §6、sdk-contract §4 层 3），与 BW witness 的按观察者补洞同构但下沉到契约层（快照带 attr_schema_version，新旧实体并存期客户端按版本选解码表，sdk-contract §6）。

### 问 6：日志收集链路

- **BigWorld**：日志做成引擎组件——独立 message_logger 进程聚合 + bw_profile 剖析工具（`server/tools/` 清点）——「BI 链路」的引擎内形态。
- **KBEngine**：独立 logger 进程（`kbe/src/server/tools/logger`）+ guiconsole；日志级别进程内配置。
- **skynet**：三板斧——logger service + debug_console + monitor（`skynet_start.c:209-211` 启动序列实证，ioc-review §16）。
- **Pomelo**：log4js 插件（C）；**EQEmu/TC**：logsys 分类文件/GM 命令日志（D）。
- **Apollo 取舍**：不造日志轮子——业务事件经 LogAgent 出 Kafka，分析面 ClickHouse（00）；运行时运维指令走 ConsoleEvent/IConsoleEventSource 接口（ioc-review §16.8 两段式：原语在模块、聚合在 apps/）。学三家的「日志一等公民」定位，弃其自建存储——ClickHouse 的列存是 message_logger 时代不可能有的选项。

### 问 7：跨服操作的原子性

- **BigWorld**：baseapp 单实体串行回调+备份冗余（baseapp 文件清点）；跨 cell 由 cellappmgr 协调；无 2PC——一致性靠「单写者+备份」。
- **KBEngine**：dbmgr 把 DB 任务集中串行（`dbmgr/buffered_dbtasks.cpp`+`dbtasks.cpp` 文件实证）——「串行化」路线的教科书实现；跨服（跨 cellapp/baseapp）靠 entity mailbox 转发，无事务原语。
- **skynet**：单 service 串行天然原子；跨节点 cluster.call 请求-响应（harbor→cluster 两代实证），失败补偿业务自写。
- **EQEmu/TC**：单库直写无跨服概念（D）。
- **Apollo 取舍**：单逻辑线程全序内串行化（大多数"跨服"其实是跨服务——在 Zone 内本就全序）；真跨进程（邮件/交易）走 DataProxy **单写者** + journal 原子提交；结算类（战斗结果落账）用补偿模式——Result Dispatcher 落账失败由容错层执行回退/补偿（25 §3.4/§7：重赛、结算补偿、checkpoint 恢复）。**不用 2PC**：5000 CCU/P99<60ms 的 SLO 下，跨进程阻塞等待两阶段的尾部延迟不可接受（00 SLO），补偿重试把一致性换成了最终性——与 BW/KBE 的单写者路线同宗，但把「单写者」从进程约定升格为 DataProxy 服务边界。

### 问 8：性能取舍——AOI 算法、tick 率、CCU 瓶颈

- **AOI 算法谱系**：九宫格（国内传统，实现简单、阈值跳变粗糙——docs/BigWorld架构深度解析.md 对比表口径）；**十字链**（KBE `cellapp/coordinate_node.*`+`coordinate_system.h`：x/z 两条有序链，移动只增删邻居，O(1) 临界更新——内存局部性优于九宫格，A 级实证）；**ghost+witness**（BW：跨进程空间共享的代价与能力同源，`buffered_ghost_message*` 全家+witness.cpp）；**relevancy 距离球**（UE：按 actor 配置半径+优先级配额，官方文档 C）。
- **tick 率**：BW/KBE 定 tick（KBE 10Hz 实证）——刷库/差分节拍稳定；skynet 纯事件驱动无 tick——省空转但要自建节拍；Apollo 20Hz 主循环+50ms 差分（05 §3.3/§6.4），Battle 实例 20-50ms 可配（25 §8）。
- **瓶颈**：BW=ghost 带宽+跨 cell 协商 CPU；KBE=单 app 逻辑线程吃满即上限；skynet=单 service MQ 串行（1024 溢出实证）；TC=单 map 全序（D）；UE=GameThread 复制收集（D）。Apollo=单场景全序——解法是**横向**:场景实例化+Orchestrator 负载调度（17 §3.2）+AOI shard 独立扩缩（17 §5.1），SLO 单服 5000+ CCU、P99<60ms（00）。
- **Apollo 取舍**：AOI 独立服务（网格+四叉树+shard，17 §5.1）而非进程内十字链——KBE 十字链快在进程内，但 Apollo 要 AOI 与逻辑进程解耦扩缩；近密远疏用 dual-axis markers 落实 BW `AoIUpdateScheme` 的分档思想（attribute-sync §10）。**不做无缝世界**：BW 的 ghost 一致性成本换不来实例制游戏的收益（追溯表 9 号）。

### 问 9：每框架一段「Apollo 取舍结论」

- **BigWorld**：采纳 Mercury filter 分层、detail level 分档、按观察者水位、64 位分段 ID、日志一等公民定位；否决无缝 cell 世界（ghost 成本）与「被大客户买断闭源」的商业模式教训（4500 万美元收购案）；改造 witness→acked_seq 水位、TimeQueue→主循环驱动。
- **KBEngine**：采纳 .def 契约生成精髓、sdks/ 布局、xlsx2py「生成器在 src 外」、Archiver 平滑刷库算法；否决 .def 存储耦合（五短板）、machine UDP 广播、Python 全功能脚本；改造平滑算法防突发、白名单收紧脚本面。
- **skynet**：采纳 C/Lua 技术栈与单 service 串行的无锁全序、自研代际更替观（harbor→cluster）；否决「框架无预设拓扑」的完全自由——Apollo 保留九服务显式拓扑（00），因为运维与排障需要静态结构；补齐 skynet 缺失的存储/观测标准栈。
- **Pomelo**：采纳多进程 frontend/backend 拆分思想（Gate/World 即其投影）；其归档教训直接支撑 Apollo 的「不绑单一语言运行时」——C++20 内核 + Lua 只做业务层，运行时生命期与 C++ 标准而非社区项目绑定。
- **NoahGameFrame**：插件化方向被 Apollo 否决（无关联容器语义，ioc-review 装配纪律）；其 actor+属性事件驱动与 Apollo 的 ECS 事件流同向，仅作参照。
- **Unity NGO**：客户端同步件定位清晰——Apollo 的四端 SDK（sdk-contract §4）在客户端侧承接同类职责（属性容器+预测双缓冲），但服务器权威与契约同源是 NGO 不可能给的。
- **UE Replication**：条件位/频率配额/dormancy 三件套被改造吸收（契约化 SYNC_*、50ms 差分、日志 journal 化即「落库休眠」的近似物）；Blueprint 不可热更的反面教材支撑 Lua 白名单热更路线。
- **EQEmu**：zone 粒度多进程是 Apollo 场景实例化的远亲（每实例一进程、故障域清晰）；共享内存分发的运维脆弱性提醒 Apollo：跨进程共享状态必须走消息+journal，不做 segment 直读。
- **WoW/TC 生态**：per-map 多线程、grid+视距 AOI、DB 表驱动内容被记录为参照系；因为引证全是近似（D 级），不作为任何 Apollo 决策的唯一依据——这也是本表把「存疑」写死的理由。
- **Space Engineers**：资料不足，唯一可用的启示是 1.187 重构方向——「从客户端信任迁到服务器权威」与 Apollo 上行 intent-only（05 §6.4）同构，佐证而非来源。

### 问 10：帧同步（lockstep）与状态同步的适配

- **只支持状态同步**：BW（witness/属性 filter——服务器权威属性流的极致）、KBE（.def flags+dirty）、EQEmu/TC（opcode 权威包）、UE（属性复制权威）、NGO（NetworkVariable 权威）。五家都把「服务器算逻辑、客户端表现」做进了内核，无 lockstep 内建。
- **两者皆不内置（同步模型业务自建）**：skynet（消息即一切，公开 MMO 项目 SkynetMMO/UnityMMO 为状态同步自建例，D）、Pomelo（channel 广播自建）。理论上一张确定性 service 网格可以跑 lockstep，但社区无成熟先例（D）。
- **lockstep 生态**：上述框架均无内建。RTS/格斗的 lockstep 依赖确定性帧序+输入广播+校验回滚，开源界多为独立方案（GGPO 类）而非服务器框架内建——此为通用结论（D）。
- **可混合的形态**：战斗帧同步+大世界状态同步的混合，要求战斗实例与外部状态**单向解耦**——lockstep 实例只吃初始快照+输入流，只吐结果。
- **Apollo 的选择与适配层**：大世界单轨状态同步（05 §6.4：上行 intent-only，服务端定夺）；战斗 offload 到 Battle 独立实例进程（25 §3.2/§3.3：Input Collection→Validation→Execution→Settlement→Broadcast 五段 Pipeline），结果经 Result Dispatcher 单向落回（25 §3.4），Gate↔Battle 不直连（25 §5）——这个结构恰好是混合形态需要的「单向解耦」。**适配缝三件**：① 契约 channels 可扩展 battle 专用通道（sdk-contract §2.3 现有 movement/attributes/events/control 之外）；② Battle 实例 tick 20-50ms 独立可配（25 §8）；③ Replay/Inspector 关键帧记录（25 §6）是走向确定性回放的第一步。**现状不实现 lockstep**：缺口=确定性运行时（定点数+受限 Lua 指令集+禁浮点），且与 20Hz 主线程的现有全序无冲突——若未来 RTS/格斗玩法立项，在 Battle 实例内单独满足，不污染大世界协议。此为设计缝而非已实现能力，如实声明。

### 问 11：数据库支持矩阵

| 框架 | 官方 DB | 证据 | ORM/连接池/分表 | PG/Mongo/Redis 扩展现状 |
|---|---|---|---|---|
| BigWorld | **MySQL**（生产）+ **XML**（开发/小规模）+ baseapp 内嵌 SQLite | `lib/db_storage_mysql/mysql_database_creation.cpp`、`mysql_billing_system.cpp`；`lib/db_storage_xml/xml_database.cpp`；`baseapp/sqlite_database.cpp`（全部 A 级实证） | .def 驱动自动建表（entity_defs→表）；无内建连接池/分表（公开口径 C） | MySQL 为事实唯一生产路径；XML 后端即官方留的「缝」（文件级实证） |
| KBEngine | **MySQL-only** | `kbe/src/lib/db_mysql` 目录+mysqlclient 库文件（A 级实证）；dbmgr 集中任务 `buffered_dbtasks.cpp` | .def 驱动建表；无 ORM 抽象（直写 SQL 封装）；无分表 | PG/Mongo 无官方支持（社区讨论，D）；Redis 未进官方（D） |
| skynet | **无内置** | 本地树 skynet-src/service 清点无 DB 组件（A 级） | 业务自选 MySQL/Redis 客户端（社区口径 D） | 全开放——没有「官方支持」概念 |
| Pomelo | 无官方统一 ORM | 归档仓库文档（C）；Lord of Pomelo 示范 MySQL+Redis dao（D） | 社区 dao 层 | Redis 民间普遍（D） |
| NoahFrame | 自带简单存储组件（细节公开资料有限，D） | — | — | — |
| Unity NGO / UE | 不涉及游戏 DB（客户端引擎；UE OnlineSubsystem 面向平台服务而非游戏存档） | 官方文档定位（C） | — | — |
| EQEmu | **MySQL**（内容+角色全在库） | 官方仓库定位+论坛（C/D） | 无 ORM；共享内存做 zone 间缓存（C/D） | 无官方 PG |
| TC/WoW 生态 | **MySQL**（auth/world 双库） | 官方构建依赖 README 口径（C/D） | DB 表驱动内容；无分表 | 社区有 PG 移植尝试（D，弱） |
| **Apollo** | **MySQL 8.0 主存 + Redis 缓存/会话 + ClickHouse 分析**（00 技术栈表） | 本仓库 B 级 | **storage.xml「语句即数据」**（MyBatis mapped-statement 形态，xml-generation §6）；连接由 DataProxy 池化；分片键=ServerID Group 位（05 §2.2） | **PG 留缝**（见下）；**Mongo 不留缝**（见下） |

- **PostgreSQL 留缝的理由（落到实现）**：Apollo 的持久层把 SQL 当**数据**而非代码——storage.xml 里是语句集（表/列提升/journal 语句），生成器只做装载（xml-generation §6）。换 PG = 换一份语句集+方言标记，契约、生成器、服务端访问器零改动。这是「学 MyBatis 而非学 ORM」的直接红利。BW 的 db_storage_mysql/xml 双后端在引擎层证明了可插后端可行，Apollo 把插缝从「编译期后端类」下沉到「语句集数据」。
- **MongoDB 不留缝的理由（落到教训）**：两段式（journal 追加+列快照+列提升，attribute-sync §8.2）依赖**关系列语义**——列提升（持久化新字段→加列）在文档模型里退化为整文档 blob，恰好复刻 KBE 的头号短板（attribute-sync §8：blob 化复杂类型、无查询面）。文档库不是「另一种方言」，是「另一种范式」，留缝的成本=重写 journal 重放语义，收益为零——明确不留。
- **Redis 的位置**：只在 DataProxy 之后做缓存/会话（00），**不进一致性路径**——与 KBE 未把 Redis 进官方、Pomelo 只在 dao 层用 Redis 的社区实践一致；write-behind journal 的正确性不依赖缓存层。

---

## 5. 资料不足与存疑汇总

1. **Space Engineers "MSE"**：公开网络与本机（SSEngine 盘点为通用 C++ 支撑库）均未找到对应工程；按资料不足处理，仅 §2.10 专条。
2. **WoW/TrinityCore 节**：源码未实读，per-map 线程、视距常量、opcode 体系均为社区口径（D 级存疑，逐处标注）；暴雪内部架构无公开资料。
3. **KBEngine 客户端 SDK（onData 等）**：SDK 仓库不在本机，客户端侧描述为官方文档口径（C 级）；服务端侧全部 A 级实读。
4. **NoahGameFrame**：AOI/同步细节、生产案例无可靠公开数据（D 级）。
5. **Pomelo 生产规模**：归档后无可靠生产 CCU 数据（D 级）。
6. **WoT 单服 19 万 CCU**：公开报道吉尼斯口径（C/D），全网 1.1M 口径未找到一手来源，未采用。
7. docs/17、docs/25 中的 NNG 表述已被 ioc-review 网络决策取代（口径声明见 §0 第 4 条）。

---

*基线：apollo main @ 30c4ca3a。本地证据：BigWorld 官方 14.4.1 包（`~/workspaces/BigWorld`）、KBEngine 浅克隆（`~/workspaces/kbengine`）、skynet（`~/workspaces/skynet`）、SSEngine（`~/workspaces/SSEngine`，盘点结论见 §0）。行号均为实读核对口径，与 docs/analysis/ioc-review.md §16 证据链一致。2026-09-29 初稿。*
