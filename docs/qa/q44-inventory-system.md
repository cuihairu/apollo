# Q44: 如何设计背包系统？

## 问题分析

本题考察对背包系统的理解：
- 背包数据模型设计
- 物品堆叠规则
- 背包扩展机制
- KBEngine 的物品管理

---

## 一、背包系统架构

### 1.1 背包系统组成

```
┌─────────────────────────────────────────────────────────────┐
│                    背包系统架构                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  背包类型 (Bag Types):                                      │
│  ├── 主背包 (Main Bag) - 默认格子，通常 20-40 格              │
│  ├── 扩展背包 (Extended Bag) - 额外购买的格子                 │
│  ├── 仓库背包 (Warehouse) - 永久存储                         │
│  ├── 装备栏 (Equipment) - 10-15 个装备槽位                    │
│  ├── 材料背包 (Material) - 专门存放材料                       │
│  └── 任务物品 (Quest Items) - 独立任务物品栏                  │
│                          │                                  │
│                          ▼                                  │
│  格子管理 (Slot Management):                                │
│  ├── 格子状态 (Empty, Occupied, Locked)                    │
│  ├── 堆叠管理 (Stackable Items)                            │
│  ├── 排序功能 (Sort by Type/Quality)                       │
│  └── 分割物品 (Split Stack)                                │
│                          │                                  │
│                          ▼                                  │
│  物品操作 (Item Operations):                                │
│  ├── 添加物品 (Add Item)                                   │
│  ├── 移除物品 (Remove Item)                                │
│  ├── 移动物品 (Move Item)                                  │
│  ├── 使用物品 (Use Item)                                   │
│  ├── 丢弃物品 (Drop Item)                                  │
│  └── 交易物品 (Trade Item)                                 │
│                          │                                  │
│                          ▼                                  │
│  约束规则 (Constraints):                                    │
│  ├── 背包容量上限                                          │
│  ├── 单个物品堆叠上限                                       │
│  ├── 绑定类型限制 (绑定/不可交易)                            │
│  └── 唯一物品限制                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 KBEngine 物品定义

```python
# KBEngine 物品定义 (entity_defs)

# entity_defs/Item.def
<Item>
    <Properties>
        <itemID>
            <Type> UINT32 </Type>
            <Flags> BASE </Flags>
            <Database> True </Database>
            <Default> 0 </Default>
        </itemID>

        <itemTemplateID>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </itemTemplateID>

        <ownerID>
            <Type> UINT64 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </ownerID>

        <count>
            <Type> UINT16 </Type>
            <Flags> BASE </Flags>
            <Default> 1 </Default>
        </count>

        <slot>
            <Type> UINT8 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </slot>

        <bagType>
            <Type> UINT8 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </bagType>

        <quality>
            <Type> UINT8 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </quality>

        <bindType>
            <Type> UINT8 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </bindType>

        <expireTime>
            <Type> UINT64 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </expireTime>

        <createTime>
            <Type> UINT64 </Type>
            <Flags> BASE </Flags>
            <Default> 0 </Default>
        </createTime>
    </Properties>
</Item>

