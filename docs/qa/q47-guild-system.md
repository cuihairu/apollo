# Q47: 如何设计公会系统？

## 问题分析

本题考察对公会系统的理解：
- 公会数据模型设计
- 成员权限管理
- 公会等级与经验
- 公会功能实现

---

## 一、公会系统架构

### 1.1 系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    公会系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  公会管理:                                                   │
│  ├── 创建公会 (消耗金币/道具)                                │
│  ├── 解散公会 (仅会长)                                      │
│  ├── 修改公会信息 (名称、公告、徽章)                          │
│  ├── 升级公会 (消耗资金/资源)                                │
│  └── 退出公会 (普通成员直接退出，官员需会长批准)               │
│                                                             │
│  成员管理:                                                   │
│  ├── 邀请成员 (副会长、官员权限)                             │
│  ├── 申请加入 (玩家发起)                                     │
│  ├── 批准/拒绝申请 (副会长、官员权限)                         │
│  ├── 踢出成员 (副会长、官员权限)                             │
│  ├── 转让会长 (仅会长)                                      │
│  └── 设置职位 (会长、副会长权限)                             │
│                                                             │
│  公会功能:                                                   │
│  ├── 公会聊天                                              │
│  ├── 公会仓库                                              │
│  ├── 公会技能                                              │
│  ├── 公会福利                                              │
│  ├── 公会活动                                              │
│  ├── 公会战                                                │
│  └── 公会领地                                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 公会职位体系

