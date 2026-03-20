# Q54: 如何处理多人同时抢怪/抢资源？

## 问题分析

本题考察对资源竞争处理的了解：
- 怪物归属机制
- 掉落物分配
- 资源锁定与解锁
- 防止抢怪冲突

---

## 一、怪物归属系统

### 1.1 归属规则

```
┌─────────────────────────────────────────────────────────────┐
│                    怪物归属规则                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. 首击归属 (First Hit)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 第一个对怪物造成伤害的玩家获得归属权            │       │
│  │  优点: 简单直观，公平                                │       │
│  │  缺点: 容易被抢怪                                    │       │
│  │  适用: 低级怪物、小怪                                │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. 仇恨归属 (Aggro)                                       │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 仇恨值最高的玩家获得归属权                    │       │
│  │  优点: 保护正在进行战斗的玩家                        │       │
│  │  缺点: 需要维护仇恨列表                              │       │
│  │  适用: 副本怪物、BOSS                               │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. 队伍归属 (Party Tag)                                   │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 队伍成员共享怪物归属权                        │       │
│  │  - 任何队员击中，全队获得归属                        │       │
│  │  - 伤害按队伍计算                                    │       │
│  │  适用: 组队游戏                                      │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. 区域归属 (Area Tag)                                    │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 特定区域的玩家有优先权                        │       │
│  │  适用: 任务区域、动态事件                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 归属标识

```
┌─────────────────────────────────────────────────────────────┐
│                    怪物归属显示                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  客户端显示:                                                │
│  ├── 无归属: 普通怪物名字 (白色)                            │
│  ├── 自己归属: 红色名字 + "归属"标签                         │
│  ├── 队友归属: 橙色名字 + "队伍"标签                         │
│  └── 其他人归属: 灰色名字 + 无法选中                         │
│                                                             │
│  服务端规则:                                                │
│  ├── 归属玩家: 可以造成伤害，获得经验和掉落                  │
│  ├── 归属队伍: 可以造成伤害，共享经验和掉落                 │
│  ├── 非归属玩家: 无法造成伤害（或伤害极低）                  │
│  └── 协助者: 造成伤害减少经验                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、怪物归属实现

### 2.1 归属系统

