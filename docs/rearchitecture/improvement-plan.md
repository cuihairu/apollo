# Apollo 改进计划（improvement-plan）

> 所属：第一阶段只读架构审查配套件之七（收口）。配套：audit.md（仓库层证据）、architecture.md（架构地图）、object-model.md（对象归属）、lifecycle.md（生命周期）、concurrency.md（并发）、persistence.md（持久化）。
> 依据：任务书 §37（不为了重构而重构）、§38（改造优先级 P0-P4）、§39（22 项最终验收标准）、§40（每阶段后重新 Build/Unit/Integration/Examples 四门禁，禁止一次性大规模重构）。
> 术语按 term-contract v1.0；差异清单见文末。本文档是七件套中**唯一有行动导向**的文件；所有先决条件以第一阶段审计结论为准，本阶段不修改任何文件。
> **执行状态（2026-10-04 对账）**：本文是审计时点的计划原件，不随批次改写。执行进展以 `docs/todo.md` 各批摘要与勾选为准：P0-1..P0-6 已交付，P1-1..P1-5 已交付，P1-6 世界侧已交付（gateway 断线结构化消息与 ON 树门禁收尾中）。文末差异清单中已随批闭环的条目（LogManager ODR 与死单例→P0-6；AOI 双实现→P1-3；持久化五套→P1-4；WorldSpace 词根→P0-3）不再逐条回注。

---

## 0. 总体战略

三条主线（与审计发现的三个结构性事实一一对应）：

1. **收敛（Consolidate）**：先删/合并双树并行的 5 套持久化栈、4~5 套 Entity/ECS、两套 AOI、三套日志/三套 RPC——不收敛，任何改造都在 N 套上重复（audit.md §5、persistence.md §2）。
2. **立核（Build the Core）**：PlayerAnchor（长期归属）+ Avatar（场景化身）+ Scene（运行时边界）+ Instance（八态生命周期）四个一等对象的模型与最小闭环——这是任务书 §39 验收 1-10 的全部内容（object-model.md §3 锚点表）。
3. **接线（Wire It）**：默认构建至少能组装并跑通「登录→落点→进场景→移动→AOI 广播」最小链路——任务书 §29 的最小可运行示例，贯通三条主线断点（architecture.md §2.2）。

分级（任务书 §38：P0 架构审计/对象所有权/Player/Scene/Instance 模型/生命周期 → P1 Scene Transfer/PlayerDirectory/AOI/Persistence/Recovery/Reconnect → P2 Battle Runtime/ECS/Social/Guild/Party → P3 Deployment/Performance/Observability/DevEx → P4 Documentation/Examples/Benchmark/Tutorial）。本计划已吸收第一阶段审计（七件套）产出，P0「架构审计」项即本交付物本身，**P0 从对象模型重建开始**。

---

## 1. P0 —— 对象所有权与核心模型重建（任务书 §38 首档）

> 目标状态：object-model.md §3 锚点表 + lifecycle.md 目标时序；验收 = 任务书 §39 第 1-6 项（Player 一等/内存权威/Scene 为运行时边界/Instance 一等/迁移/Scene 拥有 Entity/AOI/Runtime）。

### P0-1 建立「执行上下文」基础（concurrency C-4 → 消除）

原因：无场景线程概念前，一切归属重建都在悬空。
- 引入 `ThreadAffinity` 标注（对象声明 owner-context；代码件：modules/core 或 game/core 头文件）与调试校验（release 断言）。
- `WorldHost` 补编译目标（modules/runtime/CMakeLists.txt 增 world_host.cpp）+ 定帧抽象（WorldTickContext 已在其头中，落地实现；tick_rate 与 clock-and-time 文档 10Hz 对齐，diff 见 concurrency 差异清单 1）。
- **文件级变更**：
  - modules/runtime/CMakeLists.txt（+world_host.cpp，修复 C-95 型断链）
  - modules/runtime/src/world_host.cpp（实现定帧/追赶/跳拍，clock-and-time 口径）
  - 新增 modules/game/core 下 ThreadAffinity/SceneContext 头（若设计批次已有则按 design 文档命名）

### P0-2 玩家对象模型三件套落地（object-model O-1/O-4，任务书 §6）

- PlayerAnchor：补 Home Zone/route 语义字段 + 脏数据出队（journal）钩子 + `home_zone_id`（contract 已定词）；现有六态保留（Transferring 态按契约语义确认可删则删）。
- **Avatar 新建**（契约 §1.2）：`Avatar` 类（modules/game/world 或 scene 模块内），生命周期 = 进 scene 生/出 scene 死；持有移动权威/战斗权威/AOI 广播三职责（player-object-model §2 精简单）；状态从 Anchor 投影（属性/装备/长期 buff）。
- 根除命名空间混用：PlayerId/EntityId 分型（强类型避免传错——cell_server.cpp:559 的 find_by_player(entity_id) 即编译器不可见错误）；space_id 与 instance_id 分离（cell_server.cpp:544-545 的复用撤销）。
- **文件级变更**：
  - 新增 modules/game/scene（或 world）下 avatar.hpp/avatar.cpp
  - modules/game/session/player_anchor.hpp（+HomeZone 字段、-Transferring 若废）
  - modules/game/core/entity.hpp（EntityId/PlayerId 强类型族；TypeId 保留）
  - apps/cell-app/src/cell_server.cpp（attach 路径改为 create Avatar by scene；移除双 id 复用）
  - sdks/contract/entities.xml（Player 实体按 contract 修文——删 `Player` 或并入 `Avatar`；走「先 glossary 后 contract」术语流程，P2 契约批次终验，P0 先做代码侧命名对齐）

