# Q26: 玩家数据什么时候存数据库？全量保存 vs 增量保存？

## 问题分析

本题考察对数据持久化策略的理解：
- 保存时机的选择
- 全量保存 vs 增量保存
- KBEngine 的数据保存机制
- 数据一致性保证

---

## 一、保存时机

### 1.1 触发条件

```
┌─────────────────────────────────────────────────────────────┐
│                    数据保存触发时机                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 定时保存                                                │
│     ├── 固定时间间隔 (如每 5 分钟)                          │
│     ├── 节省数据库压力                                      │
│     └── 可能丢失数据                                        │
│                                                             │
│  2. 事件保存                                                │
│     ├── 玩家下线时                                          │
│     ├── 获得重要物品                                        │
│     ├── 完成关键任务                                        │
│     └── 角色升级                                            │
│                                                             │
│  3. 变化保存                                                │
│     ├── 数据变化时立即保存                                  │
│     ├── 数据安全但数据库压力大                              │
│     └── 需要优化合并                                        │
│                                                             │
│  4. 标记保存                                                │
│     ├── 脏标记机制                                          │
│     ├── 定期检查并保存                                      │
│     └── 平衡安全与性能                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 保存时机对比

| 时机 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **定时保存** | 压力均匀 | 可能丢数据 | 非关键数据 |
| **事件保存** | 数据安全 | 瞬时压力高 | 关键操作 |
| **变化保存** | 最安全 | 压力极大 | 不推荐 |
| **标记保存** | 平衡 | 实现复杂 | 推荐 |

---

## 二、全量 vs 增量

### 2.1 全量保存

```
┌─────────────────────────────────────────────────────────────┐
│                    全量保存                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  每次保存完整的玩家数据：                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  玩家数据：                                         │       │
│  │  ├── 基本信息 (id, name, level)                   │       │
│  │  ├── 属性信息 (hp, mp, exp)                        │       │
│  │  ├── 位置信息 (x, y, z)                            │       │
│  │  ├── 背包数据 (20 个格子)                          │       │
│  │  ├── 技能数据 (30 个技能)                          │       │
│  │  ├── 任务数据 (50 个任务)                          │       │
│  │  └── ... 更多数据                                  │       │
│  │                                                    │       │
│  │  每次保存：所有数据 → 约 10KB                      │       │
│  │  每 5 分钟保存一次：120KB/小时                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优点：简单、数据完整、恢复容易                              │
│  缺点：数据量大、写入慢、IO 压力高                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 增量保存

```
┌─────────────────────────────────────────────────────────────┐
│                    增量保存                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  只保存变化的数据：                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  玩家获得 100 金币：                                 │       │
│  │  ┌─────────────────────────────────────────┐     │       │
│  │  │  增量数据：                              │     │       │
│  │  │  {                                     │     │       │
│  │  │    "player_id": 12345,                │     │       │
│  │  │    "changes": {                        │     │       │
│  │  │      "gold": "+100"                    │     │       │
│  │  │    },                                   │     │       │
│  │  │    "timestamp": 1640000000             │     │       │
│  │  │  }                                     │     │       │
│  │  └─────────────────────────────────────────┘     │       │
│  │                                                    │       │
│  │  增量数据：约 50 bytes                             │       │
│  │  比全量节省 99.5%！                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  优点：数据量小、写入快、节省 IO                            │
│  缺点：需要合并、恢复复杂、可能有冗余                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 对比总结

| 维度 | 全量保存 | 增量保存 |
|------|----------|----------|
| **数据量** | 大 (10KB) | 小 (50B-500B) |
| **写入时间** | 长 | 短 |
| **IO 压力** | 高 | 低 |
| **恢复复杂度** | 简单 | 复杂 |
| **数据冗余** | 无 | 有 |
| **实现复杂度** | 简单 | 复杂 |

---

## 三、KBEngine 的数据保存

### 3.1 自动保存机制

根据 [KBEngine 源码](https://github.com/kbengine/kbengine)：

```python
# KBEngine 自动保存配置
# kbengine_defs.xml

