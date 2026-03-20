# Q82: 如何防止刷物品？

## 问题分析

本题考察对物品刷取漏洞防护的理解：
- 刷物品常见手法
- 交易验证
- 数据一致性
- 审计日志

---

## 一、刷物品手法

### 1.1 常见手法

```
┌─────────────────────────────────────────────────────────────┐
│                    刷物品手法                                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 重复利用漏洞                                              │
│  ├── 交易时利用网络延迟多次确认                               │
│  ├── 利用时序问题                                           │
│  └── 利用 rollback 机制                                      │
│                                                             │
│  2. 越权操作                                                 │
│  ├── 修改请求参数                                           │
│  ├── 绕过客户端限制                                         │
│  └── 直接发送协议包                                         │
│                                                             │
│  3. 并发竞争                                                 │
│  ├── 同时使用同一物品                                        │
│  ├── 同时完成同一任务                                        │
│  └── 转让时利用竞争                                         │
│                                                             │
│  4. 逻辑漏洞                                                 │
│  ├── 删除后获得补偿                                         │
│  ├── 兑换漏洞                                               │
│  └── 刷任务奖励                                             │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、交易验证

### 2.1 原子交易

```cpp
// 原子交易系统

class TradeSystem {
public:
    struct TradeSession {
        uint64_t id;
        uint64_t player1;
        uint64_t player2;
        std::vector<ItemOffer> offer1;
        std::vector<ItemOffer> offer2;
        TradeState state;
        uint64_t version;  // 版本号防止并发修改
    };

    enum class TradeState {
        INITIATING,
        CONFIRMING,
        COMPLETED,
        CANCELLED
    };

    // 创建交易
    uint64_t createTrade(uint64_t requesterId, uint64_t targetId) {
        // 1. 验证双方可以交易
        if (!canTrade(requesterId, targetId)) {
            return 0;
        }

        // 2. 检查距离
        if (!isInRange(requesterId, targetId)) {
            return 0;
        }

        // 3. 创建交易会话
        TradeSession session;
        session.id = generateTradeId();
        session.player1 = requesterId;
        session.player2 = targetId;
        session.state = TradeState::INITIATING;
        session.version = 1;

        trades_[session.id] = session;

        // 4. 锁定双方背包
        lockInventory(requesterId);
        lockInventory(targetId);

        return session.id;
    }

    // 添加物品到交易
    bool addItem(uint64_t playerId, uint64_t tradeId,
                 const ItemOffer& item) {
        TradeSession* trade = getTrade(tradeId);
        if (!trade || trade->state != TradeState::INITIATING) {
            return false;
        }

        // 验证是否是交易参与者
        if (trade->player1 != playerId && trade->player2 != playerId) {
            return false;
        }

        // 验证物品所有权
        if (!verifyItemOwnership(playerId, item)) {
            return false;
        }

        // 添加到对应报价
        if (trade->player1 == playerId) {
            trade->offer1.push_back(item);
        } else {
            trade->offer2.push_back(item);
        }

        // 增加版本号
        trade->version++;

        return true;
    }

    // 确认交易
    bool confirm1(uint64_t playerId, uint64_t tradeId) {
        TradeSession* trade = getTrade(tradeId);
        if (!trade) return false;

        std::lock_guard<std::mutex> lock(trade->mutex);

        // 检查版本号防止并发修改
        uint64_t clientVersion = getClientVersion(playerId);
        if (clientVersion != trade->version) {
            // 版本不匹配，交易内容已变更
            sendTradeUpdate(playerId, tradeId);
            return false;
        }

        // 标记确认
        if (trade->player1 == playerId) {
            trade->confirmed1 = true;
        } else {
            trade->confirmed2 = true;
        }

        // 双方都确认后进入最终确认阶段
        if (trade->confirmed1 && trade->confirmed2) {
            trade->state = TradeState::CONFIRMING;
            trade->version++;
        }

        return true;
    }

