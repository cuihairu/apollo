# Q32: 如何设计邮件系统？

## 问题分析

本题考察对游戏内邮件系统的理解：
- 邮件系统的核心功能
- 邮件数据结构设计
- 附件处理机制
- 邮件发送与通知

---

## 一、邮件系统需求

### 1.1 核心功能

```
┌─────────────────────────────────────────────────────────────┐
│                    邮件系统核心功能                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  邮件类型:                                                  │
│  ├── 系统邮件 (公告、奖励)                                 │
│  ├── 玩家邮件 (私信、交易)                                 │
│  ├── 战斗邮件 (战报、结算)                                 │
│  └── 活动邮件 (活动通知、奖励)                              │
│                                                             │
│  核心功能:                                                  │
│  ├── 发送邮件（系统/玩家）                                  │
│  ├── 阅读邮件                                               │
│  ├── 领取附件                                               │
│  ├── 删除邮件                                               │
│  └── 邮件通知                                               │
│                                                             │
│  附件系统:                                                  │
│  ├── 金币/钻石                                              │
│  ├── 道具/装备                                              │
│  ├── 礼包/兑换码                                            │
│  └── 过期时间                                               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 业务规则

```
┌─────────────────────────────────────────────────────────────┐
│                    邮件系统业务规则                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  邮件限制:                                                  │
│  ├── 玩家邮件箱上限: 100 封                                 │
│  ├── 已读邮件保留: 7 天                                     │
│  ├── 未读邮件永久保留                                       │
│  ├── 已领取附件的邮件: 3 天                                  │
│  └── 系统邮件自动清理                                       │
│                                                             │
│  发送规则:                                                  │
│  ├── 系统邮件无限制                                         │
│  ├── 玩家邮件限制频率 (每分钟 1 封)                          │
│  ├── 只能发给在线/好友玩家                                   │
│  └── 发送需要消耗资源 (可选)                                │
│                                                             │
│  附件规则:                                                  │
│  ├── 未读邮件无法删除附件                                   │
│  ├── 附件领取后不可退回                                     │
│  ├── 附件过期自动回收                                       │
│  └── 宝箱类附件特殊处理                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 数据库表