```cpp
// 怪物归属系统

class MonsterOwnershipSystem {
public:
    // 尝试获取怪物归属
    bool tryClaimMonster(uint64_t playerId, uint64_t monsterId) {
        auto* monster = getEntity(monsterId);
        if (!monster || !monster->isMonster()) {
            return false;
        }

        // 检查怪物是否已有归属
        if (hasOwner(monsterId)) {
            uint64_t ownerId = getOwnerId(monsterId);

            // 检查是否同队
            if (isSameTeam(playerId, ownerId)) {
                return true;  // 队友可以一起打
            }

            // 检查归属是否过期
            if (isOwnershipExpired(monsterId)) {
                clearOwnership(monsterId);
            } else {
                return false;  // 归属于其他人
            }
        }

        // 设置归属
        setOwnership(monsterId, playerId);

        return true;
    }

    // 对怪物造成伤害
    void onMonsterDamaged(uint64_t monsterId, uint64_t attackerId, int damage) {
        auto* monster = getEntity(monsterId);
        if (!monster) return;

        // 检查归属
        if (!hasOwner(monsterId)) {
            // 无归属，设置归属
            setOwnership(monsterId, attackerId);
        } else {
            uint64_t ownerId = getOwnerId(monsterId);

            // 检查是否同队
            if (isSameTeam(attackerId, ownerId)) {
                // 队友伤害，记录
                recordPartyDamage(monsterId, attackerId, damage);
            } else {
                // 非队伍成员伤害，减少经验
                recordContributorDamage(monsterId, attackerId, damage);
            }
        }

        // 更新仇恨
        updateAggro(monsterId, attackerId, damage);
    }

    // 怪物死亡
    void onMonsterDeath(uint64_t monsterId) {
        // 分配经验
        distributeExperience(monsterId);

        // 分配掉落
        distributeLoot(monsterId);

        // 清理归属
        clearOwnership(monsterId);

        // 清理仇恨
        clearAggro(monsterId);
    }

    // 获取归属者
    uint64_t getOwnerId(uint64_t monsterId) {
        auto it = monsterOwnership_.find(monsterId);
        if (it != monsterOwnership_.end()) {
            return it->second.ownerId;
        }
        return 0;
    }

    // 检查是否有归属
    bool hasOwner(uint64_t monsterId) {
        return getOwnerId(monsterId) != 0;
    }

    // 检查是否归属过期
    bool isOwnershipExpired(uint64_t monsterId) {
        auto it = monsterOwnership_.find(monsterId);
        if (it == monsterOwnership_.end()) {
            return true;
        }

        // 如果10秒内没有受到归属者的伤害，归属过期
        uint64_t lastDamageTime = it->second.lastDamageTime;
        return (getCurrentTime() - lastDamageTime) > 10000;
    }

private:
    struct MonsterOwnerInfo {
        uint64_t ownerId;
        uint64_t teamId;
        uint64_t claimTime;
        uint64_t lastDamageTime;

        // 队伍伤害记录
        std::map<uint64_t, uint64_t> partyDamage;  // playerId -> damage

        // 协助者伤害记录 (非队伍成员)
        std::map<uint64_t, uint64_t> contributorDamage;
    };

    void setOwnership(uint64_t monsterId, uint64_t playerId) {
        MonsterOwnerInfo info;
        info.ownerId = playerId;
        info.teamId = getTeamId(playerId);
        info.claimTime = getCurrentTime();
        info.lastDamageTime = getCurrentTime();

        monsterOwnership_[monsterId] = info;

        // 通知客户端
        notifyOwnershipChanged(monsterId, playerId, info.teamId);
    }

    void clearOwnership(uint64_t monsterId) {
        monsterOwnership_.erase(monsterId);
        notifyOwnershipChanged(monsterId, 0, 0);
    }

    void recordPartyDamage(uint64_t monsterId, uint64_t playerId, int damage) {
        auto it = monsterOwnership_.find(monsterId);
        if (it == monsterOwnership_.end()) {
            return;
        }

        it->second.partyDamage[playerId] += damage;
        it->second.lastDamageTime = getCurrentTime();
    }

    void recordContributorDamage(uint64_t monsterId, uint64_t playerId, int damage) {
        auto it = monsterOwnership_.find(monsterId);
        if (it == monsterOwnership_.end()) {
            return;
        }

        // 协助者造成的伤害减少效果
        it->second.contributorDamage[playerId] += damage / 2;
    }

    void updateAggro(uint64_t monsterId, uint64_t playerId, int damage) {
        // 更新仇恨列表
        // ...
    }

    void clearAggro(uint64_t monsterId) {
        // 清理仇恨列表
        // ...
    }

    void distributeExperience(uint64_t monsterId) {
        auto it = monsterOwnership_.find(monsterId);
        if (it == monsterOwnership_.end()) {
            return;
        }

        auto* monster = getEntity(monsterId);
        if (!monster) return;

        uint64_t baseExp = monster->getExperienceReward();

        // 队伍成员分配
        if (!it->second.partyDamage.empty()) {
            // 按伤害比例分配
            uint64_t totalDamage = 0;
            for (const auto& [playerId, damage] : it->second.partyDamage) {
                totalDamage += damage;
            }

            for (const auto& [playerId, damage] : it->second.partyDamage) {
                float ratio = static_cast<float>(damage) / totalDamage;
                uint64_t exp = static_cast<uint64_t>(baseExp * ratio);
                addExperience(playerId, exp);
            }
        }
    }

    void distributeLoot(uint64_t monsterId) {
        // 掉落物分配
        // 详见下节
    }

    void notifyOwnershipChanged(uint64_t monsterId, uint64_t ownerId,
                                uint64_t teamId) {
        // 通知AOI内的玩家
        auto* monster = getEntity(monsterId);
        if (!monster) return;

        for (auto* player : getAOIPlayers(monster)) {
            sendToClient(player->getId(), "onMonsterOwnership",
                       monsterId, ownerId, teamId);
        }
    }

    std::unordered_map<uint64_t, MonsterOwnerInfo> monsterOwnership_;
};
```

