# Q36: 如何处理数据库事务？

## 问题分析

本题考察对数据库事务的理解：
- ACID 特性
- 事务隔离级别
- 分布式事务
- 死锁处理

---

## 一、事务基础

### 1.1 ACID 特性

```
ACID:
- Atomicity (原子性): 全部成功或全部失败
- Consistency (一致性): 数据始终有效
- Isolation (隔离性): 事务间互不干扰
- Durability (持久性): 提交后永久保存
```

### 1.2 隔离级别

| 级别 | 脏读 | 不可重复读 | 幻读 | 性能 |
|------|------|-----------|------|------|
| Read Uncommitted | ✅ | ✅ | ✅ | 最高 |
| Read Committed | ❌ | ✅ | ✅ | 高 |
| Repeatable Read | ❌ | ❌ | ✅ | 中 |
| Serializable | ❌ | ❌ | ❌ | 低 |

### 1.3 事务实现

```cpp
// 事务处理

class TransactionManager {
public:
    // 执行事务
    template<typename Func>
    bool execute(Func&& func) {
        try {
            beginTransaction();

            func();

            commit();
            return true;
        } catch (const std::exception& e) {
            rollback();
            LOG_ERROR("Transaction failed: " + std::string(e.what()));
            return false;
        }
    }

private:
    void beginTransaction() {
        db_->execute("START TRANSACTION");
    }

    void commit() {
        db_->execute("COMMIT");
    }

    void rollback() {
        db_->execute("ROLLBACK");
    }

    Database* db_;
};

// 使用示例
void transferGold(uint64_t from, uint64_t to, int64_t amount) {
    TransactionManager txn;

    txn.execute([&]() {
        // 检查余额
        auto balance = queryBalance(from);
        if (balance < amount) {
            throw std::runtime_error("Insufficient balance");
        }

        // 扣除发送者
        updateBalance(from, -amount);

        // 增加接收者
        updateBalance(to, amount);

        // 记录日志
        logTransfer(from, to, amount);
    });
}
```

---

## 二、死锁处理

### 2.1 死锁检测

```cpp
// 死锁重试机制

class DeadlockRetryExecutor {
public:
    template<typename Func>
    auto execute(Func&& func, int maxRetries = 3) -> decltype(func()) {
        int retries = 0;

        while (retries < maxRetries) {
            try {
                return func();
            } catch (const DeadlockException& e) {
                retries++;

                // 指数退避
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << retries))
                );

                LOG_WARNING("Deadlock detected, retrying... (" +
                           std::to_string(retries) + "/" +
                           std::to_string(maxRetries) + ")");
            }
        }

        throw std::runtime_error("Transaction failed after retries");
    }
};
```

---

## 三、最佳实践

### 事务使用建议

| 建议 | 说明 |
|------|------|
| **保持事务简短** | 减少锁持有时间 |
| **按固定顺序访问** | 避免死锁 |
| **设置合理超时** | 避免长时间等待 |
| **使用保存点** | 大事务分段提交 |

---

## 参考资料

- [MySQL 事务隔离级别](https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation.html)
