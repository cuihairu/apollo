# Q31: 如何设计好友系统？

## 问题分析

本题考察对社交系统设计的理解：
- 好友系统的核心功能
- 社交关系的数据模型
- 好友推荐算法
- 在线状态管理

---

## 一、好友系统需求

### 1.1 核心功能

```
┌─────────────────────────────────────────────────────────────┐
│                    好友系统核心功能                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  好友管理:                                                  │
│  ├── 添加好友（搜索、推荐）                                 │
│  ├── 删除好友                                               │
│  ├── 好友备注                                               │
│  ├── 好友分组                                               │
│  └── 好友上限                                               │
│                                                             │
│  互动功能:                                                  │
│  ├── 查看好友资料                                           │
│  ├── 查看好友在线状态                                       │
│  ├── 一键跟随                                               │
│  ├── 组队邀请                                               │
│  └── 聊天（私聊）                                           │
│                                                             │
│  黑名单:                                                    │
│  ├── 添加到黑名单                                           │
│  ├── 屏蔽聊天                                               │
│  ├── 屏蔽组队邀请                                           │
│  └── 黑名单管理                                             │
│                                                             │
│  推荐系统:                                                  │
│  ├── 好友推荐                                               │
│  ├── 可能认识的人                                           │
│  └── 附近玩家                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 业务规则

```
┌─────────────────────────────────────────────────────────────┐
│                    好友系统业务规则                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  好友关系:                                                  │
│  ├── 需要双方同意                                           │
│  ├── A 添加 B → B 收到请求 → B 同意 → 成为好友              │
│  ├── 单向添加（关注）可选                                   │
│  └── 删除好友不需要对方确认                                  │
│                                                             │
│  好友上限:                                                  │
│  ├── 普通玩家: 50 个好友                                    │
│  ├── VIP 玩家: 100 个好友                                   │
│  ├── 好友数量可扩容                                         │
│  └── 达到上限时需要删除旧好友                                │
│                                                             │
│  亲密度:                                                    │
│  ├── 组队增加亲密度                                         │
│  ├── 赠送礼物增加亲密度                                       │
│  ├── 一起做任务增加亲密度                                    │
│  └── 亲密度影响解锁功能                                     │
│                                                             │
│  黑名单:                                                    │
│  ├── 拉黑后无法发送消息                                     │
│  ├── 拉黑后无法邀请组队                                     │
│  ├── 拉黑是单向的                                           │
│  └── 黑名单有独立上限                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 数据库表设计