---

## 三、掉落分配系统

### 3.1 分配模式

```
┌─────────────────────────────────────────────────────────────┐
│                    掉落分配模式                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  1. FFA (Free For All)                                      │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 任何人都可以拾取                              │       │
│  │  - 先到先得                                        │       │
│  │  - 谁拾到算谁的                                    │       │
│  │  适用: 低级材料、普通怪物                            │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  2. Round Robin (轮流拾取)                                  │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 队伍成员轮流获得拾取权                        │       │
│  │  - 按队伍顺序轮流                                    │       │
│  │  - 公平分配                                        │       │
│  │  适用: 队伍副本                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  3. Need Before Greed (需求优先)                           │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 需求者优先，贪婪者其次                          │       │
│  │  1. 需求: 职业可以使用且需要                          │       │
│  │  2. 贪婪: 为了卖钱                                  │       │
│  │  适用: 高级装备                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  4. Master Looter (队长分配)                               │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 队长获得拾取权，可以分配给队员                  │       │
│  │  适用: 公会活动、团队副本                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  5. 个人掉落 (Personal Loot)                                │
│  ┌─────────────────────────────────────────────────┐       │
│  │  规则: 每个玩家看到独立的掉落                          │       │
│  │  - 不会互相抢夺                                    │       │
│  │  - 每人都能获得奖励                                  │       │
│  │  适用: 大型活动、公共事件                             │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 掉落实现

```cpp
// 掉落系统

class LootSystem {
public:
    enum class LootMode {
        FFA = 0,             // 自由拾取
        ROUND_ROBIN = 1,     // 轮流
        NEED_GREED = 2,      // 需求优先
        MASTER_LOOTER = 3,   // 队长分配
        PERSONAL = 4,        // 个人掉落
    };

    // 生成掉落
    void generateLoot(uint64_t monsterId, uint64_t ownerId) {
        auto* monster = getEntity(monsterId);
        if (!monster) return;

        // 创建掉落物
        LootContainer loot;
        loot.lootId = generateLootId();
        loot.monsterId = monsterId;
        loot.ownerId = ownerId;
        loot.teamId = getTeamId(ownerId);
        loot.mode = determineLootMode(monster);
        loot.position = monster->getPosition();

        // 生成掉落物品
        auto items = calculateLootItems(monster);
        for (const auto& item : items) {
            LootEntry entry;
            entry.itemId = item.itemId;
            entry.count = item.count;
            entry.quality = item.quality;
            entry.lootMode = item.lootMode;  // 需求/贪婪/自由

            loot.entries.push_back(entry);
        }

        // 设置拾取顺序
        if (loot.mode == LootMode::ROUND_ROBIN && loot.teamId > 0) {
            setupRoundRobin(loot);
        }

        lootContainers_[loot.lootId] = loot;

        // 通知附近玩家
        notifyLootSpawned(loot);
    }

    // 拾取物品
    bool lootItem(uint64_t playerId, uint64_t lootId, int entryIndex) {
        auto it = lootContainers_.find(lootId);
        if (it == lootContainers_.end()) {
            return false;
        }

        LootContainer& loot = it->second;

        if (entryIndex < 0 || entryIndex >= loot.entries.size()) {
            return false;
        }

        LootEntry& entry = loot.entries[entryIndex];

        // 检查是否可以拾取
        if (!canLoot(playerId, loot, entry)) {
            return false;
        }

        // 添加物品到背包
        if (!addItem(playerId, entry.itemId, entry.count)) {
            sendError(playerId, "Bag full");
            return false;
        }

        // 标记为已拾取
        entry.lootedBy.push_back(playerId);

        // 通知
        notifyLootTaken(lootId, entryIndex, playerId);

        // 检查是否全部拾取
        checkAllLooted(lootId);

        return true;
    }

