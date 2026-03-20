# Q39: 如何设计战斗系统？

## 问题分析

本题考察对战斗系统设计的理解：
- 战斗系统的核心组成
- 伤害计算流程
- 技能释放机制
- 战斗同步问题

---

## 一、战斗系统架构

### 1.1 战斗系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    战斗系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  输入层:                                                    │
│  ├── 玩家操作 (点击目标、释放技能)                           │
│  ├── AI 决策 (自动攻击、技能选择)                            │
│  └── 系统触发 (定时伤害、环境效果)                           │
│                          │                                  │
│                          ▼                                  │
│  验证层:                                                    │
│  ├── 范围检查 (是否在攻击距离内)                            │
│  ├── 条件检查 (冷却、资源、状态)                             │
│  ├── 目标验证 (敌对关系、存活状态)                           │
│  └── 防作弊验证 (操作频率、异常行为)                           │
│                          │                                  │
│                          ▼                                  │
│  计算层:                                                    │
│  ├── 命中计算 (物理命中检测)                                  │
│  ├── 伤害计算 (攻击力 - 防御力)                             │
│  ├── 暴击计算 (暴击率、暴击伤害)                               │
│  ├── 闪避计算 (闪避率)                                        │
│  └── 吸收/格挡计算                                           │
│                          │                                  │
│                          ▼                                  │
│  应用层:                                                    │
│  ├── 扣除血量                                               │
│  ├── 添加 Buff/Debuff                                       │
│  ├── 触发被动效果                                             │
│  └── 更新战斗状态                                           │
│                          │                                  │
│                          ▼                                  │
│  同步层:                                                    │
│  ├── 通知客户端战斗结果                                      │
│  ├── 同步给其他玩家 (AOI 广播)                               │
│  └── 保存战斗日志                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、伤害计算

### 2.1 基础伤害公式

```
┌─────────────────────────────────────────────────────────────┐
│                    伤害计算公式                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  基础伤害 = (攻击力 - 防御力) × 技能倍率 × 暴击倍率          │
│                                                             │
│  详细公式:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  // 1. 基础伤害                                     │       │
│  │  base_damage = attacker.attack - defender.defense   │       │
│  │  base_damage = MAX(1, base_damage)  // 最小1点    │       │
│  │                                                   │       │
│  │  // 2. 技能加成                                     │       │
│  │  skill_damage = base_damage × skill.multiplier  │       │
│  │                                                   │       │
│  │  // 3. 属性克制                                     │       │
│  │  element_bonus = getElementBonus(                    │       │
│  │      attacker.element, defender.element)             │       │
│  │  )                                               │       │
│  │  damage = skill_damage × (1 + element_bonus * 0.5)    │       │
│  │                                                   │       │
│  │  // 4. 暴击                                         │       │
│  │  if (isCritical()) {                              │       │
│  │      damage *= (1 + critical_rate)                 │       │
│  │  }                                               │       │
│  │                                                   │       │
│  │  // 5. 防御加成                                     │       │
│  │  damage *= (1 - defense_rate)                      │       │
│  │  damage = MAX(1, (int)damage)                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 伤害计算实现

```cpp
// 战斗系统 - 伤害计算

class CombatSystem {
public:
    // 计算伤害
    DamageResult calculateDamage(const AttackRequest& req) {
        DamageResult result;

        // 1. 获取攻击者和防御者
        Entity* attacker = getEntity(req.attackerId);
        Entity* defender = getEntity(req.defenderId);

        if (!attacker || !defender) {
            result.valid = false;
            return result;
        }

        // 2. 验证攻击条件
        if (!validateAttack(attacker, defender, req)) {
            result.valid = false;
            result.reason = "Attack not valid";
            return result;
        }

        // 3. 计算基础伤害
        int baseDamage = attacker->getAttack() - defender->getDefense();
        baseDamage = std::max(1, baseDamage);

        // 4. 应用技能倍率
        Skill* skill = getSkill(req.skillId);
        if (skill) {
            baseDamage = baseDamage * skill->getDamageMultiplier();
        }

        // 5. 属性克制
        float elementBonus = getElementBonus(
            attacker->getElement(),
            defender->getElement()
        );
        baseDamage = (int)(baseDamage * (1 + elementBonus * 0.5f));

        // 6. 暴击计算
        if (rollCritical(attacker->getCriticalRate())) {
            baseDamage = (int)(baseDamage * (1 + attacker->getCriticalDamage()));
            result.isCritical = true;
        }

        // 7. 闪避计算
        if (rollDodge(defender->getDodgeRate())) {
            result.damage = 0;
            result.dodged = true;
            result.valid = true;
            return result;
        }

        // 8. 格挡计算
        if (defender->isBlocking()) {
            baseDamage = (int)(baseDamage * 0.5); // 格挡减半伤
        }

        // 9. 应用防御加成
        float defenseRate = defender->getDefenseRate();
        baseDamage = (int)(baseDamage * (1 - defenseRate));
        result.damage = std::max(1, baseDamage);

        result.valid = true;
        return result;
    }