### P0-3 Scene/Instance 模型重建（object-model O-2/O-3，任务书 §8/§9/§10）

- Scene 升级为运行时容器：拥有 entities_（承接 EntityManager 职责）、AOI（承接 AOIManager）、玩家进出 API（enter/leave）、tick 阶段（simulate/recalc/aoi/collect/persist 六阶段骨架，attribute-sync 口径）。
- 拆除 MapInstance→WorldSpace→Scene 三层空壳 → **一层 SceneDescriptor（Map 资产引用）+ 一层 Scene（运行时）**；Instance 成为「带玩法八态生命周期（Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed）的 Scene 载体」——任务书 §9 状态机逐态落字段与转换校验。
- `world.create_scene / scene.create_instance / instance.enter(player_id)`（任务书 §27）成为真实 API：由 Zone 世界对象（新 `World`，或 WorldHost 扩展）暴露。
- **文件级变更**：
  - 删除/合并：map_instance.hpp/cpp、world_space.hpp/cpp（或改名为 scene_runtime 语义）；world_space 命名含契约禁词 Space——随合并更名
  - 新增：modules/game/world 下 scene_runtime.hpp/instance.hpp（八态状态机 + 校验表）
  - modules/game/world/src/scene.cpp（扩实现：enter/leave/AOI 集成/tick 阶段）
  - modules/game/world/CMakeLists.txt（补全部源文件——修复 6 源未编译族）
  - apps/cell-app/src/cell_server.cpp（ensureDefaultMapInstance 替换为 world.create_scene；attach 走 instance.enter）
  - modules/game/world/world_session.cpp/hpp（并入 scene_session 语义或删除——见 diff 清单 3）

### P0-4 生命周期收口（lifecycle 全部问题）

- 状态机可观察性修复：WorldSession 的 Leaving 可观察、Closed 状态校验、transfer 异步化（失败回滚分支）；Anchor 六态与 SaveQueue 挂钩（Saving 真做保存或移除）。
- 应用层关闭协议：stop → service 停 → **脏数据 flush**（占位实现，接 P1 持久化）→ 退出。
- **文件级变更**：
  - modules/game/world/src/world_session_manager.cpp（close 流程修正；resume/complete_transfer 加态校验）
  - apps/base-app/src/base_server.cpp（shutdown handler + flush 钩子）
  - modules/runtime/src/application_host.cpp（失败路径回滚已启动 service；is_running 检查语义修正）

### P0-5 构建基座加固（audit §1.3/§3 的断裂全量修复）

- 六个未编译源挂进各自 target（world 5 源 + world_host.cpp）；game-server 链接面修复（LogManager 符号问题随 P0-6 日志统一解决）；modules/protocol 在 P1 收口前保持禁用（避免 nng 依赖），但其互斥的 app 门禁改为「以新模块为判据」。
- 目标：**GAME_MODULE=ON + surfaces 全开时，building 整仓零错误**（四门禁第一道）。
- **文件级变更**：modules/game/world/CMakeLists.txt、modules/runtime/CMakeLists.txt、modules/game/CMakeLists.txt、apps/CMakeLists.txt（门禁判据复核）、apps/cell-app/CMakeLists.txt、apps/game-server/CMakeLists.txt

### P0-6 双树收敛预备（只删不改语义的「无争议删除」）

- 三套日志收敛前置：旧 log_manager.hpp 四方法并入新实现（链虚线：snapshot/clear/set_console_enabled 在新 LogManager 补全）→ 删除 39 行旧头与死源码 log_manager.cpp（ODR 消除）；game-server 改调新 API。
- **文件级变更**：modules/core/log/{include 旧头,src/log_manager.cpp}（删）、modules/core/src/log/log_manager.cpp（删）、apps/game-server/src/main.cpp（改调）、modules/core/src/config/config_registry.cpp（保留，活件）
- 死单例第一批：BattleManager（legacy 头删除，见 P2-1 合并）；AOIManager 单例在 AOI 收敛（P1-3）前保留仅测试态。

> P0 出口判据：① GAME_MODULE=ON 全仓可编可链；② cell-app 能启动 1 scene、进入 1 玩家（内存锚点+Avatar 对象+enter 路径）；③ Scene 拥有实体/AOI 集合（实体从 EntityManager 迁移后的行为等价）；④ 生命周期状态机全部可观察、可校验；⑤ 四门禁（Build/Unit/Integration/Examples）在 CI 可跑通——其中 Unit 至少含新增的 avatar/scene/instance 单测。

---

## 2. P1 —— 换幕 / 目录 / AOI / 持久化 / 恢复 / 重连（任务书 §38 二档）

> 目标状态：验收 5、9、10（迁移/Reconnect/Recovery）；持 P0 模型继续建能力，**不增加进程**（单进程闭环优先）。

### P1-1 Scene Transfer / 换幕（lifecycle §2.3 六差距 → 全消）

- 转移协议：prepare（准入）→ detach（旧 Avatar 销毁，状态打包）→ attach（新 scene 重建 Avatar，Anchor 投影）→ state sync → resume；失败回滚至原 scene；断线发生在转移中的处理路径。
- **文件级变更**：新增 world_scene_transfer 流程模块（或 scene.cpp 内）；world_session.cpp 改造（异步化）；cell_server.cpp handleCellCrossBorder 重写；消息面：新增/修订 transfer 协议消息（sdks/contract/messages.xml + apollo_gen 重生成，golden check 全量走）

