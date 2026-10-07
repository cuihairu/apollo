# TODO 任务计划（任务书 P0-P4 重排，2026-10-04）

> 重排依据：`docs/rearchitecture/architecture-review-plan.md`（40 节任务书，用户 2026-10-04 定稿）+ 第一阶段审计七件套（`docs/rearchitecture/`：audit / architecture / object-model / lifecycle / concurrency / persistence / improvement-plan）。
> 2026-09-29 旧 9 批次计划已随本次重排归档：已交付项入「已完成」，未列主线项入文末「遗留登记」，不扩写。
> 术语纪律：术语服从 `docs/design/term-contract.md` v1.0；新词先入 glossary 再入契约。

## 已完成（审计期 2026-10-04）

- [x] 任务书入库（ce91a739）
- [x] 第一阶段只读审计七件套落库推送（aac84a25，仅文档零代码改动）
- [x] 用户审定，解冻进入代码阶段（2026-10-04）

## P0 —— 对象所有权与核心模型重建（任务书 §38 首档；验收 §39 第 1-6 项）

> 目标：object-model.md §3 锚点表 + lifecycle.md 目标时序。出口判据五条：① GAME_MODULE=ON 面全开全仓零错误可编可链；② cell-app 单 scene 单玩家最小闭环（内存锚点+Avatar+enter 路径）；③ Scene 拥有实体/AOI 集合且行为与迁移前等价；④ 生命周期状态机全部可观察可校验；⑤ 四门禁本地全绿 + CI 跑通，Unit 含新增 avatar/scene/instance 单测。

- [x] **P0-1 执行上下文基础**（concurrency C-4）：WorldHost 补编译 + 定帧抽象（tick 10Hz 口径核对收口）+ ThreadAffinity/SceneContext 标注。commit fd61ff7f。
- [x] **P0-2 玩家对象模型三件套**（object-model O-1/O-4）：Avatar 类新建（modules/game/scene，生命周期=进场景生/出场景死，持移动/战斗权威/AOI 广播三职责）；PlayerAnchor 补 Home Zone/route 语义字段 + journal 钩子 + home_zone_id；PlayerId/EntityId 强类型分型（cell_server.cpp:559 型错不可见修复）；space_id/instance_id 分离；cell_server attach 路径改 create Avatar by scene。
- [x] **P0-3 Scene/Instance 模型重建**（object-model O-2/O-3）：Scene 升级为运行时容器（entities_/AOI/enter-leave/tick 六阶段骨架）；拆 MapInstance→WorldSpace→Scene 三层空壳为「SceneDescriptor（Map 资产）+Scene（运行时）」，Instance=带八态状态机（Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed）的 Scene 载体；`world.create_scene / scene.create_instance / instance.enter(player_id)` 变真实 API；world CMakeLists 补 6 源。
- [x] **P0-4 生命周期收口**（lifecycle 全量）：WorldSession Leaving 可观察、Closed 态校验、transfer 异步化+失败回滚；Anchor 六态与 SaveQueue 挂钩；应用关闭协议 stop→service 停→脏数据 flush（占位）→退出。
- [x] **P0-5 构建基座加固**（audit §1.3/§3）：6 未编译源挂 target（world 5 源 + world_host.cpp）、零 app 断链、门禁判据改以新模块为准；game-server 链接面修复。
- [x] **P0-6 双树收敛预备**（无争议删除）：LogManager ODR 消除（旧头四方法并入新实现 → 删旧头与死源、game-server 改调）；死单例第一批（BattleManager legacy 头；AOIManager 保留仅测试态）。
- [x] **PORT-1 跨平台可移植性小批**（CI 三平台底线）：config_value.h 补 `<cstdint>`/`<utility>`（CI 根因为 int64_t 未声明，其余 value_/asInt64 报错全是解析级联，该文件本无 #ifdef 分支）；net_util.h setKeepAliveParams 三平台分支（macOS 以 TCP_KEEPALIVE 等价映射空闲时长 + INTVL/CNT 防旧 SDK 守卫，windows WSAIoctl 原样，Linux 原样）；保活语义不追求三平台一致，登记在案。

## P1 —— 换幕 / 目录 / AOI / 持久化 / 恢复 / 重连（任务书 §38 二档；验收 5、9、10）