    // 处理攻击
    void processAttack(const AttackRequest& req) {
        // 计算伤害
        DamageResult result = calculateDamage(req);

        if (!result.valid) {
            sendError(req.attackerId, result.reason);
            return;
        }

        // 应用伤害
        Entity* defender = getEntity(req.defenderId);
        defender->takeDamage(result.damage);

        // 处理死亡
        if (defender->getHP() <= 0) {
            handleDeath(defender, req.attackerId);
        }

        // 通知客户端
        broadcastDamageResult(req, result);
    }

private:
    bool validateAttack(Entity* attacker, Entity* defender,
                         const AttackRequest& req) {
        // 1. 检查距离
        float distance = attacker->distanceTo(defender);
        if (distance > attacker->getAttackRange()) {
            return false;
        }

        // 2. 检查冷却
        if (attacker->isSkillOnCooldown(req.skillId)) {
            return false;
        }

        // 3. 检查资源
        if (attacker->getMP() < req.manaCost) {
            return false;
        }

        // 4. 检查状态
        if (attacker->isStunned() || attacker->isSilenced()) {
            return false;
        }

        // 5. 检查目标
        if (!defender->isAlive() || defender->isInvincible()) {
            return false;
        }

        return true;
    }

    bool rollCritical(float rate) {
        return (rand() % 10000) < rate * 100;
    }

    bool rollDodge(float rate) {
        return (rand() % 10000) < rate * 100;
    }

    float getElementBonus(ElementType attack, ElementType defense) {
        // 火克水、水克火、火克木、木克金、金克火
        const float bonus[6][6] = {
            //物理  火    水    木    金    土
            {0.0f,  0.5f, -0.5f, 0.0f, 0.0f, 0.0f}, // 物理
            {0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 火
            {-0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 水
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 木
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}, // 金
            {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f}  // 土
        };

        return bonus[static_cast<int>(attack)][static_cast<int>(defense)];
    }

    void handleDeath(Entity* victim, uint64_t killerId) {
        // 死亡处理
        victim->setAlive(false);

        // 击杀者奖励
        Entity* killer = getEntity(killerId);
        if (killer) {
            killer->addExp(victim->getRewardExp());
            // 可能掉落
            if (rollDrop(victim->getDropRate())) {
                dropLoot(victim);
            }
        }

        // 复活处理
        scheduleRevive(victim);
    }

    void broadcastDamageResult(const AttackRequest& req,
                                const DamageResult& result) {
        // 广播给 AOI 内的玩家
        for (auto* entity : getAOIEntities(req.defenderId)) {
            sendDamageNotification(entity->getId(), req, result);
        }
    }
};

// 数据结构
struct AttackRequest {
    uint64_t attackerId;
    uint64_t defenderId;
    uint32_t skillId;
    int manaCost;
    Vector3 targetPosition;
};

struct DamageResult {
    bool valid = true;
    int damage = 0;
    bool isCritical = false;
    bool dodged = false;
    std::string reason;
};
```

---

## 三、技能系统

### 3. 技能释放流程

```
┌─────────────────────────────────────────────────────────────┐
│                    技能释放流程                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 客户端请求                                             │
│     │
│     ▼                                                      │
│  2. 服务器验证                                               │
│     ├── 检查技能是否学习                                   │
│     ├── 检查冷却时间                                       │
│     ├── 检查魔法值 (MP)                                   │
│     ├── 检查目标是否有效                                    │
│     └── 检查施法距离                                       │
│                                                             │
│     │  验证失败 → 返回错误                               │
│     │  验证成功 → 继续                                     │
│     │                                                      │
│     ▼                                                      │
│  3. 执行技能效果                                             │
│     ├── 消耗魔法值                                         │
│     ├── 设置冷却时间                                       │
│     ├── 计算伤害                                           │
│     ├── 应用技能效果                                        │
│     └── 返回结果                                            │
│                                                             │
│     ▼                                                      │
│  4. 同步结果                                                │
│     ├── 通知客户端                                         │
│     ├── 更新玩家状态                                        │
│     ├── 通知 AOI 玩家                                      │
│     └── 保存战斗日志                                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 四、战斗同步

### 4.1 延迟补偿

```
┌─────────────────────────────────────────────────────────────┐
│                    战斗同步问题                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  问题: 玩家和服务器之间有延迟                              │
│  ┌─────────────────────────────────────────────────┐       │
│  │  玩家客户端 ──延迟───► 服务器                      │       │
│  │                                                   │       │
│  │  t=0: 玩家点击攻击                                 │       │
│  │  t=50ms: 服务器收到请求                             │       │
│  │  t=80ms: 玩家收到结果                               │       │
│  │                                                   │       │
│  │  问题: 50-80ms 的延迟让玩家感觉卡顿                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  解决方案: 客户端预测                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 玩家点击攻击立即播放攻击动画                       │       │
│  │  2. 同时发送请求到服务器                             │       │
│  │  3. 服务器计算伤害后返回                           │       │
│  │  4. 客户端根据服务器结果校正:                        │       │
│  │     - 如果一致 → 无需调整                             │       │
│  │     - 如果差异大 → 强制校正位置                       │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 五、最佳实践

### 5.1 战斗系统设计建议

| 实践 | 说明 |
|------|------|
| **服务器权威** | 所有伤害计算在服务器 |
| **客户端预测** | 提升体验，需校正 |
| **AOI 广播** | 只同步可见玩家 |
| **技能队列** | 防止技能连发 |
| **状态锁** | 防止状态异常 |

### 5.2 常见问题

| 问题 | 解决方案 |
|------|----------|
| **伤害计算不准** | 服务器权威计算 |
| **技能连发 | 技能队列 + 冷却检查 |
| **延迟大** | 客户端预测 + 插值 |
| **不同步** | 时间戳 + 序列号 |

---

## 六、总结

### 战斗系统核心

```
战斗系统 = 伤害计算 + 技能系统 + 状态同步
- 服务器权威计算，防止作弊
- 客户端预测，提升体验
- AOI 优化同步范围
```

---

## 参考资料

- [MMO 战斗系统设计](https://www.gamedev.net/)
- [游戏战斗同步](https://gafferongames.com/post/networked_physics_2004/)
