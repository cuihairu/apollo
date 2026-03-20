# Q43: 如何设计属性系统？

## 问题分析

本题考察对游戏属性系统的理解：
- 属性系统的基础架构
- 属性分类与继承关系
- 属性加成计算
- KBEngine 的属性定义机制

---

## 一、属性系统架构

### 1.1 属性系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    属性系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  基础属性层 (Base Attributes):                              │
│  ├── 生命值 (HP)                                           │
│  ├── 魔法值 (MP)                                           │
│  ├── 攻击力 (Attack)                                       │
│  ├── 防御力 (Defense)                                      │
│  ├── 敏捷 (Agility)                                        │
│  ├── 智力 (Intelligence)                                   │
│  ├── 体力 (Vitality)                                       │
│  └── 幸运 (Luck)                                           │
│                          │                                  │
│                          ▼                                  │
│  派生属性层 (Derived Attributes):                          │
│  ├── 物理攻击 = BaseAttack + WeaponAttack + Buff           │
│  ├── 魔法攻击 = BaseIntelligence × 2 + StaffBonus          │
│  ├── 暴击率 = BaseCrit + Agility × 0.1% + Buff            │
│  ├── 闪避率 = BaseDodge + Agility × 0.05% + Buff          │
│  ├── 移动速度 = BaseSpeed + Buff                           │
│  └── 攻击速度 = BaseAttackSpeed + Buff                     │
│                          │                                  │
│                          ▼                                  │
│  加成层 (Bonus Layer):                                     │
│  ├── 装备加成 (Equipment Bonus)                            │
│  ├── Buff/Debuff (Status Effects)                         │
│  ├── 技能加成 (Skill Bonus)                                │
│  ├── 天赋加成 (Talent Bonus)                               │
│  └── 组队加成 (Party Bonus)                                │
│                          │                                  │
│                          ▼                                  │
│  最终属性 (Final Attributes):                              │
│  └── FinalValue = Base + (Base × PercentBonus) + FlatBonus│
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 KBEngine 属性定义

```python
# KBEngine EntityDefs 属性定义示例

# entity_defs/Avatar.def
<Avatar>
    <!-- 基础属性 -->
    <Properties>
        <level>
            <Type> UINT8 </Type>
            <Flags> BASE </Flags>
            <Default> 1 </Default>
        </level>

        <exp>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </exp>

        <!-- 战斗属性 -->
        <hp>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Default> 100 </Default>
        </hp>

        <maxHp>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Default> 100 </Default>
            <Database> True </Database>
        </maxHp>

        <mp>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Default> 50 </Default>
        </mp>

        <maxMp>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Default> 50 </Default>
            <Database> True </Database>
        </maxMp>

        <!-- 基础属性 -->
        <attack>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 10 </Default>
            <Database> True </Database>
        </attack>

        <defense>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 5 </Default>
            <Database> True </Database>
        </defense>

        <agility>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 10 </Default>
            <Database> True </Database>
        </agility>

        <intelligence>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 10 </Default>
            <Database> True </Database>
        </intelligence>

        <vitality>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 10 </Default>
            <Database> True </Database>
        </vitality>

        <!-- 派生属性 -->
        <critRate>
            <Type> FLOAT32 </Type>
            <Flags> CELL_PRIVATE </Flags>
            <Default> 0.05 </Default>
        </critRate>

        <critDamage>
            <Type> FLOAT32 </Type>
            <Flags> CELL_PRIVATE </Flags>
            <Default> 1.5 </Default>
        </critDamage>

        <dodgeRate>
            <Type> FLOAT32 </Type>
            <Flags> CELL_PRIVATE </Flags>
            <Default> 0.05 </Default>
        </dodgeRate>

        <moveSpeed>
            <Type> FLOAT32 </Type>
            <Flags> CELL_PRIVATE </Flags>
            <Default> 5.0 </Default>
        </moveSpeed>

        <attackSpeed>
            <Type> FLOAT32 </Type>
            <Flags> CELL_PRIVATE </Flags>
            <Default> 1.0 </Default>
        </attackSpeed>
    </Properties>

    <!-- 客户端方法 -->
    <ClientMethods>
        <updateAttributes>
            <Arg> ARRAY </Arg>
        </updateAttributes>
    </ClientMethods>
</Avatar>
```