    // 最终确认
    bool confirm2(uint64_t playerId, uint64_t tradeId) {
        TradeSession* trade = getTrade(tradeId);
        if (!trade || trade->state != TradeState::CONFIRMING) {
            return false;
        }

        std::lock_guard<std::mutex> lock(trade->mutex);

        // 最终确认
        if (trade->player1 == playerId) {
            trade->finalConfirmed1 = true;
        } else {
            trade->finalConfirmed2 = true;
        }

        // 双方都最终确认，执行交易
        if (trade->finalConfirmed1 && trade->finalConfirmed2) {
            return executeTrade(*trade);
        }

        return true;
    }

private:
    bool executeTrade(const TradeSession& trade) {
        // 在数据库事务中执行所有操作
        return dbTransaction([&]() {
            // 1. 移除玩家1的物品
            for (const auto& item : trade.offer1) {
                if (!removeItem(trade.player1, item)) {
                    return false;  // 回滚
                }
            }

            // 2. 移除玩家2的物品
            for (const auto& item : trade.offer2) {
                if (!removeItem(trade.player2, item)) {
                    return false;  // 回滚
                }
            }

            // 3. 添加物品到玩家1
            for (const auto& item : trade.offer2) {
                if (!addItem(trade.player1, item)) {
                    return false;  // 回滚
                }
            }

            // 4. 添加物品到玩家2
            for (const auto& item : trade.offer1) {
                if (!addItem(trade.player2, item)) {
                    return false;  // 回滚
                }
            }

            // 5. 记录日志
            logTrade(trade);

            // 6. 清理交易
            trades_.erase(trade.id);
            unlockInventory(trade.player1);
            unlockInventory(trade.player2);

            return true;
        });
    }

    std::unordered_map<uint64_t, TradeSession> trades_;
};
```

### 2.2 物品锁定

```cpp
// 物品锁定机制

class ItemLockManager {
public:
    // 锁定物品
    bool lockItem(uint64_t playerId, uint64_t itemId) {
        std::lock_guard<std::mutex> lock(mutex_);

        ItemKey key = {playerId, itemId};

        // 检查是否已锁定
        if (lockedItems_.count(key)) {
            return false;
        }

        lockedItems_.insert(key);
        return true;
    }

    // 批量锁定
    bool lockItems(uint64_t playerId, const std::vector<uint64_t>& itemIds) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 先检查是否都能锁定
        for (uint64_t itemId : itemIds) {
            ItemKey key = {playerId, itemId};
            if (lockedItems_.count(key)) {
                return false;  // 有物品已锁定
            }
        }

        // 全部锁定
        for (uint64_t itemId : itemIds) {
            lockedItems_.insert({playerId, itemId});
        }