```
┌─────────────────────────────────────────────────────────────┐
│                    公会职位体系                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  会长 (Guild Master)                                        │
│  ┌─────────────────────────────────────────────────┐       │
│  │  权限:                                           │       │
│  │  - 解散公会                                       │       │
│  │  - 转让会长                                       │       │
│  │  - 任命/罢免官员                                  │       │
│  │  - 修改公会信息                                   │       │
│  │  - 使用公会资金                                   │       │
│  │  - 发动公会战                                     │       │
│  │  - 踢出任何成员                                   │       │
│  │  - 所有副会长权限                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  副会长 (Vice Master)                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  权限:                                           │       │
│  │  - 邀请成员                                       │       │
│  │  - 批准/拒绝申请                                   │       │
│  │  - 踢出成员 (除官员外)                             │       │
│  │  - 修改公会公告                                   │       │
│  │  - 所有官员权限                                   │       │
│  │  人数限制: 2-5人                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  官员 (Officer)                                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  权限:                                           │       │
│  │  - 邀请成员                                       │       │
│  │  - 批准/拒绝申请                                   │       │
│  │  - 发送公会邮件                                   │       │
│  │  人数限制: 5-10人                                  │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  精英成员 (Elite)                                           │
│  ┌─────────────────────────────────────────────────┐       │
│  │  权限:                                           │       │
│  │  - 访问公会仓库高级区域                            │       │
│  │  - 参加公会活动优先权                              │       │
│  │  人数限制: 20-50人                                 │       │
│  └─────────────────────────────────────────────────┘       │
│                          │                                  │
│                          ▼                                  │
│  普通成员 (Member)                                          │
│  ┌─────────────────────────────────────────────────┐       │
│  │  权限:                                           │       │
│  │  - 公会聊天                                       │       │
│  │  - 使用公会福利                                   │       │
│  │  - 参加公会活动                                   │       │
│  │  - 访问公会仓库基础区域                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 数据库表结构

```sql
-- 公会表
CREATE TABLE `guild` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(64) NOT NULL COMMENT '公会名称',
    `master_id` BIGINT UNSIGNED NOT NULL COMMENT '会长ID',
    `master_name` VARCHAR(64) NOT NULL COMMENT '会长名称',
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '公会等级',
    `exp` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '公会经验',
    `money` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '公会资金',
    `member_count` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '成员数量',
    `max_members` SMALLINT UNSIGNED NOT NULL DEFAULT 30 COMMENT '最大成员数',
    `notice` VARCHAR(512) DEFAULT NULL COMMENT '公会公告',
    `emblem` VARCHAR(128) DEFAULT NULL COMMENT '公会徽章',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `last_active_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后活跃时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_name` (`name`),
    KEY `idx_master_id` (`master_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会表';

-- 公会成员表
CREATE TABLE `guild_member` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `guild_id` BIGINT UNSIGNED NOT NULL COMMENT '公会ID',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `player_name` VARCHAR(64) NOT NULL COMMENT '玩家名称',
    `player_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '玩家等级',
    `player_class` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家职业',
    `title` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '职位:0成员,1精英,2官员,3副会长,4会长',
    `contribution` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '个人贡献',
    `join_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
    `last_active_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '最后活跃时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_player_id` (`player_id`),
    KEY `idx_guild_id` (`guild_id`),
    KEY `idx_title` (`title`),
    FOREIGN KEY (`guild_id`) REFERENCES `guild`(`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会成员表';

-- 公会申请表
CREATE TABLE `guild_application` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `guild_id` BIGINT UNSIGNED NOT NULL COMMENT '公会ID',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `player_name` VARCHAR(64) NOT NULL COMMENT '玩家名称',
    `player_level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '玩家等级',
    `apply_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '申请时间',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0待审核,1已批准,2已拒绝',
    PRIMARY KEY (`id`),
    KEY `idx_guild_id` (`guild_id`, `status`),
    KEY `idx_player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会申请表';

-- 公会日志表
CREATE TABLE `guild_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `guild_id` BIGINT UNSIGNED NOT NULL COMMENT '公会ID',
    `log_type` TINYINT UNSIGNED NOT NULL COMMENT '日志类型',
    `operator_id` BIGINT UNSIGNED NOT NULL COMMENT '操作者ID',
    `operator_name` VARCHAR(64) NOT NULL COMMENT '操作者名称',
    `target_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '目标ID',
    `target_name` VARCHAR(64) DEFAULT NULL COMMENT '目标名称',
    `log_data` JSON DEFAULT NULL COMMENT '日志数据',
    `log_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '日志时间',
    PRIMARY KEY (`id`),
    KEY `idx_guild_id` (`guild_id`),
    KEY `idx_log_time` (`log_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会日志表';

-- 公会仓库表
CREATE TABLE `guild_storage` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `guild_id` BIGINT UNSIGNED NOT NULL COMMENT '公会ID',
    `slot_index` SMALLINT UNSIGNED NOT NULL COMMENT '格子位置',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品模板ID',
    `item_count` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '物品数量',
    `item_quality` TINYINT UNSIGNED DEFAULT 0 COMMENT '物品品质',
    `donator_id` BIGINT UNSIGNED NOT NULL COMMENT '捐赠者ID',
    `donate_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '捐赠时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_guild_slot` (`guild_id`, `slot_index`),
    KEY `idx_guild_id` (`guild_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会仓库表';
```

### 2.2 数据结构

```cpp
// 公会数据结构

enum class GuildTitle {
    MEMBER = 0,          // 普通成员
    ELITE = 1,           // 精英成员
    OFFICER = 2,         // 官员
    VICE_MASTER = 3,     // 副会长
    MASTER = 4,          // 会长
};

enum class GuildLogType {
    CREATE = 0,          // 创建公会
    DISBAND = 1,         // 解散公会
    JOIN = 2,            // 加入公会
    LEAVE = 3,           // 离开公会
    KICK = 4,            // 踢出公会
    PROMOTE = 5,         // 升职
    DEMOTE = 6,          // 降职
    MASTER_TRANSFER = 7, // 转让会长
    LEVEL_UP = 8,        // 公会升级
    DONATE = 9,          // 捐赠
    ANNOUNCEMENT = 10,   // 修改公告
};

// 公会信息
struct Guild {
    uint64_t guildId;
    std::string name;
    uint64_t masterId;
    std::string masterName;
    int level = 1;
    uint64_t exp = 0;
    uint64_t money = 0;
    int memberCount = 1;
    int maxMembers = 30;
    std::string notice;
    std::string emblem;
    uint64_t createTime;
    uint64_t lastActiveTime;

    // 公会等级配置
    static constexpr int MAX_LEVEL = 10;
    static constexpr uint64_t LEVEL_EXP[MAX_LEVEL + 1] = {
        0, 1000, 5000, 15000, 40000, 80000,
        150000, 250000, 400000, 600000, 1000000
    };
    static constexpr int MAX_MEMBERS[MAX_LEVEL + 1] = {
        0, 30, 40, 50, 60, 70, 80, 90, 100, 120, 150
    };
};

// 公会成员
struct GuildMember {
    uint64_t memberId;           // 记录ID
    uint64_t guildId;
    uint64_t playerId;
    std::string playerName;
    int playerLevel;
    int playerClass;
    GuildTitle title;
    uint64_t contribution;
    uint64_t joinTime;
    uint64_t lastActiveTime;
};

// 公会申请
struct GuildApplication {
    uint64_t applicationId;
    uint64_t guildId;
    uint64_t playerId;
    std::string playerName;
    int playerLevel;
    uint64_t applyTime;
    int status;  // 0:待审核, 1:已批准, 2:已拒绝
};
```

---

## 三、公会系统实现

### 3.1 公会管理器

```cpp
// 公会系统

class GuildSystem {
public:
    // 创建公会
    uint64_t createGuild(uint64_t playerId, const std::string& name,
                        const std::string& emblem) {
        // 1. 检查玩家条件
        if (!canCreateGuild(playerId)) {
            return 0;
        }

        // 2. 检查名称是否合法
        if (!isValidGuildName(name)) {
            sendError(playerId, "Invalid guild name");
            return 0;
        }

        // 3. 检查名称是否已存在
        if (getGuildByName(name) != nullptr) {
            sendError(playerId, "Guild name already exists");
            return 0;
        }

        // 4. 扣除创建费用
        uint64_t createCost = getCreateCost();
        if (!chargeGold(playerId, createCost)) {
            sendError(playerId, "Not enough gold");
            return 0;
        }

        // 5. 创建公会
        uint64_t guildId = createGuildRecord(playerId, name, emblem);
        if (guildId == 0) {
            refundGold(playerId, createCost);
            return 0;
        }

        // 6. 创建会长记录
        addMember(guildId, playerId, GuildTitle::MASTER);

        // 7. 记录日志
        addGuildLog(guildId, GuildLogType::CREATE, playerId, getPlayerName(playerId));

        // 8. 加载公会数据
        loadGuild(guildId);

        // 9. 通知客户端
        notifyGuildCreated(playerId, guildId, name);

        return guildId;
    }

    // 解散公会
    bool disbandGuild(uint64_t playerId) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return false;

        // 检查是否是会长
        if (guild->masterId != playerId) {
            sendError(playerId, "Only guild master can disband");
            return false;
        }

        // 检查是否有其他成员
        if (guild->memberCount > 1) {
            sendError(playerId, "Cannot disband with active members");
            return false;
        }

        // 移除所有成员
        removeAllMembers(guild->guildId);

        // 删除公会
        database_->execute(fmt::format(
            "DELETE FROM guild WHERE id = {}", guild->guildId
        ));

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::DISBAND, playerId, getPlayerName(playerId));

        // 通知所有成员
        broadcastToGuild(guild->guildId, "onGuildDisbanded", guild->name);

        // 卸载公会
        unloadGuild(guild->guildId);

        return true;
    }

    // 邀请成员
    bool inviteMember(uint64_t inviterId, uint64_t targetId) {
        Guild* guild = getPlayerGuild(inviterId);
        if (!guild) return false;

        // 检查邀请者权限
        GuildMember* inviter = getMember(guild->guildId, inviterId);
        if (!inviter || !canInvite(inviter->title)) {
            sendError(inviterId, "No permission to invite");
            return false;
        }

        // 检查目标是否已有公会
        if (getPlayerGuild(targetId) != nullptr) {
            sendError(inviterId, "Target already has a guild");
            return false;
        }

        // 检查公会人数
        if (guild->memberCount >= guild->maxMembers) {
            sendError(inviterId, "Guild is full");
            return false;
        }

        // 发送邀请
        sendToClient(targetId, "onGuildInvite",
                   guild->guildId, guild->name, inviterId, getPlayerName(inviterId));

        return true;
    }

    // 接受邀请
    bool acceptInvite(uint64_t playerId, uint64_t guildId) {
        // 检查玩家是否已有公会
        if (getPlayerGuild(playerId) != nullptr) {
            return false;
        }

        // 检查公会是否存在
        Guild* guild = getGuild(guildId);
        if (!guild) return false;

        // 检查公会人数
        if (guild->memberCount >= guild->maxMembers) {
            sendError(playerId, "Guild is full");
            return false;
        }

        // 添加成员
        return addMember(guildId, playerId, GuildTitle::MEMBER);
    }

    // 申请加入
    bool applyJoin(uint64_t playerId, uint64_t guildId) {
        // 检查玩家是否已有公会
        if (getPlayerGuild(playerId) != nullptr) {
            sendError(playerId, "You already have a guild");
            return false;
        }

        // 检查是否已有申请
        if (hasPendingApplication(playerId, guildId)) {
            sendError(playerId, "Application already pending");
            return false;
        }

        // 创建申请
        database_->execute(fmt::format(
            "INSERT INTO guild_application "
            "(guild_id, player_id, player_name, player_level) "
            "VALUES ({}, {}, '{}', {})",
            guildId, playerId, getPlayerName(playerId), getPlayerLevel(playerId)
        ));

        // 通知公会管理人员
        notifyGuildOfficers(guildId, playerId, getPlayerName(playerId));

        return true;
    }

    // 批准申请
    bool approveApplication(uint64_t officerId, uint64_t applicationId) {
        Guild* guild = getPlayerGuild(officerId);
        if (!guild) return false;

        // 检查权限
        GuildMember* officer = getMember(guild->guildId, officerId);
        if (!officer || !canApprove(officer->title)) {
            return false;
        }

        // 获取申请
        auto application = getApplication(applicationId);
        if (!application || application->guildId != guild->guildId) {
            return false;
        }

        // 检查公会人数
        if (guild->memberCount >= guild->maxMembers) {
            sendError(officerId, "Guild is full");
            return false;
        }

        // 添加成员
        if (!addMember(guild->guildId, application->playerId, GuildTitle::MEMBER)) {
            return false;
        }

        // 更新申请状态
        database_->execute(fmt::format(
            "UPDATE guild_application SET status = 1 WHERE id = {}",
            applicationId
        ));

        // 通知玩家
        sendToClient(application->playerId, "onGuildJoinApproved",
                   guild->guildId, guild->name);

        return true;
    }

    // 拒绝申请
    bool rejectApplication(uint64_t officerId, uint64_t applicationId) {
        Guild* guild = getPlayerGuild(officerId);
        if (!guild) return false;

        // 检查权限
        GuildMember* officer = getMember(guild->guildId, officerId);
        if (!officer || !canApprove(officer->title)) {
            return false;
        }

        // 获取申请
        auto application = getApplication(applicationId);
        if (!application || application->guildId != guild->guildId) {
            return false;
        }

        // 更新申请状态
        database_->execute(fmt::format(
            "UPDATE guild_application SET status = 2 WHERE id = {}",
            applicationId
        ));

        // 通知玩家
        sendToClient(application->playerId, "onGuildJoinRejected",
                   guild->name);

        return true;
    }

    // 踢出成员
    bool kickMember(uint64_t operatorId, uint64_t targetId) {
        Guild* guild = getPlayerGuild(operatorId);
        if (!guild) return false;

        // 检查权限
        GuildMember* operatorMember = getMember(guild->guildId, operatorId);
        if (!operatorMember) return false;

        GuildMember* targetMember = getMember(guild->guildId, targetId);
        if (!targetMember) return false;

        // 检查是否有权限踢出
        if (!canKick(operatorMember->title, targetMember->title)) {
            sendError(operatorId, "No permission to kick this member");
            return false;
        }

        // 移除成员
        removeMember(guild->guildId, targetId);

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::KICK,
                   operatorId, getPlayerName(operatorId),
                   targetId, getPlayerName(targetId));

        // 通知
        sendToClient(targetId, "onGuildKick", guild->name);
        broadcastToGuild(guild->guildId, "onMemberKicked",
                        targetId, getPlayerName(targetId));

        return true;
    }

    // 离开公会
    bool leaveGuild(uint64_t playerId) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return false;

        // 会长不能直接退出
        if (guild->masterId == playerId) {
            sendError(playerId, "Guild master cannot leave");
            return false;
        }

        // 移除成员
        GuildMember* member = getMember(guild->guildId, playerId);
        removeMember(guild->guildId, playerId);

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::LEAVE,
                   playerId, getPlayerName(playerId));

        // 通知
        sendToClient(playerId, "onGuildLeave", guild->guildId);
        broadcastToGuild(guild->guildId, "onMemberLeave",
                        playerId, getPlayerName(playerId));

        return true;
    }

    // 转让会长
    bool transferMaster(uint64_t currentMasterId, uint64_t newMasterId) {
        Guild* guild = getPlayerGuild(currentMasterId);
        if (!guild) return false;

        // 检查是否是会长
        if (guild->masterId != currentMasterId) {
            return false;
        }

        // 检查目标是否是成员
        GuildMember* newMaster = getMember(guild->guildId, newMasterId);
        if (!newMaster) {
            sendError(currentMasterId, "Target is not a guild member");
            return false;
        }

        // 转让会长
        GuildMember* oldMaster = getMember(guild->guildId, currentMasterId);

        // 更新数据库
        database_->execute(fmt::format(
            "UPDATE guild SET master_id = {}, master_name = '{}' WHERE id = {}",
            newMasterId, getPlayerName(newMasterId), guild->guildId
        ));

        database_->execute(fmt::format(
            "UPDATE guild_member SET title = 4 WHERE player_id = {}",
            newMasterId
        ));

        database_->execute(fmt::format(
            "UPDATE guild_member SET title = 3 WHERE player_id = {}",
            currentMasterId
        ));

        // 更新内存
        guild->masterId = newMasterId;
        guild->masterName = getPlayerName(newMasterId);
        newMaster->title = GuildTitle::MASTER;
        oldMaster->title = GuildTitle::VICE_MASTER;

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::MASTER_TRANSFER,
                   currentMasterId, getPlayerName(currentMasterId),
                   newMasterId, getPlayerName(newMasterId));

        // 通知
        broadcastToGuild(guild->guildId, "onMasterTransfer",
                        currentMasterId, newMasterId);

        return true;
    }

    // 捐赠
    bool donate(uint64_t playerId, uint64_t amount) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return false;

        // 检查玩家金币
        if (!chargeGold(playerId, amount)) {
            return false;
        }

        // 增加公会资金
        guild->money += amount;

        // 增加个人贡献
        GuildMember* member = getMember(guild->guildId, playerId);
        if (member) {
            member->contribution += amount;
        }

        // 更新数据库
        database_->execute(fmt::format(
            "UPDATE guild SET money = money + {} WHERE id = {}",
            amount, guild->guildId
        ));

        database_->execute(fmt::format(
            "UPDATE guild_member SET contribution = contribution + {} "
            "WHERE player_id = {}",
            amount, playerId
        ));

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::DONATE,
                   playerId, getPlayerName(playerId), 0, "", amount);

        // 通知
        sendToClient(playerId, "onDonateSuccess", amount);
        broadcastToGuild(guild->guildId, "onMemberDonate",
                        playerId, amount);

        return true;
    }

    // 获取公会成员列表
    std::vector<GuildMember> getMemberList(uint64_t guildId) {
        std::vector<GuildMember> result;

        auto rows = database_->query(fmt::format(
            "SELECT * FROM guild_member WHERE guild_id = {} ORDER BY title DESC, contribution DESC",
            guildId
        ));

        for (const auto& row : rows) {
            result.push_back(parseMemberRow(row));
        }

        return result;
    }

    // 发送公会消息
    void sendGuildChat(uint64_t playerId, const std::string& message) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return;

        // 更新最后活跃时间
        guild->lastActiveTime = getCurrentTime();

        // 广播给所有在线成员
        broadcastToGuild(guild->guildId, "onGuildChat",
                        playerId, getPlayerName(playerId), message);
    }

    // 修改公会公告
    bool setAnnouncement(uint64_t playerId, const std::string& announcement) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return false;

        // 检查权限
        GuildMember* member = getMember(guild->guildId, playerId);
        if (!member || member->title < GuildTitle::VICE_MASTER) {
            return false;
        }

        // 更新公告
        guild->notice = announcement;

        database_->execute(fmt::format(
            "UPDATE guild SET notice = '{}' WHERE id = {}",
            escapeSql(announcement), guild->guildId
        ));

        // 记录日志
        addGuildLog(guild->guildId, GuildLogType::ANNOUNCEMENT,
                   playerId, getPlayerName(playerId));

        // 通知所有成员
        broadcastToGuild(guild->guildId, "onAnnouncementChanged", announcement);

        return true;
    }

private:
    bool canCreateGuild(uint64_t playerId) {
        // 检查等级
        if (getPlayerLevel(playerId) < 20) {
            return false;
        }

        // 检查是否已有公会
        if (getPlayerGuild(playerId) != nullptr) {
            return false;
        }

        // 检查金币
        if (getPlayerGold(playerId) < getCreateCost()) {
            return false;
        }

        return true;
    }

    uint64_t getCreateCost() const {
        return 1000;  // 1000金币
    }

    uint64_t createGuildRecord(uint64_t playerId, const std::string& name,
                               const std::string& emblem) {
        database_->execute(fmt::format(
            "INSERT INTO guild (name, master_id, master_name, emblem) "
            "VALUES ('{}', {}, '{}', '{}')",
            escapeSql(name), playerId, getPlayerName(playerId), emblem
        ));

        return database_->lastInsertId();
    }

    bool addMember(uint64_t guildId, uint64_t playerId, GuildTitle title) {
        // 检查公会人数
        Guild* guild = getGuild(guildId);
        if (!guild || guild->memberCount >= guild->maxMembers) {
            return false;
        }

        // 添加成员记录
        database_->execute(fmt::format(
            "INSERT INTO guild_member "
            "(guild_id, player_id, player_name, player_level, player_class, title) "
            "VALUES ({}, {}, '{}', {}, {}, {})",
            guildId, playerId, getPlayerName(playerId),
            getPlayerLevel(playerId), getPlayerClass(playerId),
            static_cast<int>(title)
        ));

        // 更新公会人数
        guild->memberCount++;

        // 加载成员
        loadMember(guildId, playerId);

        // 记录日志
        addGuildLog(guildId, GuildLogType::JOIN,
                   playerId, getPlayerName(playerId));

        // 通知
        sendToClient(playerId, "onGuildJoin", guildId, guild->name);
        broadcastToGuild(guildId, "onMemberJoin",
                        playerId, getPlayerName(playerId));

        return true;
    }

    void removeMember(uint64_t guildId, uint64_t playerId) {
        database_->execute(fmt::format(
            "DELETE FROM guild_member WHERE guild_id = {} AND player_id = {}",
            guildId, playerId
        ));

        // 更新公会人数
        Guild* guild = getGuild(guildId);
        if (guild) {
            guild->memberCount--;
        }

        // 卸载成员
        unloadMember(guildId, playerId);
    }

    void removeAllMembers(uint64_t guildId) {
        database_->execute(fmt::format(
            "DELETE FROM guild_member WHERE guild_id = {}",
            guildId
        ));
    }

    bool canInvite(GuildTitle title) {
        return title >= GuildTitle::VICE_MASTER;
    }

    bool canApprove(GuildTitle title) {
        return title >= GuildTitle::OFFICER;
    }

    bool canKick(GuildTitle operatorTitle, GuildTitle targetTitle) {
        // 不能踢同级或更高级
        return operatorTitle > targetTitle || operatorTitle == GuildTitle::MASTER;
    }

    void addGuildLog(uint64_t guildId, GuildLogType logType,
                    uint64_t operatorId, const std::string& operatorName,
                    uint64_t targetId, const std::string& targetName,
                    uint64_t extraData) {
        database_->execute(fmt::format(
            "INSERT INTO guild_log "
            "(guild_id, log_type, operator_id, operator_name, target_id, target_name, log_data) "
            "VALUES ({}, {}, '{}', {}, {}, '{}', '{}')",
            guildId, static_cast<int>(logType),
            operatorId, operatorName, targetId, targetName, extraData
        ));
    }

    std::unordered_map<uint64_t, Guild> guilds_;
    std::unordered_map<uint64_t, GuildMember> members_;  // player_id -> member
    std::unordered_map<uint64_t, uint64_t> playerToGuild_;  // player_id -> guild_id
};
```

---

## 四、公会福利系统

### 4.1 公会等级福利

```cpp
// 公会福利系统

class GuildBenefitSystem {
public:
    struct GuildLevelConfig {
        int level;
        uint64_t expRequired;
        int maxMembers;
        int warehouseSlots;
        std::vector<int> skills;        // 解锁技能
        int dailyBonus;                 // 每日福利
    };

    static const GuildLevelConfig LEVEL_CONFIGS[];

    // 每日福利
    void claimDailyBonus(uint64_t playerId) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return;

        // 检查是否已领取
        if (hasClaimedToday(playerId)) {
            sendError(playerId, "Already claimed today");
            return;
        }

        // 获取奖励
        const auto& config = LEVEL_CONFIGS[guild->level];
        int bonus = config.dailyBonus;

        // 发放奖励
        addGold(playerId, bonus);

        // 标记已领取
        setClaimedToday(playerId);

        sendToClient(playerId, "onDailyBonusClaimed", bonus);
    }

    // 公会技能
    void applyGuildSkills(uint64_t playerId) {
        Guild* guild = getPlayerGuild(playerId);
        if (!guild) return;

        for (int level = 1; level <= guild->level; ++level) {
            const auto& config = LEVEL_CONFIGS[level];
            for (int skillId : config.skills) {
                applyGuildSkill(playerId, skillId);
            }
        }
    }
};
```

---

## 五、最佳实践

### 5.1 公会系统设计建议

| 实践 | 说明 |
|------|------|
| **异步处理** | 公会操作异步处理，避免阻塞 |
| **缓存优化** | 公会信息 Redis 缓存 |
| **权限分离** | 清晰的权限等级体系 |
| **日志记录** | 完整操作日志便于追溯 |
| **防止滥用** | 限制踢人频率等 |

### 5.2 性能优化

```
优化策略:
1. 公会信息 Redis 缓存
2. 成员列表分页加载
3. 公会聊天消息合并
4. 离线玩家延迟通知
5. 定期清理无效数据
```

---

## 六、总结

### 公会系统核心

```
公会系统 = 成员管理 + 权限体系 + 公会福利 + 活动系统
- 分级权限管理
- 公会资金和个人贡献
- 等级解锁更多功能
- 完整日志记录
```

---

## 参考资料

- [魔兽世界公会系统](https://wowpedia.fandom.com/)
- [游戏社交系统设计](https://www.gamedev.net/)