---

## 二、属性分类

### 2.1 属性类型划分

```cpp
// 属性类型定义

enum class AttributeType {
    // 基础属性
    HP = 0,              // 当前生命值
    MAX_HP = 1,          // 最大生命值
    MP = 2,              // 当前魔法值
    MAX_MP = 3,          // 最大魔法值
    STAMINA = 4,         // 体力值
    MAX_STAMINA = 5,     // 最大体力值

    // 战斗属性
    ATTACK = 10,         // 物理攻击
    MAGIC_ATTACK = 11,   // 魔法攻击
    DEFENSE = 12,        // 物理防御
    MAGIC_DEFENSE = 13,  // 魔法防御

    // 基础属性点
    STRENGTH = 20,       // 力量
    AGILITY = 21,        // 敏捷
    INTELLECT = 22,      // 智力
    VITALITY = 23,       // 体力
    LUCK = 24,           // 幸运

    // 派生属性
    CRIT_RATE = 30,      // 暴击率 (0-1)
    CRIT_DAMAGE = 31,    // 暴击伤害倍率
    DODGE_RATE = 32,     // 闪避率 (0-1)
    HIT_RATE = 33,       // 命中率 (0-1)
    ATTACK_SPEED = 34,   // 攻击速度 (次/秒)
    MOVE_SPEED = 35,     // 移动速度 (米/秒)
    CAST_SPEED = 36,     // 施法速度
    HP_REGEN = 37,       // 生命恢复/秒
    MP_REGEN = 38,       // 魔法恢复/秒

    // 抗性属性
    FIRE_RESIST = 40,    // 火抗
    ICE_RESIST = 41,     // 冰抗
    POISON_RESIST = 42,  // 毒抗
    LIGHTNING_RESIST = 43,// 雷抗

    // 其他
    LEVEL = 100,
    EXP = 101,
};

// 属性数据
struct AttributeValue {
    int base = 0;         // 基础值
    float percentBonus = 0.0f;  // 百分比加成
    int flatBonus = 0;    // 固定值加成
    int final = 0;        // 最终值 (缓存)

    // 计算最终值
    void calculate() {
        final = static_cast<int>(base * (1.0f + percentBonus)) + flatBonus;
        final = std::max(0, final);  // 最小为0
    }
};
```

### 2.2 属性加成来源

```cpp
// 加成来源

enum class BonusSource {
    EQUIPMENT = 0,       // 装备
    BUFF = 1,            // Buff/Debuff
    SKILL = 2,           // 技能被动
    TALENT = 3,          // 天赋
    PARTY = 4,           // 组队
    GUILD = 5,           // 公会
    TITLE = 6,           // 称号
    consumable = 7,      // 消耗品
};

// 加成记录
struct AttributeBonus {
    BonusSource source;
    uint64_t sourceId;   // 来源ID (如装备ID、BuffID)
    AttributeType attribute;
    float percentBonus = 0.0f;
    int flatBonus = 0;
    bool isNegative = false;  // 是否为负向效果 (Debuff)
};
```

---

## 三、属性系统实现

### 3.1 属性管理器

