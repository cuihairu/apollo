---
title: Game 模块
icon: game
order: 7
category:
  - 模块
tag:
  - game
  - 游戏
---

# Game 模块

Game 模块提供游戏领域层功能，包括实体管理、AOI、战斗、属性等。

## Entity

游戏实体基类。

```cpp
#include <apollo/game/entity.hpp>

using namespace apollo::game;

class Player : public Entity {
public:
    Player(int64_t id) : Entity(id) {}

    void update(float dt) override {
        // 每帧更新
    }

private:
    int level_ = 1;
    int64_t exp_ = 0;
};

// 创建实体
auto player = std::make_shared<Player>(12345);
player->setPosition({100, 200});
player->setRotation({0, 0, 0});
```

## 组件系统

```cpp
// 定义组件
class HealthComponent : public Component {
public:
    PROPERTY(int, hp).defaultValue(100);
    PROPERTY(int, maxHp).defaultValue(100);

    void takeDamage(int damage) {
        hp = std::max(0, hp - damage);
    }

    void heal(int amount) {
        hp = std::min(maxHp, hp + amount);
    }
};

class AttackComponent : public Component {
public:
    PROPERTY(int, attack).defaultValue(10);

    int calculateDamage() {
        return attack;
    }
};

// 使用组件
auto entity = std::make_shared<Entity>();
entity->addComponent<HealthComponent>();
entity->addComponent<AttackComponent>();

// 获取组件
auto health = entity->getComponent<HealthComponent>();
health->takeDamage(20);
```

## AOI 系统

```cpp
#include <apollo/game/world/aoi_manager.hpp>

// 创建 AOI 管理器
auto aoi = std::make_shared<AOIManager>(1000, 1000, 100);

// 实体进入
auto entity = std::make_shared<Entity>(12345);
aoi->enter(entity, {100, 100});

// 移动
aoi->move(entity, {200, 200});

// 获取视野内的实体
auto viewers = aoi->getViewers({100, 100}, 200);

// 实体离开
aoi->leave(entity);
```

## 战斗系统 (ECS)

```cpp
#include <apollo/game/battle/ecs.hpp>

using namespace apollo::game::battle;

// 定义组件
struct Position {
    float x, y;
};

struct Health {
    int current;
    int max;
};

struct Damage {
    int value;
};

// 创建世界
auto world = ECSWorld::create();

// 创建实体
auto entity = world->createEntity();
entity->add<Position>(100, 200);
entity->add<Health>(100, 100);

// 创建系统
class DamageSystem : public ECSSystem {
public:
    void update(ECSWorld* world, float dt) override {
        world->each<Damage, Health>([](Entity* entity, Damage* dmg, Health* hp) {
            hp->current -= dmg->value;
            entity->remove<Damage>();
        });
    }
};

world->addSystem<DamageSystem>();

// 游戏循环
while (running) {
    world->update(dt);
}
```

## 属性系统

```cpp
#include <apollo/game/attributes/attribute_manager.hpp>

using namespace apollo::game;

auto attrs = std::make_shared<AttributeManager>();

// 添加基础属性
attrs->set(AttributeType::HP, 100);
attrs->set(AttributeType::MP, 50);
attrs->set(AttributeType::ATTACK, 10);
attrs->set(AttributeType::DEFENSE, 5);

// 添加加成
attrs->addModifier(AttributeType::HP, AttributeModifier{
    .type = ModifierType::ADD_PERCENT,
    .value = 20  // +20%
});

// 计算最终值
int finalHp = attrs->calculate(AttributeType::HP);  // 120
```

## NPC AI

```cpp
#include <apollo/game/ai/ai_controller.hpp>

class MonsterAI : public AIController {
protected:
    void onThink(float dt) override {
        auto target = findTarget();
        if (target) {
            attack(target);
        } else {
            patrol();
        }
    }

private:
    EntityPtr findTarget() {
        // 查找攻击范围内的玩家
        auto entities = aoi_->getViewers(entity_->position(), 100);
        for (auto& e : entities) {
            if (e->type() == EntityType::PLAYER) {
                return e;
            }
        }
        return nullptr;
    }

    void attack(EntityPtr target) {
        // 攻击逻辑
    }

    void patrol() {
        // 巡逻逻辑
    }
};
```

## 依赖

- apollo::core
- apollo::base

## 链接

```cmake
find_package(apollo-game-core REQUIRED)
find_package(apollo-game-world REQUIRED)

target_link_libraries(my_app
    apollo::game_core
    apollo::game_world
)
```