### P1-2 PlayerDirectory / 在线目录（audit §2.2 第 8 行；契约 OnlineDirectory）

- baseappmgr 进程内 assignments_+SessionLocator 升为目录服务语义：在线登记（Anchor 挂 HomeZone、Session 挂 gateway）、顶号预裁、事件族（SessionUp/Down/Moved/Kicked——session-and-online-directory §4）、30s 对账、anchor_epoch。
- 单进程阶段：目录仍是内存，但**行为契约完整**（消息面+事件面），跨进程镜像留 P3。
- **文件级变更**：apps/baseappmgr/src/baseappmgr.cpp（扩展状态机）、新增协议消息（messages.xml + gen）；apps/base-app 的 bind/unbind 走目录事件。

### P1-3 AOI 收敛与接入 Scene（object-model O-10；任务书 §13）

- 两套合一：legacy 九宫格（有测试覆盖）与 cell 定长网格 → 选一代持 + 补 ENTER/SYNC 事件分发 + ViewerState（契约 viewer set 逐 viewer 水位）骨架；取消失效的 LEAVE-only 事件（aoi.cpp:95-102）；修 `GetOrCreateCell` 范围查询副作用（aoi.cpp:20,35）。
- Scene 拥有 AOI 实例（scene_id 隔离）；cell-app broadcast placeholder 换成 viewer set 驱动的下发（网关面在 P3-2）。
- **文件级变更**：modules/game/world/src/aoi.cpp（事件补全+副作用修复）；新增 viewer_state.hpp；apps/cell-app/src/cell_server.cpp（AOIManager 移除、场景 AOI 接入）；删 cell_manager.hpp 的 AOIManager。

### P1-4 持久化栈收敛 + 最小落盘（persistence.md §1-§4 全部问题）

- 五套精简为一套（建议保留 apollo::data::core/orm + MemoryConnection 为 mock，删除 B1/B3 与 C1-C3 及双份拷贝）：删除文件面见 persistence.md §2（9 个重复实现文件 + 4 个 #ifdef 死文件 + 2 个空 wrapper）。
- `SqlTemplate` 新头补实现（或换接遗留实现）——修 B4 链接死局；`table_loader.h:22` 语法错误修复（`std:: stringValue;`）；`DistributedLock` 死代码删除（消费者零）。
- 最小真链路（阶段一：单文件 JSON/SQLite 皆可，介质抽象留口）：Anchor dirty → SaveQueue workerLoop 落盘分支 → load 校验；fromJson/toJson 对称修复（x/y/z）；「player_+id」bootstrap 恶龙改为「建号流程」（P2 账号域）。
- write-behind journal 骨架：tick 定额出队 + 快照压薄（attribute-sync §8.2 口径）；崩溃恢复回放。
- **文件级变更**：persistence.md §2 表内全部文件（删/并/改）；modules/data/orm/src/sql_template.cpp（补实现）；apps/base-app/src/database_service.cpp（真读写+废 bootstrap）；新增 persist_journal.hpp/cpp；修复 table_loader。

### P1-5 Recovery（lifecycle §3 全空白 → 三档清单 + 机制）

- 定义「必须持久化/可重算/可丢」三档（任务书 §20）——以 P1-4 的 journal 为基础设施；进程重启回放 + 场景重开 + 会话重连三路径落地。
- **文件级变更**：新增 recovery 设计落地代码（journal 消费侧 + Anchor 恢复）；apps 启动序列（restore→admission→ready）。

### P1-6 Reconnect / 保活窗口（lifecycle §2.4 全链路）

- gateway 会话层（Session 对象）+ resume_token（与窗口同源 TTL）+ Suspended 窗口定时器（WorldSession/Avatar 侧挂机窗口）+ 重连恢复路径（Avatar 按场景规则重建）。
- **文件级变更**：新增 gateway Session；modules/game/world/src/world_session.cpp（窗口定时器）；协议消息（resume）；apps/gateway-app（会话承载 + disconnect handler 补全——先消灭「纯文本 disconnect」）。

> P1 出口判据：① 换幕流程在单进程两 scene 间端到端可跑；② 在线目录行为契约单测通过；③ AOI 双实现删除后测试覆盖等价（测试迁至新实现）；④ 玩家档案「写盘→重启→读回」通过（integration 测试）；⑤ 断线→wait→重连→Avatar 重建通过。

---

## 3. P2 —— Battle / ECS / Social / Guild / Party（任务书 §38 三档）

> 保序但可并用：battle 依赖 P0/P1 的 scene/avatar 边界，social 依赖 P0-2 的 Anchor 长期态。

### P2-1 ECS 收敛（object-model O-9；任务书 §15）

- 五套并存 → 场景内一套（建议 adopt modules/game/core 或 battle::ecs 中最久经测试的一支，其余删除）；**Player 不 ECS 化**（任务书 §15 建议：Player 有专属对象模型）；Battle 按「Scene └─ BattleRuntime └─ ECS」形状挂接（勿全仓 ECS 化）。
- **文件级变更**：删除 legacy ecs.h/ecs.hpp/battle::ecs/components.hpp（含不可编译的幻影 include：components.hpp:4-5）；cell_manager.hpp 内 Entity 迁移；examples/ecs_demo 重写或删除。

### P2-2 Battle Runtime（object-model O-11；任务书 §16）

