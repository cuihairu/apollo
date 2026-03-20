# Q29: Redis 在 MMO 中有哪些应用场景？

## 问题分析

本题考察对 Redis 在游戏服务器中的应用理解：
- Redis 的数据类型和特性
- MMO 中的典型应用场景
- KBEngine 与 Redis 的集成
- 最佳实践和注意事项

---

## 一、Redis 基础

### 1.1 Redis 特性

```
┌─────────────────────────────────────────────────────────────┐
│                      Redis 特性                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  核心特性：                                                  │
│  ├── 内存存储 - 极快的读写速度                              │
│  ├── 丰富数据结构 - String, Hash, List, Set, ZSet           │
│  ├── 持久化支持 - RDB/AOF                                   │
│  ├── 主从复制 - 高可用                                     │
│  ├── 集群支持 - 水平扩展                                   │
│  └── 事务支持 - MULTI/EXEC                                 │
│                                                             │
│  性能指标：                                                 │
│  ├── QPS: 100,000+ (单机)                                 │
│  ├── 延迟: < 1ms                                            │
│  └── 并发: 10,000+ 连接                                    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 数据类型与用途

| 数据类型 | MMO 应用场景 | 操作命令 |
|----------|-------------|----------|
| **String** | 玩家状态、锁 | GET, SET, INCR |
| **Hash** | 玩家数据缓存 | HGET, HSET, HMGET |
| **List** | 消息队列、日志 | LPUSH, RPOP, LRANGE |
| **Set** | 在线玩家、好友 | SADD, SREM, SMEMBERS |
| **ZSet** | 排行榜 | ZADD, ZRANGE, ZINCRBY |
| **Bitmap** | 功能开关、签到 | SETBIT, GETBIT, BITCOUNT |
| **Geo** | 位置服务 | GEOADD, GEORADIUS |

---

## 二、MMO 应用场景

### 2.1 排行榜系统

```
┌─────────────────────────────────────────────────────────────┐
│                  Redis 排行榜实现                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  使用 ZSET (Sorted Set) 实现排行榜：                        │
│                                                             │
│  添加/更新分数：                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  ZADD rank:level 50 player:1001                   │       │
│  │  ZADD rank:level 45 player:1002                   │       │
│  │  ZADD rank:level 60 player:1003                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  获取 TOP 100：                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  ZREVRANGE rank:level 0 99 WITHSCORES            │       │
│  │  返回：[(1003, 60), (1001, 50), (1002, 45)]     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  获取玩家排名：                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  ZREVRANK rank:level player:1001                 │       │
│  │  返回：1 (第 2 名)                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// C++ Redis 排行榜实现

class RedisRankManager {
public:
    // 更新玩家分数
    bool updateScore(const std::string& rankKey,
                     EntityID playerId, int64_t score) {
        std::string member = std::to_string(playerId);
        return redis_->zadd(rankKey, member, score) > 0;
    }

    // 获取排行榜
    std::vector<RankEntry> getTopRank(const std::string& rankKey,
                                       int topN) {
        std::vector<RankEntry> result;

        // ZREVRANGE key 0 (topN-1) WITHSCORES
        auto members = redis_->zrevrange(rankKey, 0, topN - 1, true);

        int rank = 1;
        for (const auto& [member, score] : members) {
            result.push_back({
                std::stoull(member),
                static_cast<int64_t>(score),
                rank++
            });
        }

        return result;
    }

