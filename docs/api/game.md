---
title: Game API
icon: game
prev: /api/README.md
---

# Game API

## apollo::game::Entity

```cpp
namespace apollo::game {
class Entity {
public:
    Entity(int64_t id);
    virtual ~Entity();

    // ID
    int64_t id() const;

    // 类型
    EntityType type() const;
    void setType(EntityType type);

    // 位置
    Position position() const;
    void setPosition(const Position& pos);

    // 旋转
    Rotation rotation() const;
    void setRotation(const Rotation& rot);

    // 组件
    template<typename T>
    std::shared_ptr<T> addComponent();

    template<typename T>
    std::shared_ptr<T> getComponent();

    template<typename T>
    void removeComponent();

    // 更新
    virtual void update(float dt);
};
}
```

**线程安全**: ❌ 必须在同一线程调用

---

## apollo::game::Component

```cpp
namespace apollo::game {
class Component {
public:
    Component();
    virtual ~Component();

    // 所属实体
    Entity* entity() const;

    // 初始化
    virtual void onAttach() {}
    virtual void onDetach() {}

    // 更新
    virtual void update(float dt) {}
};
}
```

---

## apollo::game::world::AOIManager

```cpp
namespace apollo::game::world {
class AOIManager {
public:
    AOIManager(float width, float height, float gridSize);

    // 实体进入
    void enter(EntityPtr entity, const Position& pos);

    // 移动
    void move(EntityPtr entity, const Position& newPos);

    // 离开
    void leave(EntityPtr entity);

    // 获取视野内实体
    std::vector<EntityPtr> getViewers(const Position& pos, float radius);
    std::vector<EntityPtr> getViewers(EntityPtr entity);

    // 格子信息
    int getGridX(float x) const;
    int getGridY(float y) const;
    int getGridId(const Position& pos) const;
};
}
```

**线程安全**: ❌ 必须在同一线程调用

---

## apollo::game::battle::ECSWorld

```cpp
namespace apollo::game::battle {
class ECSWorld {
public:
    static std::shared_ptr<ECSWorld> create();

    // 创建实体
    Entity* createEntity();

    // 销毁实体
    void destroyEntity(Entity* entity);

    // 添加系统
    template<typename T, typename... Args>
    void addSystem(Args&&... args);

    // 更新
    void update(float dt);

    // 查询
    template<typename... Components>
    void each(std::function<void(Entity*, Components*...)> func);

    // 获取实体
    Entity* getEntity(int64_t id);
};
}
```

**线程安全**: ❌ 必须在同一线程调用

---

## apollo::game::battle::ECSSystem

```cpp
namespace apollo::game::battle {
class ECSSystem {
public:
    virtual ~ECSSystem() = default;

    // 更新
    virtual void update(ECSWorld* world, float dt) = 0;

    // 优先级
    virtual int priority() const { return 0; }
};
}
```

---

## apollo::game::attributes::AttributeManager

```cpp
namespace apollo::game::attributes {
class AttributeManager {
public:
    AttributeManager();

    // 设置基础值
    void set(AttributeType type, int64_t value);

    // 获取计算后的值
    int64_t get(AttributeType type) const;
    int64_t calculate(AttributeType type) const;

    // 添加修饰符
    void addModifier(AttributeType type, const AttributeModifier& modifier);
    void removeModifier(AttributeType type, uint64_t modifierId);

    // 清除修饰符
    void clearModifiers(AttributeType type);

    // 重置
    void reset();
};
}
```

**线程安全**: ⚠️ 需要外部同步

---

## apollo::game::attributes::AttributeType

```cpp
namespace apollo::game::attributes {
enum class AttributeType : uint32_t {
    // 基础属性
    HP = 1,
    MP = 2,
    ATTACK = 3,
    DEFENSE = 4,
    SPEED = 5,

    // 战斗属性
    CRIT_RATE = 10,
    CRIT_DAMAGE = 11,
    PENETRATION = 12,

    // 抗性
    FIRE_RESIST = 20,
    ICE_RESIST = 21,
    POISON_RESIST = 22,
};
}
```

---

## apollo::game::attributes::AttributeModifier

```cpp
namespace apollo::game::attributes {
struct AttributeModifier {
    enum class Type {
        ADD,           // 加法
        ADD_PERCENT,   // 加法百分比
        MUL,           // 乘法
        MUL_PERCENT,   // 乘法百分比
    };

    Type type = Type::ADD;
    int64_t value = 0;
    uint64_t id = 0;  // 修饰符ID，用于移除
    float duration = 0;  // 持续时间（秒），0表示永久
};
}
```