- 新 BattleSystem 补 tick 语义 + 接入 scene tick（阶段六之后）；battle 状态与 reward 单向落 Anchor（player-object-model §3）；battle-determinism 四约束（PCG32 子流/四元组 replay/hash 链）落地为可测骨架（验证服务 gap #17 留 P4 先行验证对账）。
- **文件级变更**：modules/game/battle/*（实现）；新增 battle_replay.hpp；legacy battle 死头删除。

### P2-3 契约系统修订（audit §2.1/§2.3 第 1 条）

- entities.xml 继承链按契约修文（Player 删除或并入 Avatar；Monster/NPC 语义复核——契约只承认 Avatar 的玩家面，NPC 族可保留为场景实体但命名不占玩家词）；消息面补 P0-P1 新协议消息；全链路 golden check + compile 闸（既有护栏复用）。
- **文件级变更**：sdks/contract/entities.xml、messages.xml、attrs.xml（如需）；sdks/gen/src/main.cpp（渲染器复核）；sdks/cpp/generated/*（重生成，走 check 模式）

### P2-4 Social / Guild / Party（契约：社交面归 Anchor 长期态）

- 玩家长期态字段模型扩展（Inventory/Equipment/Quest/Progress——object-model 差异清单 1 的字段面）+ Guild/Party 对象（manager 域或 Zone 域）与协议。
- **文件级变更**：新增 modules/game/social/*；Anchor 持久态扩展；消息面 + gen。

> P2 出口判据：① 塔防/副本典型 battle 在 1 scene 内 tick 闭环（create→enter→battle→reward→leave，任务书 §30 半程）；② ECS 收敛后无并行实现残留；③ 契约树全量通过 golden check；④ guild/party 最小 CRUD 单测。

---

## 4. P3 —— Deployment / Performance / Observability / DevEx（任务书 §38 四档）

- **P3-1 部署与进程形态**：manager 域目录跨进程化（**G-1 服务发现骨架全链已交付**——批 B 库层 7a017025 → 批 C UDP 传输件 0af3099b → 批 D Machined 壳 b59cb53a → 批 E 广播发现 1de58fa6；拉起/重启监督面已随批 F 交付；G-1 收尾批三增量全交付（2026-10-08，①死亡事件跨进程上报、②在线目录跨进程镜像、③恢复相位跨进程化，各见下批记）；§6 死亡行窗口处置遗留批已交付（roster zone 列 + tick 驱动 + manager 域消费端，见下批记））；instance offload 进程形态（36号 #16——前置设计已立 `docs/design/battle-instance-offload.md`：组件骨架/确定性承载/故障域 + 三拍板点[生命周期协议、传输依赖序、崩溃裁决]待裁）；配置统一（ConfigManager 收口/删除——config 系双轨 diff 见 audit A8；零配置文件落地——批 A/A2 已交付）。
  - **批 A/A2 已交付（2026-10-07，d36529c5 + 6a73cede）**：配置统一消账（audit A8 (b)(c)）。批 A：legacy ConfigManager/ConfigNode/ConfigValue 七件删除——旧实现头（parseXml/parseLua 恒 false、notifyListeners 断头路、热更无调用方、`getValue<T>` 声明无定义，生产零消费）+ modules/core/config 转发桥双头（using 别名 + APOLLO_CONFIG_* 宏）+ config_manager.cpp/config_value.cpp 两源 + 纯 legacy 测试 test_config.cpp；apollo_core_config 收口单源 config_registry.cpp，nlohmann_json/yaml-cpp optional 依赖块随 legacy 消亡；test_core_config.cpp 收口为 ConfigRegistry 单测 5 组（23 个 legacy 用例随之删除），tests/CMakeLists.txt 的 config_tests 目标与「暂时禁用」墓碑注释同批清除。批 A2：四 app config.hpp 零消费字段删 29 项（base 11/cell 4/gateway 7/login 3，逐字段 grep 全 app 源验证；dataDir/journalPath/autoSaveIntervalMs/journalDrainQuota 等 P1-4/P1-5 活字段与 dbHost/dbPort/dbName/maxConnections/sessionTimeoutMs/maxLoginAttempts/lockoutDurationMs/gatewayUrls 等在用字段全部保留），空段标题顺清。两树 33/33、Examples 31/31 全绿。
  - **批 B 已交付（2026-10-07）**：G-1 服务发现库层骨架 `modules/net/discovery`（wire 报文 + Beacon/Registry 两侧行为语义，不含 socket）——①WirePacket 32B 定长（magic/version/op/组件三元身份/seq，偏移表逐字段小端编解码，encode 恒 memset 全包；校验面：错 magic/version/op/零 component_id 整包丢）；②DiscoveryBeacon：register_self/beat（seq 自增）/deregister，经 BeaconTransport 抽象注入发包（UDP 归进程壳批，测试内存桩捕获字节）；③DiscoveryRegistry：on_packet 注册/心跳幂等刷新 last_seen（端口可变更）、Deregister 即时下线不入死亡队列（主动与异常语义分立）、expire TTL=3×interval 超时死亡入队 + drain_deaths 消费；④单线程驱动、时间由调用方注入（骨架不起线程不依赖钟）。术语面勘误随批 C：`Machined` 系 term-contract v1.0 §1.3 定稿代码标识（本批原记「machined 不入户」系误读契约）。测试 discovery_tests 7 组；门禁 ON/OFF 34/34、Examples 32/32。
  - **批 C 已交付（2026-10-07）**：UDP 传输库件 `udp_transport.hpp/cpp`（对批 B 口径收窄——传输库件先行，进程壳批缩为守护主循环接线 + 命名拍板）：①UdpBeaconTransport：BeaconTransport 的 UDP 实现（无连接 sendto 每包带目的端点，幂等 open，点分 IPv4 字面量——主机名解析归部署配置层）；②UdpFeed：Registry 侧收包口（bind + SO_REUSEADDR 守护重启同端口 + 非阻塞 try_receive 供主循环轮询，bind_port=0 内核分配端口回读；≠32B 报文静默丢——多读 1 字节侦测超长）；③Winsock 生命周期随链接静态初始化（net/tcp net_common 同惯例），POSIX/Windows 双平台收口 sock_api_t 转型。测试 discovery_tests 增 3 组 loopback 集成共 10 组；门禁 ON/OFF 34/34、Examples 32/32 全绿。
  - **批 D 已交付（2026-10-07）**：Machined 进程壳 `apps/machined`（term-contract §1.3 定稿名）——编队目录面最小闭环：参数面 `--port`（缺省 9600）/`--interval`（缺省 1000ms，TTL=3×interval）；单线程主循环（收包排空→on_packet / 周期 expire 喂钟 / drain_deaths 死亡行 + 成员计数变更行 / 50ms 节拍）；SIGINT/SIGTERM 优雅关闭。冒烟两轮：注册→成员可见、优雅注销无死亡行、TTL 超时死亡事件、信号关闭。拉起/重启监督面与死亡事件跨进程上报（manager 域消费端）留 G-1 收尾批。apps 无门控条件（仅依赖 net_discovery），双树均构建；门禁 ON/OFF 34/34、Examples 32/32 全绿。
  - **批 F 已交付（2026-10-07）**：Machined 监督面（term-contract §1.3「拉起/重启」半边；§7 口径：进程本身拉起归 machined，重启策略 = 编队配置，mgr 只消费事件不做进程管理）——①roster 花名册（`--roster <file>`，行格式 `name | command args...`，# 注释/空行跳过，骨架期不支持引号转义；缺省无 roster = 纯目录面）；②Supervisor（apps/machined/src/supervisor）：fork/execvp 全量拉起 + waitpid(WNOHANG) 非阻塞收割 + 线性退避自动重启（第 n 次延迟 n×backoff_ms，`--backoff` 可调缺省 1000）+ 重启超限（5）放弃；③事件四型（born/child death[含退出码]/restart[attempt n]/give up[after n]）落日志，跨进程上报归 manager 域消费端后续批；④关停面 SIGTERM 全子进程；⑤POSIX only（部署目标 Linux；Windows 分支空转告警、发现面照常）。冒烟全链：born×2 → 瞬死退避重启×5 → give up → SIGINT 收割无残留；门禁 ON/OFF 34/34、Examples 32/32 全绿。
  - **批 E 已交付（2026-10-07）**：G-1 发现层 `Query/Advertise`（§7 UDP 广播发现的引导路径，KBE machine 广播应答先例的最小对应物）——①wire 增 op 4/5 + `kDirectoryComponentId=1`（machined 自身不入编队表）；Registry 成员面隔离（发现层 op 拒入，machined 路由层分流）；②`UdpBeaconTransport::enable_broadcast`（SO_BROADCAST，幂等）+ `UdpFeed::try_receive` 发送方端点捕获（recvfrom，Advertise 回执路由用）；③`DirectoryLocator`（`directory_locator.hpp/cpp`）：send_query（service_port 承载回执端口）/ collect_reply（等 Advertise，超时续等）/ locate（阻塞重试组合，真实进程启动期用）；④machined 发现应答：Query→Advertise 单播回执到 service_port@sender（独立 reply_transport，service_port=0 守卫）。测试 discovery_tests 增 3 组共 13 组（含 127.255.255.255 真广播到达 + 全握手 loopback）；真进程冒烟（Query→Advertise op=5/comp=1/端口还原）。门禁 ON/OFF 34/34、Examples 32/32 全绿。
  - **G-1 收尾批增量①已交付（2026-10-08）**：死亡事件跨进程上报（machined death 事件 → manager 域消费端接入，§7「mgr 向 machined 注册死亡监听」骨架对应物）——wire 增 op 6/7（DeathSubscribe 订阅注册/刷新幂等 + DeathNotify 死亡上报[reserved0=DeathKind、seq 复用=exit_code]）+ DeathNotifier（machined 侧订阅表/TTL 过期停推/通知广播）+ DeathListener/death_event_from（manager 侧订阅与解码守卫）+ Registry 成员面隔离扩至死亡面；machined 两死亡源合流转发（registry TTL + 监督面 roster 增可选 component_id 列）；baseappmgr --machined-host/--machined-port 死亡订阅面（周期重发刷新 + [death] 日志）。discovery_tests 增 6 组共 19；真进程冒烟 6 死亡事件全链到达；门禁 ON/OFF 36/36·33/33、Examples 34/34 全绿。残留增量②在线目录跨进程镜像、③恢复相位跨进程化续批。
  - **G-1 收尾批增量②已交付（2026-10-08）**：在线目录跨进程镜像（§5/§7/§8 G-1 平面骨架——目录=manager 域权威态，跨进程可见性=「owner 广播 delta、消费者本地镜像 + seq 续传」，镜像=只读投影最终一致）——wire 新立 "APD2" 变长家族（session 库 directory_mirror：公共头 8B + Delta 86B 定长/SnapshotRequest 8B/SnapshotReply 分片 ≤128 条；gateway_addr 不上 wire——定位面非连接面）+ DirectoryPublisher（SendFn 注入、seq 单调、全量分片含空表 1 空片）+ DirectoryMirror（seq 纪律断档 stale+请求钩子、分片重组原子全量重置）+ udp_transport 变长收发延伸（kMaxDatagramSize=16384）；baseappmgr owner 面（--mirror-port/--mirror-snapshot-ms，事件链入 + 初始/周期快照）与 base-app 消费面（--mirror-port 轮询投影 + --mirror-owner-port 请求回投）；已知口径：mark_suspended/resume 不产事件（Suspended 窗口镜像不可见，归 manager 单边）。directory_mirror_tests 新建 9 组；双进程真 UDP 冒烟（周期快照 rx 递增投影同步）；门禁 ON 37/37、OFF 34/34、Examples 35/35 全绿。
  - **G-1 收尾批增量③已交付（2026-10-08）**：恢复相位跨进程化（§6 manager 行 + §7 恢复相位排他的 G-1 平面骨架——manager 死 → machined 拉起 → 恢复相位排他[拒新直至收敛] → 各 Zone/gateway 全量重报 → 收敛开放；目录无 journal 重建 <1s@5000 条）——FullReport wire（"APD2" 家族 kind=4，公共头 8B + component/zone/report_id/total_parts/part_index + 条目×69B[与镜像面同格式，报告方 epoch 恒 0 裁决键归 manager]，单分片 ≤128 条）+ FleetReporter（分片连发、空表=1 空片、零组件号拒发、失败计片 fire-and-forget）+ FleetRecoveryCoordinator（Normal↔Recovering：begin 幂等截止不漂移、分片重组[升序/换轮重启/首片重放丢]、收敛双判据[全报或超时]、last_reported_count 观测、Normal 期照收 intake[Zone 重启路径]）+ PlayerDirectory::intake_full_report（restore-not-kick：保 epoch 刷绑定、无条目 session_up、绝不出 Kicked、空 report 不清目录）+ baseappmgr（--recovery-port/--recovery-expected/--recovery-timeout-ms，启动即排他 set_admission_gate 拒新，收敛开放即发重建快照衔接镜像面）+ base-app 报告方（--report-to-*/--component-id/--zone-id/--report-interval-ms，AnchorManager 快照启动+周期重报）。范围注记：Zone/gateway 死亡行 mark_suspended 窗口处置需 roster zone 列与 tick 驱动，留后续批。fleet_recovery_tests 新建 6 组 + baseappmgr_tests 增准入闸门组；双进程真 UDP 冒烟（排他→重报→收敛 reported=1/1→周期幂等 intake）；门禁 ON 38/38、OFF 35/35、Examples 36/36 全绿。G-1 收尾批至此三增量全交付。
  - **§6 死亡行窗口处置遗留批已交付（2026-10-08）**：Zone/gateway 死亡行 mark_suspended 窗口处置（§6 两行语义补齐，增量③范围注记消账）——roster 花名册补 zone 列（`name | component_id | zone_id | command args...`，第三字段纯数字判据防误切含 '|' 命令、两列/三列旧格式兼容；缺省 0=未分配）+ SupervisorEvent 四路 zone 透传 + machined 死亡合流上 wire（MemberId 三元身份本就携带 zone_id，零 wire 改动）；PlayerDirectory 批量反查 mark_suspended_by_zone/by_gateway（只动 Online、已挂机不重置窗口、Leaving 不回窗、不产事件、反查键 0 拒绝）+ BaseAppMgr 透传（suspend_zone_sessions/suspend_gateway_sessions/sweep_suspended）；baseappmgr 接线 --suspend-window-ticks（缺省 30=§4 建议值，tick=主循环秒拍）+ --gateway-component cid:gid 映射[wire 无 gateway 身份、gateway-app 半成品挂 net M1——骨架期显式映射桥接]+ 死亡消费即处置 + 秒拍 sweep 喂钟（到期删条目+SessionDown(kReasonWindowExpired) 进镜像面）；base-app 增 --demo-anchors 冒烟驱动面（如实注记：真实会话落锚后弃用）。测试 player_directory_tests/baseappmgr_tests 各增 1 组 + supervisor_tests 新建 3 组（Windows 空转跳过）；真进程冒烟四段全链（Zone 行挂起→重启重报恢复 intake=3、gateway 行挂起、4 ticks sweep expired=3、镜像面 seq 3→6 收敛）；门禁 ON 39/39、OFF 34/34（全新 configure 口径，contract add_test 注册怪癖另立小批已记）、Examples 37/37 全绿。
