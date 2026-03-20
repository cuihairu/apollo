# Q28: 如何解决数据一致性问题？

## 问题分析

本题考察对分布式系统数据一致性的理解：
- 一致性问题的分类和原因
- 事务处理机制
- 分布式事务解决方案
- 最终一致性实现
- KBEngine 的数据一致性处理

---

## 一、数据一致性问题

### 1.1 一致性问题分类

```
┌─────────────────────────────────────────────────────────────┐
│                    数据一致性问题分类                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 更新丢失                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  场景: 两个更新同时修改同一数据                     │       │
│  │                                                   │       │
│  │  玩家 A 有 100 金币                                  │       │
│  │  事务1: 花费 50 金币                                 │       │
│  │  事务2: 花费 30 金币                                 │       │
│  │                                                   │       │
│  │  结果: 可能只剩 70 或 50 金币（一个更新丢失）          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 脏读                                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  场景: 读取未提交的数据                             │       │
│  │                                                   │       │
│  │  事务1: 转账 100 金币（未提交）                      │       │
│  │  事务2: 读取余额（读到增加后的余额）                  │       │
│  │  事务1: 回滚                                         │       │
│  │                                                   │       │
│  │  结果: 事务2 读到脏数据                              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 不可重复读                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  场景: 同一事务内两次读取结果不同                   │       │
│  │                                                   │       │
│  │  事务1: 读取余额 = 100                              │       │
│  │  事务2: 修改余额 = 80                               │       │
│  │  事务1: 再次读取余额 = 80（与之前不同）              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 幻读                                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  场景: 查询结果集发生变化                           │       │
│  │                                                   │       │
│  │  事务1: 查询等级 > 10 的玩家（3人）                  │       │
│  │  事务2: 新增一个等级 11 的玩家                       │       │
│  │  事务1: 再次查询（4人，多了1个）                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 CAP 定理

```
┌─────────────────────────────────────────────────────────────┐
│                      CAP 定理                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  在分布式系统中，三者最多只能同时满足两点:                   │
│                                                             │
│       C ────────────────────────────────────────┐          │
│       │ 一致性 (Consistency)                      │          │
│       │ 所有节点在同一时间看到相同的数据              │          │
│       │                                   │          │
│       │                            ┌────┴────┐     │          │
│       │                            │         │     │          │
│       │                          A ────────┐ │     │          │
│       │                          │可用性   │ │     │          │
│       │                          │(Availability)│    │          │
│       │                          └─────────┘ │     │          │
│       │                                       │     │          │
│       │                          ┌─────────────┘     │          │
│       │                          │                     │          │
│       │                          P ──────────────┐    │          │
│       │                          分区容错性 (Partition Tolerance)│
│       │                          网络分区时系统仍能运行       │          │
│       └──────────────────────────┴───────────────────────┘          │
│                                                             │
│  MMO 中的权衡:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  CP: 强一致性，牺牲可用性                           │       │
│  │  - 交易系统必须保证一致                              │       │
│  │  - 网络分区时暂停服务                                │       │
│  │                                                   │       │
│  │  AP: 高可用，牺牲一致性                             │       │
│  │  - 聊天系统可以接受延迟                              │       │
│  │  - 位置同步允许短暂不一致                            │       │
│  │                                                   │       │
│  │  BASE: 基本可用，软状态，最终一致                    │       │
│  │  - 大部分游戏系统采用此方案                          │       │
│  │  - 保证最终一致性即可                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、事务处理

### 2.1 ACID 特性