    // 需求Roll点
    void rollNeed(uint64_t playerId, uint64_t lootId, int entryIndex) {
        auto it = lootContainers_.find(lootId);
        if (it == lootContainers_.end()) {
            return;
        }

        LootEntry& entry = it->second.entries[entryIndex];

        // 检查是否可以需求
        if (!canNeed(playerId, entry)) {
            return;
        }

        // Roll点
        int roll = rand() % 100 + 1;  // 1-100
        entry.rolls.push_back({playerId, roll, RollType::NEED});

        sendToClient(playerId, "onRollResult", lootId, entryIndex, roll, RollType::NEED);

        // 检查是否所有人都Roll了
        checkRollComplete(it->second, entryIndex);
    }

    // 贪婪Roll点
    void rollGreed(uint64_t playerId, uint64_t lootId, int entryIndex) {
        auto it = lootContainers_.find(lootId);
        if (it == lootContainers_.end()) {
            return;
        }

        LootEntry& entry = it->second.entries[entryIndex];

        // Roll点
        int roll = rand() % 100 + 1;
        entry.rolls.push_back({playerId, roll, RollType::GREED});

        sendToClient(playerId, "onRollResult", lootId, entryIndex, roll, RollType::GREED);

        checkRollComplete(it->second, entryIndex);
    }

private:
    struct LootEntry {
        int itemId;
        int count;
        int quality;
        int lootMode;  // 0:需求/贪婪, 1:只有需求, 2:自由

        std::vector<uint64_t> lootedBy;
        std::vector<LootRoll> rolls;

        bool isLooted() const {
            return lootedBy.size() > 0;
        }
    };

    struct LootRoll {
        uint64_t playerId;
        int roll;
        enum RollType { NEED, GREED } type;
    };

    struct LootContainer {
        uint64_t lootId;
        uint64_t monsterId;
        uint64_t ownerId;
        uint64_t teamId;
        LootMode mode;
        Vector3 position;

        std::vector<LootEntry> entries;

        int roundRobinIndex = 0;
        uint64_t masterLooterId = 0;

        uint64_t createTime;
        uint64_t expireTime;
    };

    bool canLoot(uint64_t playerId, LootContainer& loot, LootEntry& entry) {
        // 已被拾取
        if (entry.isLooted()) {
            return false;
        }

        switch (loot.mode) {
            case LootMode::FFA:
                return true;  // 任何人都可以拾取

            case LootMode::ROUND_ROBIN:
                // 轮到该玩家拾取
                if (loot.teamId > 0) {
                    auto teamMembers = getTeamMembers(loot.teamId);
                    if (loot.roundRobinIndex < teamMembers.size()) {
                        return teamMembers[loot.roundRobinIndex] == playerId;
                    }
                }
                return loot.ownerId == playerId;

            case LootMode::NEED_GREED:
                // 需要Roll点
                return hasWonRoll(playerId, entry);

            case LootMode::MASTER_LOOTER:
                // 只有队长可以拾取
                return playerId == loot.masterLooterId;

            case LootMode::PERSONAL:
                // 个人掉落，只有归属者可见
                return loot.ownerId == playerId;
        }

        return false;
    }

    bool canNeed(uint64_t playerId, LootEntry& entry) {
        // 检查职业是否可以使用
        if (!canUseItem(playerId, entry.itemId)) {
            return false;
        }

        // 检查是否已经Roll过
        for (const auto& roll : entry.rolls) {
            if (roll.playerId == playerId) {
                return false;
            }
        }

        return true;
    }