- **P3-2 性能**：按 architecture.md §6 三模型（1×1000 / 10×100 / 100×10）建 benchmark 桩（benchmark.cpp 接线 game world 真实路径，已链 apollo base/core）；锁粒度 scene 级化（concurrency §3 目标模型）；帧预算表实测化（capacity-and-benchmark）。Manager 去锁化摸底收口（2026-10-07）：三 Manager 跨线程消费实证（AnchorManager 主线程+autoSave 线程 snapshot、WorldSessionManager RPC worker 线程全触点、仅 SessionLocator 单线程但与 AnchorManager 同模块不单拆）——去锁化不可独立成批，归并 cell-app 单写者拍板项按新并发模型统一处置（详见 todo.md P3-2 批记）。
- **P3-3 可观测性**：日志收敛末批（批 A 已交付，logging.md §6 收口交付注记）；metrics/tracing 最小集（批 B 已交付 MetricRegistry 最小集——modules/core/metrics 原语载体[counter/gauge+标签规范化+快照确定性出口]，顶层 513 行零消费孤儿 metrics.h 同批删除；owning 模块接线/control 通道上行/admin exporter 未做）；结构化行格式（批 C 已交付——六键固定序+引号规则+LogContext proc/tick 注入+FileAppender structuredOutput 开关默认关，契约源 structured.h 头注）；VerifierApp（gap #17）与 LoggerApp 进程立项（**已拍板（ADR-013，2026-10-10）：LoggerApp 立项**[apps 壳+structuredOutput 开关打开；前置设计已立 docs/design/logger-app.md——apps 接线零实况+v1 tail 三岔+五拍板点]；**VerifierApp 暂缓**——Lua 未入主线，battle-verification-service §5 口径维持）。file_appender 轮转摸底（2026-10-07）：无 rename 步骤原子性疑虑消解；实证缺陷=同日重启索引归零 append 交错写同序号文件+序号无界增长（驱动实跑 app_0.log 复写实证），清理面正常——启动扫描序号续接已修复（构造/setConfig 经 nextFileIndexForDate 续接 max+1，双模式驱动实跑验证交错消除，两树门禁全绿）。崩溃采集批②③④已交付（2026-10-09 网络恢复续批，设计件 docs/design/crash-capture.md 批①2026-10-08——Crashpad out-of-process handler 选型已钉：vcpkg overlay 端口 2026-07-02 接线 + modules/runtime crash_capture helper（init_crash_capture fail-open/本地模式/pending sweep 计数 MetricRegistry）+ 七 app main 最早段接线 + handler POST_BUILD 打包 + APOLLO_ENABLE_DEBUG_SYMBOLS/split_symbols.sh/build_crash_tools.sh 符号面 + --crash-test null 验收开关；验收链实测 exit=139→dump 落盘→重启 pending=1 全链通，详见 todo.md P3-3 批记）。
- **P3-4 DevEx**：CLI 脚手架（**starter 空壳已删——2026-10-08 拍板=删除交付**：modules/starter 双 INTERFACE 零源码 + 幻影测试[三头全仓不存在]且从未入 modular 构建，补实=重建 architecture-review §17 已否决的运行期 Starter 容器且与 runtime ApplicationHost 职责重叠，C-68 消账；装配口径照旧=显式注册+main 收口[§17/§21/§24]，设计文档保留为参考件并同步头注/§14/docs/34 §129 状态注）；app 一键起停（批 A 已交付——scripts/dev_fleet.sh 包装 machined 批 F 监督面：up 生成 roster 后台拉起/roster/status/down SIGTERM 收割；dev 端口布局 9001-9005；编队=生命周期半边，各 app 未接 DiscoveryBeacon 目录注册归 G-1 收尾批；gateway exit=1 根因摸底（2026-10-07）三因实证，**已拍板（ADR-011，2026-10-10）：最小修复处方三步先行**=①start 连接失败降级警告+available=false ②roster 补传三后端 URL（脚本半边已交付 dev_fleet.sh）③ChatApp 依赖摘除；①③属功能代码待边界批落地，ingress 接线归 P3-2 gateway surface 随 net M1——详见 todo.md P3-4 批记）；gateway 必要性与前端连接层拓扑调研件已落 docs/design/gateway-topology-survey.md（2026-10-08——九框架对照[前端三形态×后端两族，Pitaya/Colyseus/Nakama/Orleans 首采]+场景数据论证；结论建议=留独立 gateway-app+后端互联照旧自研不学 nats 族；「留」结论已生效，ADR-011）。

