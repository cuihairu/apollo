# Apollo 持久化审查（persistence）

> 所属：第一阶段只读架构审查配套件之六。配套：audit.md（A7 平行栈总表）、architecture.md、object-model.md、lifecycle.md、improvement-plan.md。
> 依据：任务书 §7（Memory Authority：在线玩家内存为权威，禁止 Gameplay→DB→Result 模式）、§19（Repository/Storage/Cache/Snapshot/Journal/Write-Behind；Save 时机/Shutdown Save/Crash Recovery/Player Logout/Instance Completion/Critical Transaction）、§20（Recovery：必须持久化/可重算/可丢三档）。术语按 term-contract v1.0；差异清单见文末。

---

## 1. 总评

**代码交付 in-memory、设计承诺 write-behind、真实 DB/Redis 以死代码存档——三层错位**。仓库中**不存在任何一条能连到真实 MySQL/Redis 的持久化路径**，也不存在 csv/sqlite 可落盘链路（sqlite 仅 ipc 旁支且 IPC=OFF、vcpkg 无 sqlite3）。

实测底线（本批次审计的构建基线，全仓 88 编译单元无任何 `-D` 宏）：`APOLLO_USE_MYSQL_CONNECTOR`、`APOLLO_USE_REDIS_PLUS_PLUS`、`APOLLO_HAS_HIREDIS`、`HAVE_MYSQL` 全部未定义；vcpkg.json 依赖仅 openssl/protobuf/gtest/nlohmann-json/pugixml/readline。

---

## 2. 平行栈全景（audit.md A7 的展开——5 套 DB 抽象 + 4 份 Redis 实现）

### 2.1 数据库侧 4 套命名空间抽象

| # | 命名空间 | 实现文件 | 现状 |
|---|---|---|---|
| B1 | apollo::storage::database（IDbConnection/DbFactory/DbManager/MySQLConnection/MockConnection） | src/apollo/storage/database/{db,db_common,db_mysql,db_mock}.cpp | 全文件被 `#ifdef APOLLO_USE_MYSQL_CONNECTOR` 关死或工厂 else 分支 return nullptr；不在 modular 构建 |
| B2 | apollo::database（IDatabaseConnection/DatabaseRegistry/SqlTemplate/DataSource/ConnectionPool） | src/apollo/database/{sql_template,datasource}.cpp + modules/data/orm/src 同文件双拷贝 | 遗留树仅挂遗留库；orm/CMakeLists 只编 memory_connection+sql_template（新头无实现）；datasource.cpp 的 createConnection 因无工厂注册恒空 |
| B3 | apollo::db（IDBConnection/ConnectionPool/MySQLConnection） | src/storage/database/connection_pool.cpp ≡ modules/data/orm/src/connection_pool.cpp（同文件两份） | ConnectionPool 骨架完整（锁+cv+deque+Stats）但 **CreateConnection() 是空函数**（:139-147 仅注释）；两份均不编入 modular |
| B4 | apollo::data::{core,orm}（IConnection/IConnectSource/SimpleDataSource/MemoryConnection/SqlTemplate） | modules/data/orm/src/memory_connection.cpp + sql_template.cpp | memory_connection 可运行（SQL 精确字符串匹配，未命中静默空结果）；**新 SqlTemplate 声明的 query/update 在仓库无定义**（nm 实证只有 apollo::database::SqlTemplate 符号）→ data 系任何经 SqlTemplate 的链路链接必失败 |

### 2.2 Redis 侧 4 份实现（2 套命名空间）+ 应用层第 5 套

| # | 实现 | 文件 | 现状 |
|---|---|---|---|
| C1 | RedisPlusPlusClient（redis++ 全命令） | src/apollo/storage/redis/redis_client_impl.cpp + modules/data/redis/src 双份（已分叉 diff 141 行） | #ifdef 全文件关死；modules/data/redis/CMakeLists.txt:24 硬编码 Windows vcpkg 路径、:31-38 只查 Windows——Linux 永不命中 |
| C2 | apollo::net::redis::RedisTemplate（hiredis） | modules/data/redis/src/redis_template.cpp | 编入库但 connect() 恒 false（"hiredis not available"）；新头 `using RedisTemplate = apollo::RedisTemplate` 是无效别名（类在 apollo::net::redis，别名指错命名空间） |
| C3 | apollo::db::RedisClient（手工 RESP 协议 2066 行） | modules/data/cache/src/redis_cache.cpp + src/storage/cache/redis.cpp 双份 | 命令实现最完整（AUTH/GET/SET/EXPIRE/哈希…）但**完全未编译**（cache/CMakeLists 只编 cache_manager+primitive_cache） |
| C4 | MockRedisClient | include/apollo/storage/redis/redis_mock.h + src/apollo/storage/redis/redis.cpp 工厂默认分支 | 内存 unordered_map；唯一被编译的 redis 实现（遗留库）；唯一消费者 test_redis_new.cpp（GTest 分支默认不建） |
| 第 5 套 | base-app DatabaseService | apps/base-app/src/database_service.cpp | 与上面全部断联（不 include 任何 data/storage 头）；实例级内存栈 |

**重复件统计**：ConnectionPool ×2 同文件；RedisFactory/RedisManager ×2 逐字节相同（diff=0，新目录放的是遗留文件）;RedisClient ×2;redis_client_impl ×2 已分叉;redis_client_wrapper.cpp/redis_template_wrapper.cpp 是空文件。**9 个以上的重复实现文件，真连介质能力为零。**

