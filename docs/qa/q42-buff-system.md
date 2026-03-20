# Q42: 如何设计 Buff/Debuff 系统？

## 问题分析

本题考察对状态效果系统的理解：
- Buff/Debuff 的数据模型
- 效果叠加规则
- 时间管理机制
- KBEngine 的效果系统

---

## 一、Buff/Debuff 概述

### 1.1 定义

```
┌─────────────────────────────────────────────────────────────┐
│                  Buff/Debuff 定义                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Buff (增益效果):                                           │
│  ┌─────────────────────────────────────────────────┐       │
│  │  强化玩家能力的正面效果                             │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  ├── 攻击力 +20%                                   │       │
│  │  ├── 移动速度 +30%                                 │       │
│  │  ├── 生命恢复 5/秒                                   │       │
│  │  ├── 无敌状态                                        │       │
│  │  └── 免疫控制                                        │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  Debuff (减益效果):                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  削弱玩家能力的负面效果                             │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  ├── 移动速度 -30%                                  │       │
│  │  ├── 持续扣血 10/秒                                 │       │
│  │  ├── 无法释放技能                                    │       │
│  │  ├── 混乱                                            │       │
│  │  └── 沉默                                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、数据模型设计

### 2.1 Buff 模板

```sql
-- Buff 模板表
CREATE TABLE `buff_template` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT COMMENT 'Buff ID',
    `name` VARCHAR(64) NOT NULL COMMENT 'Buff 名称',
    `type` TINYINT UNSIGNED NOT NULL COMMENT '类型:0Buff,1Debuff',
    `icon` VARCHAR(128) DEFAULT NULL COMMENT '图标',
    `description` VARCHAR(256) DEFAULT NULL COMMENT '描述',
    `duration` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '持续时间(秒),0表示永久',
    `max_stacks` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '最大叠加层数',
    `effects` JSON NOT NULL COMMENT '效果数据',
    `remove_on_death` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '死亡时移除',
    PRIMARY KEY (`id`),
    KEY `idx_type` (`type`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Buff模板表';
```

### 2.2 玩家 Buff 记录

```sql
-- 玩家 Buff 表
CREATE TABLE `player_buff` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `buff_id` INT UNSIGNED NOT NULL COMMENT 'Buff模板ID',
    `caster_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '施放者ID',
    `start_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '开始时间',
    `end_time` DATETIME DEFAULT NULL COMMENT '结束时间',
    `stack_count` TINYINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '叠加层数',
    `param1` INT DEFAULT NULL COMMENT '参数1',
    `param2` INT DEFAULT NULL COMMENT '参数2',
    `param_str` VARCHAR(256) DEFAULT NULL COMMENT '字符串参数',
    `is_permanent` BOOLEAN NOT NULL DEFAULT FALSE COMMENT '是否永久',
    PRIMARY KEY (`id`),
    KEY `idx_player_buff` (`player_id`, `end_time`),
    KEY `idx_end_time` (`end_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家Buff表';
```

---

## 三、Buff 系统实现

### 3.1 Buff 管理

```cpp
// Buff 系统

class BuffSystem {
public:
    // 添加 Buff
    uint64_t addBuff(uint64_t playerId, int buffId, uint64_t casterId,
                     int duration = 0, int stackCount = 1) {
        // 1. 获取 Buff 模板
        BuffTemplate* template = getBuffTemplate(buffId);
        if (!template) {
            return 0;
        }

        // 2. 检查是否可以添加
        if (!canAddBuff(playerId, template)) {
            return 0;
        }

        // 3. 处理叠加
        uint64_t buffInstanceId = 0;
        if (template->maxStacks > 1) {
            // 可叠加 Buff
            buffInstanceId = addStackableBuff(playerId, buffId, casterId,
                                                 duration, stackCount);
        } else {
            // 不可叠加，刷新时间
            buffInstanceId = refreshBuff(playerId, buffId, casterId, duration);
        }

        // 4. 应用 Buff 效果
        applyBuffEffects(playerId, template);

        // 5. 设置定时器
        if (duration > 0 && !template->isPermanent) {
            scheduleBuffExpire(playerId, buffInstanceId, duration);
        }

        // 6. 通知客户端
        notifyBuffAdded(playerId, buffId);

        return buffInstanceId;
    }

    // 移除 Buff
    bool removeBuff(uint64_t playerId, uint64_t buffInstanceId) {
        // 1. 获取 Buff 信息
        PlayerBuff* buff = getPlayerBuff(playerId, buffInstanceId);
        if (!buff) {
            return false;
        }

        // 2. 移除 Buff 效果
        removeBuffEffects(playerId, buff->buffId);

        // 3. 删除记录
        database_->execute(fmt::format(
            "DELETE FROM player_buff WHERE id = {}",
            buffInstanceId
        ));

        // 4. 通知客户端
        notifyBuffRemoved(playerId, buff->buffId);

        return true;
    }

    // 检查 Buff 冲突
    bool hasConflictingBuff(uint64_t playerId, int buffId) {
        BuffTemplate* template = getBuffTemplate(buffId);

        auto buffs = getActiveBuffs(playerId);
        for (const auto& buff : buffs) {
            // 检查是否互斥
            if (isConflicting(template, buff)) {
                return true;
            }
        }

        return false;
    }

    // 获取所有活跃 Buff
    std::vector<PlayerBuff*> getActiveBuffs(uint64_t playerId) {
        auto results = database_->query(fmt::format(
            "SELECT * FROM player_buff "
            "WHERE player_id = {} AND "
            "(end_time IS NULL OR end_time > NOW()) "
            "ORDER BY start_time ASC",
            playerId
        ));

        std::vector<PlayerBuff*> buffs;
        for (const auto& row : results) {
            buffs.push_back(parsePlayerBuff(row));
        }

        return buffs;
    }

private:
    bool canAddBuff(uint64_t playerId, BuffTemplate* template) {
        // 检查冲突
        if (hasConflictingBuff(playerId, template->id)) {
            return false;
        }

        // 检查最大 Buff 数量
        auto buffs = getActiveBuffs(playerId);
        if (buffs.size() >= MAX_BUFFS) {
            return false;
        }

        return true;
    }

    uint64_t addStackableBuff(uint64_t playerId, int buffId, uint64_t casterId,
                             int duration, int stackCount) {
        // 检查是否已有该 Buff
        auto existing = getActiveBuff(playerId, buffId);

        if (existing) {
            // 叠加层数
            int newStack = std::min(existing->stackCount + stackCount,
                                   getMaxStacks(buffId));
            newStack = std::min(newStack, template->maxStacks);

            database_->execute(fmt::format(
                "UPDATE player_buff SET "
                "stack_count = {}, "
                "end_time = CASE "
                "  WHEN end_time IS NULL THEN DATE_ADD(NOW(), INTERVAL {} SECOND) "
                "  ELSE end_time "
                "END "
                "WHERE id = {}",
                newStack, duration, existing->id
            ));

            return existing->id;
        } else {
            // 新增 Buff
            database_->execute(fmt::format(
                "INSERT INTO player_buff "
                "(player_id, buff_id, caster_id, start_time, end_time, stack_count) "
                "VALUES ({}, {}, {}, NOW(), DATE_ADD(NOW(), INTERVAL {} SECOND), {})",
                playerId, buffId, casterId, duration, stackCount
            ));

            return getLastInsertId();
        }
    }

    void applyBuffEffects(uint64_t playerId, BuffTemplate* template) {
        // 解析效果数据
        json effects = json::parse(template->effects);

        // 应用每个效果
        for (const auto& effect : effects) {
            std::string type = effect["type"];
            float value = effect["value"];

            if (type == "attack_bonus") {
                modifyAttack(playerId, value);
            } else if (type == "defense_bonus") {
                modifyDefense(playerId, value);
            } else if (type == "speed_bonus") {
                modifySpeed(playerId, value);
            } else if (type == "hp_regen") {
                addHPRegen(playerId, value);
            } else if (type == "hp_drain") {
                addHPDrain(playerId, value);
            }
        }
    }

    void scheduleBuffExpire(uint64_t playerId, uint64_t buffInstanceId,
                          int duration) {
        // 使用定时器在指定时间后移除 Buff
        TimerManager::instance()->schedule(
            duration * 1000, // 毫秒
            [this, playerId, buffInstanceId]() {
                removeBuff(playerId, buffInstanceId);
            }
        );
    }

    void notifyBuffAdded(uint64_t playerId, int buffId) {
        // 通知客户端添加 Buff
        if (isPlayerOnline(playerId)) {
            sendBuffNotification(playerId, "add", buffId);
        }
    }

    void notifyBuffRemoved(uint64_t playerId, int buffId) {
        // 通知客户端移除 Buff
        if (isPlayerOnline(playerId)) {
            sendBuffNotification(playerId, "remove", buffId);
        }
    }

    static constexpr size_t MAX_BUFFS = 32;
};

// Buff 模板数据结构
struct BuffTemplate {
    int id;
    std::string name;
    BuffType type;
    std::string icon;
    std::string description;
    int duration;
    int maxStacks;
    std::string effects;  // JSON
    bool removeOnDeath;
    std::vector<int> conflictBuffs;  // 互斥的 Buff ID
};
```

### 3.2 效果叠加规则

```
┌─────────────────────────────────────────────────────────────┐
│                    效果叠加规则                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  规则 1: 相同效果取最高                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  攻击力 +20% 和 攻击力 +30% → 取 +30%            │       │
│  │                                                   │       │
│  │  实现: 使用 map 存储每个属性的最高值              │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  规则 2: 相同效果叠加 (可配置)                            │
│  ┌─────────────────────────────────────────────────┐       │
│  │  持续扣血: 多个效果叠加                             │       │
│  │  HP 回复: 通常不叠加，取最高                            │       │
│  │                                                   │       │
│  │  实现: 每个效果标记是否可叠加                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  规则 3: 互斥效果                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  无敌状态和混乱冲突                                 │       │
│  │  新效果替换旧效果                                 │       │
│  │                                                   │       │
│  │  实现: 效果组/互斥列表                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  规则 4: 独立效果同时生效                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  攻击力加成和防御力加成同时生效                       │       │
│  │  移动加成和回血加成同时生效                           │       │
│  │                                                   │       │
│  │  实现: 每个效果独立计算                           │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 效果计算实现

```cpp
// 效果计算器

class EffectCalculator {
public:
    // 获取最终属性值
    int getFinalValue(uint64_t playerId, AttributeType attr) {
        int baseValue = getBaseValue(playerId, attr);

        // 获取所有相关效果
        auto effects = getActiveEffects(playerId, attr);

        float totalBonus = 0.0f;
        for (const auto& effect : effects) {
            if (effect->isStackable()) {
                // 可叠加效果，累加
                totalBonus += effect->getValue();
            } else {
                // 不可叠加，取最大值
                totalBonus = std::max(totalBonus, effect->getValue());
            }
        }

        return (int)(baseValue * (1 + totalBonus));
    }

private:
    struct Effect {
        BuffType type;
        AttributeType attribute;
        float value;
        bool stackable;
        uint64_t expireTime;
    };

    std::vector<Effect> getActiveEffects(uint64_t playerId,
                                             AttributeType attr) {
        std::vector<Effect> effects;

        auto buffs = buffSystem_->getActiveBuffs(playerId);
        for (const auto& buff : buffs) {
            BuffTemplate* template = getBuffTemplate(buff->buffId);

            // 解析效果数据
            auto buffEffects = getBuffEffects(template);
            for (const auto& effect : buffEffects) {
                if (effectMatches(effect, attr)) {
                    Effect e;
                    e.type = template->type;
                    e.attribute = attr;
                    e.value = effect["value"];
                    e.stackable = effect["stackable"];
                    e.expireTime = buff->endTime;
                    effects.push_back(e);
                }
            }
        }

        // 过滤已过期的效果
        effects.erase(
            std::remove_if(effects.begin(), effects.end(),
                [](const Effect& e) {
                    return e.expireTime < getCurrentTime();
                })
        );

        return effects;
    }
};
```

---

## 四、时间管理

### 4.1 Buff 时间管理

```cpp
// Buff 时间管理

class BuffTimeManager {
public:
    // 每秒更新 Buff 状态
    void update(uint32 deltaTime) {
        // 1. 获取所有即将过期的 Buff
        auto expiring = getExpiringBuffs(deltaTime);

        // 2. 移除过期的 Buff
        for (const auto& buffInfo : expiring) {
            removeBuff(buffInfo.playerId, buffInfo.buffId);
        }

        // 3. 处理持续效果 (如回血、扣血)
        processPeriodicEffects(deltaTime);

        // 4. 更新 UI 倒计时
        updateBuffTimers();
    }

    // 暂停 Buff (如眩晕)
    void pauseBuff(uint64_t playerId, int buffId) {
        database_->execute(fmt::format(
            "UPDATE player_buff SET "
            "end_time = NULL, "
            "pause_time = NOW() "
            "WHERE player_id = {} AND buff_id = {} "
            "AND end_time IS NOT NULL",
            playerId, buffId
        ));

        // 重新开始时计算剩余时间
        pausedBuffs_[buffId] = playerId;
    }

    // 恢复 Buff
    void resumeBuff(uint64_t playerId, int buffId) {
        auto it = pausedBuffs_.find(buffId);
        if (it == pausedBuffs_.end() || it->second != playerId) {
            return;
        }

        database_->execute(fmt::format(
            "UPDATE player_buff SET "
            "end_time = DATE_ADD(NOW(), INTERVAL {} SECOND), "
            "pause_time = NULL "
            "WHERE player_id = {} AND buff_id = {}",
            getRemainingTime(playerId, buffId), playerId, buffId
        ));

        pausedBuffs_.erase(it);
    }

private:
    struct BuffInfo {
        uint64_t playerId;
        int buffId;
    };

    std::map<int, uint64_t> pausedBuffs_;
};
```

---

## 五、最佳实践

### 5.1 Buff/Debuff 设计建议

| 实践 | 说明 |
|------|------|
| **效果数据驱动** | 使用 JSON 配置效果 |
| **分离计算** | 效果计算独立模块 |
| **定时刷新** | 每秒更新状态 |
| **客户端预测** | Buff 剩余时间本地显示 |
| **互斥表设计** | 预定义互斥关系 |

### 5.2 性能优化

```
优化策略:
1. Buff 数量限制
2. 效果缓存计算
3. 批量更新客户端
4. 使用对象池
5. 异步处理过期
```

---

## 六、总结

### Buff/Debuff 系统核心

```
Buff/Debuff = 模板 + 实例 + 效果
- 模板定义 Buff 属性和效果
- 实例跟踪玩家身上的 Buff
- 效果计算属性加成
- 定时器处理过期
```

---

## 参考资料

- [游戏 Buff 系统设计](https://blog.codinghorror.com/)
- [状态效果叠加算法](https://www.gamedev.net/)