<Baseapp>
    <!-- 自动存档间隔 (秒) -->
    <autoArchiveTime>300</autoArchiveTime>  <!-- 5 分钟 -->

    <!-- 是否自动保存 -->
    <autoLoadEntity>true</autoLoadEntity>
</Baseapp>
```

### 3.2 BaseApp 保存实现

```cpp
// KBEngine BaseApp 保存实现
// src/server/baseapp/baseapp.cpp

class Baseapp {
public:
    // 自动保存定时器
    uint32_t autoArchiveTimerID_;
    uint32_t autoArchiveTime_;  // 默认 300 秒

    // 初始化自动保存
    void initializeAutoArchive() {
        autoArchiveTimerID_ = this->addTimer(
            autoArchiveTime_ * 1000000,  // 微秒
            [this]() {
                this->autoArchive();
            }
        );
    }

    // 自动存档
    void autoArchive() {
        // 遍历所有实体
        for (auto& [entityID, entity] : entities_) {
            if (entity->hasDB()) {
                // 检查是否需要保存
                if (entity->isDirty()) {
                    // 异步保存到数据库
                    entity->writeToDB();
                }
            }
        }
    }
};
```

### 3.3 脏标记机制

```python
# KBEngine 脏标记实现

class Account(KBEngine.Entity):
    def __init__(self):
        # 脏标记
        self._isDirty = False

        # 属性变化时设置脏标记
        self.onPropertyChanged = self._onPropertyChanged

    def _onPropertyChanged(self, propName):
        """
        属性变化回调
        """
        # 设置脏标记
        self._isDirty = True

        # 通知 BaseApp 需要保存
        KBEngine.baseAppCall("onEntityDirty", self.id)

    def writeToDB(self):
        """
        写入数据库
        """
        if not self._isDirty:
            return

        # 保存到数据库
        KBEngine.executeRawDatabaseCommand(
            "UPDATE players SET ... WHERE id = {}".format(self.id)
        )

        # 清除脏标记
        self._isDirty = False
```

---

## 四、保存策略设计

### 4.1 分级保存策略

```
┌─────────────────────────────────────────────────────────────┐
│                  分级保存策略设计                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  一级数据（关键）：立即保存                                  │
│  ├── 玩家充值                                              │
│  ├── 珍贵物品获得                                          │
│  ├── 重要任务完成                                          │
│  └── 保存方式：同步写库 + 确认                             │
│                                                             │
│  二级数据（重要）：短延迟保存                                │
│  ├── 金币/钻石变化                                         │
│  ├── 角色升级                                              │
│  ├── 装备强化                                              │
│  └── 保存方式：异步队列 + 30秒内保存                       │
│                                                             │
│  三级数据（普通）：定时保存                                  │
│  ├── 经验值变化                                            │
│  ├── 任务进度                                              │
│  ├── 副本进度                                              │
│  └── 保存方式：定时器 + 5分钟保存                          │
│                                                             │
│  四级数据（临时）：内存缓存                                  │
│  ├── 位置信息                                              │
│  ├── 临时状态                                              │
│  ├── Buff 状态                                             │
│  └── 保存方式：仅下线时保存                                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 混合保存实现