```
┌─────────────────────────────────────────────────────────────┐
│                    ACID 事务特性                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  A - 原子性 (Atomicity)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  事务中的操作要么全部成功，要么全部失败               │       │
│  │                                                   │       │
│  │  示例: 转账操作                                     │       │
│  │  1. 扣除 A 的金币                                  │       │
│  │  2. 增加 B 的金币                                  │       │
│  │                                                   │       │
│  │  如果步骤2失败，步骤1也要回滚                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  C - 一致性 (Consistency)                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  事务执行前后，数据库都处于一致状态                   │       │
│  │                                                   │       │
│  │  示例: 金币总量不变                                 │       │
│  │  转账只是金币从一个玩家到另一个玩家                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  I - 隔离性 (Isolation)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  并发事务之间相互隔离                               │       │
│  │                                                   │       │
│  │  隔离级别:                                         │       │
│  │  ├── 读未提交 (READ UNCOMMITTED)                  │       │
│  │  ├── 读已提交 (READ COMMITTED)                    │       │
│  │  ├── 可重复读 (REPEATABLE READ) ← 推荐            │       │
│  │  └── 串行化 (SERIALIZABLE)                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  D - 持久性 (Durability)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  事务提交后，数据永久保存                           │       │
│  │                                                   │       │
│  │  即使系统崩溃，已提交的数据也不会丢失                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 事务隔离级别

```
┌─────────────────────────────────────────────────────────────┐
│                  事务隔离级别对比                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  隔离级别           │ 脏读 │ 不可重复读 │ 幻读 │ 性能      │
│  ├───────────────────────────────────────────────────────┤ │
│  │ 读未提交         │  可能 │  可能    │ 可能 │ 最高      │ │
│  │ 读已提交         │  避免 │  可能    │ 可能 │ 高        │ │
│  │ 可重复读 (推荐)  │  避免 │  避免    │ 可能 │ 中        │ │
│  │ 串行化           │  避免 │  避免    │ 避免 │ 低        │ │
│                                                             │
│  MMO 推荐设置:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  SET GLOBAL TRANSACTION ISOLATION LEVEL          │       │
│  │  REPEATABLE READ;                                │       │
│  │                                                   │       │
│  │  理由:                                            │       │
│  │  - 避免脏读和不可重复读                             │       │
│  │  - 性能影响可接受                                   │       │
│  │  - 幻读在游戏场景较少                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 事务实现

```cpp
// 数据库事务封装

class DatabaseTransaction {
public:
    DatabaseTransaction(Database* db) : db_(db), committed_(false) {
        // 开始事务
        db_->execute("START TRANSACTION");
    }

    ~DatabaseTransaction() {
        if (!committed_) {
            // 未提交则回滚
            rollback();
        }
    }

    // 提交事务
    bool commit() {
        if (db_->execute("COMMIT")) {
            committed_ = true;
            return true;
        }
        return false;
    }

    // 回滚事务
    void rollback() {
        db_->execute("ROLLBACK");
        committed_ = true; // 标记为已处理
    }

    // 执行 SQL
    bool execute(const std::string& sql) {
        return db_->execute(sql);
    }

    // 查询
    Result query(const std::string& sql) {
        return db_->query(sql);
    }

private:
    Database* db_;
    bool committed_;
};

// 使用示例
void transferGold(uint64_t fromPlayer, uint64_t toPlayer, int64_t amount) {
    Database* db = Database::instance();

    DatabaseTransaction txn(db);

    // 1. 检查发送者余额
    auto result = txn.query(fmt::format(
        "SELECT gold FROM player WHERE id = {} FOR UPDATE",
        fromPlayer
    ));

    if (result.empty()) {
        throw std::runtime_error("Player not found");
    }

    int64_t gold = result[0]["gold"];
    if (gold < amount) {
        throw std::runtime_error("Insufficient gold");
    }

    // 2. 扣除发送者金币
    txn.execute(fmt::format(
        "UPDATE player SET gold = gold - {} WHERE id = {}",
        amount, fromPlayer
    ));

    // 3. 增加接收者金币
    txn.execute(fmt::format(
        "UPDATE player SET gold = gold + {} WHERE id = {}",
        amount, toPlayer
    ));

    // 4. 记录交易日志
    txn.execute(fmt::format(
        "INSERT INTO trade_log (from_player, to_player, amount, time) "
        "VALUES ({}, {}, {}, NOW())",
        fromPlayer, toPlayer, amount
    ));

    // 提交事务
    if (!txn.commit()) {
        throw std::runtime_error("Transaction commit failed");
    }
}
```

---

## 三、分布式事务

### 3.1 两阶段提交 (2PC)

