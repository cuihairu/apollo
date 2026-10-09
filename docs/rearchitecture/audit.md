# Apollo 架构审计报告（audit）

> 所属：第一阶段（只读架构审查；本阶段未修改任何源代码/构建脚本/配置文件）。
> 依据：docs/rearchitecture/architecture-review-plan.md（任务书，commit ce91a739，40 节，以下称「任务书」）。
> 术语：docs/design/term-contract.md v1.0（用户审定，以下称「契约」）。契约未定名词条按既有代码用词。
> 范围：README、docs/、examples/、tests/、src/、include/、sdks/、modules/、apps/、build 系统与 CI。
> 方法：代码与文档全部实读；存量审查件（仓库根 BIGWORLD_AUDIT.md、ipc_audit_results.md）仅作参考交叉验证，其结论未默认采信；本报告数据以 2026-10-04 工作区为准。
> 配套文件：architecture.md（架构地图）/ object-model.md（对象所有权）/ lifecycle.md（生命周期地图）/ concurrency.md（并发模型）/ persistence.md（持久化）/ improvement-plan.md（改进计划）。本文档为总览与仓库层证据。

---

## 1. 仓库概览与分析（Repository Analysis）

### 1.1 定位基线

Apollo 重新定位（任务书 §1）：**面向轻量 MMO、场景化多人在线游戏与实例化多人游戏的服务器框架**——持久在线玩家、Scene、Instance、AOI、战斗、玩家状态、场景切换、社交、公会、副本、竞技场、塔防、挂机 MMO、轻量 MMORPG、数据持久化、断线重连、服务恢复；明确**不追求无缝大世界**（设计决策 #9，docs/analysis 36号）。

对应到代码的两个事实基准：

- **设计侧**已收敛：docs/design 下 12 份设计文档 + term-contract v1.0 已立；design-gap-inventory 15/17 项 CLOSED（仅 gap #15 Bots 与 #16 map pipeline OPEN）。
- **代码侧**仍是「存量烟囱与新建模块并存」的双结构树（§5），且默认构建只编译基础库，**全部 6 个 app 默认零编译**（§3.3），大量「设计已声明、代码未落地」的缺口（见 §7 问题清单与各配套文档）。

### 1.2 代码规模统计（2026-10-04 实测，wc -l）

| 树 | 文件数 | 行数 | 说明 |
|---|---|---|---|
| modules/ | 149 | 31,947 | 新内核（base/contract/core/runtime/data/net/protocol/game/bigworld/starter） |
| include/ | 107 | 32,883 | legacy 公开头（net/net-message/thread/battle/ecs/aoi/attribute/comval…） |
| src/ | 29 | 12,394 | legacy 实现（storage/db_mysql/db_mock/memory_connection/redis/network/net-message/thread…） |
| tests/ | 41 | 16,422 | legacy 测试 |
| examples/ | 24 | 9,670 | 示例（多数依赖 legacy API） |
| apps/ | 33 | 4,822 | 6 个 app 骨架（gateway/login/base/baseappmgr/cell/game-server） |
| sdks/ | 10 | 3,947 | 契约（contract XML+apollo.xsd）、生成器 gen/src、生成的 C++/JSON/proto/lua |
| skds/ | 18 | 3,523 | Unity C# SDK 板（目录名拼写历史遗留） |
| **合计** | **411** | **~115,600** | 不含 docs/（3,097 文件 / 310,325 行，含 design 文档、analysis 记录与 VitePress 站点） |

其中 legacy（include/+src/）约 45k 行、新建模块（modules/+apps/）约 37k 行、测试 16k 行、示例 9.7k 行。**双树并存导致同一能力在仓库内存在 2~3 套并行实现**的规模是本次审计最重要的结构性发现（§5、§6）。

### 1.3 构建拓扑与门控