> 单进程闭环优先，不增加进程。出口判据：① 两 scene 间换幕端到端；② 在线目录行为契约单测；③ AOI 双实现删除后测试覆盖等价；④ 玩家档案写盘→重启→读回（integration）；⑤ 断线→wait→重连→Avatar 重建。

- [x] **P1-1 Scene Transfer/换幕**：prepare→detach→attach→state sync→resume 协议 + 失败回滚 + 断线处理；transfer 协议消息 + apollo_gen 重生成 + golden check。
- [x] **P1-2 PlayerDirectory/在线目录**：modules/game/session 新 PlayerDirectory（条目=SessionBinding+WorldAssignment+EntryState+anchor_epoch+deadline_tick+zone_id；顶号新顶旧+kReasonKickedByRelogin；Suspended 保活窗口+sweep 到期终结；resume (session_id, epoch) 双锚校验；reconcile/snapshot_reset 对账；SessionUp/Down/Moved/Kicked 事件族经 EventSink）；baseappmgr/base-app 接线（directory_ 权威表 + 事件 sink 日志；bind/unbind 产同形事件）；契约 internal 域事件族 900-905（session_up/down/moved/kicked + snapshot request/reply，dir=P2P，events 通道 reflect）+ apollo_gen 重生成 + golden check；行为契约单测 7 组（test_player_directory.cpp，直编源惯例三平台 Unit 门禁真实执行）。跨进程镜像留 P3。
- [x] **P1-3 AOI 收敛与接入 Scene**：收敛为 SceneAoi 单实现（Scene 持有、scene_id 隔离）——legacy aoi.hpp/aoi.cpp 删除（GetOrCreateCell 查询副作用、LEAVE-only 失效事件随之消灭）+ cell::AOIManager 退役（cell_manager.hpp 删类、cell_server.cpp 删实现）；补 Enter/Sync/Leave 事件分发（SceneAoi::set_event_sink，双向差集）+ ViewerState（逐 observer 水位，viewer_state.hpp/cpp）；cell_server broadcastToViewers 换 viewer set 驱动（网关面留 P3-2）；test_game 9 条 AOI 用例等价迁移 SceneAoi、test_scene 增 5 组事件面/水位用例（15/10 全绿）。
- [x] **P1-4 持久化栈收敛+最小落盘**：五套→一套（删 42 文件：B1 #ifdef 四件/B2 legacy 双拷贝含 orm 树死源/B3 connection_pool 两份/C1-C4 redis 全族+modules/data/redis 整目录+空 wrapper×2/DistributedLock/遗留测试 6 件；SqlTemplate B4 死局核实已在 P0-5 头内 inline 修掉，顺销）；table_loader.h:22 语法修复；最小真链路（SaveQueue::set_worker 真落盘分支 + DatabaseService 文件档案 tmp+rename 原子写 + load 未命中即失败废「player_+id」bootstrap；fromJson/toJson 补 playerId/exp int64/x/y/z 对称；flushDirtyAnchors 以现档为基线防默认值清档）；PlayerData 序列化归位 database_service.cpp；新增 PersistJournal write-behind 骨架（append 定额 drain 快照压薄 崩溃 replay + seq 续接，orm 库）；新增 persist_journal_tests + persistence_chain_tests（直编源三平台执行，含出口判据④ 写盘→重启→读回）。
- [x] **P1-5 Recovery**：三档清单入码（recovery_manifest()，lifecycle §3 矩阵六行：玩家长期态+instance 结算=必须持久化、scene/会话/拓扑=可重算、战斗中间态=可丢）；RecoveryCoordinator 启动序列 restore→admission→ready（异常/拒绝进 Failed 拒服务、状态机单向）；journal 消费侧接线 base-app（write-ahead append 于两保存路径 + autoSaveLoop 定额 drain 压档 + 关停收口；restart 时 journal replay 把位点压成档案）；restore_anchors 锚点重建置 Offline（档案在、会话无，登录再 Online）；recovery_tests 直编源 5 组（含崩溃回放端到端）。场景重开按可重算档由 P0-3 create_scene 路径承接，清单内标注；会话重连路径归 P1-6 resume 承接。base-app 目标受 apollo_protocol 门控（两树不编），base_server/database_service/main 三源 g++ -fsyntax-only 核验过。
- [x] **P1-6 Reconnect/保活窗口**：世界侧（819cd25e）：契约 internal 906-908（golden check 过、client_hash 不变）+ WorldSession 挂机窗口 suspend_window/resume(token,now) 双键校验 + WorldSessionManager sweep_suspended 收口链 + Scene suspend_avatar/resume_avatar + reconnect_tests 直编 5 组。收尾批：gateway 断线纯文本→结构化 GatewayClientDisconnect（codec encode/decode 补齐）；modules/protocol 启用（nng 未装两树同走桩，vcpkg/CI 同形；模块 CMake 补 nlohmann_json 声明对齐根口径）→ gateway-app/login-app/base-app/baseappmgr 四 app 全量编译，gateway_route/protocol_bootstrap/base_anchor/baseappmgr 四套入 ctest：ON 树 30/30、OFF 树 27/27 全绿。历史欠账顺清：CellCrossBorder encode 缺失补齐；GatewayAssignRequest.loginTicket、LoginResponse.loginTicket、GatewayAssignResponse.errorMessage 三字段入结构体与 json codec（admission/login 链路真实依赖，契约不含此域、client_hash 不动）；MessageHeader SizeCheck 26→28 算术修正；base_anchor 测试改档案播种（P1-4 口径 load 不再凭空 bootstrap）。907/908 resume 消息消费面归 P3-2 gateway surface。
- [x] **全量文档对账**（全局令，2026-10-04）：README/docs 全站 vs HEAD 真实现（P0+P1 现状）——引用已删文件的示例换实 API（`AOIManager`→`SceneAoi`；`ECSWorld`/`AttributeManager::set` 等旧稿 API 标注未实现或按真身重写；orm/redis 旧 API 删除）；能力矩阵三态标注（attribute-sync 可见性/持久化行、redis 热数据层、scripting-lua §8 异步层）；历史件加况标注（docs/03、docs/34）；七件套加执行状态注记（improvement-plan）；todo 本条随批勾选。站点页事实修正（quick-start/concepts/runtime/game/data API）已随「docs: 优化展示」域提交。

