# Apollo 架构地图（architecture）

> 所属：第一阶段只读架构审查配套件之二。配套：audit.md（仓库分析/构建/术语对照）、object-model.md（对象所有权）、lifecycle.md（生命周期）、concurrency.md（并发）、persistence.md（持久化）、improvement-plan.md（改进计划）。
> 依据：任务书 §4（输出完整架构地图）、§22/§23（Service 边界/Manager 泛滥）、§27（API 易用性）、§31（性能分析）、§32（复杂度审查）。术语按 term-contract v1.0；差异清单见文末。

---

## 1. 当前实际架构（以代码为准，非文档口径）

### 1.1 进程拓扑：6 个 app 骨架 + 设计目标对照

| 进程（代码） | 目录 | 设计目标角色（任务书/契约词） | 代码现状 |
|---|---|---|---|
| login-app | apps/login-app | 账号鉴权、token 签发（短连接） | 骨架完整；stub 传输模式下登录“永远成功”（`return true`）；sessionSecret 未用，ticket=时间 XOR 哈希 |
| gateway-app | apps/gateway-app | token 验签、会话承载、按 Zone 透传 | 骨架完整；客户端 ingress 未接线（NullClientIngressServer 空转）；sendToClient placeholder；resolveRoute 落点端口错配 9002 vs 9003 |
| base-app | apps/base-app | 锚点承载（PlayerAnchor 驻 Home Zone） | AnchorManager+SessionLocator 内存目录可用；activatePlayer/bindSession/unbindSession 有 handler；无 Home Zone 概念；DB 层假实现 |
| baseappmgr | apps/baseappmgr | 在线目录与落点裁决（manager 域串行） | assignments_+SessionLocator 进程内目录 + PLAYER_ASSIGN_WORLD/PLAYER_RESOLVE_ROUTE handler——**现存质量最高的 app**；顶号预裁/核销/事件族未实现 |
| cell-app | apps/cell-app | Zone/场景运行时（1..N scene） | 唯一接 WorldHost 的 app；EntityManager+cell::AOIManager+MapInstanceManager+WorldSessionManager；ensureDefaultMapInstance(id=1)；**game/world 5 个实现文件未编译 → 必链接失败（GAME_MODULE=ON 时）** |
| game-server | apps/game-server | 全流程 demo 容器 | 137 行 demo：MemoryConnection+SqlTemplate+IdPool+ThreadPool+DI(login pipeline)+ServiceHost；无游戏循环；链接失败（C-95） |

现状一句话：**6 个进程骨架齐备、消息协议齐备、数据模型齐备，但默认构建零个 app 被编译；三主线端到端各有断点**（见 §2.2）。

### 1.2 模块地图（创建/持有/调用/生命周期——按任务书 §4 逐项）

**新树 modules/（149 文件 31,947 行）**

| 模块 | 内容 | 创建什么 | 持有什么 | 调用什么 | 备注 |
|---|---|---|---|---|---|
| core | DI（ApplicationContext）、manifest、config_registry、LogManager | 按 BeanDefinition 图建 bean | 类型索引+单例表 | — | DI 为编译期类型键+名称；ctor 注入仅 singleton；拓扑排序初始化、逆序销毁；eager 默认 |
| runtime | ApplicationHost/ServiceHost/WorldHost | 服务容器与宿主机 | services_/shutdown_hooks_/console+signal 源 | 各 IHostedService/IWorldService | 阶段机 Boot→ConfigLoaded→Initialized→Ready→Stop；WorldHost 默认 20Hz（tick_rate_hz=20） |
| base | 基础件（与 core 边界需按归档核对） | — | — | — | 无本地实现细节报告（见 batch 材料） |
| contract | XML+XSD 契约解析 | 契约模型 | 内存模型 | — | sdks/contract 的解析侧；测试族最完整（contract 双闸） |
| data | cache(内存缓存)+core(连接接口)+orm(MemoryConnection/SqlTemplate)+redis | 见 persistence.md §2 | 见 persistence.md §2 | — | **5 套平行持久化栈之一**；真连介质能力为零 |
| net | tcp/reactor/rpc/http(EventLoop/RestTemplate)/websocket/protocol(Channel/Endpoint) | 见 §1.4 网络 | 见 §1.4 网络 | — | modules/net 的 .cpp 是旧树头文件的**唯一实现者**；默认构建 5 个 net 库全部编入 |
| protocol | （已注释停用）nng 系 RPC/nng_wrapper/RepSocket | — | — | — | modules/CMakeLists.txt:28-30 注释「has issues」；从未适配 nng 1.11 |
| game | world(scene/aoi/map_instance/world_session/…)+session(anchor/locator)+core(entity)+attributes+battle | 见 object-model.md | 见 object-model.md | — | 默认 GAME_MODULE=OFF；world 库只编译 aoi+scene |
| bigworld | BW 兼容层 runtime | — | — | — | 测试直编 src/runtime.cpp：11 测默认跑 |
| starter | core/net 两个空 INTERFACE 壳 | — | — | — | 无头文件（ls 实测）；comprehensive 测试引用不存在的 module_registry.hpp |