```
┌─────────────────────────────────────────────────────────────┐
│                    两阶段提交 (2PC)                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  准备阶段 (Phase 1):                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  协调者 ──prepare──► 参与者 A                      │       │
│  │  协调者 ──prepare──► 参与者 B                      │       │
│  │  协调者 ──prepare──► 参与者 C                      │       │
│  │                                                   │       │
│  │  参与者 A ──yes────────► 协调者                    │       │
│  │  参与者 B ──yes────────► 协调者                    │       │
│  │  参与者 C ──no─────────► 协调者                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  提交阶段 (Phase 2):                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  有一方拒绝，全部回滚:                              │       │
│  │  协调者 ──rollback──► 参与者 A                     │       │
│  │  协调者 ──rollback──► 参与者 B                     │       │
│  │  协调者 ──rollback──► 参与者 C                     │       │
│  │                                                   │       │
│  │  全部同意，全部提交:                                │       │
│  │  协调者 ──commit────► 参与者 A                     │       │
│  │  协调者 ──commit────► 参与者 B                     │       │
│  │  协调者 ──commit────► 参与者 C                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  问题:                                                     │
│  ├── 协调者单点故障                                       │
│  ├── 阻塞，性能差                                         │
│  └── 数据不一致窗口                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 TCC 事务

```
┌─────────────────────────────────────────────────────────────┐
│                    TCC (Try-Confirm-Cancel)                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  TCC 将业务逻辑分成三个阶段:                                 │
│                                                             │
│  Try 阶段:                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  完成业务检查                                       │       │
│  │  预留必须资源                                       │       │
│  │                                                   │       │
│  │  示例: 转账操作                                     │       │
│  │  - 检查账户余额                                     │       │
│  │  - 冻结转账金额                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  Confirm 阶段:                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  确认执行业务                                       │       │
│  │  使用 Try 阶段预留的资源                           │       │
│  │                                                   │       │
│  │  示例: 转账操作                                     │       │
│  │  - 扣除发送者冻结金额                               │       │
│  │  - 增加接收者金额                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  Cancel 阶段:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  取消执行业务                                       │       │
│  │  释放 Try 阶段预留的资源                           │       │
│  │                                                   │       │
│  │  示例: 转账操作                                     │       │
│  │  - 解冻发送者金额                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优势:                                                     │
│  ├── 性能比 2PC 好                                         │
│  ├── 业务层控制，灵活                                       │
│  └── 适合长事务                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// TCC 事务实现

// TCC 接口
class TCCTransaction {
public:
    virtual bool try() = 0;
    virtual bool confirm() = 0;
    virtual bool cancel() = 0;
};

// 转账 TCC 实现
class TransferTCC : public TCCTransaction {
public:
    TransferTCC(uint64_t from, uint64_t to, int64_t amount)
        : from_(from), to_(to), amount_(amount) {}

    // Try 阶段: 检查并冻结资金
    bool try() override {
        Database* db = Database::instance();
        DatabaseTransaction txn(db);

        // 1. 检查发送者余额
        auto result = txn.query(fmt::format(
            "SELECT gold FROM player WHERE id = {} FOR UPDATE",
            from_
        ));

        if (result.empty() || result[0]["gold"] < amount_) {
            return false;
        }

        // 2. 冻结金额
        txn.execute(fmt::format(
            "UPDATE player SET frozen_gold = frozen_gold + {} "
            "WHERE id = {}",
            amount_, from_
        ));

        // 3. 记录冻结
        txn.execute(fmt::format(
            "INSERT INTO freeze_log (player_id, amount, ref_id) "
            "VALUES ({}, {}, '{}')",
            from_, amount_, refId_
        ));

        return txn.commit();
    }

    // Confirm 阶段: 完成转账
    bool confirm() override {
        Database* db = Database::instance();
        DatabaseTransaction txn(db);

        // 1. 扣除冻结金额
        txn.execute(fmt::format(
            "UPDATE player SET gold = gold - {}, frozen_gold = frozen_gold - {} "
            "WHERE id = {}",
            amount_, amount_, from_
        ));

        // 2. 增加接收者
        txn.execute(fmt::format(
            "UPDATE player SET gold = gold + {} WHERE id = {}",
            amount_, to_
        ));

        // 3. 更新冻结记录
        txn.execute(fmt::format(
            "UPDATE freeze_log SET status = 'confirmed' "
            "WHERE ref_id = '{}'",
            refId_
        ));

        return txn.commit();
    }

