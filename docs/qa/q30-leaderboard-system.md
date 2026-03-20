# Q30: 如何设计排行榜系统？

## 问题分析

本题考察对排行榜系统设计的理解：
- 排行榜的业务需求
- Redis 排行榜实现
- 高性能排行榜设计
- 分布式排行榜方案

---

## 一、排行榜需求分析

### 1.1 业务场景

```
┌─────────────────────────────────────────────────────────────┐
│                    排行榜业务场景                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  常见排行榜类型:                                            │
│  ├── 等级排行榜 (按等级排序)                                │
│  ├── 战力排行榜 (按战力排序)                                │
│  ├── 竞技排行榜 (按积分/胜率排序)                           │
│  ├── 公会排行榜 (按公会积分/活跃度排序)                      │
│  ├── 成就排行榜 (按成就点数排序)                            │
│  ├── 消费排行榜 (按充值/消费金额排序)                        │
│  └── 活动排行榜 (按活动分数排序)                            │
│                                                             │
│  核心需求:                                                  │
│  ├── 实时更新 (分数变化时立即更新排名)                       │
│  ├── 快速查询 (获取 Top N 和玩家排名)                       │
│  ├── 分页显示 (支持分页浏览)                                │
│  ├── 周期重置 (每周/每月重置)                               │
│  └── 跨服排行 (支持跨服排行榜)                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 性能要求

```
┌─────────────────────────────────────────────────────────────┐
│                    排行榜性能要求                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  QPS (每秒查询数):                                          │
│  ├── 玩家查看排行榜: 1000-5000 QPS                          │
│  ├── 分数更新: 500-2000 QPS                                 │
│  ├── 排名查询: 2000-10000 QPS                               │
│  └── 总计: 3000-15000 QPS                                   │
│                                                             │
│  响应时间:                                                  │
│  ├── Top N 查询: < 10ms                                    │
│  ├── 个人排名查询: < 5ms                                   │
│  ├── 分数更新: < 20ms                                      │
│  └── 分页查询: < 50ms                                      │
│                                                             │
│  数据规模:                                                  │
│  ├── 单服玩家: 5000-50000                                 │
│  ├── 排行榜记录: 5000-50000                                │
│  ├── 定时更新频率: 5-30 分钟                                │
│  └── 保留时间: 永久/周期性                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、Redis 排行榜实现

### 2.1 Redis Sorted Set

```
┌─────────────────────────────────────────────────────────────┐
│                 Redis Sorted Set 原理                         │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ZADD key score member                                      │
│                                                             │
│  数据结构:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  内部实现: 跳表 (SkipList) + 哈希表               │       │
│  │                                                   │       │
│  │  player_level_rank:                               │       │
│  │  ┌──────────┬───────────────┐                     │       │
│  │  │ Member   │ Score (Level) │                     │       │
│  │  ├──────────┼───────────────┤                     │       │
│  │  │ player_1 │ 85            │                     │       │
│  │  │ player_2 │ 72            │                     │       │
│  │  │ player_3 │ 95            │                     │       │
│  │  │ player_4 │ 68            │                     │       │
│  │  │ player_5 │ 80            │                     │       │
│  │  └──────────┴───────────────┘                     │       │
│  │                                                   │       │
│  │  按分数排序: player_3(95) > player_1(85) > ...  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  常用操作:                                                  │
│  ├── ZADD: 添加/更新成员分数                                │
│  ├── ZINCRBY: 增加成员分数                                 │
│  ├── ZRANGE: 获取指定范围成员（升序）                       │
│  ├── ZREVRANGE: 获取指定范围成员（降序）                    │
│  ├── ZRANK: 获取成员排名（升序）                           │
│  ├── ZREVRANK: 获取成员排名（降序）                        │
│  ├── ZSCORE: 获取成员分数                                  │
│  └── ZREM: 删除成员                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Redis 排行榜实现

```cpp
// Redis 排行榜实现

class RedisLeaderboard {
public:
    // 排行榜类型
    enum class Type {
        Level,       // 等级排行榜
        Combat,      // 战力排行榜
        Arena,       // 竞技排行榜
        Guild,       // 公会排行榜
        Achievement  // 成就排行榜
    };

