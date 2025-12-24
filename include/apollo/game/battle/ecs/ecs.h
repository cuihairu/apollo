#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <mutex>

namespace apollo {
namespace ecs {

constexpr size_t MAX_COMPONENTS = 64;

/// 组件类型 ID
using ComponentTypeId = size_t;

/// 实体 ID
using EntityId = uint64_t;

/// 组件掩码
class ComponentMask {
public:
    ComponentMask() = default;

    void set(size_t index, bool value) {
        if (index < MAX_COMPONENTS) {
            if (value) {
                mask_[index / 64] |= (1ULL << (index % 64));
            } else {
                mask_[index / 64] &= ~(1ULL << (index % 64));
            }
        }
    }

    bool test(size_t index) const {
        if (index >= MAX_COMPONENTS) return false;
        return (mask_[index / 64] & (1ULL << (index % 64))) != 0;
    }

    bool any() const {
        for (size_t i = 0; i < (MAX_COMPONENTS / 64); ++i) {
            if (mask_[i] != 0) return true;
        }
        return false;
    }

    bool none() const { return !any(); }

    ComponentMask operator|(const ComponentMask& other) const {
        ComponentMask result = *this;
        for (size_t i = 0; i < (MAX_COMPONENTS / 64); ++i) {
            result.mask_[i] |= other.mask_[i];
        }
        return result;
    }

    ComponentMask operator&(const ComponentMask& other) const {
        ComponentMask result = *this;
        for (size_t i = 0; i < (MAX_COMPONENTS / 64); ++i) {
            result.mask_[i] &= other.mask_[i];
        }
        return result;
    }

    bool isSubsetOf(const ComponentMask& other) const {
        for (size_t i = 0; i < (MAX_COMPONENTS / 64); ++i) {
            if ((mask_[i] & other.mask_[i]) != mask_[i]) {
                return false;
            }
        }
        return true;
    }

private:
    uint64_t mask_[MAX_COMPONENTS / 64] = {0};
};

/// 组件基类
struct IComponent {
    virtual ~IComponent() = default;
};

/// 组件类型信息
template<typename T>
struct ComponentTypeInfo {
    static ComponentTypeId id() {
        static ComponentTypeId id = nextId_++;
        return id;
    }

private:
    static ComponentTypeId nextId_;
};

template<typename T>
ComponentTypeId ComponentTypeInfo<T>::nextId_ = 0;

/// 实体类
class Entity {
public:
    Entity() : id_(0), world_(nullptr), valid_(false) {}
    Entity(EntityId id, class World* world) : id_(id), world_(world), valid_(true) {}

    EntityId getId() const { return id_; }
    bool isValid() const { return valid_; }

    template<typename T, typename... Args>
    T* addComponent(Args&&... args);

    template<typename T>
    T* getComponent();

    template<typename T>
    void removeComponent();

    template<typename T>
    bool hasComponent() const;

    void destroy();

private:
    EntityId id_;
    World* world_;
    bool valid_;
};

/// 组件池
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void* get(EntityId entityId) = 0;
    virtual void* add(EntityId entityId) = 0;
    virtual void remove(EntityId entityId) = 0;
    virtual bool has(EntityId entityId) const = 0;
    virtual void clear() = 0;
};

template<typename T>
class ComponentPool : public IComponentPool {
public:
    T* get(EntityId entityId) override {
        auto it = components_.find(entityId);
        return (it != components_.end()) ? &it->second : nullptr;
    }

    void* get(EntityId entityId) override {
        return get(entityId);
    }

    void* add(EntityId entityId) override {
        auto& component = components_[entityId];
        return &component;
    }

    template<typename... Args>
    T* emplace(EntityId entityId, Args&&... args) {
        auto it = components_.find(entityId);
        if (it != components_.end()) {
            it->second = T(std::forward<Args>(args)...);
            return &it->second;
        }
        auto& component = components_[entityId];
        component = T(std::forward<Args>(args)...);
        return &component;
    }

    void remove(EntityId entityId) override {
        components_.erase(entityId);
    }

    bool has(EntityId entityId) const override {
        return components_.find(entityId) != components_.end();
    }

