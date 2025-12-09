#pragma once

#include "ecs.hpp"
#include "apollo/attribute.hpp"
#include "apollo/aoi.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace apollo::battle {

using namespace ecs;

/// 位置组件
struct TransformComponent : public IComponent {
    Vector3 position;
    Vector3 rotation;  // 欧拉角
    Vector3 scale;

    TransformComponent() : scale(1.0f, 1.0f, 1.0f) {}
    TransformComponent(const Vector3& pos) : position(pos), scale(1.0f, 1.0f, 1.0f) {}

    virtual size_t GetTypeId() const override {
        return typeid(TransformComponent).hash_code();
    }
};

/// 基础属性组件
struct AttributeComponent : public IComponent {
    AttributeContainer attributes;

    AttributeComponent() {
        // 初始化基础属性
        attributes.Set("hp", 100);
        attributes.Set("maxHp", 100);
        attributes.Set("mp", 100);
        attributes.Set("maxMp", 100);
        attributes.Set("attack", 10);
        attributes.Set("defense", 5);
        attributes.Set("speed", 5.0f);
        attributes.Set("level", 1);
        attributes.Set("exp", 0);
    }

    virtual size_t GetTypeId() const override {
        return typeid(AttributeComponent).hash_code();
    }

    // 便捷访问方法
    int32_t GetHp() const { return attributes.Get<int32_t>("hp"); }
    void SetHp(int32_t hp) { attributes.Set("hp", hp); }
    int32_t GetMaxHp() const { return attributes.Get<int32_t>("maxHp"); }

    int32_t GetMp() const { return attributes.Get<int32_t>("mp"); }
    void SetMp(int32_t mp) { attributes.Set("mp", mp); }
    int32_t GetMaxMp() const { return attributes.Get<int32_t>("maxMp"); }

    int32_t GetAttack() const { return attributes.Get<int32_t>("attack"); }
    int32_t GetDefense() const { return attributes.Get<int32_t>("defense"); }

    float GetSpeed() const { return attributes.Get<float>("speed"); }

    int32_t GetLevel() const { return attributes.Get<int32_t>("level"); }
    void SetLevel(int32_t level) { attributes.Set("level", level); }

    int32_t GetExp() const { return attributes.Get<int32_t>("exp"); }
    void AddExp(int32_t exp) { attributes.Set("exp", GetExp() + exp); }
};

/// 战斗状态组件
struct BattleStateComponent : public IComponent {
    bool inBattle = false;
    uint64_t currentBattleId = 0;
    uint64_t currentTarget = 0;  // 当前目标ID
    uint32_t lastAttackTime = 0;
    uint32_t lastSkillTime = 0;
    bool isDead = false;
    uint32_t deathTime = 0;
    uint32_t respawnTime = 0;
    bool invulnerable = false;
    uint32_t invulnerableEndTime = 0;

    virtual size_t GetTypeId() const override {
        return typeid(BattleStateComponent).hash_code();
    }
};

/// 技能组件
struct SkillComponent : public IComponent {
    struct SkillInfo {
        uint32_t skillId;
        uint32_t level;
        uint32_t cooldown;
        uint32_t lastUsedTime;
        bool isLearned;
    };

    std::unordered_map<uint32_t, SkillInfo> skills;

    virtual size_t GetTypeId() const override {
        return typeid(SkillComponent).hash_code();
    }

    void LearnSkill(uint32_t skillId, uint32_t level = 1) {
        SkillInfo info;
        info.skillId = skillId;
        info.level = level;
        info.cooldown = 0;
        info.lastUsedTime = 0;
        info.isLearned = true;
        skills[skillId] = info;
    }

    bool CanUseSkill(uint32_t skillId, uint32_t currentTime) const {
        auto it = skills.find(skillId);
        if (it == skills.end() || !it->second.isLearned) {
            return false;
        }
        return currentTime - it->second.lastUsedTime >= it->second.cooldown;
    }

    void UseSkill(uint32_t skillId, uint32_t currentTime) {
        auto it = skills.find(skillId);
        if (it != skills.end()) {
            it->second.lastUsedTime = currentTime;
        }
    }
};

/// Buff组件
struct BuffComponent : public IComponent {
    struct BuffInfo {
        uint32_t buffId;
        int32_t value;
        uint32_t startTime;
        uint32_t duration;
        int32_t tickInterval;
        int32_t lastTickTime;
        uint32_t casterId;
        bool isActive;
    };

    std::vector<BuffInfo> buffs;

    virtual size_t GetTypeId() const override {
        return typeid(BuffComponent).hash_code();
    }

    void AddBuff(uint32_t buffId, int32_t value, uint32_t duration,
                  uint32_t casterId, int32_t tickInterval = 0) {
        BuffInfo buff;
        buff.buffId = buffId;
        buff.value = value;
        buff.startTime = GetCurrentTime();
        buff.duration = duration;
        buff.tickInterval = tickInterval;
        buff.lastTickTime = buff.startTime;
        buff.casterId = casterId;
        buff.isActive = true;
        buffs.push_back(buff);
    }