    // 获取玩家排名
    int getPlayerRank(const std::string& rankKey, EntityID playerId) {
        std::string member = std::to_string(playerId);

        // ZREVRANK key member
        auto rank = redis_->zrevrank(rankKey, member);

        return rank.has_value() ? static_cast<int>(*rank) + 1 : -1;
    }
};
```

### 2.2 在线玩家管理

```
┌─────────────────────────────────────────────────────────────┐
│                  在线玩家管理 (Set + Hash)                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  使用 Set 存储在线玩家列表：                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  SADD online_players player:1001                 │       │
│  │  SADD online_players player:1002                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  使用 Hash 存储玩家详细信息：                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  HSET player:1001 name "Player1"                 │       │
│  │  HSET player:1001 level 50                       │       │
│  │  HSET player:1001 map "main_city"                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  玩家上线/下线：                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  上线：                                           │       │
│  │  ├── SADD online_players player:1001            │       │
│  │  ├── HSET player:1001 online_time now()          │       │
│  │  └── HSET player:1001 server_id server_1         │       │
│  │                                                   │       │
│  │  下线：                                           │       │
│  │  ├── SREM online_players player:1001            │       │
│  │  ├── HSET player:1001 offline_time now()         │       │
│  │  └── EXPIRE player:1001 3600 (1小时后过期)      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 分布式锁

```
┌─────────────────────────────────────────────────────────────┐
│                  Redis 分布式锁                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  场景：防止玩家数据重复处理                                 │
│                                                             │
│  获取锁：                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  SET lock:player:1001 unique_id NX EX 30        │       │
│  │  ├── NX: 只在不存在时设置                          │       │
│  │  ├── EX: 设置 30 秒过期                            │       │
│  │  └── unique_id: 确保只释放自己的锁                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  释放锁：                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Lua 脚本 (原子操作)：                            │       │
│  │  if redis.call("get", KEYS[1]) == ARGV[1] then   │       │
│  │      return redis.call("del", KEYS[1])            │       │
│  │  else                                              │       │
│  │      return 0                                      │       │
│  │  end                                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

```cpp
// Redis 分布式锁实现

class RedisLock {
public:
    // 尝试获取锁
    bool tryLock(const std::string& key, uint32_t timeoutMs) {
        std::string value = generateUniqueID();

        // SET key value NX EX timeout
        std::string cmd = format(
            "SET %s %s NX EX %d",
            key.c_str(),
            value.c_str(),
            timeoutMs / 1000
        );

        auto result = redis_->execute(cmd);
        lockValue_ = value;

        return result.has_value() && *result == "OK";
    }

    // 释放锁
    bool unlock(const std::string& key) {
        // Lua 脚本确保只删除自己的锁
        std::string luaScript =
            "if redis.call('get', KEYS[1]) == ARGV[1] then "
            "    return redis.call('del', KEYS[1]) "
            "else "
            "    return 0 "
            "end";

        auto result = redis_->eval(luaScript, {key}, {lockValue_});
        return result.has_value() && *result == 1;
    }

    // 自动锁 (RAII)
    class Guard {
    public:
        Guard(RedisLock* lock, const std::string& key, uint32_t timeout)
            : lock_(lock), key_(key), acquired_(false) {
            acquired_ = lock_->tryLock(key_, timeout);
        }

        ~Guard() {
            if (acquired_) {
                lock_->unlock(key_);
            }
        }

        bool acquired() const { return acquired_; }

    private:
        RedisLock* lock_;
        std::string key_;
        bool acquired_;
    };

private:
    std::string lockValue_;
};
```

### 2.4 限流和防刷

```
┌─────────────────────────────────────────────────────────────┐
│                  Redis 限流实现                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  滑动窗口限流：                                             │
│  ┌─────────────────────────────────────────────────┐       │
│  │  每 60 秒最多 100 次请求                         │       │
│  │                                                   │       │
│  │  执行：                                            │       │
│  │  key = "limit:player:1001"                       │       │
│  │                                                   │       │
│  │  1. 移除 60 秒前的记录                           │       │
│  │  ZREMRANGEBYSCORE key 0 (now() - 60)             │       │
│  │                                                   │       │
│  │  2. 统计当前请求数                               │       │
│  │  count = ZCARD key                                │       │
│  │                                                   │       │
│  │  3. 判断是否限流                                 │       │
│  │  if count < 100:                                 │       │
│  │      ZADD key now() request_id                   │       │
│  │      return true (允许)                          │       │
│  │  else:                                            │       │
│  │      return false (拒绝)                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  令牌桶限流：                                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  初始化：                                          │       │
│  │  令牌数: 10, 添加速率: 1个/秒                      │       │
│  │  EVAL script 10 rate_limit:player:1001            │       │
│  │                                                   │       │
│  │  请求时：                                          │       │
│  │  if 令牌 > 0:                                     │       │
│  │      消耗一个令牌                                │       │
│  │      return true                                  │       │
│  │  else:                                            │       │
│  │      return false                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、KBEngine 与 Redis