```cpp
// 属性系统

class AttributeSystem {
public:
    // 获取最终属性值
    int getAttribute(uint64_t entityId, AttributeType type) {
        auto* entity = getEntity(entityId);
        if (!entity) return 0;

        // 获取或计算
        auto& attr = entity->attributes[type];
        if (attr.dirty) {
            recalculateAttribute(entity, type);
            attr.dirty = false;
        }

        return attr.final;
    }

    // 设置基础属性
    void setBaseAttribute(uint64_t entityId, AttributeType type, int value) {
        auto* entity = getEntity(entityId);
        if (!entity) return;

        auto& attr = entity->attributes[type];
        int oldFinal = attr.final;

        attr.base = value;
        markDirty(entity, type);

        int newFinal = getAttribute(entityId, type);

        // 通知变化
        if (oldFinal != newFinal) {
            onAttributeChanged(entityId, type, oldFinal, newFinal);
        }
    }

    // 添加属性加成
    void addBonus(uint64_t entityId, const AttributeBonus& bonus) {
        auto* entity = getEntity(entityId);
        if (!entity) return;

        // 记录加成
        entity->bonuses.push_back(bonus);

        // 标记需要重新计算
        markDirty(entity, bonus.attribute);

        // 重新计算
        recalculateAttribute(entity, bonus.attribute);

        // 通知客户端
        notifyAttributeUpdate(entityId, bonus.attribute);
    }

    // 移除属性加成
    void removeBonus(uint64_t entityId, BonusSource source, uint64_t sourceId) {
        auto* entity = getEntity(entityId);
        if (!entity) return;

        auto& bonuses = entity->bonuses;
        auto it = std::remove_if(bonuses.begin(), bonuses.end(),
            [source, sourceId](const AttributeBonus& bonus) {
                return bonus.source == source && bonus.sourceId == sourceId;
            }
        );

        if (it != bonuses.end()) {
            // 记录需要重新计算的属性
            std::set<AttributeType> affectedTypes;
            for (auto bonusIt = it; bonusIt != bonuses.end(); ++bonusIt) {
                affectedTypes.insert(bonusIt->attribute);
            }

            bonuses.erase(it, bonuses.end());

            // 重新计算受影响的属性
            for (auto type : affectedTypes) {
                markDirty(entity, type);
                recalculateAttribute(entity, type);
                notifyAttributeUpdate(entityId, type);
            }
        }
    }

    // 批量获取属性 (用于网络同步)
    std::vector<std::pair<AttributeType, int>> getAllAttributes(uint64_t entityId) {
        auto* entity = getEntity(entityId);
        if (!entity) return {};

        std::vector<std::pair<AttributeType, int>> result;
        for (const auto& [type, attr] : entity->attributes) {
            result.push_back({type, getAttribute(entityId, type)});
        }
        return result;
    }

private:
    struct EntityAttributes {
        std::unordered_map<AttributeType, AttributeValue> attributes;
        std::vector<AttributeBonus> bonuses;
    };

    void recalculateAttribute(EntityAttributes* entity, AttributeType type) {
        auto& attr = entity->attributes[type];

        // 重置为基础值
        float totalPercent = 0.0f;
        int totalFlat = 0;

        // 累加所有加成
        for (const auto& bonus : entity->bonuses) {
            if (bonus.attribute == type) {
                if (bonus.isNegative) {
                    totalPercent -= bonus.percentBonus;
                    totalFlat -= bonus.flatBonus;
                } else {
                    totalPercent += bonus.percentBonus;
                    totalFlat += bonus.flatBonus;
                }
            }
        }

        // 应用百分比加成上限 (防止过度叠加)
        totalPercent = std::clamp(totalPercent, -0.9f, 5.0f);

        attr.percentBonus = totalPercent;
        attr.flatBonus = totalFlat;
        attr.calculate();
    }

    void markDirty(EntityAttributes* entity, AttributeType type) {
        auto& attr = entity->attributes[type];
        attr.dirty = true;

        // 如果是基础属性变化，可能影响派生属性
        notifyDerivedAttributesDirty(entity, type);
    }

    void notifyDerivedAttributesDirty(EntityAttributes* entity, AttributeType baseType) {
        // 定义派生关系
        static const std::map<AttributeType, std::vector<AttributeType>> derivedMap = {
            {AttributeType::STRENGTH, {AttributeType::ATTACK, AttributeType::HP}},
            {AttributeType::AGILITY, {AttributeType::CRIT_RATE, AttributeType::DODGE_RATE, AttributeType::ATTACK_SPEED}},
            {AttributeType::INTELLECT, {AttributeType::MAGIC_ATTACK, AttributeType::MAX_MP, AttributeType::CRIT_RATE}},
            {AttributeType::VITALITY, {AttributeType::MAX_HP, AttributeType::HP_REGEN, AttributeType::DEFENSE}},
            {AttributeType::LEVEL, {AttributeType::MAX_HP, AttributeType::MAX_MP, AttributeType::ATTACK, AttributeType::DEFENSE}},
        };

        auto it = derivedMap.find(baseType);
        if (it != derivedMap.end()) {
            for (auto derivedType : it->second) {
                auto derivedIt = entity->attributes.find(derivedType);
                if (derivedIt != entity->attributes.end()) {
                    derivedIt->second.dirty = true;
                }
            }
        }
    }

    void onAttributeChanged(uint64_t entityId, AttributeType type,
                           int oldValue, int newValue) {
        // 处理属性变化事件
        switch (type) {
            case AttributeType::MAX_HP:
                // 最大生命值变化，可能需要调整当前生命值
                adjustCurrentHP(entityId, oldValue, newValue);
                break;
            case AttributeType::MAX_MP:
                adjustCurrentMP(entityId, oldValue, newValue);
                break;
            default:
                break;
        }

        // 触发属性变化回调
        triggerAttributeCallbacks(entityId, type, oldValue, newValue);
    }

    void adjustCurrentHP(uint64_t entityId, int oldMax, int newMax) {
        auto* entity = getEntity(entityId);
        int currentHp = getAttribute(entityId, AttributeType::HP);

        // 按比例调整当前HP
        float ratio = oldMax > 0 ? static_cast<float>(currentHp) / oldMax : 1.0f;
        int newCurrent = static_cast<int>(newMax * ratio);
        setBaseAttribute(entityId, AttributeType::HP, newCurrent);
    }

    void notifyAttributeUpdate(uint64_t entityId, AttributeType type) {
        // 通知客户端属性更新
        int value = getAttribute(entityId, type);
        sendToClient(entityId, "onAttributeUpdate", type, value);
    }

    std::unordered_map<uint64_t, EntityAttributes> entities_;
};
```