    // 初始化排行榜
    bool init(Type type, uint32_t serverId) {
        key_ = getLeaderboardKey(type, serverId);

        // 检查 Redis 连接
        if (!redis_.connect()) {
            LOG_ERROR("Failed to connect to Redis");
            return false;
        }

        return true;
    }

    // 更新玩家分数
    bool updateScore(uint64_t playerId, const std::string& playerName, int64_t score) {
        // 添加到 Sorted Set
        redis_.zadd(key_, score, std::to_string(playerId));

        // 保存玩家名称映射
        std::string nameKey = key_ + ":names";
        redis_.hset(nameKey, std::to_string(playerId), playerName);

        return true;
    }

    // 增加玩家分数
    bool incrementScore(uint64_t playerId, int64_t delta) {
        redis_.zincrby(key_, delta, std::to_string(playerId));
        return true;
    }

    // 获取 Top N
    std::vector<LeaderboardEntry> getTopN(size_t n) {
        std::vector<std::string> members = redis_.zrevrange(key_, 0, n - 1);

        std::vector<LeaderboardEntry> result;
        result.reserve(members.size());

        for (size_t i = 0; i < members.size(); i++) {
            uint64_t playerId = std::stoull(members[i]);
            int64_t score = redis_.zscore(key_, members[i]);

            LeaderboardEntry entry;
            entry.rank = i + 1;
            entry.playerId = playerId;
            entry.score = score;
            entry.playerName = getPlayerName(playerId);

            result.push_back(entry);
        }

        return result;
    }

    // 获取玩家排名
    int64_t getPlayerRank(uint64_t playerId) {
        long rank = redis_.zrevrank(key_, std::to_string(playerId));
        return rank >= 0 ? rank + 1 : -1; // Redis 从 0 开始
    }

    // 获取玩家分数
    int64_t getPlayerScore(uint64_t playerId) {
        return redis_.zscore(key_, std::to_string(playerId));
    }

    // 获取分页数据
    std::vector<LeaderboardEntry> getRange(size_t offset, size_t limit) {
        long start = offset;
        long end = offset + limit - 1;

        std::vector<std::string> members = redis_.zrevrange(key_, start, end);

        std::vector<LeaderboardEntry> result;
        result.reserve(members.size());

        for (size_t i = 0; i < members.size(); i++) {
            uint64_t playerId = std::stoull(members[i]);
            int64_t score = redis_.zscore(key_, members[i]);

            LeaderboardEntry entry;
            entry.rank = offset + i + 1;
            entry.playerId = playerId;
            entry.score = score;
            entry.playerName = getPlayerName(playerId);

            result.push_back(entry);
        }

        return result;
    }

    // 获取玩家及其周边排名
    std::vector<LeaderboardEntry> getAroundPlayer(uint64_t playerId, size_t range) {
        long rank = redis_.zrevrank(key_, std::to_string(playerId));
        if (rank < 0) {
            return {};
        }

        long start = std::max(0L, rank - (long)range);
        long end = rank + (long)range;

        return getRange(start, end - start + 1);
    }

    // 删除玩家
    bool removePlayer(uint64_t playerId) {
        redis_.zrem(key_, std::to_string(playerId));
        return true;
    }

    // 清空排行榜
    bool clear() {
        redis_.del(key_);
        std::string nameKey = key_ + ":names";
        redis_.del(nameKey);
        return true;
    }

    // 获取总人数
    size_t getTotalCount() {
        return redis_.zcard(key_);
    }

private:
    std::string getLeaderboardKey(Type type, uint32_t serverId) {
        std::string typeStr;
        switch (type) {
            case Type::Level:
                typeStr = "level";
                break;
            case Type::Combat:
                typeStr = "combat";
                break;
            case Type::Arena:
                typeStr = "arena";
                break;
            case Type::Guild:
                typeStr = "guild";
                break;
            case Type::Achievement:
                typeStr = "achievement";
                break;
        }
        return "leaderboard:" + typeStr + ":" + std::to_string(serverId);
    }

    std::string getPlayerName(uint64_t playerId) {
        std::string nameKey = key_ + ":names";
        return redis_.hget(nameKey, std::to_string(playerId));
    }