- CMake + Ninja + vcpkg；`APOLLO_ENABLE_MODULAR_LAYOUT=ON`（默认）启用 modules/ 新树；legacy src/+include/ 在 `APOLLO_BUILD_LEGACY=ON` 时演进式参与。
- **模块门控**：
  - `APOLLO_BUILD_GAME_MODULE=OFF` 默认 —— modules/game 下的库（含 apollo::game_world）在默认构建中不编译。
  - `modules/protocol`（apollo_protocol）默认禁用 —— CMake 注释自认「has issues」；该 target 带 `APOLLO_USE_NNG=1`，从未针对 nng 1.11 编译通过（仓库已登记 breakage）。
  - `modules/data`、`modules/net` 的子模块按 find_package 结果降级（Trantor/redis++ 可选，未命中即用内置桩/降级实现，apollo_runtime CMake 中「using built-in implementation」）。
- **app 门控链**（apps/CMakeLists.txt）：6 个 app 全部依赖 `apollo_protocol`（且 login/base/baseappmgr 还要求 `APOLLO_BUILD_GAME_MODULE=ON` 的 apollo::game_session；cell-app 要求 apollo::game_core+apollo::game_world）。**默认构建（GAME_MODULE=OFF、protocol 禁用）→ 零个 app 被编译**，全部走 else 分支打印 disabled。
- 已知 breakage 登记（仓库 architecture-review.md 缺陷登记簿 C-95 等）：examples 链接断；modules/protocol 从未适配 nng 1.11；game-server 在 GAME_MODULE=ON 下存在未定义符号（LogManager::clear/set_console_enabled/snapshot、SqlTemplate 构造/query）。
- **本次审计新增确认的潜伏链接断裂（六源未编译族）**：
  1. `modules/game/world/CMakeLists.txt` 只把 `aoi.cpp; scene.cpp` 编入 `apollo_game_world`，而 `map_instance.cpp / map_instance_manager.cpp / world_space.cpp / world_session.cpp / world_session_manager.cpp` 五个实现文件不在任何构建 target 中（compile_commands.json 与 build.ninja 双重核实）；但 `apps/cell-app` 无条件调用其 API（cell_server.cpp:523-563 的 ensureDefaultMapInstance/assign_map_instance/transfer_session 等）。
  2. `modules/runtime/src/world_host.cpp`（109 行）不在 runtime/CMakeLists.txt（只编 application_host.cpp + runtime_manifest.cpp）——cell-app 还依赖 WorldHost（cell_server.cpp:281）。
  → 结论：**APOLLO_BUILD_GAME_MODULE=ON 时 cell-app 必然因 6 个缺失源 undefined reference 链接失败**（目前默认 OFF 因此潜伏）。同时 legacy `src/apollo/storage/database/db_mysql.cpp`、`redis.cpp`、`src/apollo/config/table_loader.cpp` 等 9 个文件在 88 文件的真实构建清单（compile_commands）中均为 0 次编译——legacy 持久化栈有完整的「已写未编」层；根 CMakeLists legacy 分支还引用 30+ 个**不存在的源文件**（src/network/*、src/apollo/net/*、src/apollo/bw/runtime.cpp、src/apollo/core/log/*、src/apollo/redis/redis_template.cpp 等），legacy 布局一旦开启 configure 能过、构建必失败。
- **ODR 违规（基础框架层）**：`apollo::core::log::LogManager` 在旧头 log_manager.hpp（39 行，四方法仅声明）与新头 log_manager.h（106 行）中**双定义同名类**，且 `global_log_manager()` 符号双声明、实现只有一个——同一链接图内两个类定义 = ODR UB（application_host.cpp 按 -I 顺序命中旧头、链接到新头实现，write() 语义分裂）；双 log_manager.h 版本漂移 291 行 vs 106 行（diff 证实非同一文件）。

### 1.4 仓库健康与交付纪律

- 工作区未跟踪文件：BIGWORLD_AUDIT.md、ipc_audit_results.md、Testing/Testing/（前两份为存量审查素材，按要求未纳入本次交付）。
- git 历史：main 分支以「docs:」与「refactor(apps):」为主的单一作者提交风格；最近的架构审查历史共 25 轮，defect 登记 C-1..C-95（architecture-review.md §31-§34，3,150 行）。

---

## 2. 术语契约对照与差异清单

> 规则（协调者约束 + 任务书卷首）：本套文档一律按 term-contract v1.0 用词；凡代码/存量文档与契约冲突处，正文条目按契约修文，并在**各文档末尾**附「差异清单」逐条列出。

### 2.1 「Player」一词的裁决余波（重要前置说明）

契约 v1.0 用户裁决（docs/design/player-object-model.md 与 analysis 记录）：**玩家对象 = Account/Session/Avatar 三元组，「Player 已废」**——玩家实体只有 Avatar 一个词，Player 与 Avatar 的二分被视为再造分层而废止；Avatar = 场景化身/Cell，驻 scene 随 scene 生死；PlayerAnchor = Base，驻 Home Zone（登录时一次分配）。

然而任务书 §2/§6/§39 全文以「Player 必须成为一等对象」为验收主线（Account→Player→Runtime Player；Player 拥有 Identity/Attributes/Inventory/Equipment/Quest/Progress/Social/Guild/Runtime State）。两者**不冲突但需修文对齐**：本套文档把任务书的「Player 一等对象」落地为契约的锚-化身二体模型——**契约的持久态承载 = PlayerAnchor（长期归属、内存权威、随登出落库）、任务的「Runtime Player」= Avatar（场景内空间权威）**；部分上下文（如 §6 的 Player 持久/runtime 状态分界线）"Player" 作为叙述性总称保留，代码标识一律用契约词。

### 2.2 契约词条与代码现状总对照（详情见各配套文档）

| 契约词（代码标识） | 代码现状 | 位置与证据 |
|---|---|---|
| `Scene`（场景，世界内逻辑单元） | 存在但被 MapInstance→WorldSpace→Scene 三层空壳包装；生产代码零实体、无 AOI、无玩家进出 | modules/game/world/src/scene.cpp、world_space.cpp、map_instance.cpp |
| `Instance`（副本，玩法生命周期） | 只有 create/find/destroy/update 五操作注册表，无状态机、无玩法生命周期 | modules/game/world/map_instance_manager.hpp |
| `Zone`（逻辑服进程，承载 1..N scene） | 无代码标识；cell-app 是事实上的单 scene 进程雏形 | apps/cell-app |
| `PlayerAnchor`（≡ Base，驻 Home Zone） | 存在（六态 + SessionBinding + WorldAssignment）；**无 Home Zone 概念**、无持久化钩子 | modules/game/session/player_anchor.hpp；base-app/base_server.cpp |
| `Session`/`Proxy`（连接承载） | 无 Proxy 类；WorldSession 是纯簿记状态机，与网络层零耦合；两套 Session 域并存（apps 级） | modules/game/world/world_session.hpp；apps/*
| `Avatar`（场景化身） | **无此类**。最接近的是 cell-app 内部 `cell::PlayerEntity` 与 world_session 的 avatar EntityId 字段 | apps/cell-app/src/cell_manager.hpp |
| `AOI` | **两套实现**：legacy `apollo::AOIGrid`（九宫格，仅测试消费）与 cell-app 自写 `cell::AOIManager`（定长网格）；Scene 无 AOI 成员 | modules/game/world/src/aoi.cpp；apps/cell-app/src/cell_server.cpp:61-120 |
| `OnlineDirectory`（manager 权威 + 镜像） | 仅有 baseappmgr 进程内 assignments_+SessionLocator 雏形；无跨进程目录、无事件族 | apps/baseappmgr/src/baseappmgr.cpp |
| `Suspended`（保活窗口） | WorldSession 有 Suspended 态**枚举**，无任何定时器/过期逻辑 | modules/game/world/src/world_session.cpp:74-101 |
| `PersistJournal`（write-behind journal） | 无；base-app 有应用级 SaveQueue（自动保存线程，60s 打印） | apps/base-app/src/base_server.cpp |
| `RemoteEntityCall` / `InterServerLink` | 无代码；进程间通信走 legacy RepSocket（nng 桩） | modules/net·legacy net |
| `login_token`/`session_key`/`resume_token` | login 侧 ticket = 时间 XOR 哈希（sessionSecret 未用）；resume_token 零实现 | apps/login-app；modules/game/session |
| `FrameFilter`/四通道 | 无；net-abstraction 设计文档 L0-L3 在代码中无对应 | —（详见 architecture.md） |
| `VerifierApp`/`LoggerApp`/`Machined`/`InterfacesApp` | 均无代码 | 设计文档 inbound-interfaces.md、battle-verification（gap #17） |

### 2.3 契约禁用词在现存代码中的出现（违反零出现规则，须登记）

| 禁用词 | 出现位置 |
|---|---|
| `Battle`（作为 space/实例名：历史错名，裁决 1） | legacy include/apollo/game/battle/ecs/battle_system.hpp 全套（BattleWorld/BattleManager/BattleSystem）；docs/25 语义实为副本 |
| `cellapp / baseapp` | apps/base-app、apps/baseappmgr 进程名与 CMake target 名（注意：契约「他家进程名」禁用的语义指 apollo 不拆两族进程，进程名本身是历史遗留命名，已登记为待迁移项；代码内 class 未用 baseapp 命名） |
| `Space` | world_space.cpp 的 WorldSpace 类名含 Space 词根——部分违反，登记（2026-10-07 勘误补充：**modules/protocol 消息族另有 `SpaceID`/`spaceId`/`fromSpace`/`toSpace` 与 `CellEntityMove`/`CellEntityProperty` 结构名**——BigWorld 兼容 wire 语义承载（33 号 facade 面，wire 格式不含名字），与 nng 底座同属 net M1/协议收敛批清除链——M1 内容已拍板（ADR-011），代码清除随 M1 实现批落地） |
| `Mailbox / EntityCall / witness / ghost` | 无代码出现（设计阶段已清）；残留于设计文档对照段（允许）。2026-10-07 复核：非豁免文档叙述（guide/apps/design 非对照段）残余为零——其余出现均为 KBE 源码行号引用、裁决记录引用或第三方库名字面事实陈述 |
| `nng` | modules/protocol（apollo_protocol 依赖）、modules/net 的 nng_wrapper/socket.cpp/channel.cpp 桩、legacy net 栈——**契约已禁 nng（退役，36号 #2），代码仍在依赖/残留**：见 architecture.md §网络 |
| `registry`（不引入 etcd/consul） | 无外部依赖（仅命名指导） |
| 「自行开发/遥遥领先」等 | 无 |

---

## 3. 构建与可运行性实测

### 3.1 默认构建产物

默认配置（APOLLO_ENABLE_MODULAR_LAYOUT=ON；GAME_MODULE=OFF；protocol 禁用）下实际产出：新树基础库（core/runtime/base/contract/data/net 的可用部分）+ tests + legacy 中仍挂在构建里的目标。**可执行文件层面：零个 app**。换言之当前仓库「装得起来，但跑不起任何服务进程」。

### 3.2 若打开 GAME_MODULE

- apollo::game_world 只含 aoi+scene；cell-app 依赖的 5 个 world 实现文件缺失 → 链接失败（本次审计新登记，尚未入缺陷簿）。
- game-server 链接失败（C-95 已登记：LogManager/SqlTemplate 未定义符号）。
- modules/protocol 编译失败（从未适配 nng 1.11；其 CMake 依赖 InstalledDir 猜路径）。

### 3.3 三条主线端到端断裂（apps 层，apps agent 交叉核实）

1. **client→gateway**：NullClientIngressServer 空转（acceptNewConnections 注释 "not wired yet"）、sendToClient 为 placeholder。
2. **login→base 的 assignWorld**：base-app 对 assign 消息返回 Unknown message type；login 在 "stub transport mode" 下恒 return true（登录“永远成功”）。
3. **route 解析**：gateway 将 PlayerResolveRouteRequest 路由到 base-app 端口 9002，而真实 handler 只在 baseappmgr（默认 9003）——端口错配后回落 buildDefaultRoute；gateway 无 --baseappmgr 参数。

另有：login 的 qr 配置 `chatAppUrl=tcp://127.0.0.1:9003` 与 baseappmgr 默认监听 9003 撞端口；disconnect 以纯文本发送、后端无对应消息类型；两套 Session 域并存（apps 各自为政）；无任何 reconnect 代码（reconnectWindowMs 零引用）。

### 3.4 测试现状概览（详见 §6 与各配套文档）

- tests/ 41 文件 16,422 行：以 legacy 栈（net/thread/storage/db_mock）与内存目录/锚点单测为主；**baseappmgr 与 PlayerAnchor 目录测试明确「不 start() 只测内存方法」（test_baseappmgr.cpp:24-25 注释）**；gateway/login/cell 无测试。
- 新建模块侧：modules/*/tests 存在但多数直测自己编译的那几个源；模块化开关关闭时这些测试可能不进构建（需按实测门控核对）。
- CI：Workflows 存在（push 构建 + 测试的四道门禁记忆：build/unit/integration/examples），但受 3.1-3.2 约束。