    void checkRollComplete(LootContainer& loot, int entryIndex) {
        LootEntry& entry = loot.entries[entryIndex];

        // 检查是否所有人都Roll了
        auto teamMembers = getTeamMembers(loot.teamId);
        if (entry.rolls.size() < teamMembers.size()) {
            return;
        }

        // 找出最高的Need Roll
        int highestNeed = -1;
        uint64_t needWinner = 0;

        for (const auto& roll : entry.rolls) {
            if (roll.type == LootRoll::NEED && roll.roll > highestNeed) {
                highestNeed = roll.roll;
                needWinner = roll.playerId;
            }
        }

        if (needWinner != 0) {
            // Need 获胜
            entry.lootedBy.push_back(needWinner);
            notifyRollWinner(loot.lootId, entryIndex, needWinner, RollType::NEED);
        } else {
            // 没有 Need，找出最高的 Greed Roll
            int highestGreed = -1;
            uint64_t greedWinner = 0;

            for (const auto& roll : entry.rolls) {
                if (roll.type == LootRoll::GREED && roll.roll > highestGreed) {
                    highestGreed = roll.roll;
                    greedWinner = roll.playerId;
                }
            }

            if (greedWinner != 0) {
                entry.lootedBy.push_back(greedWinner);
                notifyRollWinner(loot.lootId, entryIndex, greedWinner, RollType::GREED);
            }
        }
    }

    void setupRoundRobin(LootContainer& loot) {
        // 设置初始拾取者
        auto teamMembers = getTeamMembers(loot.teamId);
        if (!teamMembers.empty()) {
            loot.roundRobinIndex = 0;
        }
    }

    std::unordered_map<uint64_t, LootContainer> lootContainers_;
};
```

---

## 四、资源节点

### 4.1 采集点锁定

```cpp
// 资源节点系统

class ResourceNodeSystem {
public:
    // 采集资源
    bool gatherResource(uint64_t playerId, uint64_t nodeId) {
        auto* node = getResourceNode(nodeId);
        if (!node || node->isEmpty()) {
            return false;
        }

        // 检查是否被其他人采集
        if (node->isLocked() && node->lockedBy != playerId) {
            sendError(playerId, "Resource being gathered by someone else");
            return false;
        }

        // 锁定资源
        node->lock(playerId);

        // 开始采集
        startGathering(playerId, nodeId);

        return true;
    }

    // 采集完成
    void onGatherComplete(uint64_t playerId, uint64_t nodeId) {
        auto* node = getResourceNode(nodeId);
        if (!node) return;

        // 给予资源
        giveResource(playerId, node->resourceId, node->amount);

        // 解锁
        node->unlock();

        // 节点变为空
        if (node->isDepleted()) {
            node->setEmpty();
            // 设置刷新时间
            scheduleRespawn(nodeId, node->respawnTime);
        }

        // 通知
        notifyResourceGathered(nodeId, playerId);
    }

    // 取消采集
    void cancelGather(uint64_t playerId, uint64_t nodeId) {
        auto* node = getResourceNode(nodeId);
        if (!node) return;

        node->unlock();
        notifyGatherCancelled(nodeId);
    }

private:
    struct ResourceNode {
        uint64_t nodeId;
        int resourceId;
        int amount;
        bool isEmpty;

        uint64_t lockedBy;
        uint64_t lockTime;
        uint32_t respawnTime;

        bool isLocked() const { return lockedBy != 0; }
        void lock(uint64_t playerId) { lockedBy = playerId; lockTime = getCurrentTime(); }
        void unlock() { lockedBy = 0; }
        bool isDepleted() const { return amount <= 0; }
    };

    std::unordered_map<uint64_t, ResourceNode> resourceNodes_;
};
```

---

## 五、最佳实践

### 5.1 防抢怪设计建议

| 实践 | 说明 |
|------|------|
| **归属标识** | 清晰显示怪物归属 |
| **伤害比例** | 按伤害分配经验 |
| **队伍优先** | 队友共享归属 |
| **锁定机制** | 采集点锁定 |
| **个人掉落** | 大型活动个人掉落 |

---

## 六、总结

### 资源竞争处理核心

```
抢怪处理 = 归属系统 + 掉落分配 + 锁定机制
- 首击/仇恨决定归属
- 掉落按规则分配
- 采集点防止抢夺
- 公平优先于效率
```

---

## 参考资料

- [魔兽世界怪物归属](https://wowpedia.fandom.com/)
- [游戏掉落系统](https://www.gamedev.net/)
