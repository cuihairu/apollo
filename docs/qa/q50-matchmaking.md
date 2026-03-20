# Q50: 如何设计匹配系统？

## 问题分析

本题考察对匹配系统的理解：
- 匹配算法设计
- 匹配池管理
- 等级/段位匹配
- 跨服匹配实现

---

## 一、匹配系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    匹配系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  匹配类型:                                                   │
│  ├── PvP 匹配                                              │
│  │   ├── 1v1 单排                                          │
│  │   ├── 2v2 双排                                          │
│  │   ├── 3v3 三排                                          │
│  │   ├── 5v5 五排                                          │
│  │   └── 大战场 (10v10, 40v40)                              │
│  │                                                        │
│  ├── PvE 匹配                                              │
│  │   ├── 副本匹配                                           │
│  │   ├── 世界Boss匹配                                       │
│  │   └── 活动匹配                                           │
│  │                                                        │
│  └── 社交匹配                                               │
│      ├── 组队匹配                                           │
│      ├── 师徒匹配                                           │
│      └── 好友双排                                           │
│                                                             │
│  匹配流程:                                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家加入匹配队列                                │       │
│  │     ↓                                            │       │
│  │  2. 系统评估玩家实力                                │       │
│  │     ↓                                            │       │
│  │  3. 查找匹配对手                                    │       │
│  │     ↓                                            │       │
│  │  4. 组建比赛队伍                                    │       │
│  │     ↓                                            │       │
│  │  5. 传送到比赛场地                                  │       │
│  │     ↓                                            │       │
│  │  6. 开始比赛                                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 匹配算法