    void RemoveBuff(uint32_t buffId) {
        buffs.erase(
            std::remove_if(buffs.begin(), buffs.end(),
                [buffId](const BuffInfo& buff) {
                    return buff.buffId == buffId;
                }),
            buffs.end()
        );
    }

    void UpdateBuffs(uint32_t currentTime) {
        for (auto& buff : buffs) {
            if (buff.isActive) {
                // 检查是否过期
                if (currentTime - buff.startTime >= buff.duration) {
                    buff.isActive = false;
                    continue;
                }

                // 处理周期性效果
                if (buff.tickInterval > 0 &&
                    currentTime - buff.lastTickTime >= buff.tickInterval) {
                    buff.lastTickTime = currentTime;
                    ProcessBuffTick(buff);
                }
            }
        }

        // 移除非激活的Buff
        buffs.erase(
            std::remove_if(buffs.begin(), buffs.end(),
                [](const BuffInfo& buff) {
                    return !buff.isActive;
                }),
            buffs.end()
        );
    }

    bool HasBuff(uint32_t buffId) const {
        return std::any_of(buffs.begin(), buffs.end(),
            [buffId](const BuffInfo& buff) {
                return buff.buffId == buffId && buff.isActive;
            });
    }

private:
    uint32_t GetCurrentTime() {
        // TODO: 获取游戏时间
        return static_cast<uint32_t>(time(nullptr) * 1000);
    }

    void ProcessBuffTick(const BuffInfo& buff) {
        // TODO: 处理Buff周期性效果
        // 这里可以通过事件系统通知其他系统
    }
};

/// 移动组件
struct MovementComponent : public IComponent {
    Vector3 velocity;
    Vector3 targetPosition;
    bool isMoving = false;
    float speed = 5.0f;
    uint32_t lastUpdateTime = 0;

    virtual size_t GetTypeId() const override {
        return typeid(MovementComponent).hash_code();
    }

    void MoveTo(const Vector3& target, float moveSpeed = -1.0f) {
        targetPosition = target;
        isMoving = true;
        if (moveSpeed > 0) {
            speed = moveSpeed;
        }
    }

    void Stop() {
        isMoving = false;
        velocity = Vector3(0, 0, 0);
    }
};

/// AI组件
struct AIComponent : public IComponent {
    enum class AIState {
        IDLE,
        PATROL,
        CHASE,
        ATTACK,
        FLEE,
        DEAD
    };

    AIState state = AIState::IDLE;
    uint32_t aggroRange = 500;
    uint32_t attackRange = 100;
    uint32_t patrolRadius = 300;
    Vector3 homePosition;
    Vector3 patrolTarget;
    uint32_t lastDecisionTime = 0;
    uint32_t decisionInterval = 1000;  // 决策间隔（毫秒）

    virtual size_t GetTypeId() const override {
        return typeid(AIComponent).hash_code();
    }
};

/// 玩家组件
struct PlayerComponent : public IComponent {
    std::string playerId;
    std::string playerName;
    uint32_t profession = 0;
    uint32_t guildId = 0;
    uint32_t teamId = 0;
    bool isOnline = true;

    virtual size_t GetTypeId() const override {
        return typeid(PlayerComponent).hash_code();
    }
};

/// NPC组件
struct NpcComponent : public IComponent {
    uint32_t npcId;
    std::string npcName;
    uint32_t npcType;  // 0-普通NPC, 1-怪物, 2-Boss, 3-守护者
    uint32_t level = 1;
    uint32_t faction = 0;
    bool isHostile = false;
    uint32_t aiTemplateId = 0;

    virtual size_t GetTypeId() const override {
        return typeid(NpcComponent).hash_code();
    }
};

/// 投射物组件
struct ProjectileComponent : public IComponent {
    uint32_t projectileId;
    uint32_t skillId;
    uint32_t casterId;
    uint64_t targetId;
    Vector3 direction;
    float speed = 10.0f;
    float damage = 0;
    uint32_t createTime;
    float maxDistance = 100.0f;
    bool hasHit = false;

    virtual size_t GetTypeId() const override {
        return typeid(ProjectileComponent).hash_code();
    }
};

/// 区域效果组件
struct AreaEffectComponent : public IComponent {
    uint32_t effectId;
    uint32_t skillId;
    uint32_t casterId;
    Vector3 center;
    float radius = 5.0f;
    uint32_t createTime;
    uint32_t duration;
    uint32_t tickInterval = 1000;
    uint32_t lastTickTime;
    bool isActive = true;

    virtual size_t GetTypeId() const override {
        return typeid(AreaEffectComponent).hash_code();
    }
};

}  // namespace apollo::battle