## P2 —— Battle / ECS / Social / Guild / Party（任务书 §38 三档）

> 出口判据：① 塔防/副本 battle 单 scene tick 闭环（create→enter→battle→reward→leave）；② ECS 收敛无并行实现残留；③ 契约树全量 golden check；④ guild/party 最小 CRUD 单测。

- [x] **P2-1 ECS 收敛**：五套→场景内一套，adopt 支=modules/game/core Entity（Player 不 ECS 化不变；Battle 挂接留 P2-2）。a 批（2bbae2a3）：battle::ecs 全支删除（ecs.h/ecs.hpp/components.hpp+ecs.cpp+test_game 5 用例）+ legacy apollo::ecs（ecs.h）+ ecs_demo（唯一消费方随删；components.hpp 幻影 include 同批消账）。b 批（本批）：cell_manager.hpp 内 Entity/PlayerEntity/EntityManager 删除，实体集合归 Scene（scene.hpp:33 既有裁决「承接 EntityManager 职责，行为等价迁移」兑现）——玩家实体走 attach 路径入 avatars_（不变），非玩家实体 spawn_entity 入 entities_ 随 Scene::tick 驱动；spaceToScene_ 路由复用（未知空间落默认 scene，与旧「空间无关」行为等价）；写方为零的位置字段不再单设存储（NPC 位置承载随 NPC AOI 玩法批，登记在案）；broadcastToViewers 改按实体号；find_scene_by_entity 扩实体集合直查。CellWorldService 失去 EntityManager 依赖（update 由 world_.tick→scene->tick 承接，tick 链等价）。
- [x] **P2-2 Battle Runtime**：三成分全交付。① determinism 四约束可测骨架：base/rng.hpp（PCG32+splitmix64+derive_substream 纯函数，StreamId 分域 CombatRoll/Drop/AiDecision/Proc）+ battle_replay.hpp（ReplayTuple 四元组、BattleEvent 规范序、FNV-1a 64 hash 链）+ BattleRuntime 五段状态机（Created→Entering→Battling→Rewarding→Finished 单向），-ffp-contract=off 浮点纪律，battle_runtime_tests 6 组 + rng_substream_tests 6 组全绿。② battle tick 接入：Instance 挂接闭环（attach_battle 开局装载三态窗口/enter 参战同步/start 随行 begin/Running 态 tick 驱动/finish 随行结算），world→battle PUBLIC 链接，test_instance 补挂接窗口 + tick 闭环 2 组（含同四元组双实例复算 hash 相等）。③ reward 单向口：IRewardSink 抽象 + MemoryRewardSink 落账验证（真 Anchor 接线随 P2-4）。判定域唯一/迭代序归一（结算段按 (player_id) 全序）/随机无游标三约束经测试固化；legacy battle 死头已在 P0-6 清零，本批无残留。
- [x] **P2-3 契约系统修订**：三件全交付。① entities.xml 继承链修文（object-model 差异清单 #2 消账）：Player 实体删除（契约裁决 Player 已废——长期归属 PlayerAnchor、场景空间对象 Avatar，实体继承面无 Player 位，无新词故 glossary 不动）；Avatar 摘脱怪物族为根实体（玩家化身不入怪物族）；Monster←NPC 单继承保留（场景实体族，命名不占玩家词）。② P0-P1 新协议消息入库 12 条（实现侧 modules/protocol 欠账照实入库，字段有符号宽类型映射沿 900 族先例）：client 域登录族 login_request/login_response（16/17，P1-6 收尾批「契约不含此域」loginTicket 欠账消账；设计稿分步握手 LoginHello/LoginAuth 未实现不入库）+ internal 域网关指派对 gateway_assign_request/response（909/910，errorMessage 欠账消账；gateway_client_connect 为 legacy 既有不补）+ 玩家生命周期族 activate/bind_session/assign_world/resolve_route 八消息（911-918，P1-2 目录链路 + §34 baseappmgr 拆分八消息入库；DbQuery 为 base-app 进程内服务面非协议不入库）。③ 全链路闸复用既有护栏：apollo_gen 重生成基线（27 msgs/3 entities，三 hash 更新——client 域新增推 client_hash 属预期）+ golden check ctest + compile 闸 static_assert（Player 断言改「Player<0 已废机器可验」+ NPC 祖先链 [Monster] + Avatar 根实体断言）+ test_contract 随仓条数断言 15→27 同批更新；渲染器复核（client proto 投影 login_ticket、.h 全字段/元数据正确）。
- [x] **P2-4 Social/Guild/Party**：三件全交付（P2 出口判据④消账，P2 全部完成）。① Anchor 长期态字段模型扩展（object-model 差异清单 #1 消账）：Inventory（ItemStack 同 id 合并/足额扣减）+ Equipment（固定 6 槽）+ Quest（进度 KV）+ Progress（通用成长键值）+ 社交归属 guild_id，四域变更一律 mark_dirty（长期态变更即脏，随 SaveQueue/write-behind 落档）。② Guild/Party 对象：新增 modules/game/social（GuildManager guild_id/name 双唯一 + 会长保护[不可移除/仅会长解散/转让须在册]；PartyManager 首位成员即队长 + 退队自动移交最早成员 + 仅队长可踢/解散 + 空队回收；Guild 持久[manager 域]/Party 易失[Zone 域]域别注释钉死）；social 仅依赖 core，最小 CRUD 单测 5 组（test_social.cpp 直编源，出口判据④）。③ 消息面 + gen：client 域 Guild/Party 十消息（18-27：四意图 C2S + 唯一 result S2C 各族，op 1-4 收口；party_create 无字段空消息合法）入库，37 msgs 重生成（internal_hash 不变、client_hash 更新属预期），test_contract 条数断言 27→37 同批更新；**IRewardSink 真实落账接线**（P2-2 尾巴）：session 模块 AnchorRewardSink（score 累计 Progress「exp」键 + mark_dirty，单 Anchor 绑定/他人分账忽略，battle 只出不进不回写），session→battle PUBLIC 链接（无环），测试以同源子流复算选确定性命中 tick 驱动结算。门禁：ON/OFF 树 33/33（+social_tests）、Examples 树 31/31 全绿。

