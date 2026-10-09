# Apollo 并发模型（concurrency）

> 所属：第一阶段只读架构审查配套件之五。配套：audit.md、architecture.md、object-model.md、lifecycle.md、persistence.md、improvement-plan.md。
> 依据：任务书 §26（并发模型：谁可以修改 Player/Scene/Instance/Entity/Persistence，明确 Owner + 明确执行上下文）、§17（Tick/Scheduler）、§32（线程/锁复杂度）。术语按 term-contract v1.0；差异清单见文末。

---

## 1. 线程盘点（现状实测）

| 线程 | 谁建 | 跑什么 | 现状问题 |
|---|---|---|---|
| 主线程 | 各 app main | login/gateway/base/cell 的主逻辑 + 消息处理 | game-server 用 ServiceHost run_once 循环；cell-app 主循环 = gameLoop（handleMessages + map_instance_manager_.update + worker_pool 递交 + tick+sleep） |
| 自动保存线程 | base-app ctor | SaveQueue::workerLoop（100ms wait_for 轮询队列） | 只 callback(true)；无停止协议；60s autoSaveLoop 仅打印 |
| 网络收发线程 | net 库各组件 | RpcClient/RpcServer 的收发循环（10ms sleep 空转占位）；HttpServer 每连接 spawn detach 线程 | detach 线程不回收；stop 挂死 |
| WorkerPool | cell-app（ThreadPool） | 无游戏逻辑任务（空转） | 递交但无消费语义 |
| WorldHost tick | cell-app 主循环直接调 | CellWorldService::tick：EntityManager.update + MapInstanceManager.update（**持锁**） | 见 §3 |
| IPC 相关 | APOLLO_ENABLE_IPC=OFF | — | 未编 |

**没有**：场景线程、持久化写线程（真）、目录线程、多 scene 并行线程。**一切业务都在主线程 + 互斥锁保护的内存容器**。

---

## 1.1 后况勘误（2026-10-07 实测更新）

本节为 §1 快照的后况增补——P0-3 之后的重构改变了线程拓扑，本文写作时的
「一切业务都在主线程」已不成立：

- cell-app 拆出 `gameThread_`（cell_server.cpp:142）跑 `worldHost_->tick()`；
  消息 handler 由 `protocol::RepSocket` 的自持 worker 线程执行
  （socket.cpp:124 workerLoop）——**handler 写 world（create/destroy/move/
  cross-border transfer/combat）与 tick 读写 world 跨线程并发，Scene
  （P1-3 后无锁单写者形态）与 World 树在此二线程之间无任何保护**——真
  竞争（生产未触发仅因负载空洞，architecture §6.1「模型空洞」同一成因）。
  WorldSessionManager 的 mutex 只保护会话表自身，不覆盖 world 树。
- base-app：autoSaveThread_ + SaveQueue worker 线程实存（§1 表已录）；
  AnchorManager/SessionLocator 的调用线程归属待 P3-2 批 B 地图收口。

修复方向按 §3 目标模型（「其余线程经消息投递后修改」）——批 B 立项见
docs/todo P3-2；方案分叉（队列化异步受理 / 队列化+同步等待 / 最小互斥
回退）属 RPC 语义取舍，列拍板项——**前置设计已立** docs/design/cell-single-writer.md
（2026-10-10：竞争实况 A 级实读 + 三岔语义/风险对表 + 五拍板点，候选倾向
=(a) 队列化异步受理，(c) 反证否决）。

---

## 2. 锁盘点（现状实存清单）

| 锁 | 保护什么 | 持有时长/风险 |
|---|---|---|
| AnchorManager::mutex_ | player→anchor map | 短（activate/find/deactivate/count 各一临界区）——**健康** |
| SessionLocator::mutex_ | 双向绑定 map | 短——健康 |
| MapInstanceManager::mutex_ | instances_ map | **update() 在锁内跑整棵 tick 树**（map_instance_manager.cpp:25-32）——长临界区 |
| WorldSessionManager::mutex_ | sessions_+player_index_ 双 map | 短（各操作一临界区，无死锁结构） |
| EntityManager（cell） | entity map | update 持锁遍历实体调 on_update（cell_server.cpp:38-42 链） |
| cell::AOIManager | 定长网格 | 网格更新短 |
| AttributeManager/Container（两套） | 属性容器 | **锁内返回引用/指针、锁外使用者读**——竞争窗口（attribute.cpp:91-94,107-111） |
| CacheManager/PrimitiveCache | 内存缓存 | 短——健康 |
| ConnectionPool×2 | 空闲 deque+cv | 正确（GetConnection/ReleaseConnection 标准模式），但 CreateConnection 为空函数 |
| DatabaseService::cacheMutex_ | 玩家 map | 短——健康（但无介质写入） |
| SaveQueue mutex+cv | 任务队列 | 正确 |
| SessionManager::mutex_（net） | 会话表 | **自死锁**：onDisconnected 锁内调 getSession 再锁（session_manager.cpp:273-280）——非递归锁 |
| Reactor::mutex_ | socket 表 | 短 |
| io_context / 队列族 | 继承件 | 短 |
| RedisDistributedLock | （假） | 90% 随机成功，零消费者 |

**结论**：锁数量约 13 处（另有 Timer/Channel 等未编件不计）；「短临界区」占多数且结构健康；问题集中在 3 处：锁内跑业务树（MapInstanceManager）、锁内回调（EntityManager）、锁外使用引用（Attribute）+ 1 处死锁（SessionManager）+ 1 处**无锁**（DI get 并发为 UB，靠「启动前构建、运行期只读」约定——目前调用方遵守，但无校验）。

---