```sql
-- 好友关系表
CREATE TABLE `friend_relationship` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `friend_id` BIGINT UNSIGNED NOT NULL COMMENT '好友ID',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '状态:0待确认,1正常,2已删除',
    `group_name` VARCHAR(32) NOT NULL DEFAULT '默认分组' COMMENT '分组名称',
    `remark` VARCHAR(64) DEFAULT NULL COMMENT '备注名',
    `intimacy` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '亲密度',
    `request_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '请求时间',
    `confirm_time` DATETIME DEFAULT NULL COMMENT '确认时间',
    `delete_time` DATETIME DEFAULT NULL COMMENT '删除时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_friendship` (`player_id`, `friend_id`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_status` (`status`),
    KEY `idx_friend_id` (`friend_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='好友关系表';

-- 好友请求表（独立存储，方便管理）
CREATE TABLE `friend_request` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `from_player_id` BIGINT UNSIGNED NOT NULL COMMENT '发送者ID',
    `to_player_id` BIGINT UNSIGNED NOT NULL COMMENT '接收者ID',
    `message` VARCHAR(256) DEFAULT NULL COMMENT '请求消息',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '状态:0待处理,1已接受,2已拒绝,3已撤销',
    `request_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '请求时间',
    `handle_time` DATETIME DEFAULT NULL COMMENT '处理时间',
    PRIMARY KEY (`id`),
    KEY `idx_to_player` (`to_player_id`, `status`),
    KEY `idx_from_player` (`from_player_id`),
    KEY `idx_request_time` (`request_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='好友请求表';

-- 黑名单表
CREATE TABLE `blacklist` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `blocked_id` BIGINT UNSIGNED NOT NULL COMMENT '被拉黑ID',
    `block_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '拉黑时间',
    `reason` VARCHAR(256) DEFAULT NULL COMMENT '拉黑原因',
    PRIMARY KEY (`player_id`, `blocked_id`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_blocked_id` (`blocked_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='黑名单表';

-- 亲密度日志表
CREATE TABLE `friend_intimacy_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `friend_id` BIGINT UNSIGNED NOT NULL COMMENT '好友ID',
    `action` TINYINT UNSIGNED NOT NULL COMMENT '行为类型:1组队,2送礼,3任务',
    `intimacy_change` SMALLINT SIGNED NOT NULL COMMENT '亲密度变化',
    `current_intimacy` SMALLINT UNSIGNED NOT NULL COMMENT '变化后亲密度',
    `action_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '行为时间',
    PRIMARY KEY (`id`),
    KEY `idx_players` (`player_id`, `friend_id`),
    KEY `idx_action_time` (`action_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='亲密度日志表';

-- 好友分组表
CREATE TABLE `friend_group` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `group_name` VARCHAR(32) NOT NULL COMMENT '分组名称',
    `sort_order` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '排序',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    PRIMARY KEY (`player_id`, `group_name`),
    KEY `idx_player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='好友分组表';
```

### 2.2 Redis 数据结构

```
┌─────────────────────────────────────────────────────────────┐
│                 Redis 好友系统数据结构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  好友列表:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: friend:list:{playerId}                    │       │
│  │  Type: SET                                       │       │
│  │  Value: 好友的 playerId                          │       │
│  │                                                   │       │
│  │  用途: 快速查询好友列表                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  好友信息:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: friend:info:{playerId}                    │       │
│  │  Type: HASH                                       │       │
│  │  Fields:                                         │       │
│  │    - name: 玩家名称                               │       │
│  │    - level: 等级                                 │       │
│  │    - online: 在线状态                             │       │
│  │    - last_login: 最后登录                         │       │
│  │    - location: 所在位置                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  在线状态:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: online:players                              │       │
│  │  Type: SET                                       │       │
│  │  Value: 在线玩家的 playerId                      │       │
│  │                                                   │       │
│  │  用途: 快速查询哪些好友在线                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  亲密度:                                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: friend:intimacy:{playerId}                │       │
│  │  Type: ZSET                                       │       │
│  │  Score: 亲密度                                     │       │
│  │  Member: 好友的 playerId                         │       │
│  │                                                   │       │
│  │  用途: 按亲密度排序好友列表                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、好友系统实现

### 3.1 好友管理

```cpp
// 好友系统服务

class FriendService {
public:
    // 发送好友请求
    bool sendFriendRequest(uint64_t playerId, uint64_t targetId,
                          const std::string& message = "") {
        // 1. 检查是否已经是好友
        if (isFriend(playerId, targetId)) {
            sendErrorResponse(playerId, "已经是好友");
            return false;
        }

        // 2. 检查好友数量上限
        if (getFriendCount(playerId) >= getFriendLimit(playerId)) {
            sendErrorResponse(playerId, "好友数量已达上限");
            return false;
        }

        // 3. 检查是否在黑名单
        if (isInBlacklist(targetId, playerId)) {
            sendErrorResponse(playerId, "对方已将你加入黑名单");
            return false;
        }

        // 4. 检查是否已有待处理请求
        if (hasPendingRequest(playerId, targetId)) {
            sendErrorResponse(playerId, "已有待处理的好友请求");
            return false;
        }

        // 5. 创建好友请求
        database_->execute(fmt::format(
            "INSERT INTO friend_request "
            "(from_player_id, to_player_id, message, status) "
            "VALUES ({}, {}, '{}', 0)",
            playerId, targetId, escapeString(message)
        ));

        // 6. 通知目标玩家
        if (isPlayerOnline(targetId)) {
            notifyFriendRequest(targetId, playerId, message);
        }

        return true;
    }

    // 处理好友请求
    bool handleFriendRequest(uint64_t playerId, uint64_t requestId, bool accept) {
        // 1. 获取请求信息
        auto request = getFriendRequest(requestId);
        if (!request || request->to_player_id != playerId) {
            return false;
        }

        // 2. 更新请求状态
        database_->execute(fmt::format(
            "UPDATE friend_request SET status = {}, handle_time = NOW() "
            "WHERE id = {}",
            accept ? 1 : 2, requestId
        ));

        if (accept) {
            // 3. 创建好友关系（双向）
            createFriendRelationship(request->from_player_id, playerId);
            createFriendRelationship(playerId, request->from_player_id);

            // 4. 通知双方
            notifyFriendAdded(request->from_player_id, playerId);
            notifyFriendAdded(playerId, request->from_player_id);

            // 5. 更新 Redis 缓存
            updateRedisCache(request->from_player_id, playerId);
            updateRedisCache(playerId, request->from_player_id);
        } else {
            // 通知被拒绝
            notifyFriendRequestRejected(request->from_player_id);
        }

        return true;
    }

    // 删除好友
    bool removeFriend(uint64_t playerId, uint64_t friendId) {
        // 1. 检查是否是好友
        if (!isFriend(playerId, friendId)) {
            return false;
        }

        // 2. 更新关系状态（双向）
        database_->execute(fmt::format(
            "UPDATE friend_relationship SET status = 2, delete_time = NOW() "
            "WHERE (player_id = {} AND friend_id = {}) "
            "OR (player_id = {} AND friend_id = {})",
            playerId, friendId, friendId, playerId
        ));

        // 3. 更新 Redis 缓存
        redis_.srem(fmt::format("friend:list:{}", playerId), friendId);
        redis_.srem(fmt::format("friend:list:{}", friendId), playerId);

        // 4. 通知双方
        notifyFriendRemoved(playerId, friendId);
        notifyFriendRemoved(friendId, playerId);

        return true;
    }

    // 获取好友列表
    std::vector<FriendInfo> getFriendList(uint64_t playerId) {
        std::vector<FriendInfo> result;

        // 1. 从 Redis 获取好友 ID 列表
        std::string key = fmt::format("friend:list:{}", playerId);
        auto friendIds = redis_.smembers(key);

        if (friendIds.empty()) {
            // Redis 未命中，从数据库加载
            friendIds = loadFriendListFromDB(playerId);
        }

        // 2. 获取每个好友的详细信息
        for (const auto& friendId : friendIds) {
            FriendInfo info = getFriendInfo(playerId, friendId);
            result.push_back(info);
        }

        // 3. 按亲密度排序
        std::sort(result.begin(), result.end(),
            [](const FriendInfo& a, const FriendInfo& b) {
                return a.intimacy > b.intimacy;
            });

        return result;
    }

    // 设置备注
    bool setFriendRemark(uint64_t playerId, uint64_t friendId,
                        const std::string& remark) {
        database_->execute(fmt::format(
            "UPDATE friend_relationship SET remark = '{}' "
            "WHERE player_id = {} AND friend_id = {}",
            escapeString(remark), playerId, friendId
        ));

        // 更新缓存
        std::string cacheKey = fmt::format("friend:remark:{}:{}", playerId, friendId);
        redis_.set(cacheKey, remark);

        return true;
    }

    // 设置分组
    bool setFriendGroup(uint64_t playerId, uint64_t friendId,
                       const std::string& groupName) {
        database_->execute(fmt::format(
            "UPDATE friend_relationship SET group_name = '{}' "
            "WHERE player_id = {} AND friend_id = {}",
            escapeString(groupName), playerId, friendId
        ));

        return true;
    }

private:
    bool isFriend(uint64_t playerId, uint64_t friendId) {
        // 先查 Redis
        std::string key = fmt::format("friend:list:{}", playerId);
        if (redis_.sismember(key, friendId)) {
            return true;
        }

        // 再查数据库
        auto result = database_->query(fmt::format(
            "SELECT 1 FROM friend_relationship "
            "WHERE player_id = {} AND friend_id = {} AND status = 1",
            playerId, friendId
        ));

        return !result.empty();
    }

    size_t getFriendCount(uint64_t playerId) {
        std::string key = fmt::format("friend:list:{}", playerId);
        return redis_.scard(key);
    }

    size_t getFriendLimit(uint64_t playerId) {
        // 根据玩家 VIP 等级返回好友上限
        int vipLevel = getPlayerVipLevel(playerId);
        return 50 + vipLevel * 50;
    }

    void createFriendRelationship(uint64_t playerId, uint64_t friendId) {
        database_->execute(fmt::format(
            "INSERT INTO friend_relationship "
            "(player_id, friend_id, status, request_time, confirm_time) "
            "VALUES ({}, {}, 1, NOW(), NOW())",
            playerId, friendId
        ));
    }

    void updateRedisCache(uint64_t playerId, uint64_t friendId) {
        // 添加到好友列表
        std::string key = fmt::format("friend:list:{}", playerId);
        redis_.sadd(key, friendId);

        // 设置过期时间
        redis_.expire(key, 3600); // 1小时
    }

    struct FriendInfo {
        uint64_t playerId;
        std::string playerName;
        std::string remark;
        std::string groupName;
        uint16_t level;
        bool online;
        uint16_t intimacy;
    };

    Database* database_;
    RedisClient redis_;
};
```

### 3.2 黑名单管理

```cpp
// 黑名单服务

class BlacklistService {
public:
    // 添加到黑名单
    bool addToBlacklist(uint64_t playerId, uint64_t targetId,
                       const std::string& reason = "") {
        // 1. 从好友列表删除（如果是好友）
        friendService_->removeFriend(playerId, targetId);

        // 2. 添加到黑名单
        database_->execute(fmt::format(
            "INSERT INTO blacklist (player_id, blocked_id, reason) "
            "VALUES ({}, {}, '{}') "
            "ON DUPLICATE KEY UPDATE block_time = NOW()",
            playerId, targetId, escapeString(reason)
        ));

        // 3. 更新 Redis
        std::string key = fmt::format("blacklist:{}", playerId);
        redis_.sadd(key, targetId);

        // 4. 通知被拉黑玩家（可选）
        notifyBlocked(targetId);

        return true;
    }

    // 从黑名单移除
    bool removeFromBlacklist(uint64_t playerId, uint64_t targetId) {
        database_->execute(fmt::format(
            "DELETE FROM blacklist WHERE player_id = {} AND blocked_id = {}",
            playerId, targetId
        ));

        std::string key = fmt::format("blacklist:{}", playerId);
        redis_.srem(key, targetId);

        return true;
    }

    // 检查是否在黑名单
    bool isBlocked(uint64_t playerId, uint64_t targetId) {
        // 先查 Redis
        std::string key = fmt::format("blacklist:{}", playerId);
        if (redis_.sismember(key, targetId)) {
            return true;
        }

        // 再查数据库
        auto result = database_->query(fmt::format(
            "SELECT 1 FROM blacklist WHERE player_id = {} AND blocked_id = {}",
            playerId, targetId
        ));

        return !result.empty();
    }

    // 获取黑名单
    std::vector<BlockedInfo> getBlacklist(uint64_t playerId) {
        auto result = database_->query(fmt::format(
            "SELECT blocked_id, reason, block_time FROM blacklist "
            "WHERE player_id = {} ORDER BY block_time DESC",
            playerId
        ));

        std::vector<BlockedInfo> blacklist;
        for (const auto& row : result) {
            BlockedInfo info;
            info.playerId = row["blocked_id"];
            info.reason = row["reason"];
            info.blockTime = row["block_time"];
            blacklist.push_back(info);
        }

        return blacklist;
    }

private:
    FriendService* friendService_;
    Database* database_;
    RedisClient redis_;
};
```

---

## 四、好友推荐算法

### 4.1 推荐策略

```
┌─────────────────────────────────────────────────────────────┐
│                    好友推荐策略                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  策略 1: 共同好友                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  计算两个玩家的共同好友数量                           │       │
│  │  共同好友越多，推荐优先级越高                         │       │
│  │                                                   │       │
│  │  score = common_friends * 10                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 2: 附近玩家                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  基于地理位置推荐                                   │       │
│  │  同地图/同区域的玩家                                 │       │
│  │                                                   │       │
│  │  score = (1 - distance / max_distance) * 5       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 3: 相似度                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  基于玩家相似度推荐                                   │       │
│  │  相同职业、相近等级                                   │       │
│  │                                                   │       │
│  │  level_diff = abs(level_a - level_b)             │       │
│  │  score = (1 - level_diff / max_level_diff) * 3  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 4: 活跃度                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  推荐活跃玩家                                       │       │
│  │  在线时长、登录频率                                   │       │
│  │                                                   │       │
│  │  score = online_hours * 2                         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  综合评分:                                                  │
│  total_score = common_friends * 10 + proximity * 5 +          │
│                 similarity * 3 + activity * 2                │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 推荐实现

```cpp
// 好友推荐系统

class FriendRecommendationSystem {
public:
    // 获取推荐好友列表
    std::vector<RecommendedPlayer> getRecommendations(uint64_t playerId, size_t count) {
        std::map<uint64_t, float> scores;

        // 1. 共同好友得分
        addCommonFriendsScore(playerId, scores);

        // 2. 附近玩家得分
        addNearbyPlayerScore(playerId, scores);

        // 3. 相似度得分
        addSimilarityScore(playerId, scores);

        // 4. 活跃度得分
        addActivityScore(playerId, scores);

        // 5. 排序并返回 Top N
        std::vector<RecommendedPlayer> result;
        for (const auto& [candidateId, score] : scores) {
            // 过滤掉已经是好友的
            if (friendService_->isFriend(playerId, candidateId)) {
                continue;
            }

            // 过滤掉在黑名单的
            if (blacklistService_->isBlocked(playerId, candidateId)) {
                continue;
            }

            RecommendedPlayer rec;
            rec.playerId = candidateId;
            rec.score = score;
            rec.reasons = getRecommendReasons(playerId, candidateId, score);

            result.push_back(rec);

            if (result.size() >= count) {
                break;
            }
        }

        return result;
    }

private:
    void addCommonFriendsScore(uint64_t playerId,
                                std::map<uint64_t, float>& scores) {
        // 获取玩家的好友列表
        auto friends = friendService_->getFriendList(playerId);
        std::unordered_set<uint64_t> friendIds;
        for (const auto& f : friends) {
            friendIds.insert(f.playerId);
        }

        // 查找有共同好友的玩家
        for (uint64_t friendId : friendIds) {
            auto theirFriends = friendService_->getFriendList(friendId);

            for (const auto& f : theirFriends) {
                // 不推荐自己、已有好友
                if (f.playerId == playerId ||
                    friendIds.count(f.playerId) > 0) {
                    continue;
                }

                // 累加得分
                scores[f.playerId] += 10.0f;
            }
        }
    }

    void addNearbyPlayerScore(uint64_t playerId,
                              std::map<uint64_t, float>& scores) {
        // 获取玩家位置
        Position pos = getPlayerPosition(playerId);

        // 获取附近的玩家（AOI）
        auto nearbyPlayers = aoiService_->getNearbyPlayers(pos, 100.0f);

        for (uint64_t nearbyId : nearbyPlayers) {
            if (nearbyId == playerId) continue;

            Position theirPos = getPlayerPosition(nearbyId);
            float distance = pos.distanceTo(theirPos);

            // 距离越近，得分越高
            float score = std::max(0.0f, 1.0f - distance / 100.0f) * 5.0f;
            scores[nearbyId] += score;
        }
    }

    void addSimilarityScore(uint64_t playerId,
                            std::map<uint64_t, float>& scores) {
        PlayerInfo myInfo = getPlayerInfo(playerId);

        // 查找相似玩家（相同职业、相近等级）
        auto similarPlayers = findSimilarPlayers(myInfo);

        for (const auto& [otherId, similarity] : similarPlayers) {
            scores[otherId] += similarity * 3.0f;
        }
    }

    void addActivityScore(uint64_t playerId,
                          std::map<uint64_t, float>& scores) {
        // 获取活跃玩家
        auto activePlayers = getActivePlayers();

        for (uint64_t activeId : activePlayers) {
            if (activeId == playerId) continue;

            // 在线时长加分
            float onlineHours = getPlayerOnlineHours(activeId);
            scores[activeId] += std::min(onlineHours, 10.0f) * 0.2f;
        }
    }

    std::vector<std::string> getRecommendReasons(uint64_t playerId,
                                                   uint64_t candidateId,
                                                   float score) {
        std::vector<std::string> reasons;

        // 检查各种推荐条件
        int commonCount = getCommonFriendsCount(playerId, candidateId);
        if (commonCount > 0) {
            reasons.push_back(fmt::format("{} 个共同好友", commonCount));
        }

        if (isNearbyPlayer(playerId, candidateId)) {
            reasons.push_back("就在附近");
        }

        if (hasSameVocation(playerId, candidateId)) {
            reasons.push_back("相同职业");
        }

        if (isRecentlyActive(candidateId)) {
            reasons.push_back("活跃玩家");
        }

        return reasons;
    }

    struct RecommendedPlayer {
        uint64_t playerId;
        float score;
        std::vector<std::string> reasons;
    };

    FriendService* friendService_;
    BlacklistService* blacklistService_;
    AOIService* aoiService_;
};
```

---

## 五、在线状态管理

### 5.1 在线状态实现

```cpp
// 在线状态管理

class OnlineStatusManager {
public:
    // 玩家上线
    void onPlayerLogin(uint64_t playerId, const std::string& playerName) {
        // 1. 添加到在线玩家集合
        redis_.sadd("online:players", playerId);

        // 2. 设置玩家信息
        std::string infoKey = fmt::format("player:info:{}", playerId);
        redis_.hset(infoKey, "name", playerName);
        redis_.hset(infoKey, "online", "1");
        redis_.hset(infoKey, "login_time", std::to_string(getCurrentTime()));

        // 3. 通知好友玩家上线
        notifyFriendsOnline(playerId, true);

        // 4. 更新好友列表缓存
        updateFriendListCache(playerId);
    }

    // 玩家下线
    void onPlayerLogout(uint64_t playerId) {
        // 1. 从在线玩家集合移除
        redis_.srem("online:players", playerId);

        // 2. 更新玩家信息
        std::string infoKey = fmt::format("player:info:{}", playerId);
        redis_.hset(infoKey, "online", "0");
        redis_.hset(infoKey, "logout_time", std::to_string(getCurrentTime()));

        // 3. 通知好友玩家下线
        notifyFriendsOnline(playerId, false);

        // 4. 设置信息过期时间
        redis_.expire(infoKey, 3600); // 1小时后过期
    }

    // 检查玩家是否在线
    bool isPlayerOnline(uint64_t playerId) {
        std::string infoKey = fmt::format("player:info:{}", playerId);
        std::string online = redis_.hget(infoKey, "online");
        return online == "1";
    }

    // 获取在线好友
    std::vector<uint64_t> getOnlineFriends(uint64_t playerId) {
        std::vector<uint64_t> onlineFriends;

        // 获取好友列表
        std::string listKey = fmt::format("friend:list:{}", playerId);
        auto friendIds = redis_.smembers(listKey);

        // 获取在线玩家集合
        auto onlinePlayers = redis_.smembers("online:players");
        std::unordered_set<uint64_t> onlineSet;
        for (const auto& id : onlinePlayers) {
            onlineSet.insert(std::stoull(id));
        }

        // 取交集
        for (const auto& friendId : friendIds) {
            uint64_t id = std::stoull(friendId);
            if (onlineSet.count(id) > 0) {
                onlineFriends.push_back(id);
            }
        }

        return onlineFriends;
    }

private:
    void notifyFriendsOnline(uint64_t playerId, bool online) {
        auto friendIds = getFriendIds(playerId);

        for (uint64_t friendId : friendIds) {
            if (isPlayerOnline(friendId)) {
                // 发送上线/下线通知
                sendOnlineNotification(friendId, playerId, online);
            }
        }
    }

    void updateFriendListCache(uint64_t playerId) {
        // 从数据库加载好友列表到 Redis
        auto result = database_->query(fmt::format(
            "SELECT friend_id FROM friend_relationship "
            "WHERE player_id = {} AND status = 1",
            playerId
        ));

        std::string key = fmt::format("friend:list:{}", playerId);
        redis_.del(key);

        for (const auto& row : result) {
            uint64_t friendId = row["friend_id"];
            redis_.sadd(key, friendId);
        }

        redis_.expire(key, 3600);
    }

    RedisClient redis_;
    Database* database_;
};
```

---

## 六、最佳实践

### 6.1 好友系统设计建议

```
┌─────────────────────────────────────────────────────────────┐
│                  好友系统设计建议                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 数据一致性                                              │
│     ├── 好友关系需要双向存储                                │
│     ├── 删除好友需要双方都删除                              │
│     └── 使用事务确保一致性                                  │
│                                                             │
│  2. 缓存策略                                                │
│     ├── 好友列表缓存在 Redis                                │
│     ├── 在线状态使用 Redis SET                              │
│     ├── 设置合理的过期时间                                  │
│     └── 变化时主动更新缓存                                  │
│                                                             │
│  3. 性能优化                                                │
│     ├── 好友列表分页加载                                    │
│     ├── 在线状态增量更新                                    │
│     ├── 推荐结果缓存                                        │
│     └── 批量处理好友操作                                    │
│                                                             │
│  4. 用户体验                                                │
│     ├── 好友上线即时通知                                    │
│     ├── 推荐理由清晰                                        │
│     ├── 操作反馈及时                                        │
│     └── 支持备注和分组                                      │
│                                                             │
│  5. 安全控制                                                │
│     ├── 好友请求限频                                        │
│     ├── 防止恶意添加                                        │
│     ├── 黑名单功能完善                                      │
│     └── 举报机制                                            │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 常见问题处理

| 问题 | 解决方案 |
|------|----------|
| **好友数量上限** | VIP 扩容、好友分组管理 |
| **推荐不准** | 多策略组合、用户反馈学习 |
| **在线状态延迟** | WebSocket 推送 |
| **通知过多** | 设置通知开关、免打扰 |
| **数据库压力** | Redis 缓存、批量写入 |

---

## 七、总结

### 好友系统功能总结

| 功能 | 实现方式 | 复杂度 |
|------|----------|--------|
| **好友管理** | MySQL + Redis | 中 |
| **在线状态** | Redis SET | 低 |
| **黑名单** | MySQL + Redis | 低 |
| **好友推荐** | 算法计算 | 高 |
| **亲密度** | MySQL + Redis | 中 |

### Redis 数据结构总结

```
好友系统 Redis 使用:
- SET: 好友列表、在线玩家
- HASH: 玩家信息
- ZSET: 亲密度排序
- STRING: 备注、缓存
```

---

## 参考资料

- [MMO 社交系统设计](https://www.gamedev.net/)
- [Redis 社交网络数据结构](https://redis.com/redis-best-practices/)
- [好友推荐算法](https://www.kdd.org/)