---

## 4. 文档现状（docs/）

（本章节为审计结论，详细修订计划见 improvement-plan.md P4。）

- 站点：docs/ 为 VitePress 站（docs/.vitepress），310k 行含 node_modules。
- 结构成熟区：docs/design/（12 份设计文档+契约，质量高、决策齐全）、docs/analysis/（36号决策集、architecture-review 25 轮历史、design-gap-inventory）。
- 失真区（与代码不符）：
  - docs/guide/quick-start.md 引用不存在的 API（dispatcher/ServiceRegistry 等），示例代码无法编译。
  - docs/apps/BigWorld服务器应用实现.md 与代码有 9 项差异（文档假设 NNG ready、拓扑缺 baseappmgr、WorldApp 名称与 cell-app 不符、Pub/Sub 零实现、GatewayApp "optional" 与代码相反）。
  - docs/architecture/ 与 design 文档中多处声称的能力（属性同步管线、在线目录、四通道、write-behind journal）在代码中无实现（差异清单逐条见 architecture.md §7）。

---

## 5. 双结构树与并行实现总清单（结构性发现）

本仓库存在的**同一能力多套并行实现**是最大维护性负担，逐项列册（各自位置/规模/消费者），供 improvement-plan 决策：

| # | 能力 | 并行实现 | 消费者现状 |
|---|---|---|---|
| A1 | Entity/ECS | (a) modules/game/core `Entity`（string-key 组件 map）(b) legacy `apollo::ecs`（ecs.h，header-only）(c) legacy `apollo::battle::ecs`（ecs.hpp，编译于 apollo_game_battle）(d) cell-app `cell::Entity/PlayerEntity`(e) legacy include/apollo/game/ecs.h 另一套。**合计 4~5 套**，id 类型互不通用（u64 包装/uint64/uint32） | (a) 仅 cell-app 作 avatar id 载体；(b) 仅 examples/ecs_demo；(c) 仅 tests/test_game；(d) cell-app 内部；(e) 零 |
| A2 | AOI | (a) legacy `apollo::AOIGrid`（九宫格 uv，aoi.cpp 256 行）(b) cell-app `cell::AOIManager`（定长数组网格）——互相零关联；Scene 无 AOI | (a) 仅测试；(b) cell-app，但 viewer 广播是 placeholder |
| A3 | Battle | (a) modules/game/battle `BattleSystem`（vector+update，非 ECS，75 行）(b) legacy include/apollo/game/battle/ecs/battle_system.hpp 全套（BattleWorld/BattleManager，185 行头）——(b) 零引用纯死头 | (a) 零生产；(b) 零 |
| A4 | 属性系统 | (a) variant 版 AttributeContainer/AttributeManager（attribute.hpp，单例，锁内取引用）(b) ComVal 手写 union 版（comval.h 587 行 + attribute_value.h，双份：modules 内与 src/ 下逐字节相同且后者未构建）；attribute_id.h 约 290 个常量 | 均无生产消费；serialize 存在每属性 1 字节估算低估 bug（attribute_value.cpp:247 估算 vs 286 实际），尾部属性静默丢写 |
| A5 | Session/目录 | (a) modules/game/session（anchor/locator）(b) modules/game/world WorldSession（七态状态机）(c) apps 内两套 Session 域（gateway/login 各自） | (a) base-app+baseappmgr；(b) cell-app；(c) 各自为政；互通仅通过消息文本 |
| A6 | 传输层 | (a) modules/net tcp/rpc + apollo_protocol（nng 桩）(b) legacy include/apollo/net + src/apollo/network（RepSocket 等）(c) apps 直接用 RepSocket/nng_wrapper 桩 | 默认构建下路径 (a)/(c) 是桩（connect false、else 空分支） |
| A7 | 持久化 | (a) modules/data（cache/core/orm/redis）(b) legacy src/apollo/storage（db_mysql/db_mock/memory_connection）(c) base-app 的 DatabaseService+SaveQueue（内存假实现）——(a)(b) 中 9 个文件 0 编译 | 真实链路：仅 memory_connection（无文件落盘行为待核）+ 内存锚点 |
| A8 | 配置 | (a) modules/core config_registry 的 ConfigRegistry（全局 KV，**活**）(b) legacy ConfigManager/ConfigNode（文件+热更+监听，**生产零消费**：parseXml/parseLua 恒 return false、notifyListeners 断头路、热更无调用方，`getValue<T>` 声明无定义）(c) 各 app 自带 config.hpp 手写字段（约 20 个死字段） | Config/Definition/Runtime State 混用（setValue 直写 configs_）；同一数据的两个互不感知入口 |
| A9 | 日志 | (a) 旧内存版 log_manager.hpp/log_manager.cpp（四方法仅声明/死源码，无编译 target）(b) spdlog/内置版 log_manager.h（**活**，API 不同）(c) utils/logging 第三套 CamelCase——**三套并存 + ODR 违规**（LogManager 双定义同名类） | game-server 引用的 snapshot/clear/set_console_enabled 符号全图不存在（C-95）；无统一异步日志架构 |