```
┌─────────────────────────────────────────────────────────────┐
│                    匹配算法                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. ELO 等级匹配                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  核心思想: 等级接近的玩家匹配                       │       │
│  │                                                   │       │
│  │  匹配规则:                                         │       │
│  │  - 优先匹配等级相近的玩家 (±100以内)                │       │
│  │  - 随着等待时间扩大匹配范围                         │       │
│  │  - 新手保护期 (前10场)                              │       │
│  │                                                   │       │
│  │  等待时间 → 匹配范围:                               │       │
│  │  - 0-30秒: ±100                                    │       │
│  │  - 30-60秒: ±200                                   │       │
│  │  - 60-120秒: ±400                                  │       │
│  │  - 120秒以上: ±800                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 分段匹配                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  将玩家分为多个段位:                               │       │
│  │  - 青铜, 白银, 黄金, 铂金, 钻石, 大师, 王者          │       │
│  │                                                   │       │
│  │  匹配规则:                                         │       │
│  │  - 优先同段位匹配                                   │       │
│  │  - 跨段位最多1段                                   │       │
│  │  - 钻石以上严格限制                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 队伍匹配                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  队伍平均ELO = (E1 + E2 + E3 + E4 + E5) / 5        │       │
│  │                                                   │       │
│  │  匹配规则:                                         │       │
│  │  - 队伍 vs 队伍: 平均ELO接近                        │       │
│  │  - 队伍 vs 单人: 队伍ELO略高 (补偿单人劣势)          │       │
│  │  - 单人 vs 单人: ELO接近                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 职业平衡                                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  确保双方职业平衡:                                   │       │
│  │  - 坦克数量相当                                     │       │
│  │  - 治疗数量相当                                     │       │
│  │  - 输出数量相当                                     │       │
│  │                                                   │       │
│  │  优先级: 治疗 > 坦克 > 输出                         │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 数据库表结构

```sql
-- 匹配票表 (内存为主，这里仅作持久化参考)
CREATE TABLE `match_ticket` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `match_type` TINYINT UNSIGNED NOT NULL COMMENT '匹配类型:0:1v1,1:3v3,2:5v5',
    `mode` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '模式:0:排位,1:匹配',
    `rating` INT UNSIGNED NOT NULL DEFAULT 1000 COMMENT 'ELO分数',
    `tier` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '段位',
    `player_class` TINYINT UNSIGNED NOT NULL COMMENT '职业',
    `party_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '队伍ID',
    `server_id` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '服务器ID',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
    PRIMARY KEY (`id`),
    KEY `idx_match_type` (`match_type`, `mode`),
    KEY `idx_rating` (`rating`)
) ENGINE=MEMORY DEFAULT CHARSET=utf8mb4 COMMENT='匹配票表';

-- 玩家段位表
CREATE TABLE `player_rating` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `match_type` TINYINT UNSIGNED NOT NULL COMMENT '匹配类型',
    `rating` INT UNSIGNED NOT NULL DEFAULT 1000 COMMENT 'ELO分数',
    `tier` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '段位',
    `rank` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '段位内排名',
    `wins` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '胜利次数',
    `losses` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '失败次数',
    `win_streak` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '连胜',
    `best_streak` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '最佳连胜',
    `season` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '赛季',
    `last_play_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '最后游戏时间',
    PRIMARY KEY (`player_id`, `match_type`, `season`),
    KEY `idx_rating` (`rating`),
    KEY `idx_tier` (`tier`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家段位表';

-- 匹配历史表
CREATE TABLE `match_history` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `match_id` BIGINT UNSIGNED NOT NULL COMMENT '比赛ID',
    `match_type` TINYINT UNSIGNED NOT NULL COMMENT '匹配类型',
    `mode` TINYINT UNSIGNED NOT NULL COMMENT '模式',
    `team1_score` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '队伍1得分',
    `team2_score` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '队伍2得分',
    `winner` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '获胜队伍:1或2',
    `duration` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '比赛时长(秒)',
    `match_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '比赛时间',
    `season` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '赛季',
    PRIMARY KEY (`id`),
    KEY `idx_match_id` (`match_id`),
    KEY `idx_match_time` (`match_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='匹配历史表';

-- 匹配参与者表
CREATE TABLE `match_participant` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `match_id` BIGINT UNSIGNED NOT NULL COMMENT '比赛ID',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `team` TINYINT UNSIGNED NOT NULL COMMENT '队伍:1或2',
    `player_class` TINYINT UNSIGNED NOT NULL COMMENT '职业',
    `rating_before` INT UNSIGNED NOT NULL COMMENT '赛前分数',
    `rating_after` INT UNSIGNED NOT NULL COMMENT '赛后分数',
    `rating_change` INT NOT NULL COMMENT '分数变化',
    `kills` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '击杀',
    `deaths` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '死亡',
    `assists` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '助攻',
    `damage` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '伤害',
    `healing` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '治疗',
    PRIMARY KEY (`id`),
    KEY `idx_match_id` (`match_id`),
    KEY `idx_player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='匹配参与者表';
```

### 2.2 数据结构

```cpp
// 匹配系统数据结构

enum class MatchType {
    PVP_1V1 = 0,         // 1v1
    PVP_2V2 = 1,         // 2v2
    PVP_3V3 = 2,         // 3v3
    PVP_5V5 = 3,         // 5v5
    PVP_10V10 = 4,       // 10v10 大战场
    PVE_DUNGEON = 10,    // 副本
    PVE_RAID = 11,       // 团队副本
};

enum class MatchMode {
    RANKED = 0,          // 排位
    CASUAL = 1,          // 匹配
    BRAWL = 2,           // 乱斗
};

enum class PlayerClass {
    WARRIOR = 0,         // 战士
    MAGE = 1,            // 法师
    PRIEST = 2,          // 牧师
    ROGUE = 3,           // 盗贼
    HUNTER = 4,          // 猎人
    WARLOCK = 5,         // 术士
    PALADIN = 6,         // 圣骑士
    SHAMAN = 7,          // 萨满
    DRUID = 8,           // 德鲁伊
};

enum class PlayerTier {
    BRONZE = 0,          // 青铜
    SILVER = 1,          // 白银
    GOLD = 2,            // 黄金
    PLATINUM = 3,        // 铂金
    DIAMOND = 4,         // 钻石
    MASTER = 5,          // 大师
    CHALLENGER = 6,      // 王者
};

// 匹配票
struct MatchTicket {
    uint64_t playerId;
    uint64_t partyId;           // 队伍ID，0表示单人
    MatchType matchType;
    MatchMode mode;
    int rating;                 // ELO分数
    PlayerTier tier;
    PlayerClass playerClass;
    int playerLevel;
    uint32_t serverId;
    uint64_t createTime;
    uint64_t lastMatchTime;     // 上次匹配时间
};

// 匹配组 (一个队伍)
struct MatchGroup {
    uint64_t groupId;
    std::vector<MatchTicket> tickets;
    int averageRating;
    PlayerClass classMask;      // 职业掩码
    uint64_t createTime;
};

// 匹配结果
struct MatchResult {
    uint64_t matchId;
    MatchType matchType;
    MatchMode mode;
    std::vector<uint64_t> team1;
    std::vector<uint64_t> team2;
    uint64_t mapId;
};
```

---

## 三、匹配系统实现

### 3.1 匹配管理器

```cpp
// 匹配系统

class MatchmakingSystem {
public:
    static constexpr uint32_t MATCH_CHECK_INTERVAL = 1000;  // 1秒检查一次
    static constexpr int MAX_WAIT_TIME = 300;              // 最大等待5分钟
    static constexpr int BASE_RATING = 1000;               // 基础ELO

    // 初始化
    void initialize() {
        // 启动匹配定时器
        timerManager_->scheduleRepeated(
            MATCH_CHECK_INTERVAL,
            [this]() { processMatches(); }
        );
    }

    // 加入匹配
    bool joinMatchmaking(uint64_t playerId, MatchType type, MatchMode mode) {
        // 检查玩家是否已在匹配中
        if (isInMatchmaking(playerId)) {
            sendError(playerId, "Already in matchmaking");
            return false;
        }

        // 检查玩家状态
        if (!canJoinMatch(playerId, type)) {
            return false;
        }

        // 获取玩家段位
        auto rating = getPlayerRating(playerId, type);

        // 创建匹配票
        MatchTicket ticket;
        ticket.playerId = playerId;
        ticket.partyId = getPartyId(playerId);  // 0表示单人
        ticket.matchType = type;
        ticket.mode = mode;
        ticket.rating = rating.rating;
        ticket.tier = rating.tier;
        ticket.playerClass = static_cast<PlayerClass>(getPlayerClass(playerId));
        ticket.playerLevel = getPlayerLevel(playerId);
        ticket.serverId = getPlayerServerId(playerId);
        ticket.createTime = getCurrentTime();
        ticket.lastMatchTime = rating.lastPlayTime * 1000;

        // 添加到匹配池
        addToMatchPool(ticket);

        // 通知客户端
        sendToClient(playerId, "onJoinMatchmaking", static_cast<int>(type));

        return true;
    }

    // 离开匹配
    bool leaveMatchmaking(uint64_t playerId) {
        auto it = tickets_.find(playerId);
        if (it == tickets_.end()) {
            return false;
        }

        // 从匹配池移除
        removeFromMatchPool(playerId);

        // 通知客户端
        sendToClient(playerId, "onLeaveMatchmaking");

        return true;
    }

    // 处理匹配 (定期调用)
    void processMatches() {
        uint64_t now = getCurrentTime();

        // 按匹配类型分组处理
        std::map<std::pair<MatchType, MatchMode>, std::vector<MatchTicket>> groupedTickets;

        for (const auto& [playerId, ticket] : tickets_) {
            auto key = std::make_pair(ticket.matchType, ticket.mode);
            groupedTickets[key].push_back(ticket);
        }

        // 处理每种匹配类型
        for (auto& [key, tickets] : groupedTickets) {
            processMatchType(tickets, now);
        }
    }

private:
    void processMatchType(std::vector<MatchTicket>& tickets, uint64_t now) {
        if (tickets.empty()) return;

        MatchType type = tickets[0].matchType;
        int teamSize = getTeamSize(type);

        // 按等待时间和评分排序
        std::sort(tickets.begin(), tickets.end(),
            [now](const MatchTicket& a, const MatchTicket& b) {
                uint32_t waitA = (now - a.createTime) / 1000;
                uint32_t waitB = (now - b.createTime) / 1000;
                if (waitA != waitB) {
                    return waitA > waitB;  // 等待久的优先
                }
                return a.rating < b.rating;  // 同等待时间按评分
            }
        );

        // 分组：队伍在一起
        std::vector<MatchGroup> groups;
        groupTicketsByParty(tickets, groups);

        // 尝试匹配
        while (groups.size() >= 2) {
            // 寻找匹配的两个组
            auto [team1, team2] = findMatch(groups, now);
            if (!team1 || !team2) {
                break;  // 没有找到合适的匹配
            }

            // 创建比赛
            createMatch(*team1, *team2);

            // 从待匹配列表移除
            removeGroup(groups, *team1);
            removeGroup(groups, *team2);
        }
    }

    void groupTicketsByParty(const std::vector<MatchTicket>& tickets,
                            std::vector<MatchGroup>& groups) {
        std::map<uint64_t, std::vector<MatchTicket>> partyMap;

        // 按队伍ID分组
        for (const auto& ticket : tickets) {
            uint64_t partyId = ticket.partyId;
            if (partyId == 0) {
                partyId = ticket.playerId;  // 单人用自己的ID
            }
            partyMap[partyId].push_back(ticket);
        }

        // 创建匹配组
        for (auto& [partyId, partyTickets] : partyMap) {
            MatchGroup group;
            group.groupId = partyId;
            group.tickets = partyTickets;
            group.createTime = partyTickets[0].createTime;

            // 计算平均评分
            int totalRating = 0;
            int classMask = 0;

            for (const auto& ticket : partyTickets) {
                totalRating += ticket.rating;
                classMask |= (1 << static_cast<int>(ticket.playerClass));
            }

            group.averageRating = totalRating / partyTickets.size();
            group.classMask = static_cast<PlayerClass>(classMask);

            groups.push_back(group);
        }
    }

    std::pair<MatchGroup*, MatchGroup*> findMatch(
            std::vector<MatchGroup>& groups, uint64_t now) {
        if (groups.size() < 2) {
            return {nullptr, nullptr};
        }

        // 计算等待时间范围
        for (auto& group : groups) {
            group.waitTime = (now - group.createTime) / 1000;
        }

        // 尝试找到匹配
        for (size_t i = 0; i < groups.size(); ++i) {
            for (size_t j = i + 1; j < groups.size(); ++j) {
                if (canMatch(groups[i], groups[j], now)) {
                    return {&groups[i], &groups[j]};
                }
            }
        }

        return {nullptr, nullptr};
    }

    bool canMatch(const MatchGroup& group1, const MatchGroup& group2,
                  uint64_t now) {
        // 获取等待时间
        uint32_t waitTime = std::max(
            (now - group1.createTime) / 1000,
            (now - group2.createTime) / 1000
        );

        // 计算评分差
        int ratingDiff = std::abs(group1.averageRating - group2.averageRating);

        // 根据等待时间确定允许的评分差
        int maxRatingDiff = getAllowedRatingDiff(waitTime);

        if (ratingDiff > maxRatingDiff) {
            return false;
        }

        // 职业平衡检查 (可选)
        if (!checkClassBalance(group1, group2)) {
            return false;
        }

        // 段位检查 (高端段位严格限制)
        if (group1.tickets[0].tier >= PlayerTier::DIAMOND ||
            group2.tickets[0].tier >= PlayerTier::DIAMOND) {
            if (group1.tickets[0].tier != group2.tickets[0].tier) {
                return false;
            }
        }

        return true;
    }

    int getAllowedRatingDiff(uint32_t waitTime) {
        // 等待时间越长，允许的评分差越大
        if (waitTime < 30) {
            return 100;
        } else if (waitTime < 60) {
            return 200;
        } else if (waitTime < 120) {
            return 400;
        } else {
            return 800;
        }
    }

    bool checkClassBalance(const MatchGroup& group1, const MatchGroup& group2) {
        // 简单的职业平衡检查
        // 统计每种职业的数量

        std::map<int, int> classCount1, classCount2;

        for (const auto& ticket : group1.tickets) {
            classCount1[static_cast<int>(ticket.playerClass)]++;
        }
        for (const auto& ticket : group2.tickets) {
            classCount2[static_cast<int>(ticket.playerClass)]++;
        }

        // 检查治疗职业数量 (牧师, 圣骑士, 萨满, 德鲁伊)
        int healer1 = classCount1[2] + classCount1[6] + classCount1[7] + classCount1[8];
        int healer2 = classCount2[2] + classCount2[6] + classCount2[7] + classCount2[8];

        if (std::abs(healer1 - healer2) > 1) {
            return false;
        }

        // 检查坦克职业数量 (战士, 圣骑士, 德鲁伊)
        int tank1 = classCount1[0] + classCount1[6] + classCount1[8];
        int tank2 = classCount2[0] + classCount2[6] + classCount2[8];

        if (std::abs(tank1 - tank2) > 1) {
            return false;
        }

        return true;
    }

    void createMatch(const MatchGroup& team1, const MatchGroup& team2) {
        // 生成比赛ID
        uint64_t matchId = generateMatchId();

        // 收集玩家ID
        std::vector<uint64_t> players1, players2;
        for (const auto& ticket : team1.tickets) {
            players1.push_back(ticket.playerId);
        }
        for (const auto& ticket : team2.tickets) {
            players2.push_back(ticket.playerId);
        }

        // 从匹配池移除
        for (uint64_t playerId : players1) {
            removeFromMatchPool(playerId);
        }
        for (uint64_t playerId : players2) {
            removeFromMatchPool(playerId);
        }

        // 选择地图
        uint64_t mapId = selectMap(team1.tickets[0].matchType);

        // 创建比赛
        MatchResult result;
        result.matchId = matchId;
        result.matchType = team1.tickets[0].matchType;
        result.mode = team1.tickets[0].mode;
        result.team1 = players1;
        result.team2 = players2;
        result.mapId = mapId;

        // 通知所有玩家
        for (uint64_t playerId : players1) {
            sendToClient(playerId, "onMatchFound", matchId, 1, mapId);
        }
        for (uint64_t playerId : players2) {
            sendToClient(playerId, "onMatchFound", matchId, 2, mapId);
        }

        // 传送到比赛场地
        teleportPlayersToMatch(players1, players2, mapId);

        // 通知战斗系统开始比赛
        startMatch(result);
    }

    void removeFromMatchPool(uint64_t playerId) {
        tickets_.erase(playerId);
    }

    bool isInMatchmaking(uint64_t playerId) {
        return tickets_.find(playerId) != tickets_.end();
    }

    int getTeamSize(MatchType type) {
        switch (type) {
            case MatchType::PVP_1V1: return 1;
            case MatchType::PVP_2V2: return 2;
            case MatchType::PVP_3V3: return 3;
            case MatchType::PVP_5V5: return 5;
            case MatchType::PVP_10V10: return 10;
            default: return 1;
        }
    }

    PlayerRating getPlayerRating(uint64_t playerId, MatchType type) {
        // 从数据库或缓存获取玩家段位
        auto result = database_->query(fmt::format(
            "SELECT rating, tier, last_play_time FROM player_rating "
            "WHERE player_id = {} AND match_type = {} AND season = {}",
            playerId, static_cast<int>(type), getCurrentSeason()
        ));

        if (!result.empty()) {
            return {
                .rating = result[0]["rating"].get<int>(),
                .tier = static_cast<PlayerTier>(result[0]["tier"].get<int>()),
                .lastPlayTime = result[0]["last_play_time"].get<uint64_t>()
            };
        }

        // 新玩家默认
        return {BASE_RATING, PlayerTier::BRONZE, 0};
    }

    void updateRating(uint64_t playerId, MatchType type, bool won,
                     int opponentRating) {
        auto playerRating = getPlayerRating(playerId, type);
        int expectedScore = getExpectedScore(playerRating.rating, opponentRating);
        int score = won ? 1 : 0;
        int kFactor = getKFactor(playerRating.rating);
        int newRating = playerRating.rating + kFactor * (score - expectedScore);

        // 更新数据库
        database_->execute(fmt::format(
            "INSERT INTO player_rating (player_id, match_type, rating, tier, season) "
            "VALUES ({}, {}, {}, {}, {}) "
            "ON DUPLICATE KEY UPDATE rating = {}, tier = {}, wins = wins + {}, last_play_time = NOW()",
            playerId, static_cast<int>(type), newRating,
            static_cast<int>(ratingToTier(newRating)), getCurrentSeason(),
            newRating, static_cast<int>(ratingToTier(newRating)),
            won ? 1 : 0
        ));
    }

    int getExpectedScore(int ratingA, int ratingB) {
        // ELO预期分数公式
        return 1.0f / (1.0f + pow(10.0f, (ratingB - ratingA) / 400.0f));
    }

    int getKFactor(int rating) {
        // K因子：高分玩家K因子更小
        if (rating < 2000) return 32;
        if (rating < 2400) return 24;
        return 16;
    }

    PlayerTier ratingToTier(int rating) {
        if (rating < 1200) return PlayerTier::BRONZE;
        if (rating < 1400) return PlayerTier::SILVER;
        if (rating < 1600) return PlayerTier::GOLD;
        if (rating < 1800) return PlayerTier::PLATINUM;
        if (rating < 2000) return PlayerTier::DIAMOND;
        if (rating < 2400) return PlayerTier::MASTER;
        return PlayerTier::CHALLENGER;
    }

    void addToMatchPool(const MatchTicket& ticket) {
        tickets_[ticket.playerId] = ticket;
    }

    void removeGroup(std::vector<MatchGroup>& groups, const MatchGroup& target) {
        auto it = std::find_if(groups.begin(), groups.end(),
            [&target](const MatchGroup& g) {
                return g.groupId == target.groupId;
            }
        );
        if (it != groups.end()) {
            groups.erase(it);
        }
    }

    std::unordered_map<uint64_t, MatchTicket> tickets_;
};
```

---

## 四、跨服匹配

### 4.1 跨服匹配架构

```
┌─────────────────────────────────────────────────────────────┐
│                    跨服匹配架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Server1          Server2          Server3                  │
│     │                │                │                     │
│     └────────────────┼────────────────┘                    │
│                      ▼                                     │
│              ┌───────────────┐                              │
│              │  Match Maker  │  匹配服务器                  │
│              │   (Central)   │                              │
│              └───────────────┘                              │
│                      │                                     │
│                      ▼                                     │
│              ┌───────────────┐                              │
│              │ Battle Server │  战斗服务器                  │
│              └───────────────┘                              │
│                                                             │
│  流程:                                                      │
│  1. 玩家从各自服务器加入匹配                                 │
│  2. MatchMaker 收集所有服务器的匹配请求                       │
│  3. MatchMaker 进行匹配计算                                  │
│  4. 匹配成功后分配战斗服务器                                 │
│  5. 玩家传送到战斗服务器进行战斗                             │
│  6. 战斗结束后玩家返回原服务器                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 五、最佳实践

### 5.1 匹配系统设计建议

| 实践 | 说明 |
|------|------|
| **ELO评分** | 准确评估玩家实力 |
| **动态范围** | 等待越久匹配范围越大 |
| **职业平衡** | 确保双方职业均衡 |
| **段位保护** | 防止玩家掉段过快 |
| **胜场奖励** | 连胜获得额外奖励 |

### 5.2 性能优化

```
优化策略:
1. 匹配池使用内存表
2. 定期清理过期匹配票
3. 异步处理比赛结算
4. 玩家状态缓存
5. 跨服匹配使用消息队列
```

---

## 六、总结

### 匹配系统核心

```
匹配系统 = 匹配池 + ELO算法 + 队伍平衡 + 比赛管理
- ELO算法评估玩家实力
- 动态范围平衡等待时间和质量
- 队伍平衡确保公平性
- 跨服匹配扩大玩家池
```

---

## 参考资料

- [ELO评分系统](https://en.wikipedia.org/wiki/Elo_rating_system)
- [游戏匹配算法设计](https://www.gamedev.net/)