    void clear() override {
        components_.clear();
    }

    const std::unordered_map<EntityId, T>& getAll() const {
        return components_;
    }

private:
    std::unordered_map<EntityId, T> components_;
};

/// 世界/场景管理器
class World {
public:
    World();
    ~World();

    /// 创建实体
    Entity createEntity();

    /// 获取实体
    Entity getEntity(EntityId entityId);

    /// 销毁实体
    void destroyEntity(EntityId entityId);

    /// 创建实体并指定 ID（用于网络同步）
    Entity createEntityWithId(EntityId entityId);

    template<typename T, typename... Args>
    T* addComponent(EntityId entityId, Args&&... args);

    template<typename T>
    T* getComponent(EntityId entityId);

    template<typename T>
    void removeComponent(EntityId entityId);

    template<typename T>
    bool hasComponent(EntityId entityId) const;

    /// 获取实体组件掩码
    const ComponentMask& getComponentMask(EntityId entityId) const;

    /// 检查实体是否有效
    bool isEntityValid(EntityId entityId) const;

    /// 视图 - 遍历拥有特定组件的实体
    template<typename... Components>
    class View;

    template<typename... Components>
    View<Components...> view();

    /// 每一帧更新
    void update(float deltaTime);

    /// 获取实体数量
    size_t getEntityCount() const { return entities_.size(); }

    /// 获取活跃实体数量
    size_t getAliveEntityCount() const { return aliveEntityCount_; }

private:
    template<typename T>
    ComponentPool<T>* getOrCreatePool();

    EntityId nextEntityId_;
    size_t aliveEntityCount_;

    struct EntityData {
        bool alive;
        ComponentMask mask;
    };

    std::unordered_map<EntityId, EntityData> entities_;
    std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentPool>> componentPools_;
    std::mutex mutex_;
};

/// 视图 - 用于遍历拥有特定组件的实体
template<typename... Components>
class World::View {
public:
    View(World* world) : world_(world) {}

    /// 遍历所有拥有指定组件的实体
    template<typename F>
    void each(F&& func) {
        auto& entities = world_->entities_;
        ComponentMask required;

        // 构建所需组件掩码
        for (ComponentTypeId id : {ComponentTypeInfo<Components>::id()...}) {
            required.set(id, true);
        }

        for (auto& pair : entities) {
            if (!pair.second.alive) continue;

            if (pair.second.mask.isSubsetOf(required)) {
                Entity entity(pair.first, world_);
                func(entity, *world_->getComponent<Components>(pair.first)...);
            }
        }
    }

    /// 获取所有匹配的实体 ID
    std::vector<EntityId> entities() const {
        std::vector<EntityId> result;
        auto& entities = world_->entities_;
        ComponentMask required;

        for (ComponentTypeId id : {ComponentTypeInfo<Components>::id()...}) {
            required.set(id, true);
        }

        for (auto& pair : entities) {
            if (pair.second.alive && pair.second.mask.isSubsetOf(required)) {
                result.push_back(pair.first);
            }
        }

        return result;
    }

private:
    World* world_;
};

/// 系统 - 处理特定组件的逻辑
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void update(World* world, float deltaTime) = 0;
};

/// 系统管理器
class SystemManager {
public:
    template<typename T, typename... Args>
    T* addSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        systems_.push_back(std::move(system));
        return ptr;
    }

    void update(World* world, float deltaTime) {
        for (auto& system : systems_) {
            system->update(world, deltaTime);
        }
    }

    void clear() {
        systems_.clear();
    }

private:
    std::vector<std::unique_ptr<ISystem>> systems_;
};

//==============================================================================
// 模板方法实现
//==============================================================================

inline World::World() : nextEntityId_(1), aliveEntityCount_(0) {}

inline World::~World() {
    std::lock_guard<std::mutex> lock(mutex_);
    componentPools_.clear();
    entities_.clear();
}

inline Entity World::createEntity() {
    std::lock_guard<std::mutex> lock(mutex_);

    EntityId id = nextEntityId_++;
    EntityData data;
    data.alive = true;
    entities_[id] = data;
    ++aliveEntityCount_;

    return Entity(id, this);
}