    // Cancel 阶段: 解冻
    bool cancel() override {
        Database* db = Database::instance();
        DatabaseTransaction txn(db);

        // 1. 解冻金额
        txn.execute(fmt::format(
            "UPDATE player SET frozen_gold = frozen_gold - {} "
            "WHERE id = {}",
            amount_, from_
        ));

        // 2. 更新冻结记录
        txn.execute(fmt::format(
            "UPDATE freeze_log SET status = 'cancelled' "
            "WHERE ref_id = '{}'",
            refId_
        ));

        return txn.commit();
    }

private:
    uint64_t from_;
    uint64_t to_;
    int64_t amount_;
    std::string refId_ = generateUUID();
};

// TCC 协调器
class TCCCoordinator {
public:
    // 执行 TCC 事务
    bool execute(std::vector<std::shared_ptr<TCCTransaction>> transactions) {
        // Try 阶段
        for (auto& txn : transactions) {
            if (!txn->try()) {
                // 失败，取消已执行的 Try
                for (auto& t : executedTries_) {
                    t->cancel();
                }
                return false;
            }
            executedTries_.push_back(txn);
        }

        // Confirm 阶段
        for (auto& txn : transactions) {
            if (!txn->confirm()) {
                // Confirm 失败，记录日志
                LOG_ERROR("TCC Confirm failed, need manual intervention");
                // 生产环境需要补偿机制
            }
        }

        return true;
    }

private:
    std::vector<std::shared_ptr<TCCTransaction>> executedTries_;
};
```

---

## 四、最终一致性

### 4.1 BASE 理论

```
┌─────────────────────────────────────────────────────────────┐
│                      BASE 理论                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  B - Basically Available (基本可用)                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  系统保证可用性，但不保证强一致                       │       │
│  │  即使部分节点故障，系统仍能响应                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  S - Soft State (软状态)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  允许系统中的数据存在中间状态                         │       │
│  │  这个中间状态不影响系统可用性                         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  E - Eventually Consistent (最终一致)                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  系统保证在没有新更新的情况下                       │       │
│  │  经过一段时间后，数据最终达到一致状态                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 最终一致性实现