### 3.2 KBEngine 属性同步

```python
# KBEngine 属性同步机制

# scripts/entity_defs/avatar.py
import KBEngine
from KBEDebug import *

class Avatar(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 注册属性监听
        self.registerHandle("onLevelChanged", self.onLevelChanged)
        self.registerHandle("onHpChanged", self.onHpChanged)

        # 初始化派生属性
        self.recalcDerivedAttributes()

    def onRemoteMethodCall(self, methodName, args):
        """客户端调用属性变更"""
        if methodName == "addBasePoint":
            attrType, value = args
            self.onAddBasePoint(attrType, value)

    def onAddBasePoint(self, attrType, value):
        """添加基础属性点"""
        if self.freePoints <= 0:
            return

        if attrType == "strength":
            self.strength += value
        elif attrType == "agility":
            self.agility += value
        elif attrType == "intellect":
            self.intellect += value
        elif attrType == "vitality":
            self.vitality += value

        self.freePoints -= value

        # 重新计算派生属性
        self.recalcDerivedAttributes()

    def recalcDerivedAttributes(self):
        """重新计算派生属性"""
        # 最大HP = 基础 + 体力 × 10
        self.maxHp = 100 + self.vitality * 10

        # 最大MP = 基础 + 智力 × 5
        self.maxMp = 50 + self.intellect * 5

        # 物理攻击 = 力量 × 2
        self.attack = self.strength * 2

        # 魔法攻击 = 智力 × 2
        self.magicAttack = self.intellect * 2

        # 物理防御 = 力量 + 体力 / 2
        self.defense = self.strength + self.vitality // 2

        # 暴击率 = 5% + 敏捷 × 0.1%
        self.critRate = 0.05 + self.agility * 0.001
        self.critRate = min(self.critRate, 0.8)  # 上限80%

        # 闪避率 = 5% + 敏捷 × 0.05%
        self.dodgeRate = 0.05 + self.agility * 0.0005
        self.dodgeRate = min(self.dodgeRate, 0.6)  # 上限60%

        # 同步到客户端
        self.client.syncAttributes(self.getAllAttributes())

    def getAllAttributes(self):
        """获取所有属性"""
        return {
            "level": self.level,
            "exp": self.exp,
            "hp": self.hp,
            "maxHp": self.maxHp,
            "mp": self.mp,
            "maxMp": self.maxMp,
            "attack": self.attack,
            "defense": self.defense,
            "strength": self.strength,
            "agility": self.agility,
            "intellect": self.intellect,
            "vitality": self.vitality,
            "critRate": self.critRate,
            "dodgeRate": self.dodgeRate,
        }

    def onLevelChanged(self, oldValue):
        """等级变化处理"""
        if self.level > oldValue:
            # 升级奖励属性点
            self.freePoints += 5
            # 升级回满血
            self.hp = self.maxHp
            self.mp = self.maxMp

    def onHpChanged(self, oldValue):
        """HP变化处理"""
        if self.hp <= 0:
            self.onDeath()
        elif self.hp > self.maxHp:
            self.hp = self.maxHp
```