inline Entity World::createEntityWithId(EntityId entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    EntityData data;
    data.alive = true;
    entities_[entityId] = data;
    ++aliveEntityCount_;

    if (entityId >= nextEntityId_) {
        nextEntityId_ = entityId + 1;
    }

    return Entity(entityId, this);
}

inline Entity World::getEntity(EntityId entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it != entities_.end() && it->second.alive) {
        return Entity(entityId, this);
    }
    return Entity();
}

inline void World::destroyEntity(EntityId entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it != entities_.end() && it->second.alive) {
        it->second.alive = false;
        it->second.mask = ComponentMask();
        --aliveEntityCount_;

        // 移除所有组件
        for (auto& pair : componentPools_) {
            pair.second->remove(entityId);
        }
    }
}

template<typename T>
ComponentPool<T>* World::getOrCreatePool() {
    ComponentTypeId id = ComponentTypeInfo<T>::id();

    auto it = componentPools_.find(id);
    if (it == componentPools_.end()) {
        auto pool = std::make_unique<ComponentPool<T>>();
        ComponentPool<T>* ptr = pool.get();
        componentPools_[id] = std::move(pool);
        return ptr;
    }

    return static_cast<ComponentPool<T>*>(it->second.get());
}

template<typename T, typename... Args>
T* World::addComponent(EntityId entityId, Args&&... args) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it == entities_.end() || !it->second.alive) {
        return nullptr;
    }

    auto pool = getOrCreatePool<T>();
    T* component = pool->emplace(entityId, std::forward<Args>(args)...);

    it->second.mask.set(ComponentTypeInfo<T>::id(), true);

    return component;
}

template<typename T>
T* World::getComponent(EntityId entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto poolIt = componentPools_.find(ComponentTypeInfo<T>::id());
    if (poolIt == componentPools_.end()) {
        return nullptr;
    }

    auto pool = static_cast<ComponentPool<T>*>(poolIt->second.get());
    return pool->get(entityId);
}

template<typename T>
void World::removeComponent(EntityId entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it == entities_.end()) {
        return;
    }

    it->second.mask.set(ComponentTypeInfo<T>::id(), false);

    auto poolIt = componentPools_.find(ComponentTypeInfo<T>::id());
    if (poolIt != componentPools_.end()) {
        poolIt->second->remove(entityId);
    }
}

template<typename T>
bool World::hasComponent(EntityId entityId) const {
    auto it = entities_.find(entityId);
    if (it == entities_.end()) {
        return false;
    }
    return it->second.mask.test(ComponentTypeInfo<T>::id());
}

inline const ComponentMask& World::getComponentMask(EntityId entityId) const {
    static const ComponentMask emptyMask;
    auto it = entities_.find(entityId);
    return (it != entities_.end()) ? it->second.mask : emptyMask;
}

inline bool World::isEntityValid(EntityId entityId) const {
    auto it = entities_.find(entityId);
    return it != entities_.end() && it->second.alive;
}

inline void World::update(float deltaTime) {
    // 由外部 SystemManager 调用
}

template<typename... Components>
World::View<Components...> World::view() {
    return View<Components...>(this);
}

//==============================================================================
// Entity 模板方法实现
//==============================================================================

template<typename T, typename... Args>
T* Entity::addComponent(Args&&... args) {
    if (!valid_ || !world_) return nullptr;
    return world_->addComponent<T>(id_, std::forward<Args>(args)...);
}

template<typename T>
T* Entity::getComponent() {
    if (!valid_ || !world_) return nullptr;
    return world_->getComponent<T>(id_);
}

template<typename T>
void Entity::removeComponent() {
    if (!valid_ || !world_) return;
    world_->removeComponent<T>(id_);
}

template<typename T>
bool Entity::hasComponent() const {
    if (!valid_ || !world_) return false;
    return world_->hasComponent<T>(id_);
}

inline void Entity::destroy() {
    if (valid_ && world_) {
        world_->destroyEntity(id_);
        valid_ = false;
    }
}

} // namespace ecs
} // namespace apollo
