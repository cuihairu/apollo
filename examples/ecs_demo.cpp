/**
 * @file ecs_demo.cpp
 * @brief ECS (Entity Component System) 使用示例
 *
 * 演示如何使用 Apollo ECS 系统构建游戏逻辑
 */

#include "apollo/game/battle/ecs/ecs.h"
#include <iostream>
#include <cmath>

using namespace apollo::ecs;

//==============================================================================
// 组件定义
//==============================================================================

/**
 * @brief 位置组件
 */
struct Position : public IComponent {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Position() = default;
    Position(float x, float y, float z) : x(x), y(y), z(z) {}

    float distanceTo(const Position& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

/**
 * @brief 速度组件
 */
struct Velocity : public IComponent {
    float vx = 0.0f;
    float vy = 0.0f;
    float vz = 0.0f;

    Velocity() = default;
    Velocity(float vx, float vy, float vz) : vx(vx), vy(vy), vz(vz) {}
};

/**
 * @brief 生命值组件
 */
struct Health : public IComponent {
    float current = 100.0f;
    float maximum = 100.0f;

    Health() = default;
    Health(float hp) : current(hp), maximum(hp) {}

    float getPercent() const {
        return maximum > 0 ? (current / maximum) * 100.0f : 0.0f;
    }

    bool isAlive() const {
        return current > 0;
    }

    void damage(float amount) {
        current = std::max(0.0f, current - amount);
    }

    void heal(float amount) {
        current = std::min(maximum, current + amount);
    }
};

/**
 * @brief 名字组件
 */
struct Name : public IComponent {
    std::string value;

    Name() = default;
    Name(const std::string& name) : value(name) {}
};

/**
 * @brief 玩家标记组件
 */
struct Player : public IComponent {
    uint64_t playerId;
    uint32_t level = 1;
    uint64_t exp = 0;

    Player() = default;
    Player(uint64_t pid) : playerId(pid) {}

    void addExp(uint64_t amount) {
        exp += amount;
        // 简单的升级公式
        while (exp >= level * 1000) {
            exp -= level * 1000;
            ++level;
            std::cout << "  Player leveled up! Now level " << level << std::endl;
        }
    }
};

/**
 * @brief 敌人标记组件
 */
struct Enemy : public IComponent {
    uint32_t enemyType;
    float attackPower = 10.0f;
    float aggroRange = 50.0f;

    Enemy() = default;
    Enemy(uint32_t type) : enemyType(type) {}
};

/**
 * @brief 攻击者组件（用于战斗计算）
 */
struct Attacker : public IComponent {
    float damage = 10.0f;
    float attackRange = 5.0f;
    float attackCooldown = 1.0f;
    float lastAttackTime = 0.0f;

    Attacker() = default;
    Attacker(float dmg) : damage(dmg) {}
};

//==============================================================================
// 系统定义
//==============================================================================

/**
 * @brief 移动系统 - 更新实体位置
 */
class MovementSystem : public ISystem {
public:
    void update(World* world, float deltaTime) override {
        // 遍历所有有位置和速度的实体
        auto view = world->view<Position, Velocity>();

        view.each([&](Entity entity, Position& pos, Velocity& vel) {
            pos.x += vel.vx * deltaTime;
            pos.y += vel.vy * deltaTime;
            pos.z += vel.vz * deltaTime;

            // 简单的摩擦力
            vel.vx *= 0.95f;
            vel.vy *= 0.95f;
            vel.vz *= 0.95f;

            // 停止微小移动
            if (std::abs(vel.vx) < 0.01f) vel.vx = 0;
            if (std::abs(vel.vy) < 0.01f) vel.vy = 0;
            if (std::abs(vel.vz) < 0.01f) vel.vz = 0;
        });
    }
};

/**
 * @brief 战斗系统 - 处理攻击逻辑
 */
class CombatSystem : public ISystem {
public:
    void update(World* world, float deltaTime) override {
        currentTime_ += deltaTime;

        // 获取所有玩家
        auto players = world->view<Player, Position, Health, Attacker>();
        // 获取所有敌人
        auto enemies = world->view<Enemy, Position, Health, Name>();

        // 玩家攻击敌人
        players.each([&](Entity playerEntity, Player& player, Position& playerPos,
                          Health& playerHealth, Attacker& attacker) {
            // 检查玩家是否存活
            if (!playerHealth.isAlive()) return;

            enemies.each([&](Entity enemyEntity, Enemy& enemy, Position& enemyPos,
                             Health& enemyHealth, Name& enemyName) {
                if (!enemyHealth.isAlive()) return;

                float distance = playerPos.distanceTo(enemyPos);

                // 在攻击范围内且冷却完毕
                if (distance <= attacker.attackRange &&
                    currentTime_ - attacker.lastAttackTime >= attacker.attackCooldown) {

                    // 造成伤害
                    enemyHealth.damage(attacker.damage);
                    attacker.lastAttackTime = currentTime_;

                    std::cout << "  Player " << player.playerId
                              << " attacks " << enemyName.value
                              << " for " << attacker.damage << " damage!";

                    if (!enemyHealth.isAlive()) {
                        std::cout << " Enemy defeated!";
                        // 玩家获得经验
                        player.addExp(100);
                    }
                    std::cout << std::endl;
                }
            });
        });

        // 敌人攻击玩家
        enemies.each([&](Entity enemyEntity, Enemy& enemy, Position& enemyPos,
                          Health& enemyHealth) {
            if (!enemyHealth.isAlive()) return;

            players.each([&](Entity playerEntity, Player& player, Position& playerPos,
                              Health& playerHealth, Name& playerName) {
                if (!playerHealth.isAlive()) return;

                float distance = enemyPos.distanceTo(playerPos);

                if (distance <= enemy.aggroRange && currentTime_ - lastEnemyAttack_ >= 2.0f) {
                    playerHealth.damage(enemy.attackPower);
                    lastEnemyAttack_ = currentTime_;

                    std::cout << "  " << playerName.value << " takes "
                              << enemy.attackPower << " damage from enemy!";

                    if (!playerHealth.isAlive()) {
                        std::cout << " Player died!";
                    }
                    std::cout << std::endl;
                }
            });
        });
    }

private:
    float currentTime_ = 0.0f;
    float lastEnemyAttack_ = 0.0f;
};

/**
 * @brief 生命值显示系统
 */
class HealthDisplaySystem : public ISystem {
public:
    void update(World* world, float deltaTime) override {
        auto view = world->view<Health, Name>();

        static float lastUpdate = 0.0f;
        lastUpdate += deltaTime;

        // 每秒更新一次显示
        if (lastUpdate >= 1.0f) {
            lastUpdate = 0.0f;

            std::cout << "\n--- Health Status ---" << std::endl;
            view.each([&](Entity entity, Health& health, Name& name) {
                std::cout << "  " << name.value << ": "
                          << health.current << "/" << health.maximum
                          << " (" << health.getPercent() << "%)"
                          << (health.isAlive() ? "" : " [DEAD]")
                          << std::endl;
            });
        }
    }
};

/**
 * @brief 清理系统 - 移除死亡实体
 */
class CleanupSystem : public ISystem {
public:
    void update(World* world, float deltaTime) override {
        auto view = world->view<Health>();

        std::vector<EntityId> toRemove;
        view.each([&](Entity entity, Health& health) {
            if (!health.isAlive()) {
                toRemove.push_back(entity.getId());
            }
        });

        for (EntityId id : toRemove) {
            std::cout << "\n  Removing dead entity: " << id << std::endl;
            world->destroyEntity(id);
        }
    }
};

//==============================================================================
// 游戏主逻辑
//==============================================================================

class GameWorld {
public:
    void initialize() {
        std::cout << "=== Initializing Game World ===" << std::endl;

        // 注册系统
        systemManager_.addSystem<MovementSystem>();
        systemManager_.addSystem<CombatSystem>();
        systemManager_.addSystem<HealthDisplaySystem>();
        systemManager_.addSystem<CleanupSystem>();

        std::cout << "Systems registered" << std::endl;
    }

    Entity createPlayer(uint64_t playerId, const std::string& name, float x, float y, float z) {
        auto entity = world_.createEntity();
        entity.addComponent<Position>(x, y, z);
        entity.addComponent<Velocity>(0, 0, 0);
        entity.addComponent<Health>(100.0f);
        entity.addComponent<Name>(name);
        entity.addComponent<Player>(playerId);
        entity.addComponent<Attacker>(15.0f);

        std::cout << "Created player: " << name << " at (" << x << ", " << y << ", " << z << ")" << std::endl;
        return entity;
    }

    Entity createEnemy(uint32_t enemyType, const std::string& name, float x, float y, float z, float hp) {
        auto entity = world_.createEntity();
        entity.addComponent<Position>(x, y, z);
        entity.addComponent<Velocity>(0, 0, 0);
        entity.addComponent<Health>(hp);
        entity.addComponent<Name>(name);
        entity.addComponent<Enemy>(enemyType);

        std::cout << "Created enemy: " << name << " at (" << x << ", " << y << ", " << z << ")" << std::endl;
        return entity;
    }

    void update(float deltaTime) {
        systemManager_.updateAll(&world_, deltaTime);
    }

    size_t getEntityCount() const {
        return world_.getAliveEntityCount();
    }

    World& getWorld() { return world_; }

private:
    World world_;
    SystemManager systemManager_;
};

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo ECS Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    GameWorld game;
    game.initialize();

    std::cout << "\n=== Creating Entities ===" << std::endl;

    // 创建玩家
    game.createPlayer(1001, "Hero", 0.0f, 0.0f, 0.0f);
    game.createPlayer(1002, "Warrior", 5.0f, 0.0f, 0.0f);
    game.createPlayer(1003, "Mage", -5.0f, 0.0f, 0.0f);

    // 创建敌人
    game.createEnemy(1, "Goblin", 3.0f, 0.0f, 2.0f, 50.0f);
    game.createEnemy(1, "Goblin", -3.0f, 0.0f, 2.0f, 50.0f);
    game.createEnemy(2, "Orc", 0.0f, 0.0f, 5.0f, 100.0f);
    game.createEnemy(3, "Dragon", 20.0f, 0.0f, 20.0f, 500.0f);

    // 模拟游戏循环
    std::cout << "\n=== Starting Game Loop ===" << std::endl;
    std::cout << "(Simulating 10 seconds of gameplay...)" << std::endl;

    float deltaTime = 0.1f;  // 100ms per tick
    for (int i = 0; i < 100; ++i) {
        game.update(deltaTime);
    }

    std::cout << "\n=== Game Over ===" << std::endl;
    std::cout << "Remaining entities: " << game.getEntityCount() << std::endl;

    return 0;
}