    struct LeaderboardEntry {
        size_t rank;
        uint64_t playerId;
        std::string playerName;
        int64_t score;
    };

    std::string key_;
    RedisClient redis_;
};
```

---

## 三、高性能排行榜设计

### 3.1 分层缓存架构

```
┌─────────────────────────────────────────────────────────────┐
│                 分层缓存排行榜架构                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  L1: 内存缓存 (进程内)                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - Top 100 缓存                                   │       │
│  │  - 5秒刷新                                        │       │
│  │  - 响应 < 1ms                                    │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  L2: Redis 缓存 (分布式)                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 全部排名                                       │       │
│  │  - 实时更新                                       │       │
│  │  - 响应 < 10ms                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  L3: 数据库 (持久化)                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  - 历史记录                                       │       │
│  │  - 定时同步                                       │       │
│  │  - 响应 < 100ms                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 本地缓存实现

```cpp
// 本地缓存排行榜

class CachedLeaderboard {
public:
    CachedLeaderboard(RedisLeaderboard& redis)
        : redis_(redis), running_(true) {

        // 启动刷新线程
        refreshThread_ = std::thread(&CachedLeaderboard::refreshLoop, this);
    }

    ~CachedLeaderboard() {
        running_ = false;
        if (refreshThread_.joinable()) {
            refreshThread_.join();
        }
    }

    // 获取 Top N (优先从缓存)
    std::vector<LeaderboardEntry> getTopN(size_t n) {
        std::shared_lock<std::shared_mutex> lock(cacheMutex_);

        if (topCache_.empty()) {
            // 缓存为空，从 Redis 加载
            lock.unlock();
            refreshTopCache();
            lock.lock();
        }

        // 返回缓存数据
        std::vector<LeaderboardEntry> result;
        size_t count = std::min(n, topCache_.size());

        for (size_t i = 0; i < count; i++) {
            result.push_back(topCache_[i]);
        }

        return result;
    }

    // 获取玩家排名（优先从缓存）
    int64_t getPlayerRank(uint64_t playerId) {
        // 先查本地缓存
        {
            std::shared_lock<std::shared_mutex> lock(cacheMutex_);
            auto it = rankCache_.find(playerId);
            if (it != rankCache_.end()) {
                return it->second;
            }
        }

        // 缓存未命中，从 Redis 获取
        int64_t rank = redis_.getPlayerRank(playerId);

        if (rank > 0) {
            std::unique_lock<std::shared_mutex> lock(cacheMutex_);
            rankCache_[playerId] = rank;
        }

        return rank;
    }

    // 更新玩家分数
    bool updateScore(uint64_t playerId, const std::string& name, int64_t score) {
        // 更新 Redis
        bool success = redis_.updateScore(playerId, name, score);

        if (success) {
            // 更新本地缓存
            std::unique_lock<std::shared_mutex> lock(cacheMutex_);

            // 更新 Top 缓存中的数据
            updateTopCache(playerId, name, score);

            // 更新排名缓存
            rankCache_[playerId] = redis_.getPlayerRank(playerId);
        }

        return success;
    }

private:
    void refreshLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));

            // 刷新 Top 缓存
            refreshTopCache();

            // 清理过期排名缓存
            cleanRankCache();
        }
    }

    void refreshTopCache() {
        auto topData = redis_.getTopN(200);

        std::unique_lock<std::shared_mutex> lock(cacheMutex_);
        topCache_ = std::move(topData);
    }

    void updateTopCache(uint64_t playerId, const std::string& name, int64_t score) {
        // 查找是否在 Top 缓存中
        auto it = std::find_if(topCache_.begin(), topCache_.end(),
            [playerId](const LeaderboardEntry& e) {
                return e.playerId == playerId;
            });

        if (it != topCache_.end()) {
            // 更新现有数据
            it->playerName = name;
            it->score = score;
        } else {
            // 检查是否应该进入 Top
            if (topCache_.size() < 200 || score > topCache_.back().score) {
                // 重新加载整个 Top 缓存
                refreshTopCache();
            }
        }
    }

    void cleanRankCache() {
        std::unique_lock<std::shared_mutex> lock(cacheMutex_);

        // 只保留最近查询的排名
        if (rankCache_.size() > 10000) {
            // 清理旧数据
            rankCache_.clear();
        }
    }

    RedisLeaderboard& redis_;
    std::thread refreshThread_;
    std::atomic<bool> running_;

    std::shared_mutex cacheMutex_;
    std::vector<LeaderboardEntry> topCache_;
    std::unordered_map<uint64_t, int64_t> rankCache_;

    struct LeaderboardEntry {
        size_t rank;
        uint64_t playerId;
        std::string playerName;
        int64_t score;
    };
};
```