## 3. 「谁可以修改谁」矩阵（任务书 §26 的答案——现状）

| 被修改对象 | 现状允许者 | 实际进入路径 | 问题 |
|---|---|---|---|
| PlayerAnchor | AnchorManager 持有者（base-app 请求线程） | activate/bind/unbind/deactivate 全在请求 handler 线程 | 单线程+锁=灰区；无「改变者身份」概念（将来多线程时无护栏） |
| WorldSession | cell-app 主线程（持 WorldSessionManager 锁后） | attach/detach/transfer | 单写者是好的；但锁内操作簿记，无场景线程边界 |
| Scene.entities_ | cell-app 主线程 | Scene::update 单线程 | 生产零实体——模型空洞 |
| Entity（cell） | 持锁更新 | on_update 在锁内 | 锁内回调：回调再入 manager 即死锁风险 |
| MapInstance 树 | 锁内 tick | update 链 | 长临界区与广播混合 |
| 属性容器 | 任何线程 get 引用 | AttributeManager（锁内取、锁外写） | 竞争窗口 |
| 会话表（net） | SessionManager | OnDisconnected 锁内再入 | 自死锁 |
| 缓存 | 池锁 | — | 健康 |
| DB 假实现 | 请求线程 | savePlayer 同步 | 无真写者 |

**目标模型（契约/设计口径 + 任务书 §26 推荐）**：

```text
明确的 Owner + 明确的执行上下文：
  PlayerAnchor：Home Zone「主页线程」单写者（其余线程经消息投递后修改）
  Scene/Instance/Entity/AOI：scene 线程（每 scene 一个执行上下文；tick 固定步长 10Hz 设计口径；
      当前 WorldHost 20Hz 默认值需与 clock-and-time 文档的 10Hz 对齐——见差异清单 5）
  Session/Connection：gateway 连接线程（单向下行缓冲）
  Persistence：写为异步（write-behind 出队在专用线程）；读为同步预热
  Manager 层：全部去锁化（单写者后 mutex 变断言工具而非同步机制，或直接从热路径移除）
```

---

## 4. Tick/Scheduler 现状与目标（任务书 §17）

| 层级 | 现状 | 目标 |
|---|---|---|
| World | WorldHost 20Hz 单循环（未编译） | Zone 世界钟（10Hz 设计口径；clock-and-time 三钟：steady tick / wall / now） |
| Scene | 无独立 tick（Scene::update 只是遍历空 entities_，随 MapInstanceManager::update 一起跑） | scene tick：simulate→recalc→aoidecay→collect→budget+flush→persist-batch（attribute-sync 六阶段） |
| Instance | 无（与 scene 同一棵树） | 玩法 tick（等待/运行/结算阶段各自节奏） |
| Battle | 无（两层空壳） | battle runtime 在 scene tick 内（或独立定频） |
| 多线程 | 主循环单线程 | 多 scene 可并行（P2/P3 边界；先单 scene 单线程正确性） |

**过度复杂检查（§17 后半）**：现无复杂调度器——这是好事；风险在另一面：**框架层没有定帧抽象可用**（WorldHost 未编译 + game-server 无循环），唯一在跑的是 cell-app 手写 `gameLoop`（tick + 100ms sleep 混杂，sleep 与帧率预算无关）。

---

## 5. 并发问题清单（severity 排序）

| # | 问题 | 证据 | 级别 |
|---|---|---|---|
| C-1 | MapInstanceManager/EntityManager 锁内跑业务树（tick 回调在临界区内） | map_instance_manager.cpp:25-32 | P0（场景并发正确性基础） |
| C-2 | SessionManager 自死锁（非递归锁内再入） | session_manager.cpp:273-280 | P1 |
| C-3 | 属性容器锁内取得引用/指针、锁外使用（竞争窗口） | attribute.cpp:91-94,107-111 | P1 |
| C-4 | 无「执行上下文」概念：一切对象缺 owner-thread 声明（谁在哪个线程跑无文档无校验） | 全代码 | P0（任务书 §26 首要） |
| C-5 | HttpServer detach 线程不回收 + stop 挂死（并发资源泄漏） | http.cpp:506-549 | P1 |
| C-6 | DI get 并发 UB（无锁、靠约定） | application_context.hpp | P2（记录勿改语义） |
| C-7 | RepSocket::stop 先 join 后关 socket 死锁（模块停用中，修复列入传输收口） | modules/protocol/src/socket.cpp:127-133 | P2 |
| C-8 | 无 TSAN/sanitizer 门禁、无并发压测 | CI 全布局核查 | P3 |

---

## 6. 术语契约差异清单（concurrency 级）

1. **「场景线程单写者」**：契约 §1.5 tick 词条（固定节拍全序推进，场景线程单写者 10Hz）。代码无场景线程概念（§4）、无 10Hz（WorldHost 默认 20Hz，world_host.hpp:17——clock-and-time 文档 10Hz 与代码默认值不一致，登记）；「单写者」现状事实为「主线程+锁」，语义符合但命名面不存在。差异登记。
2. **tick 号（tick 序列）**：契约要求 tick 号全序唯一；WorldTickContext{tick_index, delta_seconds, now} 头文件有字段（恰是契约设计）但 WorldHost 未编译——实现侧待落地。登记。
3. **「锁数量」复杂度审查结论**（任务书 §32 口径）：现有锁 13+ 处对「单进程 1..N scene」目标规模是**过量**（目标模型锁数 ≈ 2：保存队列 + 目录镜像），P0 去锁化目标纳入 improvement-plan 收敛判据。
4. 其余契约词（四通道 seq、acked_seq、ChangeHistory）本文件不涉及，见 architecture.md §1.5 差距表。

---

（完）