> P3 出口判据：三模型基准有可重复数字；单场景 1000 人广播在目标帧预算内（以文档预算表为准）。

---

## 5. P4 —— Documentation / Examples / Benchmark / Tutorial（任务书 §38 五档 + §33/§34）

- P4-1 文档修订：quick-start 引用实 API（批 B 已交付 2026-10-07——教学示例实对账+实编译验证：add_subdirectory 真实消费形态替代虚构 find_package 导出、APOLLO_LOG()->info 替代虚构宏、Ctrl+C 真实收口写法、GAME_MODULE=ON+toolchain 两必带参数实测；独立工程实构建实运行验证）；apps 文档复核（批 C 已交付 2026-10-07——docs/apps 两篇 11 项差异修正：三缺席 app 补节[baseappmgr/machined/game-server]、消息类型表对齐协议实况、login-app 虚构参数清欠、WorldApp 现状注记、NNG 段改退役路线口径、运行指南真实产物路径）；tests/README 墓碑清理（批 A 已交付 2026-10-07——308 行 Actor 墓碑重写为现状套件文档，ctest -N 实测 36 项为准的按域套件表 + 三树门禁口径 + 新增测试惯例；摸底发现：48 个 test_*.cpp 仅 33 个进门禁，15 个 legacy GTest 批随 APOLLO_BUILD_GTESTS=OFF 默认不编不跑）；README 第一屏按 §34 重构（批 D 已交付 2026-10-07——「核心概念」六词表落位 + 核心特性精简 + 快速开始补最小示例[实编译验证]）；docs/architecture/overview.md（§35）（批 E 已交付 2026-10-07——七层下行数据流各层职责+实件+边界，sidebar 登记）；ADR-001..009 记录（§36：Player Runtime Authority / Scene as Runtime Boundary / Instance First-Class / Scene Transfer / AOI Ownership / Battle Runtime / Persistence Model / Recovery Model / Concurrency Ownership）（批 F 已交付 2026-10-07——docs/architecture/adr.md 单文件九节简式[状态/背景/决策/后果+实件锚点]，sidebar 登记；**P4-1 六子项全清**）。
- P4-2 最小可运行示例 = 任务书 §29 全链路（login→lobby→create instance→enter→spawn→AOI→battle→reward→leave）（P0-P2 完成后此例即全流程验证器）。批 A 已交付（2026-10-07）——examples/full_loop_demo.cpp 进程内八步驱动六模块真件，36 断言门全过；跨进程接线归 G-1 收尾批与 net M1。
- P4-3 Benchmark/Tutorial：三模型基准（P3-2 复用）+ 新手上路教程（已交付 2026-10-07——docs/guide/tutorial.md 八步教学化 + 三模型基准复用节，样例/基准双实跑验证，sidebar 登记）。
- P4-4 术语承接：全仓核对 term-contract 零出现（禁用词扫描：Space/nng/Battle(space 义)/Player(玩家实体义)/baseapp 进程名迁移后清账）（摸底批 2026-10-07——文档叙述层清零达成[三类豁免判定：KBE 源码引用/裁决记录/第三方库名字面]；代码层残余三簇[protocol BW wire 层/cell-app 过渡路由/nng 底座]同挂 net M1 拍板链；audit.md 补勘误登记）。