### 3.1 集成方案

```python
# KBEngine 集成 Redis

import redis
import KBEngine

# Redis 连接池
redis_pool = redis.ConnectionPool(
    host='localhost',
    port=6379,
    db=0,
    max_connections=50
)

redis_client = redis.Redis(connection_pool=redis_pool)

class RedisManager(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)

    def setPlayerOnline(self, playerId):
        """设置玩家在线"""
        redis_client.sadd('online_players', playerId)
        redis_client.hset(f'player:{playerId}', 'online', '1')

    def setPlayerOffline(self, playerId):
        """设置玩家离线"""
        redis_client.srem('online_players', playerId)
        redis_client.hset(f'player:{playerId}', 'online', '0')
        # 1 小时后过期
        redis_client.expire(f'player:{playerId}', 3600)

    def updateRank(self, rankKey, playerId, score):
        """更新排行榜"""
        redis_client.zadd(rankKey, {playerId: score})

    def getTopRank(self, rankKey, n=100):
        """获取排行榜"""
        return redis_client.zrevrange(rankKey, 0, n-1, withscores=True)
```

### 3.2 缓存策略

```
┌─────────────────────────────────────────────────────────────┐
│                  KBEngine + Redis 缓存架构                   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐         ┌─────────────┐                  │
│  │  BaseApp    │         │  Redis      │                  │
│  │             │◄──────►│             │                  │
│  │  数据缓存    │         │  热点数据    │                  │
│  │  - 玩家列表   │         │  - 排行榜    │                  │
│  │  - 排行榜    │         │  - 在线列表   │                  │
│  │  - 通知队列   │         │  - 分布式锁   │                  │
│  └──────┬──────┘         └──────┬──────┘                  │
│         │                        │                          │
│         │    缓存未命中          │                          │
│         ▼                        ▼                          │
│  ┌─────────────────────────────────────────┐               │
│  │              DBMgr                     │               │
│  │                                          │               │
│  │  ┌─────────────────────────────────┐   │               │
│  │  │        MySQL 数据库              │   │               │
│  │  │  - 玩家完整数据                  │   │               │
│  │  │  - 持久化存储                    │   │               │
│  │  └─────────────────────────────────┘   │               │
│  └─────────────────────────────────────────┘               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、高级应用

### 4.1 HyperLogLog 统计

```cpp
// HyperLogLog 用于唯一统计

class RedisUVCounter {
public:
    // 记录访问
    void recordAccess(const std::string& key, EntityID playerId) {
        std::string member = std::to_string(playerId);
        redis_->pfadd(key, {member});
    }

    // 获取唯一访问数
    uint64_t getUniqueCount(const std::string& key) {
        auto count = redis_->pfcount(key);
        return count.value_or(0);
    }

    // 合并统计（多个 Key）
    uint64_t mergeAndCount(const std::vector<std::string>& keys,
                             const std::string& destKey) {
        redis_->pfmerge(destKey, keys.begin(), keys.end());
        return getUniqueCount(destKey);
    }
};
```

### 4.2 Pub/Sub 消息

```cpp
// Redis Pub/Sub 跨服通信

class RedisPubSub {
public:
    // 发布消息
    void publish(const std::string& channel, const std::string& message) {
        redis_->publish(channel, message);
    }