---

## 3. 内存权威模型现状（任务书 §7 检查）

### 3.1 玩家对象（Anchor ↔ DB）

```text
现状：
  load：DatabaseService::loadPlayer == cache_[id]；miss 则构造 bootstrap 玩家 username="player_+id" 写回缓存（database_service.cpp:31-49）
        ——「从空气里长出档案」，且 toJson/fromJson 不对称（fromJson 缺 x/y/z，base_server.cpp:87-94）
  save：savePlayer 仅 cache_[id]=data（:48-52）；savePlayerAsync 同步完成；SaveQueue::workerLoop 只 callback(true) 不写任何介质（:158）
  autoSave：autoSaveLoop 仅打印 "Auto-save triggered"；handleDbQueryRequest "Not implemented"
  结论：进程一停，玩家数据全部丢失
```

### 3.2 权威模型对照

| 任务书 §7 的推荐 | 代码 | 判定 |
|---|---|---|
| 在线 Player 内存为权威 | **是**（内存 map 唯一真相） | 一半符合——权威在内存 ✓，但下游（journal/snapshot/落库）全空 ✗ |
| Gameplay→DB→Result 反模式禁止 | 现状不是反模式，是「Gameplay→内存→（无）**」——**缺的不是权威方向而是落盘** | 结构性半程到位 |
| Dirty/Journal/Snapshot 三态 | dirty 标记在 PlayerAnchor 存在（dirty_reasons_），但 needs_save 仅测试消费；journal 零实现 | ✗ |

### 3.3 存储介质盘点

| 介质 | 代码 | 可运行性 |
|---|---|---|
| MySQL | B1 #ifdef 关死 | ✗ |
| Redis | C1-C3 关死/未编/connect false | ✗ |
| SQLite | src/apollo/ipc/sqlite_service_discovery.cpp（IPC=OFF + vcpkg 无 sqlite3） | ✗ |
| 文件/JSON 落盘 | 无 | ✗ |
| 内存 | MemoryConnection + DatabaseService map | ✓ 唯一现网路径 |

---

## 4. Save 时机与操作正确性审查（任务书 §19 清单逐项）

| 时机 | 现状 | 判定 |
|---|---|---|
| 正常保存（dirty 驱动） | 无（autoSave 空转；mark_dirty 只置位） | ✗ |
| Shutdown Save | 无（停止时无 flush 协议） | ✗ |
| Crash Recovery | journal/WAL/redo 全仓零命中（grep 实证） | ✗ |
| Player Logout | 无登出消息类型；deactivate 直接擦除 | ✗ |
| Instance Completion | 无 instance 生命周期（结算结果无处落） | ✗ |
| Critical Transaction | 事务只存在于 mock（test_database.cpp）与遗留 SqlTemplate 实现；无生产消费 | ✗ |
| 数据一致性 | 无版本/增量对比；fromJson/toJson 不对称是现存 round-trip 缺陷 | ✗ |

**唯一「真」的保存路径**：game-server demo 的 MemoryConnection seed_query（SQL 精确字符串命中，未命中返回空行集不报错）——演示性质，且经 SqlTemplate 层即链接失败（§2.1 B4）。

---

## 5. 目标模型（contract §1.5 + attribute-sync §8.2；improvement-plan P1 锚点）

```text
Load:   启动/登录 → 读 DB（或预热缓存）→ Anchor 内存权威
Runtime:Anchor 在线态内存权威；L3 易失态（HP/位置）不落档
Dirty:  write-behind journal（PersistJournal）：变更先追加日志（兼热备流/恢复位点）
        每 tick 定额均匀出队（attribute-sync §8.2：每 tick 定比、有界队列）
Snapshot:定时快照压薄 journal；进程重启从 journal 位点回放
Redis:  仅跨进程热数据（在线目录镜像/订阅推送），**不是实体缓存**（attribute-sync 口径）
层级:   Repository（DAO 映射）→ Storage（介质抽象）→ Cache（可选读缓存）→ Journal
```

与任务书 §19 的对应：Repository/Storage/Cache/Snapshot/Journal/Write-Behind 六件在当前代码**全部不存在**；目标模型六件齐备后，§4 的七个时机逐项可答。

---

## 6. 术语契约差异清单（persistence 级）

1. **`PersistJournal`（write-behind journal）**：契约 §1.5 先记日志后刷快照；代码零实现（grep journal/snapshot/redo/undo/WAL 在数据体系零命中；命中的只有 log_manager 的“日志快照”词汇）——**命名契约已在，实现缺失**。差异登记。
2. **契约「Redis 仅作为跨进程热数据」**（attribute-sync 口径）：代码侧 Redis 全链关死，无偏离也无实现——登记为「无代码可对齐」。
3. **`AnchorManager` 与存档**：契约 Anchor 断线驻留保状态、存档走 write-behind；代码 save 假实现——功能缺口登记（persistence §3.1）。
4. **MemoryConnection 的「静默空结果」**（未命中 SQL 返回 ok=true）：与任务书 §25 错误处理（Recoverable/Retryable/Fatal 明确）相悖；属 bug 面，登记于 improvement-plan P1-4。
5. 其余（Repository/DAO 面）契约未定词，用代码词或工业化词（如 `Repository`），不与他家冲突。

---

（完）