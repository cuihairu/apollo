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
- [ ] **P2-4 Social/Guild/Party**：Anchor 长期态字段模型扩展（Inventory/Equipment/Quest/Progress）+ Guild/Party 对象与协议。

## P3 —— Deployment / Performance / Observability / DevEx（任务书 §38 四档）

> 出口判据：三模型基准可重复数字；单场景 1000 人广播在目标帧预算内。

- [ ] **P3-1 部署与进程形态**：manager 域目录跨进程化（machined + UDP 广播 G-1 骨架）；instance offload 进程形态；base/baseappmgr 进程名更名（走新术语流程定名，此前不变名）；配置统一（ConfigManager 收口/删除）。
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
- **契约差异清欠**（七件套各文档文末差异清单）：entities.xml `Player parent=Avatar` 冲突待 P2-3 修文；WorldSpace 词根随 P0-3 更名消账；base/baseappmgr 进程名随 P3-1；nng/modules·protocol 随 P1 收口前保持禁用。

## 推进纪律（任务书 §37/§40 + improvement-plan §7）

1. 每批（批次内小步）完成即重跑四门禁（Build / Unit / Integration / Examples），全绿才提交推送；CI 触发跑一轮。
2. 小步提交推送：每批一个提交，中文信息，仅含本批文件；禁止一次性大规模重构；先删后建（收敛）与先建后删（立核）批次分开。
3. 术语纪律：新对象/文件/字段名先查 term-contract；新词先入 glossary 再入契约。
4. 批内文件范围以 improvement-plan.md 文件级清单为准，超范围改动即上报协调。
5. 每批提交同步勾选本文档状态。