```cpp
// 最终一致性：异步同步 + 冲突解决

class EventuallyConsistentStore {
public:
    // 更新玩家数据（异步写入数据库）
    void updatePlayer(uint64_t playerId, const std::string& key,
                     const std::string& value) {
        // 1. 立即写入缓存
        cache_.set(playerId, key, value);

        // 2. 加入同步队列
        SyncTask task;
        task.playerId = playerId;
        task.key = key;
        task.value = value;
        task.timestamp = getCurrentTime();
        task.version = ++version_;

        syncQueue_.push(task);
    }

    // 读取玩家数据
    std::string getPlayer(uint64_t playerId, const std::string& key) {
        // 先读缓存
        std::string value = cache_.get(playerId, key);
        if (!value.empty()) {
            return value;
        }

        // 缓存未命中，读数据库
        value = loadFromDatabase(playerId, key);
        if (!value.empty()) {
            cache_.set(playerId, key, value);
        }

        return value;
    }

private:
    // 同步队列处理
    void syncLoop() {
        while (running_) {
            SyncTask task;
            if (syncQueue_.pop(task, 1000)) {
                syncToDatabase(task);
            }

            // 批量刷新缓存到数据库
            if (syncQueue_.size() >= BATCH_SIZE) {
                flushBatch();
            }
        }
    }

    void syncToDatabase(const SyncTask& task) {
        // 使用 UPSERT
        std::string sql = fmt::format(
            "INSERT INTO player_data (player_id, data_key, data_value, version) "
            "VALUES ({}, '{}', '{}', {}) "
            "ON DUPLICATE KEY UPDATE "
            "data_value = VALUES(data_value), "
            "version = VALUES(version)",
            task.playerId, task.key, task.value, task.version
        );

        database_->execute(sql);
    }

    void flushBatch() {
        std::vector<SyncTask> batch;
        while (syncQueue_.size() > 0 && batch.size() < BATCH_SIZE) {
            SyncTask task;
            if (syncQueue_.try_pop(task)) {
                batch.push_back(task);
            }
        }

        // 批量写入
        if (!batch.empty()) {
            bulkInsert(batch);
        }
    }

    struct SyncTask {
        uint64_t playerId;
        std::string key;
        std::string value;
        uint64_t timestamp;
        uint64_t version;
    };

    ThreadSafeQueue<SyncTask> syncQueue_;
    MemoryCache cache_;
    Database* database_;

    static constexpr size_t BATCH_SIZE = 100;
    uint64_t version_ = 0;
};

// 冲突解决：版本向量
class VersionVector {
public:
    // 检查是否冲突
    bool hasConflict(const Operation& op1, const Operation& op2) {
        // 并发操作可能冲突
        return !(op1.version < op2.version || op2.version < op1.version);
    }

    // 合并操作
    Operation merge(const Operation& op1, const Operation& op2) {
        if (!hasConflict(op1, op2)) {
            // 无冲突，选择较新的
            return op1.version > op2.version ? op1 : op2;
        }

        // 有冲突，应用解决策略
        return resolveConflict(op1, op2);
    }

private:
    Operation resolveConflict(const Operation& op1, const Operation& op2) {
        // 策略1: 时间戳优先
        if (op1.timestamp > op2.timestamp) {
            return op1;
        }
        return op2;

        // 策略2: 业务规则
        // 例如: 加法操作可以合并，取最大值等
    }

    struct Operation {
        uint64_t playerId;
        std::string key;
        std::string value;
        uint64_t version;
        uint64_t timestamp;
    };
};
```

---

## 五、KBEngine 数据一致性

### 5.1 KBEngine 存档机制

```cpp
// KBEngine 自动存档机制
// src/server/entitydef/entity_def.h

namespace KBEngine {

class Entity {
public:
    // 写存档（自动调用）
    void writeToDB() {
        // 1. 检查是否需要存档
        if (!shouldArchive()) {
            return;
        }

        // 2. 序列化实体数据
        MemoryStream stream;
        addToStream(stream);

        // 3. 发送到 DBMgr
        Bundle* pBundle = DBMgrInterface::createBundle(
            dbmgr,
            ::onRemoteAutoLoadEntityCreateAccountMailbox
        );

        (*pBundle) << id_;
        (*pBundle) << className_;
        (*pBundle) << stream;

        DBMgrInterface::send(pBundle);

        // 更新存档时间
        lastArchiveTime_ = timeStamp();
    }

    // 检查是否需要存档
    bool shouldArchive() {
        uint64_t now = timeStamp();
        uint64_t interval = now - lastArchiveTime_;

        return interval > g_kbeSrvConfig.entityArchiveInterval();
    }

    // 从数据库加载
    bool loadFromDB(EntityID id) {
        // 请求数据库加载数据
        Bundle* pBundle = DBMgrInterface::createBundle(
            dbmgr,
            dbmgr::queryEntity
        );

        (*pBundle) << id;

        DBMgrInterface::send(pBundle);

        return true;
    }

private:
    uint64_t lastArchiveTime_ = 0;
};

} // namespace KBEngine
```

### 5.2 KBEngine 数据备份

```python
# KBEngine 数据库备份脚本
# db_backups/backup_db.sh

#!/bin/bash

# 配置
DB_USER="kbengine"
DB_PASS="password"
DB_NAME="kbengine"
BACKUP_DIR="/backup/kbengine"
DATE=$(date +%Y%m%d_%H%M%S)

# 创建备份目录
mkdir -p $BACKUP_DIR

# 备份数据库
mysqldump -u$DB_USER -p$DB_PASS $DB_NAME > $BACKUP_DIR/kbengine_$DATE.sql

# 压缩备份
gzip $BACKUP_DIR/kbengine_$DATE.sql

# 删除7天前的备份
find $BACKUP_DIR -name "*.sql.gz" -mtime +7 -delete

echo "Backup completed: kbengine_$DATE.sql.gz"
```

