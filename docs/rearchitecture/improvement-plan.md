# Apollo 改进计划（improvement-plan）

> 所属：第一阶段只读架构审查配套件之七（收口）。配套：audit.md（仓库层证据）、architecture.md（架构地图）、object-model.md（对象归属）、lifecycle.md（生命周期）、concurrency.md（并发）、persistence.md（持久化）。
> 依据：任务书 §37（不为了重构而重构）、§38（改造优先级 P0-P4）、§39（22 项最终验收标准）、§40（每阶段后重新 Build/Unit/Integration/Examples 四门禁，禁止一次性大规模重构）。
> 术语按 term-contract v1.0；差异清单见文末。本文档是七件套中**唯一有行动导向**的文件；所有先决条件以第一阶段审计结论为准，本阶段不修改任何文件。

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

- **P3-1 部署与进程形态**：manager 域目录跨进程化（G-1 服务发现骨架：machined 单机守护 + 广播，文档已立）；instance offload 进程形态（36号 #16）；配置统一（ConfigManager 收口/删除——config 系双轨 diff 见 audit A8；零配置文件落地）。
- **P3-2 性能**：按 architecture.md §6 三模型（1×1000 / 10×100 / 100×10）建 benchmark 桩（benchmark.cpp 接线——当前未链接 apollo）；锁粒度 scene 级化（concurrency §3 目标模型）；帧预算表实测化（capacity-and-benchmark）。
- **P3-3 可观测性**：日志三套收敛末批（P0-6 完成后 infra 统一）；metrics/tracing 最小集；VerifierApp（gap #17）与 LoggerApp 进程立项。
- **P3-4 DevEx**：CLI 脚手架（starter 模块补实或删除——空壳现状见 runtime 报告 G 节）；app 一键起停。

> P3 出口判据：三模型基准有可重复数字；单场景 1000 人广播在目标帧预算内（以文档预算表为准）。

---

## 5. P4 —— Documentation / Examples / Benchmark / Tutorial（任务书 §38 五档 + §33/§34）

- P4-1 文档修订：quick-start 引用实 API；apps 文档复核（9 项差异）；tests/README 墓碑清理；README 第一屏按 §34 重构（Player/Scene/Instance/AOI/Battle/Persistence 六核心词 + 最小 example）；docs/architecture/overview.md（§35）；ADR-001..009 记录（§36：Player Runtime Authority / Scene as Runtime Boundary / Instance First-Class / Scene Transfer / AOI Ownership / Battle Runtime / Persistence Model / Recovery Model / Concurrency Ownership）。
- P4-2 最小可运行示例 = 任务书 §29 全链路（login→lobby→create instance→enter→spawn→AOI→battle→reward→leave）（P0-P2 完成后此例即全流程验证器）。
- P4-3 Benchmark/Tutorial：三模型基准（P3-2 复用）+ 新手上路教程。
- P4-4 术语承接：全仓核对 term-contract 零出现（禁用词扫描：Space/nng/Battle(space 义)/Player(玩家实体义)/baseapp 进程名迁移后清账）。

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

1. **进程名迁移登记**：apps/base-app/baseappmgr 的 `base`/`baseappmgr` 命名契约冲突（禁 cellapp/baseapp）——按契约 §2 应在收口批次更名（候选：anchor-app / directory-app，**最终名走新术语流程定**）；本计划 P3-1 部署批次执行，此前**不改名**（避免阶段 0 大爆炸）。
2. **WorldHost vs clock 文档 10Hz**：P0-1 对齐时以 clock-and-time 文档与契约 §1.5（tick 词条）为准修正默认值 20Hz——若设计批次另有裁决（20Hz 为上线的实际帧率，文档待勘），以裁决为准并同步文档；登记为本计划执行时的第一项「设计口径核对」。
3. **`Player` 作为叙述词**：本计划沿用任务书 §39 表格原文（"Player 是明确一等对象"等），其语义落点=Anchor+Avatar 二体（object-model.md 差异清单 1）；代码标识一律契约词。

---

（七件套完）