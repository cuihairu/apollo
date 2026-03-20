# Q35: 如何实现数据库分片？

## 问题分析

本题考察对数据库分片的理解：
- 分片的原因和策略
- 水平分片 vs 垂直分片
- 分片路由算法
- 分布式事务处理

---

## 一、分片策略

### 1.1 分片类型对比

```
┌─────────────────────────────────────────────────────────────┐
│                    分片策略对比                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  水平分片 (Sharding):                                     │
│  ┌─────────────────────────────────────────────────┐       │
│  │  按行分片到不同数据库                               │       │
│  │                                                   │       │
│  │  player_0: ID 1-100000                          │       │
│  │  player_1: ID 100001-200000                      │       │
│  │  player_2: ID 200001-300000                      │       │
│  │                                                   │       │
│  │  优点:                                            │       │
│  │  ├── 分散写入压力                                   │       │
│  │  ├── 提高并发能力                                   │       │
│  │  └── 易于扩展                                      │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 跨分片查询复杂                                 │       │
│  │  └── 数据分布不均                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  垂直分片 (Partitioning):                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  按时间/类型分片                                   │       │
│  │                                                   │       │
│  │  log_202401: 2024年1月数据                       │       │
│  │  log_202402: 2024年2月数据                       │       │
│  │                                                   │       │
│  │  优点:                                            │       │
│  │  ├── 历史数据归档                                   │       │
│  │  ├── 方便清理                                       │       │
│  │  └── 查询效率高                                   │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 需要确定分片键                                 │       │
│  │  └── 跨片查询需要 union                              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 分片算法

```cpp
// 分片路由算法

class ShardRouter {
public:
    enum class Strategy {
        Hash,           // 哈希分片
        Range,          // 范围分片
        ConsistentHash  // 一致性哈希
    };

    // 哈希分片
    static size_t hashShard(uint64_t key, size_t shardCount) {
        return key % shardCount;
    }

    // 范围分片
    static size_t rangeShard(uint64_t key, size_t shardCount) {
        // 假设每分片 100 万数据
        return key / 1000000;
    }

    // 一致性哈希
    static size_t consistentHashShard(uint64_t key, size_t shardCount) {
        // 使用一致性哈希算法
        uint32_t hash = murmur3(key);
        return hash % shardCount;
    }

private:
    static uint32_t murmur3(uint64_t key) {
        key ^= key >> 33;
        key *= 0xff51afd7ed558ccd;
        key ^= key >> 33;
        key *= 0xc4ceb9fe1a85ec53;
        key ^= key >> 33;
        return key;
    }
};
```

---

## 二、分片实现

### 2.1 分片管理器

```cpp
// 分片数据库管理器

class ShardedDatabase {
public:
    struct ShardConfig {
        std::string host;
        uint16_t port;
        std::string database;
        std::string user;
        std::string password;
        size_t shardId;
    };

    ShardedDatabase(const std::vector<ShardConfig>& shards) {
        for (const auto& config : shards) {
            auto* pool = new ConnectionPool(config);
            pools_[config.shardId] = pool;
        }
    }

    // 查询（自动路由）
    template<typename Func>
    auto query(uint64_t playerId, Func&& func) -> decltype(func(std::declval<Database*>())) {
        size_t shardId = ShardRouter::hashShard(playerId, pools_.size());
        Connection* conn = pools_[shardId]->acquire();

        auto result = func(conn);

        pools_[shardId]->release(conn);
        return result;
    }

    // 广播查询（所有分片）
    template<typename Func>
    std::vector<decltype(func(std::declval<Database*>()))> broadcast(Func&& func) {
        std::vector<decltype(func(std::declval<Database*>()))> results;

        for (auto& [shardId, pool] : pools_) {
            Connection* conn = pool->acquire();

            auto result = func(conn);
            results.push_back(result);

            pool->release(conn);
        }

        return results;
    }

private:
    std::unordered_map<size_t, ConnectionPool*> pools_;
};
```

---

## 三、最佳实践

### 3.1 分片设计建议

| 实践 | 说明 |
|------|------|
| **按玩家 ID 分片** | 均匀分布、易路由 |
| **分片数量** | 2^n 个，便于扩容 |
| **预留扩容能力** | 一致性哈希支持 |
| **避免跨片事务** | 业务设计规避 |

### 3.2 分片迁移

```
┌─────────────────────────────────────────────────────────────┐
│                    分片扩容流程                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 新增分片                                               │
│     → 添加新数据库服务器                                   │
│                                                             │
│  2. 数据迁移                                               │
│     → 部分数据迁移到新分片                                 │
│     → 使用双写保证数据一致性                               │
│                                                             │
│  3. 切换路由                                               │
│     → 更新路由表                                         │
│     → 切换到新分片                                       │
│                                                             │
│  4. 清理旧数据                                             │
│     → 删除旧分片数据                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、总结

### 分片方案选择

| 方案 | 优点 | 缺点 | 适用场景 |
|------|------|------|----------|
| **哈希分片** | 分布均匀 | 扩容复杂 | 大量玩家 |
| **范围分片** | 查询高效 | 可能热点 | 时间序列 |
| **地理位置** | 低延迟 | 复杂 | 全球部署 |

---

## 参考资料

- [数据库分片最佳实践](https://www.mongodb.com/docs/sharding/)
- [一致性哈希算法](https://www.tom-e-white.com/2007/11/consistent-hashing/)