```cpp
// 混合保存策略实现

class HybridSaveStrategy {
public:
    // 数据类型
    enum class SavePriority {
        CRITICAL,      // 立即同步保存
        HIGH,          // 短延迟异步保存
        NORMAL,        // 定时保存
        TEMPORARY      // 仅下线保存
    };

    // 数据变更
    void onDataChanged(EntityID entityId, const std::string& key,
                       const SaveVariant& value, SavePriority priority) {
        switch (priority) {
            case SavePriority::CRITICAL:
                // 立即同步保存
                saveSync(entityId, key, value);
                break;

            case SavePriority::HIGH:
                // 添加到高优先级队列
                highPriorityQueue_.push({entityId, key, value});
                break;

            case SavePriority::NORMAL:
                // 设置脏标记，等待定时保存
                markDirty(entityId);
                break;

            case SavePriority::TEMPORARY:
                // 仅内存缓存
                memoryCache_[entityId][key] = value;
                break;
        }
    }

    // 高优先级队列处理
    void processHighPriorityQueue() {
        while (!highPriorityQueue_.empty()) {
            auto task = highPriorityQueue_.front();
            highPriorityQueue_.pop();

            // 异步保存
            saveAsync(task.entityId, task.key, task.value);
        }
    }

    // 定时保存
    void onTimer() {
        for (EntityID entityId : dirtyEntities_) {
            Entity* entity = getEntity(entityId);
            if (entity && entity->isDirty()) {
                // 全量保存
                saveFull(entity);
            }
        }
        dirtyEntities_.clear();
    }

private:
    std::queue<SaveTask> highPriorityQueue_;
    std::unordered_set<EntityID> dirtyEntities_;
    std::unordered_map<EntityID, std::unordered_map<std::string, SaveVariant>> memoryCache_;
};
```

---

## 五、数据一致性

### 5.1 事务保证

```cpp
// 数据库事务保存

class TransactionalSave {
public:
    // 保存多个实体（事务）
    bool saveEntities(const std::vector<Entity*>& entities) {
        db_->begin();

        try {
            for (auto* entity : entities) {
                // 序列化数据
                std::string data = serializeEntity(entity);

                // 更新数据库
                std::string sql = format(
                    "UPDATE players SET data = '%s' WHERE id = %lu",
                    escape(data).c_str(),
                    entity->id()
                );

                if (!db_->execute(sql)) {
                    throw std::runtime_error("Database error");
                }
            }

            // 提交事务
            db_->commit();
            return true;

        } catch (...) {
            // 回滚事务
            db_->rollback();
            return false;
        }
    }
};
```

### 5.2 备份机制

```cpp
// 数据备份机制

class BackupManager {
public:
    // 保存前备份
    bool saveWithBackup(Entity* entity) {
        // 1. 备份旧数据
        std::string oldData = loadFromDB(entity->id());

        // 2. 保存新数据
        if (!saveToDB(entity)) {
            return false;
        }

        // 3. 保存备份
        saveBackup(entity->id(), oldData);

        return true;
    }

    // 恢复备份
    bool restoreFromBackup(EntityID entityId) {
        // 获取备份数据
        std::string backupData = loadBackup(entityId);

        if (backupData.empty()) {
            return false;
        }

        // 恢复数据
        return restoreData(entityId, backupData);
    }
};
```

---

## 六、性能优化

### 6.1 批量保存

```cpp
// 批量保存优化

class BatchSaveManager {
public:
    // 待保存队列
    std::vector<Entity*> pendingSave_;

    // 添加待保存实体
    void addPendingSave(Entity* entity) {
        pendingSave_.push_back(entity);

        // 达到批量大小或超时时触发保存
        if (pendingSave_.size() >= BATCH_SIZE ||
            lastFlushTime_ - getCurrentTime() >= FLUSH_INTERVAL) {
            flush();
        }
    }

    // 批量保存
    void flush() {
        if (pendingSave_.empty()) {
            return;
        }

        // 构建 SQL
        std::string sql = "INSERT INTO player_data (id, data) VALUES ";

        for (size_t i = 0; i < pendingSave_.size(); ++i) {
            if (i > 0) sql += ", ";
            Entity* entity = pendingSave_[i];

            sql += format("(%lu, '%s')",
                           entity->id(),
                           escape(serialize(entity)).c_str());
        }

        sql += " ON DUPLICATE KEY UPDATE data = VALUES(data)";

        // 执行
        db_->execute(sql);

        pendingSave_.clear();
        lastFlushTime_ = getCurrentTime();
    }

private:
    static constexpr size_t BATCH_SIZE = 100;
    static constexpr uint32_t FLUSH_INTERVAL = 5000;  // 5 秒
    uint64_t lastFlushTime_ = 0;
};
```