---

## 6. 与任务书 §39 最终验收标准的逐项对照（当前状态 → 目标阶段）

| # | 验收标准 | 当前状态 | 达成阶段 |
|---|---|---|---|
| 1 | Player 是明确一等对象 | ✗（无 Avatar；Anchor 半程） | P0-2 |
| 2 | Player 在线状态以内存为权威 | ◐（权威在内存但无落盘） | P0-2 + P1-4 |
| 3 | Scene 是明确运行时边界 | ✗（三层空壳、无 AOI/玩家） | P0-3 |
| 4 | Instance 是明确一等对象 | ✗（无状态机） | P0-3 |
| 5 | Player 可在 Scene/Instance 间迁移 | ✗（transfer 字段搬运） | P0-3 + P1-1 |
| 6 | Scene 拥有自己的 Entity/AOI/Runtime | ✗（EntityManager/AOIManager 外置） | P0-3 + P1-3 |
| 7 | Battle 可独立运行 | ✗（两层空壳） | P2-2 |
| 8 | Persistence 与 Runtime 解耦 | ✗（应用级内存混编） | P1-4 |
| 9 | Reconnect 有明确模型 | ✗（零实现） | P1-6 |
| 10 | Recovery 有明确模型 | ✗（三档清单零实现） | P1-5 |
| 11 | 对象所有权明确 | ✗ | P0 全程（object-model 锚点表） |
| 12 | 并发模型明确 | ✗（C-1..C-4） | P0-1 + P1-3 + P3-2 |
| 13 | 生命周期明确 | ◐（Host/Session 有、业务对象无） | P0-4 |
| 14 | API 简单（§27 形状） | ✗（world/scene/instance 链不存在） | P0-3 + P1-1 |
| 15 | 无 God Manager/Context | ✓（现状无——保真） | 保持 |
| 16 | ECS 不吞噬框架 | ✓（现状无 ECS 疾患——保真） | 保持 + P2-1 |
| 17 | 网络层与 Gameplay 解耦 | ◐（shredded 但未接线） | P1-6 + P3 收口（协议单套） |
| 18-22 | 塔防/挂机/副本/竞技场/轻量 MMORPG 自然实现 | ✗（当前跑不通最小链路） | P2 半程 + P4-2 全流程 |