```sql
-- 邮件主表
CREATE TABLE `mail` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `mail_id` VARCHAR(64) NOT NULL COMMENT '全局唯一ID',
    `receiver_id` BIGINT UNSIGNED NOT NULL COMMENT '接收者ID',
    `sender_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '发送者ID (0表示系统)',
    `sender_name` VARCHAR(64) NOT NULL COMMENT '发送者名称',
    `type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '类型:0系统,1玩家,2战斗,3活动',
    `title` VARCHAR(128) NOT NULL COMMENT '邮件标题',
    `content` TEXT DEFAULT NULL COMMENT '邮件内容',
    `has_attachment` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '是否有附件',
    `attachment_data` JSON DEFAULT NULL COMMENT '附件数据',
    `gold` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '附件金币',
    `diamond` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '附件钻石',
    `read_time` DATETIME DEFAULT NULL COMMENT '阅读时间',
    `attachment_taken` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '附件是否已领取',
    `attachment_time` DATETIME DEFAULT NULL COMMENT '附件领取时间',
    `expire_time` DATETIME NOT NULL COMMENT '过期时间',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `delete_time` DATETIME DEFAULT NULL COMMENT '删除时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_mail_id` (`mail_id`),
    KEY `idx_receiver_read` (`receiver_id`, `read_time`),
    KEY `idx_receiver_expire` (`receiver_id`, `expire_time`),
    KEY `idx_create_time` (`create_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='邮件表';

-- 邮件附件记录表（用于审计）
CREATE TABLE `mail_attachment_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `mail_id` BIGINT UNSIGNED NOT NULL COMMENT '邮件ID',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品ID',
    `item_count` SMALLINT UNSIGNED NOT NULL COMMENT '物品数量',
    `take_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '领取时间',
    PRIMARY KEY (`id`),
    KEY `idx_mail_id` (`mail_id`),
    KEY `idx_player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='邮件附件记录';
```

### 2.2 Redis 数据结构

```
┌─────────────────────────────────────────────────────────────┐
│                 Redis 邮件系统数据结构                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  未读邮件计数:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: mail:unread_count:{playerId}               │       │
│  │  Type: STRING                                      │       │
│  │  Value: 未读邮件数量                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  邮件列表摘要:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: mail:list:{playerId}                       │       │
│  │  Type: LIST                                       │       │
│  │  Value: 邮件ID 列表 (最新50封)                    │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  邮件内容缓存:                                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Key: mail:detail:{mailId}                       │       │
│  │  Type: HASH                                       │       │
│  │  Fields:                                         │       │
│  │    - title: 标题                                  │       │
│  │    - sender: 发送者                               │       │
│  │    - content: 内容                                │       │
│  │    - has_attachment: 有附件                        │       │
│  │    - read: 已读标记                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 三、邮件系统实现

### 3.1 邮件服务

```cpp
// 邮件服务

class MailService {
public:
    // 发送系统邮件
    std::string sendSystemMail(uint64_t receiverId,
                              const std::string& title,
                              const std::string& content,
                              const MailAttachment& attachment = {}) {
        Mail mail;
        mail.mailId = generateMailId();
        mail.receiverId = receiverId;
        mail.senderId = 0; // 0 表示系统
        mail.senderName = "系统";
        mail.type = MailType::System;
        mail.title = title;
        mail.content = content;
        mail.hasAttachment = !attachment.isEmpty();
        mail.attachment = attachment;
        mail.expireTime = getCurrentTime() + 30 * 24 * 3600; // 30天

        return sendMail(mail);
    }

    // 发送玩家邮件
    std::string sendPlayerMail(uint64_t senderId, uint64_t receiverId,
                              const std::string& title,
                              const std::string& content,
                              const MailAttachment& attachment = {}) {
        // 检查发送频率
        if (!checkSendLimit(senderId)) {
            sendErrorMessage(senderId, "发送邮件过于频繁");
            return "";
        }

        // 检查接收者邮箱
        if (!checkReceiverMailbox(receiverId)) {
            sendErrorMessage(senderId, "对方邮箱已满");
            return "";
        }

        Mail mail;
        mail.mailId = generateMailId();
        mail.receiverId = receiverId;
        mail.senderId = senderId;
        mail.senderName = getPlayerName(senderId);
        mail.type = MailType::Player;
        mail.title = title;
        mail.content = content;
        mail.hasAttachment = !attachment.isEmpty();
        mail.attachment = attachment;

        // 玩家邮件 7 天过期
        mail.expireTime = getCurrentTime() + 7 * 24 * 3600;

        return sendMail(mail);
    }

    // 获取邮件列表
    std::vector<MailSummary> getMailList(uint64_t playerId) {
        // 先从 Redis 获取
        std::string cacheKey = fmt::format("mail:list:{}", playerId);
        auto cached = redis_.lrange(cacheKey, 0, 49);

        if (!cached.empty()) {
            return getMailSummariesFromCache(cached);
        }

        // Redis 未命中，从数据库获取
        auto result = database_->query(fmt::format(
            "SELECT mail_id, sender_name, type, title, has_attachment, "
            "read_time, create_time FROM mail "
            "WHERE receiver_id = {} AND delete_time IS NULL "
            "ORDER BY create_time DESC LIMIT 50",
            playerId
        ));

        std::vector<MailSummary> summaries;
        for (const auto& row : result) {
            MailSummary summary;
            summary.mailId = row["mail_id"];
            summary.senderName = row["sender_name"];
            summary.type = row["type"];
            summary.title = row["title"];
            summary.hasAttachment = row["has_attachment"];
            summary.isRead = !row["read_time"].is_null();
            summary.createTime = row["create_time"];
            summaries.push_back(summary);
        }

        // 更新缓存
        updateMailListCache(playerId, summaries);

        return summaries;
    }

    // 读取邮件
    MailDetail readMail(uint64_t playerId, const std::string& mailId) {
        // 检查权限
        if (!checkMailOwner(playerId, mailId)) {
            throw std::runtime_error("No permission");
        }

        // 从数据库获取邮件详情
        auto result = database_->query(fmt::format(
            "SELECT * FROM mail WHERE mail_id = '{}'",
            mailId
        ));

        if (result.empty()) {
            throw std::runtime_error("Mail not found");
        }

        MailDetail detail;
        detail.mailId = result[0]["mail_id"];
        detail.senderName = result[0]["sender_name"];
        detail.title = result[0]["title"];
        detail.content = result[0]["content"];
        detail.hasAttachment = result[0]["has_attachment"];
        detail.attachmentTaken = result[0]["attachment_taken"];
        detail.createTime = result[0]["create_time"];

        // 更新已读状态
        if (result[0]["read_time"].is_null()) {
            database_->execute(fmt::format(
                "UPDATE mail SET read_time = NOW() WHERE mail_id = '{}'",
                mailId
            ));

            // 更新未读计数
            decrementUnreadCount(playerId);
        }

        return detail;
    }

    // 领取附件
    bool takeAttachment(uint64_t playerId, const std::string& mailId) {
        // 获取邮件信息
        auto mail = getMail(mailId);
        if (!mail || mail->receiverId != playerId) {
            return false;
        }

        // 检查是否已有附件
        if (!mail->hasAttachment || mail->attachmentTaken) {
            return false;
        }

        // 检查是否已读
        if (mail->read_time.is_null()) {
            return false; // 必须先读邮件
        }

        // 领取附件
        DatabaseTransaction txn(database_);

        // 1. 更新邮件状态
        txn.execute(fmt::format(
            "UPDATE mail SET attachment_taken = 1, attachment_time = NOW() "
            "WHERE mail_id = '{}'",
            mailId
        ));

        // 2. 发放奖励
        if (mail->gold > 0) {
            addGold(playerId, mail->gold);
        }
        if (mail->diamond > 0) {
            addDiamond(playerId, mail->diamond);
        }

        // 3. 发放物品
        if (mail->attachment.hasItems()) {
            for (const auto& item : mail->attachment.items) {
                giveItem(playerId, item.itemId, item.count);

                // 记录附件日志
                txn.execute(fmt::format(
                    "INSERT INTO mail_attachment_log "
                    "(mail_id, player_id, item_id, item_count) "
                    "VALUES ((SELECT id FROM mail WHERE mail_id = '{}'), {}, {}, {})",
                    mailId, playerId, item.itemId, item.count
                ));
            }
        }

        return txn.commit();
    }

    // 删除邮件
    bool deleteMail(uint64_t playerId, const std::string& mailId) {
        // 检查权限和状态
        auto mail = getMail(mailId);
        if (!mail || mail->receiverId != playerId) {
            return false;
        }

        // 已读或有附件的邮件才能删除
        if (mail->read_time.is_null() && mail->hasAttachment) {
            return false;
        }

        // 标记删除
        database_->execute(fmt::format(
            "UPDATE mail SET delete_time = NOW() WHERE mail_id = '{}'",
            mailId
        ));

        // 从缓存移除
        removeFromCache(playerId, mailId);

        return true;
    }

    // 清理过期邮件
    void cleanupExpiredMails() {
        database_->execute(fmt::format(
            "DELETE FROM mail WHERE "
            "expire_time < NOW() OR "
            "(read_time IS NOT NULL AND read_time < DATE_SUB(NOW(), INTERVAL 7 DAY) AND attachment_taken = 1) OR "
            "(delete_time IS NOT NULL AND delete_time < DATE_SUB(NOW(), INTERVAL 3 DAY))"
        ));
    }

private:
    std::string sendMail(const Mail& mail) {
        // 保存到数据库
        database_->execute(fmt::format(
            "INSERT INTO mail "
            "(mail_id, receiver_id, sender_id, sender_name, type, title, content, "
            "has_attachment, attachment_data, gold, diamond, expire_time) "
            "VALUES ('{}', {}, {}, '{}', {}, '{}', '{}', {}, '{}', {}, {}, FROM_UNIXTIME({}))",
            mail.mailId, mail.receiverId, mail.senderId,
            escapeString(mail.senderName), static_cast<int>(mail.type),
            escapeString(mail.title), escapeString(mail.content),
            mail.hasAttachment, mail.attachment.toJson(),
            mail.attachment.gold, mail.attachment.diamond,
            mail.expireTime
        ));

        // 更新未读计数
        incrementUnreadCount(mail.receiverId);

        // 添加到接收者邮件列表缓存
        std::string listKey = fmt::format("mail:list:{}", mail.receiverId);
        redis_.lpush(listKey, mail.mailId);
        redis_.ltrim(listKey, 0, 49); // 只保留最新 50 封

        // 通知接收者
        if (isPlayerOnline(mail.receiverId)) {
            notifyNewMail(mail.receiverId, mail);
        }

        return mail.mailId;
    }

    void incrementUnreadCount(uint64_t playerId) {
        std::string key = fmt::format("mail:unread_count:{}", playerId);
        redis_.incr(key);
        redis_.expire(key, 3600);
    }

    void decrementUnreadCount(uint64_t playerId) {
        std::string key = fmt::format("mail:unread_count:{}", playerId);
        long count = redis_.incrby(key, -1);
        if (count <= 0) {
            redis_.del(key);
        }
    }

    std::string generateMailId() {
        return fmt::format("{}{}",
            getCurrentTime(), // 时间戳
            generateRandomString(8)); // 随机字符串
    }

    struct Mail {
        std::string mailId;
        uint64_t receiverId;
        uint64_t senderId;
        std::string senderName;
        MailType type;
        std::string title;
        std::string content;
        bool hasAttachment;
        MailAttachment attachment;
        int gold = 0;
        int diamond = 0;
        uint64_t expireTime;
    };

    Database* database_;
    RedisClient redis_;
};

// 邮件附件
struct MailAttachment {
    int gold = 0;
    int diamond = 0;
    std::vector<ItemData> items;

    bool isEmpty() const {
        return gold == 0 && diamond == 0 && items.empty();
    }

    std::string toJson() const {
        json j;
        j["gold"] = gold;
        j["diamond"] = diamond;
        j["items"] = json::array();
        for (const auto& item : items) {
            json ij;
            ij["item_id"] = item.itemId;
            ij["count"] = item.count;
            j["items"].push_back(ij);
        }
        return j.dump();
    }
};
```

---

## 四、最佳实践

### 4.1 邮件系统优化

```
┌─────────────────────────────────────────────────────────────┐
│                  邮件系统优化建议                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 性能优化                                                │
│     ├── 邮件列表只返回摘要                                 │
│     ├── 详细内容按需加载                                   │
│     ├── 使用 Redis 缓存未读计数                             │
│     └── 批量删除过期邮件                                   │
│                                                             │
│  2. 附件处理                                                │
│     ├── 附件数据 JSON 存储                                 │
│     ├── 领取时使用事务                                     │
│     ├── 记录附件领取日志                                   │
│     └── 过期附件自动回收                                   │
│                                                             │
│  3. 通知机制                                                │
│     ├── 使用 WebSocket 推送                               │
│     ├── 离线邮件上线通知                                   │
│     ├── 重要邮件强提醒                                     │
│     └── 防止通知骚扰                                       │
│                                                             │
│  4. 安全控制                                                │
│     ├── 验证邮件所有权                                     │
│     ├── 防止刷邮件                                          │
│     ├── 附件发放二次验证                                   │
│     └── 敏感操作记录日志                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 五、总结

### 邮件系统功能总结

| 功能 | 实现要点 |
|------|----------|
| **发送邮件** | 系统邮件无限制、玩家邮件限频 |
| **阅读邮件** | 标记已读、更新未读计数 |
| **附件领取** | 先读后领、事务保证 |
| **邮件清理** | 定时清理过期邮件 |
| **通知推送** | 实时通知、离线缓存 |

### 数据存储策略

```
邮件系统存储:
- MySQL: 邮件持久化、附件记录
- Redis: 未读计数、邮件列表缓存
- 混合使用平衡性能和一致性
```

---

## 参考资料

- [游戏邮件系统设计](https://www.gamedev.net/)
- [Redis 邮件队列实现](https://redis.io/docs/use-cases/)
- [KBEngine 邮件系统](https://github.com/kbengine/kbengine)