### 6.2 异步保存

```cpp
// 异步保存实现

class AsyncSaveManager {
public:
    // 保存线程
    std::thread saveThread_;
    std::queue<SaveTask> saveQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;

    // 异步保存
    void saveAsync(Entity* entity, std::function<void(bool)> callback) {
        SaveTask task;
        task.entityId = entity->id();
        task.data = serialize(entity);
        task.callback = callback;

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            saveQueue_.push(task);
        }
        queueCV_.notify_one();
    }

    // 保存线程主循环
    void saveThreadMain() {
        while (running_) {
            SaveTask task;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCV_.wait(lock, [this] {
                    return !saveQueue_.empty() || !running_;
                });

                if (!running_) break;

                task = saveQueue_.front();
                saveQueue_.pop();
            }

            // 保存到数据库
            bool success = saveToDB(task);

            // 回调
            if (task.callback) {
                task.callback(success);
            }
        }
    }

private:
    bool running_ = true;
};
```

---

## 七、最佳实践

### 7.1 推荐策略

```
┌─────────────────────────────────────────────────────────────┐
│                  推荐的数据保存策略                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 采用混合策略                                            │
│     ├── 关键数据：立即同步保存                              │
│     ├── 重要数据：异步队列保存                              │
│     ├── 普通数据：定时批量保存                              │
│     └── 临时数据：仅下线保存                                │
│                                                             │
│  2. 使用脏标记机制                                          │
│     ├── 只保存变化的数据                                    │
│     ├── 定期合并保存                                        │
│     └── 减少数据库压力                                      │
│                                                             │
│  3. 异步 + 备份                                            │
│     ├── 主流程异步保存                                      │
│     ├── 保存前备份旧数据                                    │
│     └── 确保数据可恢复                                      │
│                                                             │
│  4. 监控保存状态                                            │
│     ├── 记录保存失败                                        │
│     ├── 定期检查一致性                                      │
│     └── 及时处理异常                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 KBEngine 配置建议

```python
# KBEngine 数据保存配置建议

# kbengine_defs.xml

<Baseapp>
    <!-- 自动保存间隔：300 秒 (5 分钟) -->
    <autoArchiveTime>300</autoArchiveTime>

    <!-- 启用实体备份 -->
    <shouldAutoArchive>true</shouldAutoArchive>

    <!-- 备份间隔：600 秒 (10 分钟) -->
    <autoArchiveTime*0.5>150</autoArchiveTime*0.5>
</Baseapp>

# 脚本中添加脏标记

class Player(KBEngine.Entity):
    def __init__(self):
        self.__dirty = False

    def onPropertyChanged(self, propName):
        # 设置脏标记
        self.__dirty = True
```

---

## 八、总结

### 保存策略总结

| 数据类型 | 保存时机 | 保存方式 | 优先级 |
|----------|----------|----------|--------|
| **充值数据** | 立即 | 同步全量 | 最高 |
| **重要物品** | 立即 | 异步全量 | 高 |
| **角色升级** | 短延迟 | 异步增量 | 中 |
| **普通数据** | 定时 | 批量增量 | 中 |
| **临时数据** | 下线时 | 全量 | 低 |

### 最佳实践

```
1. 合理选择保存时机
   - 关键操作立即保存
   - 普通变化定时保存
   - 临时数据下线保存

2. 优化保存方式
   - 使用脏标记减少保存
   - 批量保存减少 IO
   - 异步保存提高响应

3. 确保数据安全
   - 重要数据备份
   - 事务保证一致性
   - 监控保存状态
```

---

## 参考资料

- [KBEngine GitHub - DBMgr](https://github.com/kbengine/kbengine/tree/master/kbe/src/server/dbmgr)
- [MySQL 批量插入优化](https://dev.mysql.com/doc/refman/8.0/en/optimization-bulk-load.html)
- [数据库事务隔离级别](https://dev.mysql.com/doc/refman/8.0/en/innodb-transaction-isolation.html)