---

## 四、属性计算

### 4.1 百分比加成 vs 固定加成

```
┌─────────────────────────────────────────────────────────────┐
│                    属性加成计算顺序                            │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  计算公式:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  Final = Base × (1 + ΣPercentBonus) + ΣFlatBonus │       │
│  │                                                   │       │
│  │  示例:                                            │       │
│  │  BaseAttack = 100                               │       │
│  │  装备加成: +20% (PercentBonus = 0.2)            │       │
│  │  Buff加成: +50% (PercentBonus = 0.5)            │       │
│  │  天赋加成: +30 (FlatBonus = 30)                 │       │
│  │                                                   │       │
│  │  Final = 100 × (1 + 0.2 + 0.5) + 30            │       │
│  │        = 100 × 1.7 + 30 = 200                  │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  注意事项:                                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  1. 百分比加成先乘法，再加固定值                    │       │
│  │  2. 设置最大百分比上限防止过度叠加                   │       │
│  │  3. 负面效果 (Debuff) 减少百分比                     │       │
│  │  4. 最小值为0，不允许负属性                          │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 加成优先级

```cpp
// 加成优先级处理

enum class BonusPriority {
    BASE = 0,           // 基础值
    PASSIVE = 10,       // 被动技能
    EQUIPMENT = 20,     // 装备
    BUFF = 30,          // Buff
    TEMPORARY = 40,     // 临时效果
};

// 按优先级分组的加成
struct PriorityBonusGroup {
    float totalPercent = 0.0f;
    int totalFlat = 0;
};

int calculateFinalValue(AttributeValue& baseAttr,
                        const std::vector<AttributeBonus>& bonuses) {
    // 按优先级分组
    std::map<BonusPriority, PriorityBonusGroup> groups;

    for (const auto& bonus : bonuses) {
        BonusPriority priority = getPriority(bonus.source);
        groups[priority].totalPercent += bonus.percentBonus;
        groups[priority].totalFlat += bonus.flatBonus;
    }

    // 逐层应用
    int currentValue = baseAttr.base;

    for (auto& [priority, group] : groups) {
        // 应用百分比
        currentValue = static_cast<int>(currentValue * (1.0f + group.totalPercent));
        // 应用固定值
        currentValue += group.totalFlat;
    }

    return std::max(0, currentValue);
}

BonusPriority getPriority(BonusSource source) {
    switch (source) {
        case BonusSource::SKILL: return BonusPriority::PASSIVE;
        case BonusSource::EQUIPMENT: return BonusPriority::EQUIPMENT;
        case BonusSource::BUFF: return BonusPriority::BUFF;
        case BonusSource::consumable: return BonusPriority::TEMPORARY;
        default: return BonusPriority::BASE;
    }
}
```

---

## 五、最佳实践

### 5.1 属性系统设计建议

| 实践 | 说明 |
|------|------|
| **分离计算** | 属性计算独立模块，避免耦合 |
| **增量更新** | 只更新变化的属性 |
| **客户端缓存** | 客户端本地缓存减少网络同步 |
| **校验机制** | 服务端权威，客户端显示 |
| **数据驱动** | 属性配置化，便于调整 |

### 5.2 性能优化

```
优化策略:
1. 属性缓存，避免重复计算
2. 脏标记，按需更新
3. 批量同步，减少网络消息
4. 使用对象池，避免频繁分配
5. 派生属性延迟计算
```

---

## 六、总结

### 属性系统核心

```
属性系统 = 基础属性 + 派生属性 + 加成系统
- 基础属性是玩家可分配的属性点
- 派生属性由基础属性计算得出
- 加成系统处理装备、Buff等临时效果
- 服务端权威计算，防止作弊
```

---

## 参考资料

- [KBEngine EntityDef文档](https://kbengine.github.io/docs/programming/entitydefs.html)
- [KBEngine 属性同步机制](https://kbengine.github.io/docs/programming/entity.html)
- [游戏属性系统设计](https://www.gamedev.net/)