    // 订阅消息
    void subscribe(const std::string& channel,
                  std::function<void(const std::string&)> callback) {
        subscribers_[channel] = callback;

        // 在后台线程中订阅
        std::thread([this, channel]() {
            while (running_) {
                auto msg = redis_->subscribe({channel});
                if (msg && msg->channel == channel) {
                    if (subscribers_.count(channel)) {
                        subscribers_[channel](msg->payload);
                    }
                }
            }
        }).detach();
    }

private:
    std::unordered_map<std::string,
        std::function<void(const std::string&)>> subscribers_;
    bool running_ = true;
};
```

---

## 五、最佳实践

### 5.1 数据结构选择

| 场景 | Redis 类型 | 原因 |
|------|-----------|------|
| **排行榜** | ZSet | 自动排序，范围查询 |
| **玩家数据** | Hash | 结构化存储，字段访问 |
| **在线列表** | Set | 去重，集合操作 |
| **消息队列** | List | FIFO，阻塞操作 |
| **计数器** | String + INCR | 原子递增 |
| **功能开关** | Bitmap | 空间效率高 |
| **位置服务** | Geo | 距离计算，范围查询 |

### 5.2 性能优化

```
┌─────────────────────────────────────────────────────────────┐
│                  Redis 性能优化                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 使用 Pipeline 批量操作                                 │
│     ├── 减少网络往返                                      │
│     └── 提升吞吐量                                        │
│                                                             │
│  2. 合理设置过期时间                                       │
│     ├── 避免内存无限增长                                  │
│     └── 自动清理过期数据                                    │
│                                                             │
│  3. 使用连接池                                             │
│     ├── 减少连接创建开销                                  │
│     └── 复用连接                                           │
│                                                             │
│  4. 选择合适的数据结构                                     │
│     ├── ZSet vs Hash + sort                               │
│     ├── Set vs List (去重需求)                             │
│     └── Bitmap vs Set (空间效率)                           │
│                                                             │
│  5. 使用 Lua 脚本                                          │
│     ├── 原子操作                                           │
│     └── 减少网络传输                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.3 注意事项

```
┌─────────────────────────────────────────────────────────────┐
│                  Redis 使用注意                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 数据持久化                                               │
│     ├── Redis 不是最终存储                                 │
│     ├── 需要定期同步到 MySQL                               │
│     └── 使用 RDB/AOF 持久化                                │
│                                                             │
│  2. 内存管理                                                │
│     ├── 设置 maxmemory 限制                                │
│     ├── 使用 allkeys-lru 淘汰策略                          │
│     └── 监控内存使用                                        │
│                                                             │
│  3. 高可用配置                                               │
│     ├── 使用 Redis Sentinel 主从                           │
│     ├── 配置自动故障转移                                   │
│     └── 监控节点状态                                        │
│                                                             │
│  4. 安全设置                                                │
│     ├── 设置密码认证                                        │
│     ├── 绑定特定 IP                                        │
│     └── 禁用危险命令                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 六、总结

### Redis 应用场景总结

| 应用场景 | Redis 类型 | 复杂度 | 重要性 |
|----------|-----------|--------|--------|
| **排行榜** | ZSet | 低 | 高 |
| **在线管理** | Set + Hash | 低 | 高 |
| **分布式锁** | String + Lua | 中 | 中 |
| **限流防刷** | ZSet/String | 中 | 高 |
| **消息队列** | List/PubSub | 低 | 中 |
| **位置服务** | Geo | 中 | 中 |
| **统计去重** | HyperLogLog | 低 | 低 |

### 最佳实践

```
1. Redis 作为缓存层
   - 热点数据缓存
   - 减轻数据库压力
   - 定期同步持久化

2. 合理使用数据类型
   - 根据场景选择类型
   - 注意内存占用
   - 优化数据结构

3. 保证数据安全
   - 主从复制
   - 持久化配置
   - 监控告警
```

---

## 参考资料

- [Redis 官方文档](https://redis.io/documentation)
- [KBEngine Lab - 数据库管理](https://www.kbelab.com/manual/dbmgr.html)
- [Redis 集群教程](https://redis.io/topics/cluster-tutorial)
