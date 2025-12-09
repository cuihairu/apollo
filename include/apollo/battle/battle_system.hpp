#pragma once

#include "ecs.hpp"
#include "components.hpp"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace apollo::battle {

using namespace ecs;

/// 战斗系统接口
class IBattleSystem {
public:
    virtual ~IBattleSystem() = default;
    virtual void Update(float deltaTime) = 0;
    virtual void Initialize() {}
    virtual void Shutdown() {}
};

/// 移动系统
class MovementSystem : public System {
public:
    void Update(float deltaTime) override {
        auto& world = *world_;
        // TODO: 遍历所有有MovementComponent和TransformComponent的实体
        // 更新位置
    }
};

/// AI系统
class AISystem : public System {
public:
    void Update(float deltaTime) override {
        uint32_t currentTime = GetCurrentTime();

        // 遍历所有AI实体
        // 决策逻辑
    }

private:
    uint32_t GetCurrentTime() {
        return static_cast<uint32_t>(time(nullptr) * 1000);
    }
};

/// 战斗逻辑系统
class BattleSystem : public System {
public:
    void Update(float deltaTime) override {
        ProcessAttack(deltaTime);
        ProcessSkill(deltaTime);
        ProcessBuff(deltaTime);
    }

private:
    void ProcessAttack(float deltaTime);
    void ProcessSkill(float deltaTime);
    void ProcessBuff(float deltaTime);
};

/// 碰撞检测系统
class CollisionSystem : public System {
public:
    void Update(float deltaTime) override {
        CheckProjectileCollisions();
        CheckAreaEffectCollisions();
    }

private:
    void CheckProjectileCollisions();
    void CheckAreaEffectCollisions();
};

/// 战斗世界
class BattleWorld {
public:
    BattleWorld(uint32_t battleId, uint32_t maxEntities = 1000);
    ~BattleWorld();

    /// 初始化战斗世界
    bool Initialize();

    /// 更新战斗世界
    void Update(float deltaTime);

    /// 创建实体
    Entity* CreateEntity(uint64_t entityId);

    /// 销毁实体
    void DestroyEntity(uint64_t entityId);

    /// 获取实体
    Entity* GetEntity(uint64_t entityId);

    /// 添加系统
    void AddSystem(std::shared_ptr<IBattleSystem> system);

    /// 添加玩家
    Entity* AddPlayer(uint64_t playerId, const Vector3& position);

    /// 添加NPC
    Entity* AddNpc(uint32_t npcId, const Vector3& position);

    /// 移除玩家
    void RemovePlayer(uint64_t playerId);

    /// 移除NPC
    void RemoveNpc(uint32_t npcId);

    /// 获取指定范围内的实体
    std::vector<Entity*> GetEntitiesInRange(const Vector3& center, float radius);

    /// 设置战斗结束回调
    void SetBattleEndCallback(std::function<void(bool)> callback) {
        battleEndCallback_ = callback;
    }

    /// 获取战斗ID
    uint32_t GetBattleId() const { return battleId_; }

    /// 是否战斗结束
    bool IsBattleEnd() const { return battleEnd_; }

private:
    World world_;
    uint32_t battleId_;
    std::vector<std::shared_ptr<IBattleSystem>> systems_;
    std::unordered_map<uint64_t, Entity*> players_;
    std::unordered_map<uint32_t, Entity*> npcs_;
    std::function<void(bool)> battleEndCallback_;
    bool battleEnd_ = false;
    uint32_t maxEntities_;
};

/// 战斗管理器
class BattleManager {
public:
    static BattleManager& Instance() {
        static BattleManager instance;
        return instance;
    }

    /// 创建战斗
    uint32_t CreateBattle(const std::vector<uint64_t>& players,
                         const std::vector<uint32_t>& npcs = {});

    /// 销毁战斗
    void DestroyBattle(uint32_t battleId);

    /// 获取战斗世界
    BattleWorld* GetBattle(uint32_t battleId);

    /// 更新所有战斗
    void UpdateAll(float deltaTime);

    /// 玩家加入战斗
    bool PlayerJoinBattle(uint32_t battleId, uint64_t playerId);

    /// 玩家离开战斗
    void PlayerLeaveBattle(uint32_t battleId, uint64_t playerId);

    /// 获取玩家所在的战斗ID
    uint32_t GetPlayerBattleId(uint64_t playerId) const;

    /// 获取战斗统计信息
    struct BattleStats {
        uint32_t totalBattles;
        uint32_t activeBattles;
        size_t totalPlayers;
        size_t totalEntities;
    };

    BattleStats GetStats() const;

private:
    BattleManager() = default;

    std::unordered_map<uint32_t, std::unique_ptr<BattleWorld>> battles_;
    std::unordered_map<uint64_t, uint32_t> playerBattleMap_;
    uint32_t nextBattleId_ = 1;
};

}  // namespace apollo::battle