# scripts/data/item_templates.py
# 物品模板配置
ITEM_TEMPLATES = {
    # 消耗品
    1001: {
        "id": 1001,
        "name": "初级生命药水",
        "type": Item.TYPE_CONSUMABLE,
        "quality": Item.QUALITY_COMMON,
        "maxStack": 99,
        "sellPrice": 10,
        "effects": [
            {"type": "restore_hp", "value": 100}
        ]
    },
    1002: {
        "id": 1002,
        "name": "初级魔法药水",
        "type": Item.TYPE_CONSUMABLE,
        "quality": Item.QUALITY_COMMON,
        "maxStack": 99,
        "sellPrice": 10,
        "effects": [
            {"type": "restore_mp", "value": 50}
        ]
    },

    # 装备
    2001: {
        "id": 2001,
        "name": "新手铁剑",
        "type": Item.TYPE_WEAPON,
        "quality": Item.QUALITY_COMMON,
        "maxStack": 1,
        "sellPrice": 100,
        "equipSlot": Item.SLOT_MAIN_HAND,
        "requirements": {
            "level": 1,
            "class": ["warrior", "paladin"]
        },
        "attributes": {
            "attack": 10,
            "strength": 2
        }
    },
    2002: {
        "id": 2002,
        "name": "精钢长剑",
        "type": Item.TYPE_WEAPON,
        "quality": Item.QUALITY_RARE,
        "maxStack": 1,
        "sellPrice": 1000,
        "equipSlot": Item.SLOT_MAIN_HAND,
        "requirements": {
            "level": 10
        },
        "attributes": {
            "attack": 35,
            "strength": 5,
            "critRate": 0.05
        }
    },

    # 材料
    3001: {
        "id": 3001,
        "name": "铁矿石",
        "type": Item.TYPE_MATERIAL,
        "quality": Item.QUALITY_COMMON,
        "maxStack": 999,
        "sellPrice": 1
    },

    # 任务物品
    4001: {
        "id": 4001,
        "name": "破损的地图",
        "type": Item.TYPE_QUEST,
        "quality": Item.QUALITY_COMMON,
        "maxStack": 1,
        "sellPrice": 0,
        "cannotDrop": True,
        "cannotTrade": True
    }
}
```

---

## 二、数据模型设计

### 2.1 背包表结构

```sql
-- 背包表
CREATE TABLE `player_bag` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `bag_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '背包类型:0主背包,1仓库,2装备栏',
    `slot_index` SMALLINT UNSIGNED NOT NULL COMMENT '格子索引',
    `item_id` INT UNSIGNED NOT NULL COMMENT '物品模板ID',
    `item_count` SMALLINT UNSIGNED NOT NULL DEFAULT 1 COMMENT '物品数量',
    `item_unique_id` BIGINT UNSIGNED DEFAULT NULL COMMENT '唯一物品ID',
    `quality` TINYINT UNSIGNED DEFAULT 0 COMMENT '品质',
    `bind_type` TINYINT UNSIGNED DEFAULT 0 COMMENT '绑定类型',
    `expire_time` DATETIME DEFAULT NULL COMMENT '过期时间',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `enchant_level` TINYINT UNSIGNED DEFAULT 0 COMMENT '强化等级',
    `enchant_exp` SMALLINT UNSIGNED DEFAULT 0 COMMENT '强化经验',
    `extra_data` JSON DEFAULT NULL COMMENT '扩展数据(宝石、随机属性等)',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_player_bag_slot` (`player_id`, `bag_type`, `slot_index`),
    KEY `idx_player_id` (`player_id`),
    KEY `idx_item_unique_id` (`item_unique_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='玩家背包表';

-- 唯一物品表 (装备等需要独立ID的物品)
CREATE TABLE `player_item_unique` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `player_id` BIGINT UNSIGNED NOT NULL COMMENT '玩家ID',
    `template_id` INT UNSIGNED NOT NULL COMMENT '物品模板ID',
    `create_time` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_player_id` (`player_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='唯一物品表';
```

### 2.2 物品类型定义

```cpp
// 物品类型枚举

enum class ItemType {
    // 基础类型
    NONE = 0,
    CONSUMABLE = 1,      // 消耗品 (药水、食物)
    EQUIPMENT = 2,       // 装备 (武器、防具)
    MATERIAL = 3,        // 材料 (矿石、草药)
    QUEST = 4,           // 任务物品
    CURRENCY = 5,        // 货币 (金币、钻石)

    // 装备子类型
    WEAPON = 10,         // 武器
    ARMOR = 11,          // 防具
    ACCESSORY = 12,      // 饰品
};

// 物品品质
enum class ItemQuality {
    COMMON = 0,          // 普通 (白)
    UNCOMMON = 1,        // 优秀 (绿)
    RARE = 2,            // 精良 (蓝)
    EPIC = 3,            // 史诗 (紫)
    LEGENDARY = 4,       // 传说 (橙)
    MYTHIC = 5,          // 神话 (红)
};

// 绑定类型
enum class BindType {
    NONE = 0,            // 不绑定
    PICKUP = 1,          // 拾取绑定
    EQUIP = 2,           // 装备绑定
    USE = 3,             // 使用绑定
};

// 背包类型
enum class BagType {
    MAIN = 0,            // 主背包
    WAREHOUSE = 1,       // 仓库
    EQUIPMENT = 2,       // 装备栏
    MATERIAL = 3,        // 材料包
    QUEST = 4,           // 任务包
};

// 物品模板
struct ItemTemplate {
    int id;
    std::string name;
    ItemType type;
    ItemQuality quality;
    int maxStack;                // 最大堆叠数
    int sellPrice;               // 出售价格
    int buyPrice;                // 购买价格
    BindType bindType;           // 绑定类型
    bool canDrop;                // 可丢弃
    bool canTrade;               // 可交易
    bool canUse;                 // 可使用
    bool hasExpire;              // 有过期时间
    int expireSeconds;           // 过期秒数
    int requiredLevel;           // 需求等级

    // 装备特有属性
    int equipSlot;               // 装备槽位
    std::map<std::string, int> attributes;  // 属性加成

    // 消耗品效果
    std::vector<ItemEffect> effects;

    // 扩展数据
    nlohmann::json extraData;
};

// 物品效果
struct ItemEffect {
    std::string type;            // 效果类型
    int value;                   // 效果值
    int duration;                // 持续时间
};

// 物品实例
struct ItemInstance {
    uint64_t uniqueId;           // 唯一ID
    int templateId;              // 模板ID
    uint64_t ownerId;            // 拥有者ID
    int count;                   // 数量
    BagType bagType;             // 背包类型
    int slot;                    // 格子位置
    ItemQuality quality;         // 品质
    BindType bindType;           // 绑定类型
    uint64_t expireTime;         // 过期时间
    uint64_t createTime;         // 创建时间

    // 装备特有
    int enchantLevel;            // 强化等级
    nlohmann::json extraData;    // 扩展数据
};
```

---

## 三、背包系统实现

### 3.1 背包管理器

```cpp
// 背包系统

class InventorySystem {
public:
    // 背包容量
    static constexpr int DEFAULT_MAIN_BAG_SIZE = 30;
    static constexpr int MAX_BAG_SIZE = 200;
    static constexpr int WAREHOUSE_SIZE = 100;

    // 添加物品
    AddItemResult addItem(uint64_t playerId, int templateId, int count = 1,
                         BindType bindType = BindType::NONE) {
        ItemTemplate* tmpl = getItemTemplate(templateId);
        if (!tmpl) {
            return {AddItemResult::INVALID_ITEM, 0, {}};
        }

        // 检查是否可堆叠
        if (tmpl->maxStack > 1) {
            return addStackableItem(playerId, tmpl, count, bindType);
        } else {
            return addNonStackableItem(playerId, tmpl, count, bindType);
        }
    }

    // 移除物品
    bool removeItem(uint64_t playerId, uint64_t uniqueId, int count) {
        auto* instance = getItemInstance(playerId, uniqueId);
        if (!instance) return false;

        if (instance->count < count) {
            return false;  // 数量不足
        }

        if (instance->count == count) {
            // 完全移除
            database_->execute(fmt::format(
                "DELETE FROM player_bag WHERE id = {}",
                uniqueId
            ));
            unloadItem(playerId, uniqueId);
        } else {
            // 减少数量
            instance->count -= count;
            database_->execute(fmt::format(
                "UPDATE player_bag SET item_count = {} WHERE id = {}",
                instance->count, uniqueId
            ));
        }

        notifyItemRemoved(playerId, uniqueId, count);
        return true;
    }

    // 按模板ID移除物品
    bool removeItemByTemplate(uint64_t playerId, int templateId, int count) {
        auto items = getItemsByTemplate(playerId, templateId);
        int remaining = count;

        for (auto* item : items) {
            if (remaining <= 0) break;

            int removeCount = std::min(item->count, remaining);
            if (removeItem(playerId, item->uniqueId, removeCount)) {
                remaining -= removeCount;
            }
        }

        return remaining == 0;
    }

    // 移动物品
    bool moveItem(uint64_t playerId, uint64_t uniqueId, BagType fromBag,
                  int fromSlot, BagType toBag, int toSlot) {
        // 检查目标格子是否为空
        if (!isSlotEmpty(playerId, toBag, toSlot)) {
            return false;
        }

        // 更新数据库
        database_->execute(fmt::format(
            "UPDATE player_bag SET bag_type = {}, slot_index = {} "
            "WHERE id = {} AND player_id = {}",
            static_cast<int>(toBag), toSlot, uniqueId, playerId
        ));

        // 更新内存
        auto* item = getItemInstance(playerId, uniqueId);
        if (item) {
            item->bagType = toBag;
            item->slot = toSlot;
        }

        notifyItemMoved(playerId, uniqueId, fromBag, fromSlot, toBag, toSlot);
        return true;
    }

    // 使用物品
    bool useItem(uint64_t playerId, uint64_t uniqueId) {
        auto* instance = getItemInstance(playerId, uniqueId);
        if (!instance) return false;

        ItemTemplate* tmpl = getItemTemplate(instance->templateId);
        if (!tmpl || !tmpl->canUse) {
            return false;
        }

        // 检查使用条件
        if (!checkUseRequirements(playerId, tmpl)) {
            return false;
        }

        // 应用物品效果
        for (const auto& effect : tmpl->effects) {
            applyItemEffect(playerId, effect);
        }

        // 消耗品使用后减少数量
        if (tmpl->type == ItemType::CONSUMABLE) {
            removeItem(playerId, uniqueId, 1);
        }

        // 装备绑定处理
        if (tmpl->bindType == BindType::USE) {
            bindItem(playerId, uniqueId);
        }

        notifyItemUsed(playerId, uniqueId);
        return true;
    }

    // 扩展背包
    bool expandBag(uint64_t playerId, int extraSlots) {
        int currentSize = getBagSize(playerId, BagType::MAIN);
        int newSize = currentSize + extraSlots;

        if (newSize > MAX_BAG_SIZE) {
            return false;  // 超过最大容量
        }

        // 更新玩家背包容量
        database_->execute(fmt::format(
            "UPDATE player SET bag_size = {} WHERE id = {}",
            newSize, playerId
        ));

        notifyBagExpanded(playerId, newSize);
        return true;
    }

    // 整理背包
    void sortBag(uint64_t playerId, BagType bagType) {
        auto items = getAllItems(playerId, bagType);

        // 按类型、品质、名称排序
        std::sort(items.begin(), items.end(),
            [](const ItemInstance* a, const ItemInstance* b) {
                if (a->templateId != b->templateId) {
                    return a->templateId < b->templateId;
                }
                return a->quality < b->quality;
            }
        );

        // 重新分配格子
        int slot = 0;
        for (auto* item : items) {
            if (item->slot != slot) {
                moveItem(playerId, item->uniqueId, bagType, item->slot,
                        bagType, slot);
            }
            slot++;
        }

        notifyBagSorted(playerId, bagType);
    }

    // 获取背包物品
    std::vector<ItemInstance*> getAllItems(uint64_t playerId, BagType bagType) {
        auto it = playerInventories_.find(playerId);
        if (it == playerInventories_.end()) {
            loadPlayerInventory(playerId);
            it = playerInventories_.find(playerId);
        }

        std::vector<ItemInstance*> result;
        for (auto& [uniqueId, item] : it->second.items) {
            if (item.bagType == bagType) {
                result.push_back(&item);
            }
        }

        return result;
    }

    // 检查是否有足够物品
    bool hasItemCount(uint64_t playerId, int templateId, int count) {
        auto items = getItemsByTemplate(playerId, templateId);
        int totalCount = 0;

        for (auto* item : items) {
            totalCount += item->count;
            if (totalCount >= count) {
                return true;
            }
        }

        return false;
    }

private:
    struct AddItemResult {
        enum Result {
            SUCCESS,
            INVALID_ITEM,
            BAG_FULL,
            PARTIAL_SUCCESS,
        };

        Result result;
        int addedCount;
        std::vector<uint64_t> affectedSlots;
    };

    struct PlayerInventory {
        std::map<uint64_t, ItemInstance> items;  // uniqueId -> item
        int mainBagSize = DEFAULT_MAIN_BAG_SIZE;
        int warehouseSize = WAREHOUSE_SIZE;
    };

    AddItemResult addStackableItem(uint64_t playerId, ItemTemplate* tmpl,
                                   int count, BindType bindType) {
        AddItemResult result{AddItemResult::SUCCESS, 0, {}};

        auto* inv = getOrCreateInventory(playerId);

        // 先尝试堆叠到现有物品
        auto items = getItemsByTemplate(playerId, tmpl->id);
        for (auto* item : items) {
            if (item->count < tmpl->maxStack && item->bindType == bindType) {
                int canAdd = std::min(tmpl->maxStack - item->count, count);
                item->count += canAdd;
                count -= canAdd;
                result.addedCount += canAdd;
                result.affectedSlots.push_back(item->uniqueId);

                database_->execute(fmt::format(
                    "UPDATE player_bag SET item_count = {} WHERE id = {}",
                    item->count, item->uniqueId
                ));

                if (count <= 0) break;
            }
        }

        // 剩余物品需要新格子
        while (count > 0) {
            int emptySlot = findEmptySlot(playerId, BagType::MAIN);
            if (emptySlot < 0) {
                result.result = AddItemResult::BAG_FULL;
                break;
            }

            int addCount = std::min(tmpl->maxStack, count);
            uint64_t uniqueId = createItemInstance(playerId, tmpl->id,
                                                   addCount, BagType::MAIN,
                                                   emptySlot, bindType);

            count -= addCount;
            result.addedCount += addCount;
            result.affectedSlots.push_back(uniqueId);
        }

        if (result.addedCount > 0) {
            notifyItemsAdded(playerId, result.affectedSlots);
        }

        return result;
    }

    AddItemResult addNonStackableItem(uint64_t playerId, ItemTemplate* tmpl,
                                      int count, BindType bindType) {
        AddItemResult result{AddItemResult::SUCCESS, 0, {}};

        for (int i = 0; i < count; ++i) {
            int emptySlot = findEmptySlot(playerId, BagType::MAIN);
            if (emptySlot < 0) {
                result.result = AddItemResult::BAG_FULL;
                break;
            }

            uint64_t uniqueId = createItemInstance(playerId, tmpl->id, 1,
                                                   BagType::MAIN,
                                                   emptySlot, bindType);
            result.addedCount++;
            result.affectedSlots.push_back(uniqueId);
        }

        if (result.addedCount > 0) {
            notifyItemsAdded(playerId, result.affectedSlots);
        }

        return result;
    }

    uint64_t createItemInstance(uint64_t playerId, int templateId, int count,
                               BagType bagType, int slot, BindType bindType) {
        // 生成唯一物品ID
        uint64_t uniqueId = generateUniqueId();

        // 写入数据库
        database_->execute(fmt::format(
            "INSERT INTO player_bag "
            "(player_id, bag_type, slot_index, item_id, item_count, bind_type) "
            "VALUES ({}, {}, {}, {}, {}, {})",
            playerId, static_cast<int>(bagType), slot, templateId, count,
            static_cast<int>(bindType)
        ));

        // 加载到内存
        ItemInstance instance;
        instance.uniqueId = uniqueId;
        instance.templateId = templateId;
        instance.ownerId = playerId;
        instance.count = count;
        instance.bagType = bagType;
        instance.slot = slot;
        instance.bindType = bindType;
        instance.createTime = getCurrentTime();

        auto* inv = getOrCreateInventory(playerId);
        inv->items[uniqueId] = instance;

        return uniqueId;
    }

    int findEmptySlot(uint64_t playerId, BagType bagType) {
        auto* inv = getInventory(playerId);
        if (!inv) return -1;

        int maxSize = getBagSizeLimit(bagType);

        std::set<int> occupied;
        for (const auto& [uniqueId, item] : inv->items) {
            if (item.bagType == bagType) {
                occupied.insert(item.slot);
            }
        }

        for (int slot = 0; slot < maxSize; ++slot) {
            if (!occupied.contains(slot)) {
                return slot;
            }
        }

        return -1;  // 背包已满
    }

    bool isSlotEmpty(uint64_t playerId, BagType bagType, int slot) {
        auto* inv = getInventory(playerId);
        if (!inv) return true;

        for (const auto& [uniqueId, item] : inv->items) {
            if (item.bagType == bagType && item.slot == slot) {
                return false;
            }
        }

        return true;
    }

    int getBagSizeLimit(BagType bagType) {
        switch (bagType) {
            case BagType::MAIN: return MAX_BAG_SIZE;
            case BagType::WAREHOUSE: return WAREHOUSE_SIZE;
            case BagType::EQUIPMENT: return 15;  // 装备栏固定
            default: return DEFAULT_MAIN_BAG_SIZE;
        }
    }

    void applyItemEffect(uint64_t playerId, const ItemEffect& effect) {
        if (effect.type == "restore_hp") {
            auto* entity = getEntity(playerId);
            if (entity) {
                entity->addHP(effect.value);
            }
        } else if (effect.type == "restore_mp") {
            auto* entity = getEntity(playerId);
            if (entity) {
                entity->addMP(effect.value);
            }
        }
        // 其他效果类型...
    }

    bool checkUseRequirements(uint64_t playerId, ItemTemplate* tmpl) {
        auto* entity = getEntity(playerId);
        if (!entity) return false;

        // 检查等级
        if (entity->getLevel() < tmpl->requiredLevel) {
            return false;
        }

        // 检查职业
        if (!tmpl->requiredClasses.empty()) {
            if (!tmpl->requiredClasses.contains(entity->getClass())) {
                return false;
            }
        }

        return true;
    }

    void bindItem(uint64_t playerId, uint64_t uniqueId) {
        database_->execute(fmt::format(
            "UPDATE player_bag SET bind_type = {} WHERE id = {}",
            static_cast<int>(BindType::PICKUP), uniqueId
        ));

        auto* item = getItemInstance(playerId, uniqueId);
        if (item) {
            item->bindType = BindType::PICKUP;
        }
    }

    std::vector<ItemInstance*> getItemsByTemplate(uint64_t playerId, int templateId) {
        std::vector<ItemInstance*> result;
        auto* inv = getInventory(playerId);
        if (!inv) return result;

        for (auto& [uniqueId, item] : inv->items) {
            if (item.templateId == templateId) {
                result.push_back(&item);
            }
        }

        return result;
    }

    void loadPlayerInventory(uint64_t playerId) {
        PlayerInventory inv;

        auto results = database_->query(fmt::format(
            "SELECT * FROM player_bag WHERE player_id = {}",
            playerId
        ));

        for (const auto& row : results) {
            ItemInstance item;
            item.uniqueId = row["id"].get<uint64_t>();
            item.templateId = row["item_id"].get<int>();
            item.ownerId = playerId;
            item.count = row["item_count"].get<int>();
            item.bagType = static_cast<BagType>(row["bag_type"].get<int>());
            item.slot = row["slot_index"].get<int>();
            item.quality = static_cast<ItemQuality>(row["quality"].get<int>());
            item.bindType = static_cast<BindType>(row["bind_type"].get<int>());

            inv.items[item.uniqueId] = item;
        }

        playerInventories_[playerId] = std::move(inv);
    }

    PlayerInventory* getInventory(uint64_t playerId) {
        auto it = playerInventories_.find(playerId);
        if (it == playerInventories_.end()) {
            loadPlayerInventory(playerId);
            it = playerInventories_.find(playerId);
        }
        return it != playerInventories_.end() ? &it->second : nullptr;
    }

    PlayerInventory* getOrCreateInventory(uint64_t playerId) {
        auto* inv = getInventory(playerId);
        if (!inv) {
            loadPlayerInventory(playerId);
            inv = getInventory(playerId);
        }
        return inv;
    }

    void notifyItemsAdded(uint64_t playerId, const std::vector<uint64_t>& itemIds) {
        sendToClient(playerId, "onItemsAdded", itemIds);
    }

    void notifyItemRemoved(uint64_t playerId, uint64_t uniqueId, int count) {
        sendToClient(playerId, "onItemRemoved", uniqueId, count);
    }

    void notifyItemMoved(uint64_t playerId, uint64_t uniqueId,
                        BagType fromBag, int fromSlot,
                        BagType toBag, int toSlot) {
        sendToClient(playerId, "onItemMoved", uniqueId, fromBag, fromSlot, toBag, toSlot);
    }

    void notifyBagExpanded(uint64_t playerId, int newSize) {
        sendToClient(playerId, "onBagExpanded", newSize);
    }

    void notifyBagSorted(uint64_t playerId, BagType bagType) {
        sendToClient(playerId, "onBagSorted", bagType);
    }

    void notifyItemUsed(uint64_t playerId, uint64_t uniqueId) {
        sendToClient(playerId, "onItemUsed", uniqueId);
    }

    std::unordered_map<uint64_t, PlayerInventory> playerInventories_;
    std::unordered_map<int, ItemTemplate> itemTemplates_;
};
```

---

## 四、KBEngine 背包系统

### 4.1 KBEngine 物品管理

```python
# KBEngine 背包系统实现

# scripts/entities/avatar.py
import KBEngine
from KBEDebug import *

class Avatar(KBEngine.Entity):
    def __init__(self):
        KBEngine.Entity.__init__(self)

        # 背包数据
        self.bagSize = 30
        self.bagItems = {}  # {slot: {item_id, count, unique_id}}

        # 加载背包
        self.loadBag()

    def onClientReady(self):
        """客户端准备好，同步背包数据"""
        self.client.syncBag(self.bagItems, self.bagSize)

    def loadBag(self):
        """从数据库加载背包"""
        # KBEngine 会自动加载 entity 的 properties
        # bagItems 应该在 entitydefs 中定义为 BASE 类型且有 Database=True

        for slot, itemData in self.bagItems.items():
            INFO(f"Loaded item at slot {slot}: {itemData}")

    def addItem(self, itemId, count=1):
        """添加物品"""
        tmpl = ItemTemplates.get(itemId)
        if not tmpl:
            ERROR(f"Invalid item id: {itemId}")
            return False

        added = 0

        # 可堆叠物品，先尝试堆叠
        if tmpl["maxStack"] > 1:
            for slot, itemData in self.bagItems.items():
                if itemData["item_id"] == itemId:
                    canAdd = min(tmpl["maxStack"] - itemData["count"], count)
                    if canAdd > 0:
                        itemData["count"] += canAdd
                        count -= canAdd
                        added += canAdd

                        # 同步到客户端
                        self.client.onItemUpdated(slot, itemData)

                        if count <= 0:
                            break

        # 需要新格子
        while count > 0:
            emptySlot = self.findEmptySlot()
            if emptySlot is None:
                ERROR("Bag is full!")
                self.client.onAddItemFailed(itemId, count, "Bag full")
                break

            addCount = min(tmpl["maxStack"], count)
            uniqueId = self.generateUniqueId()

            self.bagItems[emptySlot] = {
                "item_id": itemId,
                "count": addCount,
                "unique_id": uniqueId
            }

            count -= addCount
            added += addCount

            # 同步到客户端
            self.client.onItemAdded(emptySlot, self.bagItems[emptySlot])

        # 标记需要保存
        self.writeToDB()

        INFO(f"Added {added} of item {itemId}")
        return added > 0

    def removeItem(self, slot, count=1):
        """移除物品"""
        if slot not in self.bagItems:
            return False

        itemData = self.bagItems[slot]

        if itemData["count"] < count:
            return False

        if itemData["count"] == count:
            # 完全移除
            del self.bagItems[slot]
            self.client.onItemRemoved(slot)
        else:
            # 减少数量
            itemData["count"] -= count
            self.client.onItemUpdated(slot, itemData)

        self.writeToDB()
        return True

    def moveItem(self, fromSlot, toSlot):
        """移动物品"""
        if fromSlot not in self.bagItems:
            return False

        if toSlot in self.bagItems:
            return False  # 目标格子已有物品

        self.bagItems[toSlot] = self.bagItems[fromSlot]
        del self.bagItems[fromSlot]

        self.client.onItemMoved(fromSlot, toSlot)
        self.writeToDB()
        return True

    def useItem(self, slot):
        """使用物品"""
        if slot not in self.bagItems:
            return False

        itemData = self.bagItems[slot]
        tmpl = ItemTemplates.get(itemData["item_id"])

        if not tmpl or not tmpl.get("canUse", False):
            return False

        # 检查使用条件
        if not self.checkUseRequirements(tmpl):
            self.client.onUseItemFailed(slot, "Requirements not met")
            return False

        # 应用效果
        effects = tmpl.get("effects", [])
        for effect in effects:
            self.applyEffect(effect)

        # 消耗品使用后减少
        if tmpl["type"] == "consumable":
            self.removeItem(slot, 1)

        self.client.onItemUsed(slot)
        return True

    def findEmptySlot(self):
        """查找空格子"""
        for slot in range(self.bagSize):
            if slot not in self.bagItems:
                return slot
        return None

    def generateUniqueId(self):
        """生成唯一物品ID"""
        import time
        return int(time.time() * 1000000) + self.id % 1000000

    def checkUseRequirements(self, tmpl):
        """检查使用条件"""
        requiredLevel = tmpl.get("requiredLevel", 0)
        if self.level < requiredLevel:
            return False
        return True

    def applyEffect(self, effect):
        """应用物品效果"""
        effectType = effect["type"]

        if effectType == "restore_hp":
            self.addHP(effect["value"])
        elif effectType == "restore_mp":
            self.addMP(effect["value"])
        # 其他效果...


# scripts/data/item_templates.py
ItemTemplates = {
    1001: {
        "id": 1001,
        "name": "初级生命药水",
        "type": "consumable",
        "maxStack": 99,
        "canUse": True,
        "effects": [
            {"type": "restore_hp", "value": 100}
        ]
    },
    1002: {
        "id": 1002,
        "name": "初级魔法药水",
        "type": "consumable",
        "maxStack": 99,
        "canUse": True,
        "effects": [
            {"type": "restore_mp", "value": 50}
        ]
    }
}
```

---

## 五、最佳实践

### 5.1 背包系统设计建议

| 实践 | 说明 |
|------|------|
| **异步加载** | 背包数据异步加载，不阻塞登录 |
| **批量操作** | 批量添加/移除物品减少数据库操作 |
| **客户端预测** | 本地预测操作结果 |
| **延迟保存** | 非关键操作延迟保存 |
| **物品缓存** | 物品模板内存缓存 |

### 5.2 性能优化

```
优化策略:
1. 物品模板只加载一次
2. 背包变化批量保存
3. 使用对象池避免频繁分配
4. 堆叠计算优化
5. 网络同步压缩
```

---

## 六、总结

### 背包系统核心

```
背包系统 = 格子管理 + 物品实例 + 堆叠规则 + 操作约束
- 格子管理负责空间分配
- 物品实例处理具体物品
- 堆叠规则优化空间利用
- 服务端权威防止作弊
```

---

## 参考资料

- [KBEngine EntityDef 物品定义](https://kbengine.github.io/docs/programming/entitydefs.html)
- [KBEngine 数据库存储](https://kbengine.github.io/docs/programming/db.html)
- [游戏背包系统设计](https://www.gamedev.net/)