## P3 —— Deployment / Performance / Observability / DevEx（任务书 §38 四档）

> 出口判据：三模型基准可重复数字；单场景 1000 人广播在目标帧预算内。

- [ ] **P3-1 部署与进程形态**：manager 域目录跨进程化（machined + UDP 广播 **G-1 骨架全链已交付**：批 B 库层 7a017025 → 批 C UDP 传输件 0af3099b → 批 D Machined 壳 b59cb53a → 批 E 广播发现 1de58fa6——machined+UDP 广播骨架口径完成；残留为 G-1 主体跨进程化：拉起/重启监督面 + manager 域消费端[恢复相位/在线目录]，另立后续批）；instance offload 进程形态；base/baseappmgr 进程名更名（走新术语流程定名，此前不变名）；配置统一（ConfigManager 收口/删除）。批 A（d36529c5）/批 A2（6a73cede）已交付——配置统一消账（audit A8）：legacy ConfigManager/ConfigNode/ConfigValue 七件删除（旧实现头 + 转发桥双头 + 实现/值源两 cpp + 纯 legacy 测试；parseXml/parseLua 恒 false、热更断头路、生产零消费），apollo_core_config 收口单源 config_registry，json/yaml optional 依赖块随 legacy 消亡，test_core_config 收口为 ConfigRegistry 单测 5 组；四 app config.hpp 零消费字段删 29 项（逐字段 grep 全 app 源验证，dataDir/journalPath/autoSaveIntervalMs 等活字段保留）。 批 B 已交付——G-1 服务发现**库层**骨架（modules/net/discovery）：wire 32B 定长报文（magic "APD1" 0x41504431 + version + op Register/Heartbeat/Deregister + 组件三元身份 + seq，逐字段小端编解码不 memcpy 结构体防对齐/端序漂移；错 magic/version/op/零组件号整包丢），进程侧 DiscoveryBeacon（注册/心跳 seq 自增/优雅注销）经 BeaconTransport 注入发包——UDP 实现归进程壳批、测试注入内存桩零 flake，目录侧 DiscoveryRegistry（收包幂等刷新 last_seen、TTL=3×心跳周期超时死亡入队、优雅注销直接下线不入死亡队列——主动行为与异常死亡语义分立、死亡队列 drain 消费），单线程驱动+喂钟注入不起内部线程；守护进程代码标识 `Machined`（2026-10-07 勘误：本批原记「machined 名不入户 §0-2」系误读 term-contract——§1.3 明列守护进程行、§2 禁用表无 machined，契约 v1.0 定稿名可直接入户）；discovery_tests 7 组，四门禁 ON/OFF 树 34/34、Examples 树 32/32 全绿。 批 C 已交付——UDP 传输库件（modules/net/discovery，对批 B「UDP 归进程壳批」口径收窄：传输库件先行落层，进程壳批缩为守护主循环接线+命名）：UdpBeaconTransport（BeaconTransport 的 UDP 实现，无连接 sendto，点分 IPv4 字面量——主机名解析归部署层）+ UdpFeed（bind+SO_REUSEADDR+非阻塞 try_receive，多读 1 字节侦测超长报文静默丢，bind_port=0 内核分配回读）；Winsock 生命周期随链接静态初始化（net/tcp 同惯例）；discovery_tests 增 3 组 loopback 集成（Feed 生命周期/全链往返/守卫面+超长污染）共 10 组；门禁 ON/OFF 34/34、Examples 32/32 全绿。 批 D 已交付——Machined 进程壳（apps/machined，term-contract §1.3 定稿名，勘误后无命名阻塞）：编队目录面最小闭环——bind UDP（--port 缺省 9600/--interval 缺省 1000ms）非阻塞收包喂 Registry、周期 expire 喂钟（节奏=心跳周期）、死亡事件与成员增减落日志、SIGINT/SIGTERM 优雅关闭；冒烟两轮全链验证（注册→成员可见、优雅注销无死亡行、TTL 超时死亡事件、信号关闭）；apps 无门控条件（仅依赖 net_discovery，双树均构建）；拉起/重启监督面与死亡事件跨进程上报（manager 域消费端）留 G-1 收尾批；门禁 ON/OFF 34/34、Examples 32/32 全绿。 批 F 已交付——Machined 监督面（「拉起/重启」半边，§7 口径：进程拉起归 machined、重启策略=编队配置、mgr 只消费事件）：roster 花名册（name | command args...，# 注释/空行跳过，骨架期不支持引号转义）启动期全量拉起；主循环 waitpid(WNOHANG) 非阻塞收割 + 线性退避自动重启（第 n 次延迟 n×backoff_ms，--backoff 可调，缺省 1000）+ 超限放弃；事件四型落日志（born/child death/restart/give up）；关停 SIGTERM 全子进程收割；POSIX only（部署目标 Linux 服务器，Windows 分支空转告警、发现面照常）；冒烟：born×2→瞬死退避重启×5→give up→SIGINT 收割存活子进程无残留；门禁 ON/OFF 34/34、Examples 32/32 全绿。 批 E 已交付——G-1 发现层 Query/Advertise（§7 UDP 广播发现引导路径）：wire 增 op 4/5 + kDirectoryComponentId=1（machined 自身不入编队表，Registry 成员面隔离拒入）；UdpBeaconTransport::enable_broadcast（SO_BROADCAST 幂等）+ UdpFeed::try_receive 发送方端点捕获（recvfrom）；DirectoryLocator 新建（send_query 回执端口语义 / collect_reply 等 Advertise / locate 阻塞重试）；machined 发现应答（Query→Advertise 单播回执 service_port@sender，独立 reply_transport）；discovery_tests 增 3 组共 13 组（含真广播到达 + 全握手 loopback）+ 真进程冒烟；门禁 ON/OFF 34/34、Examples 32/32 全绿。
- [ ] **P3-2 性能**：三模型 benchmark 桩（1×1000 / 10×100 / 100×10，benchmark.cpp 接线）；scene 级锁粒度；帧预算表实测化。
- [ ] **P3-3 可观测性**：日志三套收口末批（P0-6 后 infra 统一）；metrics/tracing 最小集；VerifierApp/LoggerApp 立项。
- [ ] **P3-4 DevEx**：CLI 脚手架（starter 补实或删除）；app 一键起停。

