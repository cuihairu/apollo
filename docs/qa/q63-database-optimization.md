# Q63: 如何优化数据库查询？

## 问题分析

本题考察对数据库查询优化的理解：
- 索引优化
- 查询语句优化
- 表结构设计
- 读写分离
- 缓存策略

---

## 一、查询优化基础

### 1.1 索引类型

```
┌─────────────────────────────────────────────────────────────┐
│                    索引类型                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  B-Tree 索引:                                               │
│  ├── 适合: 范围查询、排序                                   │
│  ├── 示例: WHERE level > 10                                 │
│  └── 限制: 不适合前缀模糊查询                                │
│                                                             │
│  哈希索引:                                                  │
│  ├── 适合: 等值查询                                         │
│  ├── 示例: WHERE id = 123                                   │
│  └── 限制: 不支持范围查询                                    │
│                                                             │
│  组合索引:                                                  │
│  ├── 遵循最左前缀原则                                       │
│  ├── 示例: INDEX(player_id, item_type)                      │
│  └── 注意: 列顺序很重要                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 查询优化示例

```sql
-- 1. 避免 SELECT *
-- ✅ 好
SELECT id, name, level FROM players WHERE id = 123;

-- 2. 使用索引
-- ❌ 不好 - 索引列上使用函数
SELECT * FROM players WHERE YEAR(create_time) = 2024;
-- ✅ 好
SELECT * FROM players WHERE create_time >= '2024-01-01';

-- 3. 批量操作
-- ✅ 好 - 一次查询
SELECT * FROM items WHERE player_id IN (1, 2, 3, ...);

-- 4. 分页优化
-- ❌ 不好 - 深分页
SELECT * FROM logs ORDER BY id LIMIT 1000000, 10;
-- ✅ 好 - 使用游标
SELECT * FROM logs WHERE id > last_seen_id ORDER BY id LIMIT 10;
```

---

## 二、KBEngine 数据库设计

### 2.1 玩家表结构

```sql
CREATE TABLE `players` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `account_id` BIGINT UNSIGNED NOT NULL,
  `name` VARCHAR(64) NOT NULL,
  `level` INT UNSIGNED NOT NULL DEFAULT 1,
  `exp` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `hp` INT NOT NULL DEFAULT 100,
  `position_x` FLOAT NOT NULL DEFAULT 0,
  `position_y` FLOAT NOT NULL DEFAULT 0,
  `position_z` FLOAT NOT NULL DEFAULT 0,
  `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `login_time` DATETIME,
  `logout_time` DATETIME,
  PRIMARY KEY (`id`),
  UNIQUE KEY `uk_account_id` (`account_id`),
  UNIQUE KEY `uk_name` (`name`),
  KEY `idx_level` (`level`),
  KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

---

## 三、多级缓存

```cpp
// 三级缓存架构

class CacheSystem {
public:
    PlayerData getPlayerData(uint64_t playerId) {
        // L1: 本地缓存
        if (auto data = localCache.get(playerId)) {
            return *data;
        }

        // L2: Redis
        if (auto data = redisCache.get(playerId)) {
            localCache.put(playerId, data);
            return data;
        }

        // L3: 数据库
        auto data = databaseCache.get(playerId);
        redisCache.put(playerId, data);
        localCache.put(playerId, data);
        return data;
    }

private:
    LocalCache localCache;
    RedisCache redisCache;
    DatabaseCache databaseCache;
};
```

---

## 四、最佳实践

| 实践 | 说明 |
|------|------|
| **索引覆盖** | 查询字段都在索引中 |
| **批量操作** | 减少网络往返 |
| **避免 SELECT *** | 只查询需要的字段 |
| **使用连接池** | 复用数据库连接 |
| **定期分析** | ANALYZE TABLE |

---

## 五、总结

```
查询优化 = 索引设计 + SQL 优化 + 缓存策略 + 读写分离
```

---

## 参考资料

- [MySQL Optimization](https://dev.mysql.com/doc/refman/8.0/en/optimization.html)
- [KBEngine Database](https://kbengine.github.io/docs/)