---

## 六、最佳实践

### 6.1 一致性策略选择

```
┌─────────────────────────────────────────────────────────────┐
│              不同场景的一致性策略                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  强一致性 (必须使用事务):                                   │
│  ├── 货币/钻石交易                                          │
│  ├── 道具购买/消耗                                          │
│  ├── 跨服数据同步                                           │
│  └── 重要状态变化                                           │
│                                                             │
│  最终一致性 (可异步):                                       │
│  ├── 玩家状态数据                                           │
│  ├── 非重要日志                                             │
│  ├── 统计数据                                               │
│  └── 排行榜                                                 │
│                                                             │
│  无需一致性 (独立操作):                                     │
│  ├── 聊天消息                                               │
│  ├── 临时效果                                               │
│  └── 显示数据                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 异步处理模式

```cpp
// 异步处理最佳实践

class AsyncDataProcessor {
public:
    // 关键操作：同步 + 重试
    bool syncUpdate(uint64_t playerId, int64_t goldChange) {
        int retries = 0;
        while (retries < MAX_RETRIES) {
            try {
                DatabaseTransaction txn(db_);

                // 检查余额
                auto result = txn.query(fmt::format(
                    "SELECT gold FROM player_{} WHERE id = {} FOR UPDATE",
                    playerId % TABLE_COUNT, playerId
                ));

                if (result.empty()) {
                    return false;
                }

                int64_t currentGold = result[0]["gold"];
                if (currentGold + goldChange < 0) {
                    return false; // 余额不足
                }

                // 更新
                txn.execute(fmt::format(
                    "UPDATE player_{} SET gold = gold + {} WHERE id = {}",
                    playerId % TABLE_COUNT, goldChange, playerId
                ));

                if (txn.commit()) {
                    return true;
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Sync update failed: " + std::string(e.what()));
            }

            retries++;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 * retries)
            );
        }

        return false;
    }

    // 非关键操作：异步 + 批量
    void asyncUpdateLog(uint64_t playerId, const std::string& log) {
        logQueue_.push({playerId, log, getCurrentTime()});

        if (logQueue_.size() >= BATCH_SIZE) {
            flushLogs();
        }
    }

private:
    void flushLogs() {
        std::vector<LogEntry> batch;
        while (!logQueue_.empty() && batch.size() < BATCH_SIZE) {
            LogEntry entry;
            if (logQueue_.try_pop(entry)) {
                batch.push_back(entry);
            }
        }

        if (!batch.empty()) {
            bulkInsertLogs(batch);
        }
    }

    struct LogEntry {
        uint64_t playerId;
        std::string log;
        uint64_t timestamp;
    };

    ThreadSafeQueue<LogEntry> logQueue_;
    Database* db_;

    static constexpr int MAX_RETRIES = 3;
    static constexpr size_t BATCH_SIZE = 100;
    static constexpr size_t TABLE_COUNT = 16;
};
```

---

## 七、总结

### 一致性方案对比

| 方案 | 一致性 | 性能 | 复杂度 | 适用场景 |
|------|--------|------|--------|----------|
| **强事务** | 强 | 低 | 低 | 关键交易 |
| **2PC** | 强 | 很低 | 中 | 跨库事务 |
| **TCC** | 最终 | 中 | 高 | 长事务 |
| **异步同步** | 最终 | 高 | 中 | 状态同步 |
| **事件溯源** | 最终 | 中 | 很高 | 审计系统 |

### KBEngine 一致性保证

```
KBEngine 保证:
1. 定时自动存档
2. 实体数据序列化
3. 数据库持久化

开发者需要:
1. 关键操作使用事务
2. 实现重试机制
3. 处理并发冲突
```

---

## 参考资料

- [KBEngine GitHub - 数据库接口](https://github.com/kbengine/kbengine/tree/master/kbe/src/server/dbmgr)
- [CAP 定理详解](https://www.oracle.com/java/technologies/cap.html)
- [MySQL 事务隔离级别](https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation-levels.html)