---

## 7. 执行纪律（任务书 §37/§40 再强调）

1. **每个阶段结束重跑四门禁**（Build/Unit/Integration/Examples——当前 Examples 门禁默认空转，P0-5 起把 game-server demo 纳入 Examples 面作为最小链路冒烟）。
2. **禁止一次性大规模重构**：每批 ≤ 文件级清单所列范围；先删后建（收敛）与先建后删（立核）的批次分开，避免任何时刻「两套都活」。
3. **不为了重构而重构**：命名不理想 → 小范围调整；文档不清晰 → 修文档；只有职责/所有权/生命周期/并发模型错误才动结构（§37 原文判据）——本计划的 P0 均为判据命中项；未列批次的存量（如 queue 族继承件、IPC 旁支）保持现状并登记，不再扩写。
4. **术语纪律**：任何新对象/文件/字段名先查 term-contract；新词先入 glossary 再入契约（§0 规则 4）。
5. **本轮审计遗留登记**：本计划未包含第 4 档新增件以外的 legacy 死件删除（如 comval 手写 union、attribute_id 290 常量、table_loader、DistributedLock、LocalServiceDiscovery）——并入各阶段「先删后建」批内清账，不留新孤儿。

---

## 8. 术语契约差异清单（improvement-plan 级）

1. **进程名迁移登记**：apps/base-app/baseappmgr 的 `base`/`baseappmgr` 命名契约冲突（禁 cellapp/baseapp）——按契约 §2 应在收口批次更名（**已定案（ADR-014，2026-10-10）：baseappmgr→manager（ManagerApp 契约定稿名）、base-app→zone-app（Zone 定稿名），cell-app 原型名不动；零行为变更，CTest 套件名不变**）；落地归后续改名批，此前**不改名**（避免阶段 0 大爆炸）。
2. **WorldHost vs clock 文档 10Hz**：P0-1 对齐时以 clock-and-time 文档与契约 §1.5（tick 词条）为准修正默认值 20Hz——若设计批次另有裁决（20Hz 为上线的实际帧率，文档待勘），以裁决为准并同步文档；登记为本计划执行时的第一项「设计口径核对」。
3. **`Player` 作为叙述词**：本计划沿用任务书 §39 表格原文（"Player 是明确一等对象"等），其语义落点=Anchor+Avatar 二体（object-model.md 差异清单 1）；代码标识一律契约词。

---

（七件套完）