**旧树 include/(107 文件 32,883 行) + src/(29 文件 12,394 行)**

- 头文件侧是**活 API 层**：include/apollo/net/*（Connection/Session/NetworkManager/MessageCodec/packet/websocket/http/adapters）由 modules/net 的 .cpp 实现；include/apollo/network/* 仅 modules/net/tcp 私有规格层。
- 实现侧整体「已写未编」：legacy apollo 库目标源码清单引用 30+ 个**不存在的文件**（src/network/*、src/apollo/net/*、src/apollo/bw/runtime.cpp、src/apollo/actor/* 9 个、src/apollo/redis/redis_template.cpp 等）——legacy 布局一旦开启 configure 能过、构建必失败（audit.md §3）；当前默认 modular=ON 因此长期潜伏。
- include/apollo/game/*：battle/ecs/aoi/attribute/comval 等 legacy 游戏头——大部分零消费者或仅测试（audit.md §5 表）。

### 1.3 层叠关系（任务书 §4 的 Client→Persistence 栈，按代码实际回填）

```text
Client（GametestClient 示例，独立工程，不走主构建）
  ↓ 裸 TCP（gateway socket bind/listen）
Gateway-app（NullClientIngress 空转 / sendToClient placeholder）
  ↓ netproto::Channel（新 nng 系，stub）/ apollo::protocol::RpcClient（旧 nng 系，模块停用）
Login-app / Base-app / Baseappmgr（RepSocket handler 表；base-app 端口 9002、baseappmgr 9003）
  ↓ 消息类型文本（disconnect 为纯文本，无对应 handler）
Cell-app（WorldHost 20Hz tick）
  ↓
EntityManager（cell::Entity/PlayerEntity，裸指针持有） + MapInstanceManager（锁内 tick 树）
  ↓
MapInstance → WorldSpace → Scene（三层空壳，生产实体零注入）
  ↓
Persistence：base-app DatabaseService（内存 unordered_map，load 造假数据，save 恒成功回调）
```

### 1.4 网络层实况（网络 agent 逐字通读 28 个旧头 + 全部实现）

**关键架构事实**：
1. 迁移只搬了实现文件：modules/net 的 .cpp 是旧树头（include/apollo/net/*、include/apollo/network/*）的唯一实现者；旧 apollo 库目标因源码缺失 30+ 文件结构性不可构建。
2. 当前默认构建的 5 个 net 库中，apollo_net_protocol（Channel/Endpoint）在无 nng 环境下是**纯 stub**（connect/send/receive 一律 false）；RestTemplate 无 curl 编译为纯存根；net/http、net/websocket 走内置实现（未用 Drogon）。
3. 体系内命名撞车三套：`MessageHeader` 三种（4B 主机序 packed / 12B 大端 / "BWTR" 24B）；`Channel`/`Message`/`ChannelConfig` 在 net::protocol、ipc、net 三处各定义各互不相干；`RpcClient/RpcServer` 至少三套实现（protobuf 系占位壳、net 系相对完整但 cleanupTimeouts 为空、nng 系从未编译）。
4. IPC 体系（include/apollo/ipc + src/apollo/ipc）是离线旁支：与 net 零代码共享；APOLLO_ENABLE_IPC=OFF；socket_transport.h 声明的类无任何实现文件。

**实证功能缺陷（含行号）**：
- SessionManager 自死锁：onDisconnected 在锁内调 getSession（getSession 再锁同一非递归 mutex）——session_manager.cpp:273-280。
- TemplateSessionFactory：make_shared 配裸 delete——session_factory.h:50-57。
- native_adapter：setSessionFactory 存入的工厂成员与 handleAccept 使用的成员不同 → 设置永不生效，且创建的 NativeSession 裸指针从不被持有 → 会话泄漏——native_adapter.cpp:413-415/534-542。
- nng 系：NngSocket::receive 泄漏消息并错误使用四参 nng_recv（nng_wrapper.hpp:105-113）；RepSocket::stop 先 join 后关 socket → 死锁（modules/protocol/src/socket.cpp:127-133）。
- HttpServer：每连接 detach 线程+vector 无界增长+removeConnection 从不调用+stop 挂死——http.cpp:506-549。
- httpGet/httpPost 硬编码 localhost:80 路径 `/`（http.cpp:777-812）；crc32 表只初始化 8 项（packet.h:488-493）；getLocalAddresses 只返回 127.0.0.1。
- 假异步：NativeNetworkManager processEvents 纯 poll 轮询，epollFd_/ioPort_ 从未使用（native_adapter.cpp:778-790 vs 1033-1087）。
- sdnet_adapter.h 整体不可用：SSCP 为本地伪造仿品、CreateListener 被注释、getConnectionCount 返回 0——对应 .cpp 不存在（与 net-abstraction 设计文档「NNG wrapper 退役、sdnet_adapter 删除」的 G-1/G-2 计划方向一致：此件应删）。

### 1.5 设计与代码的对应面（docs/design ↔ 代码）

| 设计文档能力 | 代码落点 | 状态 |
|---|---|---|
| 四通道（movement/attributes/events/control） | 无任何实现 | 未落地 |
| FrameFilter/CryptoFilter | 无（frame 层仅 MessageCodec 12B 头） | 未落地 |
| InterServerLink | 无（进程间走 legacy RepSocket/nng 桩） | 未落地 |
| session_key/login_token/resume_token | ticket=时间 XOR 哈希；sessionKey/resumeToken 零实现 | 部分(伪)/未落地 |
| OnlineDirectory（manager 权威+镜像） | baseappmgr 进程内 assignments_+SessionLocator | 雏形 |
| 顶号/保活窗口/Suspended | WorldSession Suspended 态枚举；无定时器；SessionLocator::bind 顶号 | 枚举级 |
| 属性同步 L1-L4/RO_MIRROR/viewer set | 两套属性容器均无网络管线；dirty 仅 API 层 | 未落地 |
| write-behind journal（8.2） | 无；base-app SaveQueue 应用级 60s 空转 | 未落地 |
| PCG32 substreams/battle determinism | battle 两层空壳 | 未落地 |
| 契约 schema_hash/descriptor.bin | sdks 契约族完整（XML+XSD+gen+golden check） | **已落地且 CI 在跑** |
| 脚本 Lua 5.5 | 无 scripting 模块 | 未落地 |
| 服务发现 G-1（machined 双层） | 无 | 未落地 |
| 热备 G-2（reviver） | 无 | 未落地 |

契约体系是**唯一设计-代码-测试闭环**；其余能力大多停留在「设计文档已承诺、代码以 stub/dead 存档」。

---

## 2. 端到端数据流（任务书 §4 的对象关系，按代码实读）

### 2.1 登录链（login-flow 设计 §10 已有「preparePlayerOnline 直呼 base」迁移登记）

代码现状（login-app + base-app + baseappmgr 交叉核实）：
1. client → login-app（tcp stub）→ `login()` 在 stub 模式恒成功（login_server.cpp:536-541）。
2. base-app 的 `PLAYER_ACTIVATE` → activatePlayer：DatabaseService（内存）load "player_+id" 造假档案 → AnchorManager::activate → bindSession。
3. login-app 的 assignWorld 消息打到 base-app 9002 → base-app 无此消息 handler（Unknown message type 被 catch-and-accept 吞掉）；真实 handler PLAYER_ASSIGN_WORLD_REQUEST 在 baseappmgr 9003。
4. gateway resolveRoute 把 PlayerResolveRouteRequest 发 base-app URL(9002) → 无 → fallback buildDefaultRoute。

**结论：真实登录-落点-进场景的闭环在任何配置下都跑不通**；内存锚点目录是唯一被单测覆盖（且明确「不 start() 只测目录方法」）的进程内环节。

### 2.2 三条主线的断点（集中列账）

| 断点 | 位置 | 后果 |
|---|---|---|
| 客户端入站 | gateway_server.cpp:356-362（"not wired yet"） | 无客户端可接入 |
| login→base assignWorld 端口/语义错配 | login→9002 无handler vs baseappmgr 9003 | 落点裁决永远是默认 |
| gateway→baseappmgr route | gateway 无 --baseappmgr 参数 | route 永走 buildDefaultRoute |
| cell-app 游戏闭环 | game/world 未编译 + broadcast 占位 + 战斗硬编码 -10 | 场景/战斗链路不存在 |
| 持久化 | DatabaseService 内存假实现 + SaveQueue 恒成功回调 | 重启即丢全部数据 |

### 2.3 移动→广播→战斗（任务书 §13/§16 焦点）

- 移动：无玩家移动输入路径（无 intent 上行、无 movement 通道）。
- 广播：cell-app `broadcastToViewers` 是明确 placeholder（注释「实际实现需要查找玩家对应的 Gateway 连接」）；AOI 事件只有 LEAVE 一种被分发，ENTER/SYNC 从未填充（aoi.cpp:95-102）。
- 战斗：cell-server 的 handleCombatSkillCast 硬编码 damage -10 广播，不落伤害；新 BattleSystem 零生产消费者；legacy battle 死头。
- 确定性（battle-determinism 四约束）：代码零实现（无 PCG 子流、无四元组 replay）。

---

## 3. Service 边界与 Manager 泛滥（任务书 §22/§23）

### 3.1 现存 Service/Manager/Context 逐审查

| 件 | 管理什么 | 拥有谁 | 修改什么 | 依赖什么 | 应否存在 |
|---|---|---|---|---|---|
| AnchorManager | player→锚点 | Anchor（shared_ptr） | 状态/绑定 | 无 | 保留（base-app 承载，语义正确） |
| SessionLocator | player↔session | 双 map | 绑定 | 无 | 保留但并入目录 |
| MapInstanceManager | instance 注册表 | MapInstance | 增删查 | 无 | 简化（无状态机） |
| WorldSessionManager | 会话状态机 | WorldSession | 七态 | 无 | 重构（见 object-model §6） |
| EntityManager（cell） | 实体注册表 | 裸指针 | update（锁内） | — | 重构（场景内实体归 Scene） |
| AOIManager（cell 版） | 定长网格 | 实体引用 | 网格迁移 | — | 合并进 Scene AOI |
| AOIGrid（legacy） | 九宫格 | 自身拷贝 | — | — | 二选一合并 |
| DatabaseService | 玩家档案 | unordered_map | 读写 | 无（内存） | 重写（真持久化栈） |
| SaveQueue | 保存任务 | 任务队列 | callback(true) | — | 重写（落盘分支不存） |
| RpcManager/ServiceManager | 消息分发 | handler 表 | — | — | 保留（正确） |
| RpcServiceManager(net/tcp) | TODO 占位 | — | — | — | 删除占位 |
| AttributeManager/ContainerManager 单例 | 属性容器 | 单例 | 锁内引用+锁外使用 | — | 收敛（两套合一） |
| BattleManager 单例（legacy 死头） | — | — | — | — | 删除 |
| LocalServiceDiscovery | 本地注册表 | unordered_map | — | — | 删除或并入 G-1 |
| RedisDistributedLock | 分布式锁（假） | void* 客户端 | 90% 随机成功 | — | 删除（零消费者） |

### 3.2 God Manager/God Context 检查结论

- **无**典型 God Manager（所有 manager 职责单一），但存在「Manager 不离 Registry」的通胀——生产路径构造链「Manager→Context→Registry→Factory→Dispatcher」是直连式而非链式，**复杂度不在这里，而在双树重复与未接线**（audit.md §5 表 A1-A9）。
- 任务书 §23 的判据（"是否真正拥有明确生命周期"）：MapInstanceManager/WorldSessionManager 有生命周期但无状态机支撑；EntityManager 无生命周期（裸指针）；DatabaseService 生命周期为空 implemented。

---

## 4. API 易用性评估（任务书 §27 + §34）

### 4.1 目标 API vs 当前事实

目标（§27 推荐形状）：`world.create_scene → scene.create_instance → instance.enter(player_id) → player.send(...)`。

当前事实：
1. **不存在 world/scene/instance/player 对象链**：WorldHost 只管服务 tick；Scene 无创建者 API（ensureDefaultMapInstance 硬编码 id=1）；player 进场景需要穿透 cell-server 私有方法 attachPlayerWorldSession（内部再 assign map+space 两套 id）。
2. 简单操作穿越层数实测：`CellServer::handleCellCrossBorder → world_session_manager → WorldSession → (entity_id 当 player_id 查)`——仅「挂起+转移+完成」编组就是 3 层簿记 + 命名空间混用。
3. examples 现状：16 个 demo（ECS/RPC/session/game_server/benchmark/serializer 等）全部门控关、CI 不跑；benchmark.cpp 甚至未链接 apollo；README/examples-README 引用的 all_features_demo/ioc_example 已删除（墓碑）。
4. 最小可运行示例（任务书 §29：login→lobby→create instance→enter→spawn→AOI→battle→reward→leave→lobby）**当前代码任何配置下都跑不完**（§2.2 五条断点）。

### 4.2 App 配置启动面

- 无统一配置文件；每个 app 自带 config.hpp 手写字段（约 20 个死字段：端口/超时/开关写了没人读）；login sessionSecret 未用；reconnectWindowMs 零引用；chatAppUrl 9003 与 baseappmgr 默认 9003 撞端口。

---

## 5. 复杂度审查（任务书 §32：代码量/依赖/接口/线程/锁/生命周期）

### 5.1 核心模块复杂度表

| 件 | 代码量 | 外部依赖 | 公开接口 | 线程 | 锁 | 生命周期复杂度 | 该复杂度是否解决问题 |
|---|---|---|---|---|---|---|---|
| modules/game/world | 639+231 | 无 | ≈56+7 态 | 0（被 tick 驱动） | mgr 各 1 mutex | 状态机七态但 Leaving 不可观察、transfer 同步化 | **否**：三层空壳+未编译 |
| modules/game/session | 302 | 无 | ≈33 | 0 | 1 | 锚六态正确 | 部分（语义对，缺持久化钩子） |
| modules/game/core+attributes+battle | 741 | 无 | ≈35+ | 0 | 属性容器锁 | 重复 attach bug（on_attach 两调） | 否：五套 ECS 之一 |
| include/apollo/game legacy 9 头 | 3,054 | 幻影头 | ≈300+（含 290 常量） | 0 | 0 | — | **否**：约 1/3 纯死 |
| modules/net + 旧头 | ~9,600 | nng(桩)/curl(桩)/Drogon(未用) | ≈340 | 多（每连接 detach 线程、收发线程） | 自死锁风险 | HttpServer 连接不回收 | 部分失真：三套并存 |
| modules/protocol（停用） | 未编译 | nng | — | worker 线程 | — | RepSocket::stop 死锁 | 否 |
| modules/core DI+runtime | 1,500 | 无 | ≈80 | 0（宿主外） | 0 | 正确（topo/逆序） | **是**（质量最高） |
| src/legacy 持久化 9 文件 | 0 编译 | mysql/redis++(关) | ≈90 | 池线程 | 池锁（正确） | CreateConnection 空 | 否 |
| apps（6 个） | 4,822 | nng(桩)+game(断) | 消息表≈35 | 每 app 1 主线程+1 保存线程 | 目录锁 | 各有生命周期但无状态机 | 部分：骨架对、接线断 |

### 5.2 提前引入的复杂设计盘点（任务书 §14/§15/§19 问题的另一半）

- **AOI 两套**（legacy 九宫格 + cell 定长网格）均未接 Scene，全部浪费。
- **IPC 全体系**（40 纯虚函数 Channel/BackpressureManager/ReconnectConfig/服务发现）默认 OFF、零消费者、无实现——典型「预埋复杂度」。
- **分布式锁**（RedisDistributedLock 90% 随机成功）零消费者。
- **队列族**：继承 pthread 手写版/ThreadPool/SPSC/无锁队列/内存池——多套并存（tests 有覆盖但生产零消费）。
- **可砍可留判定**：EntityId 包装（留）、DI（留）、WorldHost 阶段机（留）、契约族（留）；九宫格与 cell 网格（合一）、IPC（归档）、协议三套（收口一套）、属性两套（合一）。

---

## 6. 性能分析（任务书 §31，基于真实模型）

> 前提声明：当前代码**无法运行任何游戏负载**（三条主线断 + cell-app 链接失败 + 广播 placeholder），以下为「把现有结构跑起来后」的推算 + 未来基准设计；不构成实测数据。

### 6.1 模型 A：1 Scene / 1000 Players（大场景）

- 结构：1 个 MapInstance→WorldSpace→Scene + cell::EntityManager(1000 Entity) + AOI(定长网格)。
- Tick：WorldHost 20Hz，单线程；EntityManager::update **持锁遍历 1000 实体**（锁内调 on_update）。
- 广播：AOI 兴趣管理 O(1) 网格查找 + 邻域（当前广播是 placeholder，假设 N 个 viewer 各发 1 消息）→ 理论 ~1000 消息/ick 输出；以每消息 64B 计 ≈ 1.28MB/s/tick 方向 → 网关连接面是瓶颈（gateway 本身未接线）。
- 结论方向：单 Scene 千人在**当前单线程模型**下可行但无优化空间——实体 update 应去锁化（场景线程独占），AOI 网格应防 `GetOrCreateCell` 副作用新建（aoi.cpp:20,35 范围查询会写网格）。

### 6.2 模型 B：10 Scenes / 100 Players

- 结构：cell-app 单进程 10 instance（MapInstanceManager 持有 10 叉树）——**目前无多 scene 路径**（ensureDefaultMapInstance 硬编码 1 个；MapInstanceManager::create_instance 存在但无生产调用方）。
- 每 Scene 100 实体的 tick 是 A 模型 1/10 负担；锁粒度仍是整体（MapInstanceManager 一把锁盖全部 instance）→ 跨 instance 锁竞争面 10×100 后仍是一把锁。
- 可行结论：单进程多 scene 结构合理，但**锁粒度要从「管理器级」降到「scene 级」**（concurrency.md §4）。

### 6.3 模型 C：100 Instances / 10 Players（副本制典型负载）

- 结构：100 个 MapInstance（每 10 人）→ 单进程 1,000 实体。
- Tick 总预算：1,000 实体 × 20Hz；如果 tick 树整体在一线程，单帧实体 update ≤ 50µs/实体则 1,000 实体=50ms → **超 20Hz 帧预算（50ms）**，需要实体分片/场景并行或调低每实例实体更新成本；如果按 Instance 分片到多线程，100 个 instance 的锁竞争与迁移边界立即出现——当前无分片机制。
- Instance 生命周期（创建/等待/运行/结算/销毁 5 态）当前不存在 → 副本制负载的**扩缩容与回收**路径为空。
- 内存：100×10 实体每实体几百 B 量级无压力；问题在 AOI 无会话级 viewer 状态（ViewerState 设计未实现）时广播成本失控。

### 6.4 帧预算与基准设计（写入 improvement-plan P3）

- 目标帧预算（capacity-and-benchmark 文档已立：5000 CCU、帧预算表）；代码侧 benchmark.cpp 未链接 apollo（examples/CMakeLists.txt:17-18），benchmark 形态（单测基准/压测工具/长跑）全为零；Redis 热数据跨进程通道零实现。
- 约束提醒：性能目标应服务「多 Scene+大量 Instance」而不是「单一巨大无缝世界」（任务书 §31 原文）。

---

## 7. 任务书 §4 问题逐项回答（当前代码事实）

1. **创建什么对象**：见 §1.3 层叠图与 object-model.md；生产路径实际建的对象仅「锚点目录、会话记录、实例注册表、实体表」，游戏对象（Avatar/NPC/Monster）无构造路径。
2. **持有什么**：manager 持 map；MapInstance 按值持 WorldSpace 按值持 Scene；cell EntityManager 裸指针持有。
3. **调用什么**：host→service→manager→实体链（见 lifecycle.md 调用链图）。
4. **生命周期**：app 级有 Host 阶段机；业务对象级只有 WorldSession 七态与 Anchor 六态；Scene/Instance/Entity 无状态机。
5. **谁拥有对象**：见 object-model.md 表；现状=「管理器拥有一切」。
6. **谁修改对象**：持有者；跨线程修改事实见 concurrency.md（EntityManager 锁内 update、AttributeContainer 锁外引用）。
7. **谁销毁对象**：容器擦除/析构；NoAvatar 清理路径零实现。
8. **是否跨线程**：App 主线程+保存线程+网络线程混杂（concurrency.md）。
9. **是否跨进程**：是（设计），当前零（唯一跨进程路径是 stub）。
10. **是否持久化**：否（内存权威且无落盘）。
11. **依赖其他模块**：依赖图见 §8。

---

## 8. 依赖地图（Dependency Map）

```text
apps/game-server ──> apollo::game_core ──> modules/runtime ──> modules/core
apps/cell-app ──> apollo::game_world + game_core + apollo_protocol(停用) ──> runtime ──> core
apps/base-app、baseappmgr ──> apollo::game_session + apollo_protocol(停用) ──> runtime ──> core
apps/gateway-app ──> apollo::net_protocol + apollo_protocol(停用)（双门禁）──> net ──> core
apps/login-app ──> apollo_protocol(停用) ──> net ──> core

modules/net（tcp/http/websocket/rpc/protocol）──> 旧树头（include/apollo/net、include/apollo/network）
modules/game ──> legacy 头（apollo/game/aoi/aoi.hpp 等）+ runtime
modules/data ──> （自身）+（拷贝自 legacy 的重复件）
tests/net_tests ──> apollo 聚合（INTERFACE：net 五库 + base）
legacy apollo 库 ──> 引用 30+ 缺失源文件 → 结构性不可构建（仅 NOT modular 布局时）
sdks/gen ──> pugixml（XML 解析、vcpkg）──> 生成 C++/JSON/proto/lua/契约
```

花括号注：依赖环不存在（core 无反向依赖），**但模块-目录双树造成「同一头文件两个架位」的隐式依赖**（如 memory_connection.cpp 在 data/orm 与 src/apollo/database 双份）；命名空间 apollo::storage::database / apollo::database / apollo::db / apollo::data::{core,orm} 四套并存（audit.md A7 表）。

---

## 9. 术语契约差异清单（architecture 级）

1. **Zone 无代码标识**：契约 Zone=逻辑服进程（承载 1..N scene）；代码中唯一接近者是 apps/cell-app（单词 scene 的进程雏形），命名不冲突但职责未达（cell-app 名是 BW 词根，契约禁「cellapp」——见 audit.md §2.3 第 4 条，迁移时更名）。
2. **Home Zone / PlayerAnchor 语义**：PlayerAnchor 有类无 Home Zone 定位（anchor 存 base-app 进程内 map，与 "登录时分配 Home Zone、永不迁移" 的设计没有对应字段/机制）；WorldAssignment 四个 id（world/map/instance/space）+ route_version 是「换幕」机制的字段级雏形，但 cell-app 把 map_instance_id 当 space_id 用（cell_server.cpp:544-545）——两层 ID 命名空间交叉混用，契约的 scene 唯一空间单元口径与代码三层 ID 冲突（object-model.md §4 详表）。
3. **Avatar 无类**：最接近承担者 cell::PlayerEntity（cell_manager.hpp:42-93）+ world_session 的 avatar EntityId 字段；契约词继承链关系（entities.xml Monster→NPC→Avatar→Player）在代码中无对应（无 Monster/NPC 类）。
4. **instance 无状态机**：契约 instance=副本玩法生命周期（Create→Initialize→Waiting→Running→Finishing→Rewarding→Draining→Destroyed，任务书 §9 建议统一）；代码 MapInstance 五操作无状态（object-model.md §5）。
5. **Suspended 无窗口**：契约 Suspended=掉线保活窗口；代码仅枚举值（lifecycle.md §3 详述）。
6. **OnlineDirectory 无代码**：契约 manager 权威+镜像；代码仅有 baseappmgr 进程内表（audit.md §2.2 第 8 行）。
7. **网络叙事面**：net-abstraction 设计（L0-L3/四通道/InterServerLink/FrameFilter）与代码（三套 MessageHeader、三套 RPC、两套 Session 规范）的差距即 §1.4；契约词 RemoteEntityCall/InterServerLink 需在收口后出现，当前**无代码可对齐**（登记，不强行命名）。
8. **「换幕」vs 代码 transfer**：world_session 的 begin_transfer/complete_transfer 字段搬运即「换幕」的即时同步版——正确方向（无跨进程税）但无状态投影（Base 长期态→新 Cell）与易失态丢弃规则（contract §1.2：L3 不落档）；见 lifecycle.md §4。
9. **battle 可用语义**：契约「battle 专属战斗验证域」；代码两层 battle 空壳名义上是「战斗系统」——与契约语义（battle=战斗逻辑域，验证/结算）不冲突但皆为空壳，登记。

---

（完）