> 表内「零」指 grep 全仓无消费方（测试除外）。本表在 object-model.md / architecture.md 中展开为对象级所有权与依赖地图。

---

## 6. 测试覆盖与四道门禁缺口

测试代码总量：tests/ 40 文件 16,422 行 + modules/*/tests/ 14 文件 8,091 行 ≈ 24,513 行。但受三把开关支配，**实际默认构建只跑 16 个 target**：

1. **档位 1（默认/CI 实际构建）**：tests/CMakeLists.txt else 分支注册 13 个 target（bigworld_api/memory/string/terminal/thread_pool/time/core_lifecycle/core_log/core_config/runtime/data/net/game）+ 模块侧 contract 双闸（apollo_contract_tests 788 行、apollo_contract_gen_compile_test）+ sdks/gen golden check。CI（ci.yml）只传 `-DAPOLLO_BUILD_TESTS=ON`。
2. **档位 2（GTest 分支 16 个 target）**：`APOLLO_BUILD_GTESTS=OFF` 默认 + CI 不传 → protobuf/network/log/config/redis/database/sql_template/rest_template/integration/channel/… 全部不构建；其中 channel_tests 还需 APOLLO_ENABLE_IPC=ON（默认关）。**README 宣称的「数据库/Redis 操作测试」在 CI 中实际为零**。且 GTest 分支与 else 分支互斥——两档各丢一半资产（game_tests、session_world_tests 只在 else 分支）。
3. **档位 3（模块级 12 个 target，7,194 行）**：`BUILD_TESTING` 恒假（根 CMake 全仓无 include(CTest)）→ base/core/runtime/data/net/game/starter/bigworld/protocol 各 *tests 与 *_comprehensive_tests 永不编译；其中多个套件即使放行也编不过（net_comprehensive 引用幻影头 websocket.hpp/message_codec.hpp、starter_comprehensive 引用空壳模块的 module_registry.hpp、bigworld_comprehensive 的 BigWorld.h 有 20 处编译错——C-64 已登记）。
4. **app 级 4 个 target**（base_anchor/session_world/baseappmgr/gateway_route/protocol_bootstrap 类，把 apps 源直接编进测试二进制）：全部依赖被默认关掉的 target 守卫（apollo_protocol、game_session）→ 默认全跳过；**与 apps 自身「同生共死」**。apps 树没有独立 tests/ 目录。
5. **三处墓碑文档**：README.md:317 引用的 `./examples/all_features_demo` 已随 ce5f07e8 删除（现存 test_all_features.cpp 依赖头已删、编译必败）；tests/README.md 整篇是 Actor 套件墓碑（引用的 test_actor_framework.cpp 不存在、覆盖率表无支撑）；examples/README.md 的 ioc_example 等不存在。examples 全部门控 `APOLLO_BUILD_EXAMPLES=OFF`（CI 显式关）——**CI 从不编译/运行任何 example**（benchmark.cpp 甚至未链接 apollo）。

任务书 §28 覆盖矩阵核对（精确版）：

| 领域 | 默认构建覆盖 | 缺口 |
|---|---|---|
| Runtime 生命周期 | test_runtime.cpp 26 测（Host start/stop/run_once/hooks/失败回滚） | 无 tick 抛异常恢复、无故障注入；模块级回调用例永不编译 |
| Player 登录/登出/重连 | 无 | 无端到端登录、无登出、无重连（全仓真正连 socket 的 reconnect 零引用） |
| Scene/Instance | test_game.cpp:568-624 Scene 构造/spawn/update | MapInstance 仅 create/update/destroy（test_session_world.cpp:107-125）；无 start/finish 状态机；instance 生产路径未编译 |
| AOI | test_game.cpp:304-432 legacy 九宫格 9 测（enter/leave/move/visibility） | cell::AOIManager 无测试 |
| Battle | test_game.cpp:434-568 legacy ECS | 新 BattleSystem 零实例化（comprehensive 仅 include） |
| Persistence | test_data.cpp 20 测（内存连接/cache） | 真库零测；base-app DatabaseService 无测试 |
| 网络 | test_net.cpp 76 测（header 级：包/校验和/HTTP/WS 头） | 真 socket 回环测试（test_network.cpp）在 GTest 分支 → CI 从不跑 |

基础设施：断言主体是自写零依赖宏（TEST_ASSERT 约 30 个文件、再实现 TEST_CASE/INTEGRATION_TEST 注册器），真 GTest 仅 9 个文件；**无任何 sanitizer/valgrind 接线**（tests/README 声称的 -DSANITIZE_* 全仓无定义）；覆盖率 option 存在但 codecov.yml 忽略 tests/**；契约测试族（test_contract.cpp + xmllint XSD 闸 + golden check）是全仓接线最完整、CI 真跑的测试族。

---

## 7. 最重要的架构问题（severity 排序总纲）

> 详细论证与位置证据见 architecture.md §8 与 object-model.md、lifecycle.md、concurrency.md、persistence.md；改造分级见 improvement-plan.md。

1. **[P0] 玩家对象模型与代码脱节**：契约/设计（PlayerAnchor+Avatar 二体、换幕、Suspended 窗口）vs 代码（无 Avatar、无 Home Zone、WorldSession 字段搬运式 transfer、断线零处理）。任务书 §2/§5/§6/§12 的全部焦点。
2. **[P0] Scene/Instance 未成为运行时边界**：三层空壳包装（MapInstance→WorldSpace→Scene）、无 AOI、无 tick 阶段、无生命周期；生产实体全走 cell::EntityManager。任务书 §8/§9/§10/§11。
3. **[P0] 对象所有权/执行上下文无归属**：mutex+map 的 manager 群、锁内 tick 树（MapInstanceManager::update 持锁跑实体）、锁外使用容器引用（AttributeManager）、EntityId/PlayerId 命名空间混用（cell_server.cpp:559 find_by_player(entity_id)）；任务书 §5/§26。
4. **[P1] 传输与协议层双树未收口**：nng 尚未退役（契约禁用）、NNG stubs 空转、三主线断点（3.3）、端口撞车；任务书 §21。
5. **[P1] 持久化栈假实现**：DatabaseService::initialize 恒 true、loadPlayer 造假、fromJson/toJson 不对称（缺 x/y/z）、SaveQueue direct callback(true)、无 write-behind/journal；任务书 §7/§19。
6. **[P1] 恢复/重连零实现**：无 reconnect 代码、无保活窗口、无 resume_token；任务书 §20。
7. **[P1] AOI 双实现且未接入 Scene**；任务书 §13。
8. **[P2] 四~五套 Entity/ECS 并行**（A1）；任务书 §14/§15。
9. **[P2] Battle 两层空壳**（新 BattleSystem 无生产、legacy battle 纯死头）；任务书 §16。
10. **[P3] 默认构建零 app、三处潜伏链接断裂**（含新登记 cell-app），CI 四门禁无法覆盖真实链路；任务书 §3/§29。
11. **[P3] 文档失真面**（quick-start 不可编译、apps 文档 9 差异、设计文档能力未落地）；任务书 §33/§34。
12. **[P3] 复杂度过剩的存量**：约 1/3 死接口与 4 个全局单例（AOIManager/AttributeManager/AttributeContainerManager/BattleManager）、comval 手写 union、attribute_id 290 常量；任务书 §23/§32/§37。

---

## 8. 术语契约差异清单（audit 级汇总）

> 各配套文档末尾按各自主题附局部清单；此处为跨文档总表。

1. **`Player` 代码标识仍在 sdks/contract/entities.xml:8 与生成的 contract.lua/apollo_contract.h**（`<entity id="Player" parent="Avatar">`），继承链 Monste→NPC→Avatar→Player 四层；契约裁决玩家实体只有 Avatar 一个词（Player 已废）→ **契约文件与生成物需要按 new term-contract 修订（语义上等价的落点：Avatar 承载社交面/长期态归属 Anchor，`Player` 实体应并入 Avatar 或删除，由后续代码批次裁决，本阶段只登记）**。
2. **`Battle`（space 义）残留**：include/apollo/game/battle/ecs/battle_system.hpp（BattleWorld/BattleManager 等全套）；契约禁用（裁决 1），且该文件本身零引用。
3. **`nng` 未退役**：modules/protocol CMake/代码、modules/net 的 socket.cpp/channel.cpp/nng_wrapper.hpp 桩、legacy net 引用；契约 §2 禁「nng」（36号 #2 退役）——代码未跟随设计（net-abstraction.md G-1 自持四层未落实）。
4. **process 命名 base-app/baseappmgr**：契约禁「baseapp/cellapp」作 apollo 进程名（Zone 替代）；现存两个 app 目录/CMake target 名与契约冲突，迁移后应更名（如 anchor-app/directory-app，具体由改进计划 P1/P3 定名并走新术语流程）。
5. **WorldSpace 类名含「Space」词根**：契约禁 Space 裸用；world_space.hpp 的 `WorldSpace`（且设计称其为「每实例一个 Scene 的三层空壳」）建议随 Scene 模型统一时消解。
6. **`line/room` 等**：无代码出现，合规。
7. **设计文档中的旧词（docs/25 等）**：属历史件对照段，按契约规则 2 允许，但文档修订（P4）时统一。
8. **`Instance` 代码标识 = MapInstance**：契约要求 `instance` 义（副本玩法生命周期）；现有 MapInstance 无状态机——不是命名错误而是**语义未达标**（object-model.md 详述），命名保留。
9. **contract 体系（entities.xml）与 sdk-contract 文档的 client/internal 域划分**：文档已立（1-899/900+），XML 与生成器是否执行该划分需代码对照（见 architecture.md §契约，sdks/gen 侧数据由批内材料核证）。
10. **"Player" 出现在本套文档叙述性文本中**（如“在线 Player 的内存对象是运行时权威”，任务书 §7 原句）：按契约规则，凡代码/概念标识一律写 PlayerAnchor/Avatar；叙述沿用任务书原句处用引号标注，不作代码标识。

---

（完）