---

## 四、数据库持久化

### 4.1 排行榜数据表

```sql
-- 排行榜历史表（分区）
CREATE TABLE `leaderboard_history` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `server_id` TINYINT UNSIGNED NOT NULL COMMENT '服务器ID',
    `leaderboard_type` TINYINT UNSIGNED NOT NULL COMMENT '排行榜类型',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `player_name` VARCHAR(64) NOT NULL COMMENT '玩家名称',
    `score` BIGINT NOT NULL COMMENT '分数',
    `rank` INT UNSIGNED NOT NULL COMMENT '排名',
    `snapshot_time` DATETIME NOT NULL COMMENT '快照时间',
    `snapshot_date` DATE NOT NULL COMMENT '快照日期',
    PRIMARY KEY (`id`, `snapshot_time`),
    KEY `idx_player` (`player_id`, `leaderboard_type`),
    KEY `idx_type_date` (`leaderboard_type`, `snapshot_date`),
    KEY `idx_server` (`server_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
PARTITION BY RANGE (TO_DAYS(snapshot_time)) (
    PARTITION p202401 VALUES LESS THAN (TO_DAYS('2024-02-01')),
    PARTITION p202402 VALUES LESS THAN (TO_DAYS('2024-03-01')),
    PARTITION p202403 VALUES LESS THAN (TO_DAYS('2024-04-01')),
    PARTITION pmax VALUES LESS THAN MAXVALUE
);