        return true;
    }

    // 解锁物品
    void unlockItem(uint64_t playerId, uint64_t itemId) {
        std::lock_guard<std::mutex> lock(mutex_);
        lockedItems_.erase({playerId, itemId});
    }

    // 解锁玩家所有物品
    void unlockPlayerItems(uint64_t playerId) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = lockedItems_.begin();
        while (it != lockedItems_.end()) {
            if (it->playerId == playerId) {
                it = lockedItems_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool isItemLocked(uint64_t playerId, uint64_t itemId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lockedItems_.count({playerId, itemId}) > 0;
    }

private:
    struct ItemKey {
        uint64_t playerId;
        uint64_t itemId;

        bool operator<(const ItemKey& other) const {
            return playerId != other.playerId ?
                   playerId < other.playerId :
                   itemId < other.itemId;
        }
    };

    std::mutex mutex_;
    std::set<ItemKey> lockedItems_;
};
```

---

## 三、数据库一致性

### 3.1 事务操作

```cpp
// 数据库事务保证一致性

class DatabaseItemManager {
public:
    // 转移物品 (原子操作)
    bool transferItem(uint64_t fromPlayer, uint64_t toPlayer,
                     uint64_t itemId, int count) {
        return dbTransaction([&]() {
            // 1. 读取源物品
            auto sourceItem = queryItem(fromPlayer, itemId);
            if (!sourceItem || sourceItem->count < count) {
                return false;
            }

            // 2. 读取目标背包
            auto targetItems = queryInventory(toPlayer);
            if (targetItems.size() >= getMaxInventorySlots()) {
                return false;
            }

            // 3. 更新源物品
            if (sourceItem->count == count) {
                deleteItem(fromPlayer, itemId);
            } else {
                updateItemCount(fromPlayer, itemId,
                               sourceItem->count - count);
            }

            // 4. 添加到目标背包
            addItem(toPlayer, sourceItem->itemTemplateId, count);

            // 5. 记录转移日志
            logItemTransfer(fromPlayer, toPlayer, itemId, count);

            return true;
        });
    }

    // 删除并补偿 (防止刷)
    bool deleteItemWithCompensation(uint64_t playerId, uint64_t itemId) {
        return dbTransaction([&]() {
            // 先获取物品信息
            auto item = queryItem(playerId, itemId);
            if (!item) return false;

            // 删除物品
            deleteItem(playerId, itemId);

            // 如果是付费物品，不补偿
            if (item->isPaid) {
                return true;
            }

            // 检查是否已有补偿记录
            if (hasCompensationRecord(playerId, itemId)) {
                return false;  // 已补偿过，拒绝再次补偿
            }

            // 给予补偿
            giveCompensation(playerId, item);

            // 记录补偿
            recordCompensation(playerId, itemId);

            return true;
        });
    }

private:
    bool dbTransaction(std::function<bool()> fn) {
        // 开始事务
        db_->begin();

        try {
            bool result = fn();

            if (result) {
                db_->commit();
                return true;
            } else {
                db_->rollback();
                return false;
            }
        } catch (...) {
            db_->rollback();
            return false;
        }
    }

    Database* db_;
};
```

---

## 四、审计日志

### 4.1 完整日志

```cpp
// 物品操作审计日志

class ItemAuditLogger {
public:
    void logItemOperation(const ItemOperation& op) {
        AuditLogEntry entry;
        entry.timestamp = getCurrentTime();
        entry.playerId = op.playerId;
        entry.operation = op.operation;
        entry.itemId = op.itemId;
        entry.itemTemplateId = op.itemTemplateId;
        entry.count = op.count;
        entry.from = op.from;
        entry.to = op.to;
        entry.reason = op.reason;

        // 序列化
        std::string logLine = serialize(entry);

        // 写入日志文件
        logToFile(logLine);

        // 异步写入数据库
        asyncWriteToDB(entry);

        // 检查可疑操作
        checkSuspicious(entry);
    }

    // 分析日志查找异常
    std::vector<AuditLogEntry> analyzePlayerActivity(uint64_t playerId,
                                                      int hours) {
        auto entries = queryLogs(playerId, hours);

        std::vector<AuditLogEntry> suspicious;

        // 检查短时间内大量获得物品
        auto gained = filterByOperation(entries, "gain");
        if (gained.size() > 100) {
            suspicious.insert(suspicious.end(), gained.begin(), gained.end());
        }

        // 检查物品异常转移
        auto transferred = filterByOperation(entries, "transfer");
        for (const auto& entry : transferred) {
            if (entry.count > 1000) {
                suspicious.push_back(entry);
            }
        }

        return suspicious;
    }

private:
    struct AuditLogEntry {
        uint64_t timestamp;
        uint64_t playerId;
        std::string operation;  // gain, lose, transfer, delete
        uint64_t itemId;
        int itemTemplateId;
        int count;
        uint64_t from;
        uint64_t to;
        std::string reason;
    };
};
```

---

## 五、防刷策略

### 5.1 速率限制

```cpp
// 物品操作速率限制

class ItemRateLimiter {
public:
    bool checkOperation(uint64_t playerId, const std::string& operation) {
        Key key = {playerId, operation};
        auto now = getCurrentTime();

        // 清理过期记录
        cleanup(now);

        // 获取计数
        int count = 0;
        auto it = counters_.find(key);
        if (it != counters_.end()) {
            count = it->second.count;
        }

        // 检查限制
        int limit = getLimit(operation);
        if (count >= limit) {
            logRateLimitExceed(playerId, operation, count);
            return false;
        }

        // 增加计数
        counters_[key] = {now, count + 1};

        return true;
    }

private:
    int getLimit(const std::string& operation) {
        static std::unordered_map<std::string, int> limits = {
            {"trade", 10},        // 每小时10次交易
            {"drop_item", 50},    // 每小时50次丢弃
            {"delete_item", 20},  // 每小时20次删除
            {"mail", 30},         // 每小时30次邮件
            {"craft", 100}        // 每小时100次制作
        };

        auto it = limits.find(operation);
        return it != limits.end() ? it->second : 100;
    }

    struct Key {
        uint64_t playerId;
        std::string operation;

        bool operator<(const Key& other) const {
            return playerId != other.playerId ?
                   playerId < other.playerId :
                   operation < other.operation;
        }
    };

    struct Counter {
        uint64_t windowStart;
        int count;
    };

    void cleanup(uint64_t now) {
        const uint64_t WINDOW = 3600000;  // 1小时

        auto it = counters_.begin();
        while (it != counters_.end()) {
            if (now - it->second.windowStart > WINDOW) {
                it = counters_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::map<Key, Counter> counters_;
};
```

---

## 六、KBEngine 防刷

### 6.1 KBEngine 物品管理

```python
# KBEngine 风格的防刷机制

class ItemManager:
    """
    KBEngine 物品管理:
    1. 服务端权威
    2. 物品锁定
    3. 事务操作
    4. 日志审计
    """

    def __init__(self):
        self.locked_items = set()  # (player_id, item_id)
        self.pending_trades = {}

    def add_item(self, player_id, item_id, count):
        """添加物品"""
        # 数据库操作
        success = db.execute(
            "INSERT INTO items (player_id, item_id, count) "
            "VALUES (?, ?, ?)",
            player_id, item_id, count
        )

        if success:
            # 记录日志
            self.log_item_operation("add", player_id, item_id, count)

        return success

    def remove_item(self, player_id, item_id, count):
        """移除物品"""
        # 先锁定物品
        if not self.lock_item(player_id, item_id):
            return False

        # 检查数量
        current = self.get_item_count(player_id, item_id)
        if current < count:
            self.unlock_item(player_id, item_id)
            return False

        # 更新数据库
        success = db.execute(
            "UPDATE items SET count = count - ? "
            "WHERE player_id = ? AND item_id = ? AND count >= ?",
            count, player_id, item_id, count
        )

        if success:
            self.log_item_operation("remove", player_id, item_id, count)

        self.unlock_item(player_id, item_id)
        return success

    def transfer_item(self, from_player, to_player, item_id, count):
        """转移物品 (原子操作)"""
        # 事务操作
        with db.transaction():
            # 锁定双方物品
            if not self.lock_item(from_player, item_id):
                db.rollback()
                return False

            # 检查数量
            current = self.get_item_count(from_player, item_id)
            if current < count:
                self.unlock_item(from_player, item_id)
                db.rollback()
                return False

            # 移除
            if not self.remove_item(from_player, item_id, count):
                self.unlock_item(from_player, item_id)
                db.rollback()
                return False

            # 添加
            if not self.add_item(to_player, item_id, count):
                # 回滚添加
                self.add_item(from_player, item_id, count)
                self.unlock_item(from_player, item_id)
                db.rollback()
                return False

            self.unlock_item(from_player, item_id)

            # 记录日志
            self.log_transfer(from_player, to_player, item_id, count)

            db.commit()
            return True
```

---

## 七、总结

### 防刷物品核心

```
防刷 = 原子操作 + 物品锁定 + 事务保证 + 日志审计
- 交易使用两阶段确认
- 数据库事务保证一致性
- 详细日志追踪
- 速率限制异常操作
```

---

## 参考资料
- [Game Security Best Practices](https://www.gamedeveloper.com/)
- [Database Transaction Isolation](https://en.wikipedia.org/wiki/Isolation_(database_systems))
