---
title: Game API
icon: game
prev: /api/README.md
---

# Game API

> 2026-10-04 对账：本页按 HEAD 实况重写。P1-3 后全仓唯一 AOI 实现为
> `SceneAoi`；`ECSWorld`/`ECSSystem` 不存在（ECS 收敛随 P2）；属性为
> `AttributeContainer`/`AttributeManager` 容器模型（修饰符计算未实现）。

## apollo::game::core::EntityId / PlayerId

```cpp
namespace apollo::game::core {
class EntityId {
public:
    constexpr EntityId();
    constexpr explicit EntityId(uint64_t id);
    constexpr uint64_t value() const;
    constexpr bool is_valid() const;
    static constexpr EntityId invalid();
};

// 玩家 ID 强分型（P0-2）：与 EntityId 同构但互不隐式转换——
// PlayerId 标识长期玩家对象（PlayerAnchor 域），EntityId 标识场景内实体
class PlayerId {
public:
    constexpr explicit PlayerId(uint64_t id);
    constexpr uint64_t value() const;
    constexpr bool is_valid() const;
};
}
```

---

## apollo::game::core::Entity

```cpp
namespace apollo::game::core {
class IEntity {
public:
    virtual ~IEntity() = default;
    virtual EntityId get_id() const = 0;
    virtual std::string get_type() const = 0;
    virtual void on_spawn() {}
    virtual void on_despawn() {}
    virtual void on_update(float delta_time) {}
};

class Entity : public IEntity {
public:
    explicit Entity(EntityId id, std::string type = "Entity");

    EntityId get_id() const override;
    std::string get_type() const override;

    void on_spawn() override;
    void on_despawn() override;
    void on_update(float delta_time) override;

    // 组件（IEntityComponent：get_type_name + on_attach/on_detach/on_update）
    template <typename T>
    std::shared_ptr<T> add_component();
    void add_component(ComponentPtr component);
    void remove_component(const std::string& component_type);
    template <typename T>
    std::shared_ptr<T> get_component() const;
};
}
```

**线程安全**: ❌ 必须在同一线程调用（场景线程单写者）

---

## apollo::game::world::SceneAoi

P1-3 收敛后的全仓唯一 AOI 实现：九宫格，Scene 独享、按 scene 隔离，
与实体类型解耦（只认 EntityId + 位置值）。

```cpp
namespace apollo::game::world {
class SceneAoi {
public:
    struct Vec3 { float x = 0.0f; float y = 0.0f; float z = 0.0f; };

    struct Event {
        enum class Kind : std::uint8_t { Enter = 0, Sync, Leave };
        Kind kind = Kind::Enter;
        apollo::game::core::EntityId observer{};  // 事件发给谁
        apollo::game::core::EntityId subject{};   // 关于谁
    };
    using EventSink = std::function<void(const Event&)>;

    void set_event_sink(EventSink sink);   // 缺省静默

    SceneAoi();                            // 1x1 退化网格
    SceneAoi(float width, float height, float grid_size, float view_radius);

    // 集合维护（维护即分发：Enter/Sync/Leave）
    void enter(apollo::game::core::EntityId id, const Vec3& position);
    void move(apollo::game::core::EntityId id, const Vec3& new_position);
    void leave(apollo::game::core::EntityId id);

    // 查询（九宫格 + 半径过滤；viewers_of 含自身）
    std::vector<apollo::game::core::EntityId> viewers_of(apollo::game::core::EntityId id) const;
    std::vector<apollo::game::core::EntityId> viewers_at(const Vec3& position) const;
    bool contains(apollo::game::core::EntityId id) const noexcept;
    std::size_t entity_count() const noexcept;
};
}
```

**线程安全**: ❌ 归场景线程（单写者，无内锁）

---

## apollo::game::world::ViewerState

逐观察者视野水位，AOI 事件分发的差集基准（「谁看见什么」的权威账本）。

```cpp
namespace apollo::game::world {
class ViewerState {
public:
    void reset(apollo::game::core::EntityId observer);   // 登记（重复登记清空重置）
    void drop(apollo::game::core::EntityId observer);    // 注销
    bool tracked(apollo::game::core::EntityId observer) const noexcept;
    bool visible(apollo::game::core::EntityId observer,
                 apollo::game::core::EntityId subject) const noexcept;
    std::vector<apollo::game::core::EntityId> visible_set(
        apollo::game::core::EntityId observer) const;
    bool add(apollo::game::core::EntityId observer,
             apollo::game::core::EntityId subject);      // true = 新进（ENTER）
    bool remove(apollo::game::core::EntityId observer,
                apollo::game::core::EntityId subject);   // true = 出视野（LEAVE）
    std::vector<apollo::game::core::EntityId> remove_everywhere(
        apollo::game::core::EntityId subject);
    std::size_t size() const noexcept;
};
}
```

---

## apollo::game::battle::BattleSystem（骨架）

ECS 收敛随 P2-1/P2-2；现状为实体集合登记 + tick 更新的场景内骨架。

```cpp
namespace apollo::game::battle {
class BattleSystem {
public:
    BattleSystem();
    void update(float delta_time);
    void add_entity(EntityPtr entity);
    void remove_entity(apollo::game::core::EntityId id);
    size_t get_entity_count() const;
};
}
```

---

## apollo::AttributeContainer（属性容器）

头文件 `include/apollo/game/attributes/attribute.hpp`。`AttributeValue` 为
`std::variant<int32_t, int64_t, float, double, bool, std::string>`。

```cpp
namespace apollo {
class AttributeContainer {
public:
    explicit AttributeContainer(uint64_t objectId);

    bool SetAttribute(uint32_t attributeId, const AttributeValue& value);
    bool GetAttribute(uint32_t attributeId, AttributeValue& value) const;
    template<typename T>
    T GetAttribute(uint32_t attributeId, const T& defaultValue = T{}) const;
    bool AddAttribute(uint32_t attributeId, const AttributeValue& delta);
    void SetAttributes(const std::vector<std::pair<uint32_t, AttributeValue>>& attrs);
    const std::unordered_map<uint32_t, AttributeValue>& GetAllAttributes() const;
    void Clear();
    void SetChangeListener(std::function<void(const AttributeChangeEvent&)> listener);

private:
    mutable std::mutex mutex_;   // 内部互斥
};
}
```

---

## apollo::AttributeManager（属性定义注册）

```cpp
namespace apollo {
class AttributeManager {
public:
    static AttributeManager& Instance();

    bool RegisterAttribute(const AttributeDef& def);
    const AttributeDef* GetAttributeDef(uint32_t attributeId) const;
    const std::unordered_map<uint32_t, AttributeDef>& GetAllAttributes() const;

    std::shared_ptr<AttributeContainer> CreateContainer(uint64_t objectId);
    std::shared_ptr<AttributeContainer> GetContainer(uint64_t objectId);
    void DestroyContainer(uint64_t objectId);

    bool LoadFromConfig(const std::string& configFile);

private:
    AttributeManager() = default;   // 单例
    mutable std::mutex mutex_;
};
}
```

`AttributeDef`（属性定义）：`id`/`name`/`type`（值类型枚举 INT32..STRING）/
`defaultValue`/`minValue`/`maxValue`/`persistent`/`syncToClient`。
`AttributeChangeEvent`：`objectId`/`attributeId`/`oldValue`/`newValue`。

**未实现**：修饰符计算（addModifier/AttributeModifier）、逐 viewer delta
同步管线（契约 `attr_batch` 已入 messages.xml，管线未接线）。