-- 每日排行榜快照表
CREATE TABLE `leaderboard_daily` (
    `snapshot_date` DATE NOT NULL COMMENT '快照日期',
    `leaderboard_type` TINYINT UNSIGNED NOT NULL COMMENT '排行榜类型',
    `server_id` TINYINT UNSIGNED NOT NULL COMMENT '服务器ID',
    `rank` INT UNSIGNED NOT NULL COMMENT '排名',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `player_name` VARCHAR(64) NOT NULL COMMENT '玩家名称',
    `score` BIGINT NOT NULL COMMENT '分数',
    `reward_status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '奖励发放状态',
    PRIMARY KEY (`snapshot_date`, `leaderboard_type`, `server_id`, `rank`),
    KEY `idx_player` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.2 定时持久化

```cpp
// 排行榜定时持久化

class LeaderboardPersistence {
public:
    LeaderboardPersistence(Database* db, RedisLeaderboard& redis)
        : db_(db), redis_(redis) {}

    // 每日快照
    void dailySnapshot() {
        std::string date = getCurrentDate();

        for (int type = 0; type < 5; type++) {
            auto topData = redis_.getTopN(1000);

            for (size_t i = 0; i < topData.size(); i++) {
                const auto& entry = topData[i];

                std::string sql = fmt::format(
                    "INSERT INTO leaderboard_daily "
                    "(snapshot_date, leaderboard_type, server_id, rank, "
                    "player_id, player_name, score) "
                    "VALUES ('{}', {}, {}, {}, {}, '{}', {}) "
                    "ON DUPLICATE KEY UPDATE "
                    "player_name = VALUES(player_name), "
                    "score = VALUES(score)",
                    date, type, serverId_, i + 1,
                    entry.playerId, entry.playerName, entry.score
                );

                db_->execute(sql);
            }
        }
    }

    // 每周重置（竞技场）
    void weeklyReset() {
        // 保存本周数据
        weeklySnapshot();

        // 清空 Redis 排行榜
        redis_.clear();

        // 发放奖励
        sendWeeklyRewards();
    }

private:
    void weeklySnapshot() {
        std::string date = getCurrentDate();

        auto topData = redis_.getTopN(100);

        for (const auto& entry : topData) {
            std::string sql = fmt::format(
                "INSERT INTO leaderboard_history "
                "(server_id, leaderboard_type, player_id, player_name, "
                "score, rank, snapshot_time, snapshot_date) "
                "VALUES ({}, {}, {}, '{}', {}, {}, NOW(), '{}')",
                serverId_, 0, entry.playerId, entry.playerName,
                entry.score, entry.rank, date
            );

            db_->execute(sql);
        }
    }

    void sendWeeklyRewards() {
        auto topData = redis_.getTopN(100);

        for (size_t i = 0; i < topData.size(); i++) {
            const auto& entry = topData[i];

            // 发放排名奖励
            Reward reward = calculateWeeklyReward(i + 1);
            sendRewardToPlayer(entry.playerId, reward);
        }
    }

    Database* db_;
    RedisLeaderboard& redis_;
    uint32_t serverId_ = 1;
};
```

---

## 五、分布式排行榜

### 5.1 跨服排行榜

```
┌─────────────────────────────────────────────────────────────┐
│                    跨服排行榜架构                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   Server 1      Server 2      Server 3                     │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐                  │
│  │ 玩家数据 │   │ 玩家数据 │   │ 玩家数据 │                  │
│  └────┬────┘   └────┬────┘   └────┬────┘                  │
│       │              │              │                       │
│       │  同步         │  同步         │  同步                │
│       ▼              ▼              ▼                       │
│  ┌──────────────────────────────────────────────┐           │
│  │           Redis Cluster (统一排行榜)          │           │
│  │  ┌────────────────────────────────────┐        │           │
│  │  │  cross:leaderboard:arena            │        │           │
│  │  │  cross:leaderboard:combat           │        │           │
│  │  │  cross:leaderboard:consumption       │        │           │
│  │  └────────────────────────────────────┘        │           │
│  └──────────────────────────────────────────────┘           │
│                         │                                  │
│                         ▼                                  │
│  ┌──────────────────────────────────────────────┐           │
│  │          全局排行榜服务                         │           │
│  │  - 聚合所有服务器数据                            │           │
│  │  - 计算跨服排名                                  │           │
│  │  - 提供跨服查询                                  │           │
│  └──────────────────────────────────────────────┘           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 跨服排行榜实现

```cpp
// 跨服排行榜聚合

class CrossServerLeaderboard {
public:
    // 汇总各服数据
    void aggregateFromServers() {
        std::vector<uint32_t> servers = getServerList();

        for (uint32_t serverId : servers) {
            // 从各服 Redis 获取 Top 数据
            auto serverData = fetchServerRanking(serverId);

            // 合并到全局排行榜
            mergeIntoGlobal(serverData);
        }
    }

    // 获取跨服 Top N
    std::vector<CrossServerEntry> getCrossTopN(size_t n) {
        std::string key = "cross:leaderboard:arena";

        std::vector<std::string> members = redis_.zrevrange(key, 0, n - 1);

        std::vector<CrossServerEntry> result;
        result.reserve(members.size());

        for (size_t i = 0; i < members.size(); i++) {
            // member 格式: "serverId:playerId"
            auto parts = split(members[i], ':');
            uint32_t serverId = std::stoul(parts[0]);
            uint64_t playerId = std::stoull(parts[1]);

            int64_t score = redis_.zscore(key, members[i]);

            CrossServerEntry entry;
            entry.rank = i + 1;
            entry.serverId = serverId;
            entry.playerId = playerId;
            entry.score = score;
            entry.playerName = getPlayerName(serverId, playerId);

            result.push_back(entry);
        }

        return result;
    }

private:
    // 从单服获取排名数据
    std::vector<ServerRankingData> fetchServerRanking(uint32_t serverId) {
        std::string key = "leaderboard:arena:" + std::to_string(serverId);

        // 远程获取该服务器的 Top 数据
        auto redisClient = getRedisClient(serverId);
        auto topMembers = redisClient->zrevrange(key, 0, 99);

        std::vector<ServerRankingData> result;
        for (const auto& member : topMembers) {
            int64_t score = redisClient->zscore(key, member);

            ServerRankingData data;
            data.serverId = serverId;
            data.playerId = std::stoull(member);
            data.score = score;

            result.push_back(data);
        }

        return result;
    }

    // 合并到全局排行榜
    void mergeIntoGlobal(const std::vector<ServerRankingData>& serverData) {
        std::string key = "cross:leaderboard:arena";

        for (const auto& data : serverData) {
            // member 格式: "serverId:playerId"
            std::string member = std::to_string(data.serverId) + ":" +
                                  std::to_string(data.playerId);

            // 添加到全局排行榜
            redis_.zadd(key, data.score, member);
        }
    }

    std::string getPlayerName(uint32_t serverId, uint64_t playerId) {
        // 从对应服务器获取玩家名称
        auto redisClient = getRedisClient(serverId);
        std::string nameKey = "player:name:" + std::to_string(playerId);
        return redisClient->get(nameKey);
    }

    struct ServerRankingData {
        uint32_t serverId;
        uint64_t playerId;
        int64_t score;
    };

    struct CrossServerEntry {
        size_t rank;
        uint32_t serverId;
        uint64_t playerId;
        std::string playerName;
        int64_t score;
    };

    RedisClient redis_;
};
```

---

## 六、最佳实践

### 6.1 排行榜优化建议

```
┌─────────────────────────────────────────────────────────────┐
│                  排行榜优化建议                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 数据结构选择                                            │
│     ├── 使用 Redis Sorted Set                               │
│     ├── 避免频繁的全量排序                                  │
│     └── 只保留必要的数据                                    │
│                                                             │
│  2. 缓存策略                                                │
│     ├── L1: 内存缓存 Top 100                               │
│     ├── L2: Redis 全部数据                                  │
│     ├── L3: 数据库历史记录                                  │
│     └── 定期刷新，而非实时同步                               │
│                                                             │
│  3. 更新策略                                                │
│     ├── 批量更新而非单条更新                                 │
│     ├── 使用 Pipeline 减少网络开销                          │
│     ├── 异步更新 Redis                                      │
│     └── 定时持久化到数据库                                   │
│                                                             │
│  4. 查询优化                                                │
│     ├── Top N 查询: 使用本地缓存                            │
│     ├── 个人排名: 缓存查询结果                               │
│     ├── 分页查询: 限制页数大小                               │
│     └── 周边排名: 预计算范围                                  │
│                                                             │
│  5. 重置策略                                                │
│     ├── 每周/每月重置竞技排行榜                              │
│     ├── 重置前保存历史记录                                   │
│     ├── 发放排名奖励                                         │
│     └── 优雅重置（通知玩家）                                 │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 常见问题处理

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| **Redis 内存占用高** | 存储了过多数据 | 限制排行榜数量，定期清理 |
| **查询慢** | 数据量太大 | 使用本地缓存 Top N |
| **分数相同** | 默认按字典序 | 使用 timestamp 作为第二排序 |
| **重置后排名混乱** | 清空顺序问题 | 使用临时 key 重命名 |
| **跨服数据不一致** | 同步延迟 | 定时同步 + 增量更新 |

---

## 七、总结

### 排行榜方案总结

| 方案 | 复杂度 | 实时性 | 适用规模 |
|------|--------|--------|----------|
| **内存排序** | 低 | 低 | < 1000 |
| **Redis Sorted Set** | 中 | 高 | < 10万 |
| **分层缓存** | 中 | 高 | < 50万 |
| **分片 Redis** | 高 | 高 | > 50万 |

### Redis 排行榜命令

```
常用命令:
ZADD key score member      # 添加/更新
ZINCRBY key delta member    # 增加分数
ZREVRANGE key 0 99          # 获取 Top 100
ZREVRANK key member         # 获取排名
ZSCORE key member           # 获取分数
ZREM key member             # 删除成员

组合使用:
ZADD + ZINCRBY              # 更新分数
ZREVRANGE + ZSCORE          # 获取排行榜
ZREVRANK + ZREVRANGE        # 获取周边排名
```

---

## 参考资料

- [Redis Sorted Set 文档](https://redis.io/docs/data-types/sorted-sets/)
- [游戏排行榜设计实践](https://leopard.in.ua/2015/02/20/redis-leaderboard/)
- [大型游戏排行榜架构](https://highscalability.com/blog/2023/12/04/designing-a-scalable-leaderboard/)