## P4 —— Documentation / Examples / Benchmark / Tutorial（任务书 §38 五档 + §33/§34）

- [ ] **P4-1 文档修订**：quick-start 引用实 API；apps 文档 9 项差异复核；tests/README 墓碑清理；README 第一屏按 §34 重构（Player/Scene/Instance/AOI/Battle/Persistence 六核心词 + 最小 example）；docs/architecture/overview.md（§35）；ADR-001..009（§36）。
- [ ] **P4-2 最小可运行示例** = 任务书 §29 全链路（login→lobby→create instance→enter→spawn→AOI→battle→reward→leave）——P0-P2 完成后即全流程验证器。
- [ ] **P4-3 Benchmark/Tutorial**：三模型基准复用 + 新手上路教程。
- [ ] **P4-4 术语承接**：全仓 term-contract 零出现（禁用词扫描清零：Space / nng / Battle(space 义) / Player(玩家实体义) / baseapp 进程名）。

## 遗留登记（已关 / 未列主线项，不扩写）

- **旧 9 批次计划**（2026-09-29，docs/36 决策表依据）：批次 1 契约系统已交付；批次 2-4 重组件（db-app/cell-appmgr/base-appmgr）中与任务书重叠者已并入 P1-P4 对应批次；批次 5 AI 寻路（Recast/Detour）、批次 6 脚本（Lua）、批次 7 客户端 SDK 投影、批次 8 监控运维——审计后未列入轻量 MMO 主线，保持现状登记，主线稳定后评估。
- **审计遗留死件清账**（并入各阶段「先删后建」批内，不留新孤儿）：comval 手写 union、attribute_id 290 常量、LocalServiceDiscovery、queue 族继承件、IPC 旁支（improvement-plan §7.5）。
- **契约差异清欠**（七件套各文档文末差异清单）：entities.xml `Player parent=Avatar` 冲突已随 P2-3 修文消账（Player 删除、Avatar 摘脱怪物族）；WorldSpace 词根随 P0-3 更名消账；base/baseappmgr 进程名随 P3-1；nng/modules·protocol：vcpkg nng 1.11 依赖与解禁尝试已归档（§34.3，git 可溯——nng_socket struct 化/nng_flag 移除/四参 nng_recv 消失，评估为传输层重写级断裂后回退），模块保持禁用，传输层重写（M1 自研内核）随 P3-2 批重估。

## 推进纪律（任务书 §37/§40 + improvement-plan §7）

1. 每批（批次内小步）完成即重跑四门禁（Build / Unit / Integration / Examples），全绿才提交推送；CI 触发跑一轮。
2. 小步提交推送：每批一个提交，中文信息，仅含本批文件；禁止一次性大规模重构；先删后建（收敛）与先建后删（立核）批次分开。
3. 术语纪律：新对象/文件/字段名先查 term-contract；新词先入 glossary 再入契约。
4. 批内文件范围以 improvement-plan.md 文件级清单为准，超范围改动即上报协调。
5. 每批提交同步勾选本文档状态。