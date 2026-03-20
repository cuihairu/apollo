# Q27: 如何设计数据库表结构？

## 问题分析

本题考察对数据库设计的理解：
- MMO 数据库设计原则
- 玩家数据表设计
- 游戏数据表设计
- KBEngine 的数据库结构

---

## 一、MMO 数据库设计原则

### 1.1 设计原则

```
┌─────────────────────────────────────────────────────────────┐
│                  MMO 数据库设计原则                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 读写分离                                                │
│     ├── 读操作远多于写操作                                   │
│     ├── 考虑主从复制                                         │
│     └── 统计查询使用从库                                    │
│                                                             │
│  2. 分表分库                                                │
│     ├── 玩家数据按 ID 分表                                   │
│     ├── 日志数据按时间分表                                   │
│     └── 大表考虑水平拆分                                     │
│                                                             │
│  3. 数据完整性                                              │
│     ├── 关键数据使用事务                                     │
│     ├── 设置合理的约束                                       │
│     └── 定期校验数据一致性                                   │
│                                                             │
│  4. 性能优先                                                │
│     ├── 避免大表关联                                        │
│     ├── 合理使用索引                                         │
│     ├── 冷热数据分离                                         │
│     └── 非关键数据异步处理                                   │
│                                                             │
│  5. 扩展性                                                  │
│     ├── 预留扩展字段                                         │
│     ├── 考虑分片策略                                         │
│     └── 避免跨库事务                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、玩家数据表设计

### 2.1 核心玩家表

```sql
-- 玩家账号表
CREATE TABLE `account` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '账号ID',
    `account_name` VARCHAR(64) NOT NULL COMMENT '账号名',
    `password` VARCHAR(128) NOT NULL COMMENT '密码(哈希)',
    `salt` VARCHAR(32) NOT NULL COMMENT '盐值',
    `status` TINYINT NOT NULL DEFAULT 0 COMMENT '状态:0正常,1封禁',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `last_login_time` DATETIME DEFAULT NULL COMMENT '最后登录时间',
    `last_login_ip` VARCHAR(64) DEFAULT NULL COMMENT '最后登录IP',
    `ban_until` DATETIME DEFAULT NULL COMMENT '封禁到期时间',
    `ban_reason` VARCHAR(256) DEFAULT NULL COMMENT '封禁原因',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_account_name` (`account_name`),
    KEY `idx_last_login` (`last_login_time`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='账号表';

-- 玩家角色表
CREATE TABLE `player` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '玩家ID',
    `account_id` BIGINT UNSIGNED NOT NULL COMMENT '账号ID',
    `name` VARCHAR(64) NOT NULL COMMENT '角色名',
    `level` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '等级',
    `exp` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '经验值',
    `vocation` TINYINT UNSIGNED NOT NULL COMMENT '职业:1战士,2法师,3射手',
    `gender` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '性别:1男,2女',
    `map_id` INT UNSIGNED NOT NULL DEFAULT 1 COMMENT '当前地图ID',
    `position_x` FLOAT NOT NULL DEFAULT 0 COMMENT 'X坐标',
    `position_y` FLOAT NOT NULL DEFAULT 0 COMMENT 'Y坐标',
    `position_z` FLOAT NOT NULL DEFAULT 0 COMMENT 'Z坐标',
    `direction` FLOAT NOT NULL DEFAULT 0 COMMENT '朝向',
    `hp` INT UNSIGNED NOT NULL DEFAULT 100 COMMENT '当前血量',
    `mp` INT UNSIGNED NOT NULL DEFAULT 100 COMMENT '当前魔法',
    `max_hp` INT UNSIGNED NOT NULL DEFAULT 100 COMMENT '最大血量',
    `max_mp` INT UNSIGNED NOT NULL DEFAULT 100 COMMENT '最大魔法',
    `gold` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '金币',
    `diamond` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '钻石',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `delete_time` DATETIME DEFAULT NULL COMMENT '删除时间',
    `total_online_time` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '总在线时长(秒)',
    `last_logout_time` DATETIME DEFAULT NULL COMMENT '最后下线时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_name` (`name`),
    KEY `idx_account_id` (`account_id`),
    KEY `idx_level` (`level`),
    KEY `idx_delete_time` (`delete_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家角色表';

-- 玩家属性表
CREATE TABLE `player_property` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `strength` SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '力量',
    `agility` SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '敏捷',
    `intelligence` SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '智力',
    `vitality` SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '体力',
    `luck` SMALLINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '幸运',
    `attack_power` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '攻击力',
    `defense_power` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '防御力',
    `critical_rate` SMALLINT UNSIGNED NOT NULL DEFAULT 5 COMMENT '暴击率(%)',
    `critical_damage` SMALLINT UNSIGNED NOT NULL DEFAULT 150 COMMENT '暴击伤害(%)',
    `speed` SMALLINT UNSIGNED NOT NULL DEFAULT 100 COMMENT '移动速度',
    PRIMARY KEY (`player_id`),
    CONSTRAINT `fk_player_property_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家属性表';
```

### 2.2 背包物品表

```sql
-- 物品定义表
CREATE TABLE `item_template` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '物品ID',
    `name` VARCHAR(128) NOT NULL COMMENT '物品名称',
    `type` TINYINT UNSIGNED NOT NULL COMMENT '类型:1消耗品,2装备,3材料',
    `sub_type` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '子类型',
    `quality` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '品质:1白,2绿,3蓝,4紫,5橙',
    `stack_size` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '堆叠上限',
    `sell_price` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '出售价格',
    `is_tradeable` BOOLEAN NOT NULL DEFAULT TRUE COMMENT '是否可交易',
    `is_dropable` BOOLEAN NOT NULL DEFAULT TRUE COMMENT '是否可丢弃',
    `effects` JSON DEFAULT NULL COMMENT '物品效果(JSON)',
    `description` VARCHAR(512) DEFAULT NULL COMMENT '描述',
    PRIMARY KEY (`id`),
    KEY `idx_type` (`type`),
    KEY `idx_quality` (`quality`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='物品定义表';

-- 玩家背包表
CREATE TABLE `player_bag` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `bag_index` SMALLINT UNSIGNED NOT NULL COMMENT '背包位置(0-主背包,100-扩展)',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品ID',
    `count` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '数量',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '获得时间',
    `expire_time` DATETIME DEFAULT NULL COMMENT '过期时间',
    `binding_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '绑定:0无,1拾取绑定,2使用绑定',
    PRIMARY KEY (`player_id`, `bag_index`),
    KEY `idx_player_id` (`player_id`),
    CONSTRAINT `fk_player_bag_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE,
    CONSTRAINT `fk_player_bag_item` FOREIGN KEY (`item_id`)
        REFERENCES `item_template` (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家背包表';

-- 装备表
CREATE TABLE `player_equipment` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `equip_slot` TINYINT UNSIGNED NOT NULL COMMENT '装备槽位:1武器,2头盔,3衣服...',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品ID',
    `strength` SMALLINT SIGNED NOT NULL DEFAULT 0 COMMENT '强化等级',
    `star_level` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '星级',
    `enchant_data` JSON DEFAULT NULL COMMENT '附魔数据',
    `gem_slots` JSON DEFAULT NULL COMMENT '宝石槽',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '装备时间',
    PRIMARY KEY (`player_id`, `equip_slot`),
    KEY `idx_player_id` (`player_id`),
    CONSTRAINT `fk_player_equipment_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家装备表';
```

---

## 三、游戏数据表设计

### 3.1 社交系统表

```sql
-- 好友关系表
CREATE TABLE `player_friend` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `friend_id` BIGINT UNSIGNED NOT NULL COMMENT '好友ID',
    `group_name` VARCHAR(32) NOT NULL DEFAULT '默认分组' COMMENT '分组名称',
    `remark` VARCHAR(64) DEFAULT NULL COMMENT '备注名',
    `intimacy` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '亲密度',
    `add_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '添加时间',
    `blacklist` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '是否在黑名单',
    PRIMARY KEY (`player_id`, `friend_id`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_friend_id` (`friend_id`),
    CONSTRAINT `fk_player_friend_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='好友关系表';

-- 公会表
CREATE TABLE `guild` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '公会ID',
    `name` VARCHAR(64) NOT NULL COMMENT '公会名称',
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '公会等级',
    `exp` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '公会经验',
    `leader_id` BIGINT UNSIGNED NOT NULL COMMENT '会长ID',
    `notice` VARCHAR(512) DEFAULT NULL COMMENT '公会公告',
    `member_count` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '成员数量',
    `max_members` SMALLINT UNSIGNED NOT NULL DEFAULT 50 COMMENT '最大成员数',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `disband_time` DATETIME DEFAULT NULL COMMENT '解散时间',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_name` (`name`),
    KEY `idx_leader_id` (`leader_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会表';

-- 公会成员表
CREATE TABLE `guild_member` (
    `guild_id` BIGINT UNSIGNED NOT NULL COMMENT '公会ID',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `position` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '职位:1成员,2长老,3副会长,4会长',
    `contribution` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '贡献度',
    `join_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '加入时间',
    `last_login_time` DATETIME DEFAULT NULL COMMENT '最后登录时间',
    PRIMARY KEY (`guild_id`, `player_id`),
    KEY `idx_player_id` (`player_id`),
    CONSTRAINT `fk_guild_member_guild` FOREIGN KEY (`guild_id`)
        REFERENCES `guild` (`id`) ON DELETE CASCADE,
    CONSTRAINT `fk_guild_member_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='公会成员表';
```

### 3.2 任务系统表

```sql
-- 任务定义表
CREATE TABLE `quest_template` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '任务ID',
    `name` VARCHAR(128) NOT NULL COMMENT '任务名称',
    `type` TINYINT UNSIGNED NOT NULL COMMENT '类型:1主线,2支线,3日常,4循环',
    `level_req` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '等级要求',
    `prev_quest_id` INT UNSIGNED DEFAULT NULL COMMENT '前置任务ID',
    `next_quest_id` INT UNSIGNED DEFAULT NULL COMMENT '后续任务ID',
    `rewards` JSON DEFAULT NULL COMMENT '奖励数据',
    `objectives` JSON NOT NULL COMMENT '目标数据',
    `auto_accept` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '是否自动接取',
    `auto_complete` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '是否自动完成',
    PRIMARY KEY (`id`),
    KEY `idx_type` (`type`),
    KEY `idx_prev_quest` (`prev_quest_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='任务定义表';

-- 玩家任务表
CREATE TABLE `player_quest` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `quest_id` INT UNSIGNED NOT NULL COMMENT '任务ID',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '状态:0未接取,1进行中,2已完成,3已提交',
    `accept_time` DATETIME DEFAULT NULL COMMENT '接取时间',
    `complete_time` DATETIME DEFAULT NULL COMMENT '完成时间',
    `progress` JSON DEFAULT NULL COMMENT '任务进度',
    `choice_reward` INT UNSIGNED DEFAULT NULL COMMENT '选择的奖励',
    PRIMARY KEY (`player_id`, `quest_id`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_status` (`status`),
    CONSTRAINT `fk_player_quest_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家任务表';
```

### 3.3 战斗数据表

```sql
-- 技能定义表
CREATE TABLE `skill_template` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '技能ID',
    `name` VARCHAR(64) NOT NULL COMMENT '技能名称',
    `vocation` TINYINT UNSIGNED NOT NULL COMMENT '职业',
    `level_req` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '学习等级',
    `max_level` TINYINT UNSIGNED NOT NULL DEFAULT 10 COMMENT '最大等级',
    `cast_time` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '施法时间(ms)',
    `cooldown` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '冷却时间(ms)',
    `mana_cost` SMALLINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '魔法消耗',
    `range` FLOAT UNSIGNED NOT NULL DEFAULT 0 COMMENT '施法距离',
    `effects` JSON NOT NULL COMMENT '技能效果',
    `icon` VARCHAR(128) DEFAULT NULL COMMENT '图标',
    PRIMARY KEY (`id`),
    KEY `idx_vocation` (`vocation`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='技能定义表';

-- 玩家技能表
CREATE TABLE `player_skill` (
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `skill_id` INT UNSIGNED NOT NULL COMMENT '技能ID',
    `level` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '技能等级',
    `exp` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '技能经验',
    `shortcut_slot` TINYINT UNSIGNED DEFAULT NULL COMMENT '快捷栏位置',
    `learn_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '学习时间',
    PRIMARY KEY (`player_id`, `skill_id`),
    KEY `idx_player_id` (`player_id`),
    CONSTRAINT `fk_player_skill_player` FOREIGN KEY (`player_id`)
        REFERENCES `player` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家技能表';

-- 战斗日志表（分区表）
CREATE TABLE `combat_log` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `server_id` TINYINT UNSIGNED NOT NULL COMMENT '服务器ID',
    `combat_id` VARCHAR(64) NOT NULL COMMENT '战斗ID',
    `combat_type` TINYINT UNSIGNED NOT NULL COMMENT '战斗类型',
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `result` TINYINT UNSIGNED NOT NULL COMMENT '结果:0胜利,1失败,2平局',
    `duration` INT UNSIGNED NOT NULL COMMENT '战斗时长(秒)',
    `damage_dealt` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '造成伤害',
    `damage_taken` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '承受伤害',
    `participants` JSON DEFAULT NULL COMMENT '参与者数据',
    `rewards` JSON DEFAULT NULL COMMENT '战斗奖励',
    `combat_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '战斗时间',
    PRIMARY KEY (`id`, `combat_time`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_combat_time` (`combat_time`),
    KEY `idx_server_id` (`server_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='战斗日志表'
PARTITION BY RANGE (TO_DAYS(combat_time)) (
    PARTITION p202401 VALUES LESS THAN (TO_DAYS('2024-02-01')),
    PARTITION p202402 VALUES LESS THAN (TO_DAYS('2024-03-01')),
    PARTITION p202403 VALUES LESS THAN (TO_DAYS('2024-04-01')),
    PARTITION pmax VALUES LESS THAN MAXVALUE
);
```

---

## 四、KBEngine 数据库结构

### 4.1 KBEngine 表结构

```sql
-- KBEngine 默认表结构
-- 参考自: https://github.com/kbengine/kbengine/tree/master/kbe/res/db_tables

-- 账号表
CREATE TABLE `kb_accounts` (
    `accountName` VARCHAR(255) NOT NULL COMMENT '账号名',
    `password` VARCHAR(255) NOT NULL COMMENT '密码',
    `entityID` BIGINT DEFAULT NULL COMMENT '关联的实体ID',
    `logins` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '登录次数',
    `lastLoginTime` DATETIME DEFAULT NULL COMMENT '最后登录时间',
    `flags` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '标志位',
    PRIMARY KEY (`accountName`),
    UNIQUE KEY `uk_entityID` (`entityID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 实体表 (所有实体共享)
CREATE TABLE `kb_entity` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT '实体ID',
    `type` VARCHAR(64) NOT NULL COMMENT '实体类型',
    `positionX` FLOAT NOT NULL DEFAULT 0 COMMENT 'X坐标',
    `positionY` FLOAT NOT NULL DEFAULT 0 COMMENT 'Y坐标',
    `positionZ` FLOAT NOT NULL DEFAULT 0 COMMENT 'Z坐标',
    `direction` FLOAT NOT NULL DEFAULT 0 COMMENT '朝向',
    `cell` VARCHAR(64) DEFAULT NULL COMMENT '所属CellApp',
    `data` BLOB DEFAULT NULL COMMENT '实体数据(序列化)',
    `createTime` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    `updateTime` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    PRIMARY KEY (`id`),
    KEY `idx_type` (`type`),
    KEY `idx_cell` (`cell`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 玩家邮件表
CREATE TABLE `kb_mails` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `playerID` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `title` VARCHAR(256) NOT NULL COMMENT '邮件标题',
    `content` TEXT DEFAULT NULL COMMENT '邮件内容',
    `senderID` BIGINT UNSIGNED DEFAULT NULL COMMENT '发送者ID',
    `senderName` VARCHAR(64) DEFAULT NULL COMMENT '发送者名称',
    `attachment` JSON DEFAULT NULL COMMENT '附件数据',
    `gold` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '附件金币',
    `readTime` DATETIME DEFAULT NULL COMMENT '阅读时间',
    `attachmentTime` DATETIME DEFAULT NULL COMMENT '附件领取时间',
    `expireTime` DATETIME NOT NULL COMMENT '过期时间',
    `createTime` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    PRIMARY KEY (`id`),
    KEY `idx_playerID` (`playerID`),
    KEY `idx_expireTime` (`expireTime`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 交易记录表
CREATE TABLE `kb_trade_logs` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `tradeID` VARCHAR(64) NOT NULL COMMENT '交易ID',
    `playerA` BIGINT UNSIGNED NOT NULL COMMENT '玩家A',
    `playerB` BIGINT UNSIGNED NOT NULL COMMENT '玩家B',
    `itemsA` JSON DEFAULT NULL COMMENT '玩家A物品',
    `itemsB` JSON DEFAULT NULL COMMENT '玩家B物品',
    `goldA` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家A金币',
    `goldB` BIGINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '玩家B金币',
    `status` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '状态',
    `createTime` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '交易时间',
    PRIMARY KEY (`id`),
    KEY `idx_playerA` (`playerA`),
    KEY `idx_playerB` (`playerB`),
    KEY `idx_tradeID` (`tradeID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### 4.2 KBEngine 数据持久化

```cpp
// KBEngine 实体数据持久化
// src/server/dbmgr/dbmgr_interface.cpp

namespace KBEngine {

class DBMgr {
public:
    // 保存实体到数据库
    bool saveEntity(Entity* pEntity) {
        // 1. 序列化实体数据
        MemoryStream stream;
        pEntity->addToStream(stream);

        // 2. 构建 SQL
        std::string sql = buildSaveSQL(pEntity, stream);

        // 3. 执行 SQL
        return executeSQL(sql);
    }

    // 从数据库加载实体
    Entity* loadEntity(EntityID id) {
        // 1. 查询数据库
        std::string sql = "SELECT * FROM kb_entity WHERE id = " +
                         std::to_string(id);

        auto result = executeQuery(sql);

        if (result.empty()) {
            return nullptr;
        }

        // 2. 反序列化实体
        Entity* pEntity = EntityFactory::create(result["type"]);
        pEntity->createFromStream(result["data"]);

        return pEntity;
    }

private:
    std::string buildSaveSQL(Entity* pEntity, MemoryStream& stream) {
        std::string data = base64Encode(
            stream.str(),
            stream.size()
        );

        // 使用 UPSERT
        return fmt::format(
            "INSERT INTO kb_entity (id, type, positionX, positionY, "
            "positionZ, direction, data) VALUES ({}, '{}', {}, {}, {}, "
            "{}, '{}') ON DUPLICATE KEY UPDATE "
            "positionX = VALUES(positionX), "
            "positionY = VALUES(positionY), "
            "positionZ = VALUES(positionZ), "
            "direction = VALUES(direction), "
            "data = VALUES(data)",
            pEntity->id(),
            pEntity->type(),
            pEntity->position().x,
            pEntity->position().y,
            pEntity->position().z,
            pEntity->direction().yaw,
            data
        );
    }
};

} // namespace KBEngine
```

---

## 五、分表策略

### 5.1 玩家数据分表

```
┌─────────────────────────────────────────────────────────────┐
│                    玩家数据分表策略                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  策略 1: 按玩家 ID 取模分表                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  table_index = player_id % table_count           │       │
│  │                                                   │       │
│  │  player_0, player_1, ..., player_N              │       │
│  │                                                   │       │
│  │  优点:                                            │       │
│  │  ├── 分布均匀                                       │       │
│  │  ├── 实现简单                                       │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 扩容需要迁移数据                                 │       │
│  │  └── 查询需要知道分表规则                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 2: 按范围分表                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  player_0:  ID 1 - 1000000                      │       │
│  │  player_1:  ID 1000001 - 2000000                │       │
│  │  ...                                              │       │
│  │                                                   │       │
│  │  优点:                                            │       │
│  │  ├── 新玩家集中在最新表                              │       │
│  │  ├── 方便归档旧数据                                  │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 可能分布不均                                     │       │
│  │  └── 热点数据集中                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  策略 3: 一致性哈希分表                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  table_index = hash(player_id) % table_count    │       │
│  │                                                   │       │
│  │  优点:                                            │       │
│  │  ├── 扩容影响最小                                     │       │
│  │  ├── 数据均匀分布                                     │       │
│  │                                                   │       │
│  │  缺点:                                            │       │
│  │  ├── 实现复杂                                         │       │
│  │  └── 需要维护路由表                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 分表实现

```cpp
// 分表路由实现

class TableRouter {
public:
    // 获取玩家数据表名
    static std::string getPlayerTable(uint64_t playerId) {
        size_t tableIndex = playerId % PLAYER_TABLE_COUNT;
        return "player_" + std::to_string(tableIndex);
    }

    // 获取日志表名（按时间）
    static std::string getLogTable(const std::string& date) {
        return "game_log_" + date; // 例如: game_log_20240101
    }

    // 获取战斗日志表名（按月）
    static std::string getCombatLogTable(uint32_t timestamp) {
        time_t t = timestamp;
        struct tm* tm = localtime(&t);
        char buf[32];
        strftime(buf, sizeof(buf), "combat_log_%Y%m", tm);
        return std::string(buf);
    }

private:
    static constexpr size_t PLAYER_TABLE_COUNT = 16;
};

// 使用分表的 DAO
class PlayerDAO {
public:
    Player* loadPlayer(uint64_t playerId) {
        std::string tableName = TableRouter::getPlayerTable(playerId);

        std::string sql = fmt::format(
            "SELECT * FROM {} WHERE id = {}",
            tableName,
            playerId
        );

        return executeQuery(sql);
    }

    bool savePlayer(const Player& player) {
        std::string tableName = TableRouter::getPlayerTable(player.id);

        std::string sql = fmt::format(
            "INSERT INTO {} (...) VALUES (...) "
            "ON DUPLICATE KEY UPDATE ...",
            tableName
        );

        return executeSQL(sql);
    }
};
```

---

## 六、最佳实践

### 6.1 数据库设计建议

```
┌─────────────────────────────────────────────────────────────┐
│                  数据库设计最佳实践                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 命名规范                                                │
│     ├── 表名: 小写，下划线分隔                                │
│     ├── 字段名: 小写，下划线分隔                              │
│     ├── 索引: idx_前缀                                       │
│     ├── 唯一索引: uk_前缀                                     │
│     └── 外键: fk_表名_引用表名                                │
│                                                             │
│  2. 字段类型选择                                            │
│     ├── 整数: 根据范围选择 TINYINT/SMALLINT/INT/BIGINT      │
│     ├── 字符串: VARCHAR(长度) 而非 TEXT                    │
│     ├── 小数: DECIMAL 而非 FLOAT                           │
│     ├── 金额: BIGINT (分为单位)                              │
│     └── 时间: DATETIME 或 TIMESTAMP                         │
│                                                             │
│  3. 索引设计                                                │
│     ├── 主键: 自增 ID                                       │
│     ├── 外键: 添加索引                                       │
│     ├── 查询条件: 添加索引                                   │
│     ├── 联合索引: 考虑查询顺序                               │
│     └── 避免过多索引                                         │
│                                                             │
│  4. 约束设计                                                │
│     ├── 主键约束: 必需                                       │
│     ├── 唯一约束: 业务唯一字段                                │
│     ├── 外键约束: 关键数据                                   │
│     ├── 非空约束: 重要字段                                   │
│     └── 默认值: 合理设置                                     │
│                                                             │
│  5. 数据类型                                                │
│     ├── 状态: 使用 TINYINT 而非字符串                        │
│     ├── 枚举: 使用 TINYINT 或 ENUM                          │
│     ├── JSON: 灵活数据                                       │
│     └── BLOB: 二进制数据                                     │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 表设计检查清单

| 检查项 | 说明 |
|--------|------|
| **主键** | 每个表都有主键 |
| **索引** | 查询字段有索引 |
| **约束** | 必要的字段有非空约束 |
| **默认值** | 合理设置默认值 |
| **注释** | 表和字段有注释 |
| **字符集** | 统一使用 utf8mb4 |
| **引擎** | InnoDB 支持事务 |
| **外键** | 关键数据有外键 |
| **分表** | 大表考虑分表 |
| **归档** | 历史数据有归档策略 |

---

## 七、总结

### 数据库设计要点

| 方面 | 建议 |
|------|------|
| **表结构** | 规范化与性能平衡 |
| **分表** | 按业务需求选择策略 |
| **索引** | 合理添加，避免过多 |
| **约束** | 关键数据必须约束 |
| **扩展** | 预留扩展字段 |

### KBEngine 数据库特点

```
KBEngine 使用:
1. 统一的实体表存储所有实体
2. 序列化 BLOB 存储详细数据
3. 灵活但查询能力有限
4. 需要自行设计业务表
```

---

## 参考资料

- [KBEngine GitHub - 数据库表结构](https://github.com/kbengine/kbengine/tree/master/kbe/res/db_tables)
- [MySQL 分表分库最佳实践](https://dev.mysql.com/doc/)
- [数据库设计规范](https://wiki.postgresql.org/wiki/Don%27t_Do_This)
