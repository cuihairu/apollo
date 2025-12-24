#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <bitset>
#include <typeindex>
#include <functional>
#include <cstdint>

namespace apollo::battle::ecs {

constexpr size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;

// 前向声明
class Entity;
class World;
class System;
class ComponentManager;

/// 组件基类
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual size_t GetTypeId() const = 0;
};

/// 组件类型信息
struct ComponentTypeInfo {
    size_t id;
    size_t size;
    std::function<void*(void)> create;
    std::function<void(void*)> destroy;
    std::function<void(void*, const void*)> copy;
};

/// 组件管理器
class ComponentManager {
public:
    template<typename T>
    void RegisterComponent() {
        size_t id = GetComponentTypeId<T>();
        if (componentTypes_.find(id) == componentTypes_.end()) {
            ComponentTypeInfo info;
            info.id = id;
            info.size = sizeof(T);
            info.create = []() { return new T(); };
            info.destroy = [](void* ptr) { delete static_cast<T*>(ptr); };
            info.copy = [](void* dest, const void* src) {
                *static_cast<T*>(dest) = *static_cast<const T*>(src);
            };
            componentTypes_[id] = info;
            componentPools_[id] = std::make_unique<ComponentPool>(sizeof(T));
        }
    }

    template<typename T, typename... Args>
    T* AddComponent(uint32_t entityId, Args&&... args) {
        size_t id = GetComponentTypeId<T>();
        auto pool = componentPools_[id].get();
        void* ptr = pool->Allocate(entityId);
        new(ptr) T(std::forward<Args>(args)...);
        return static_cast<T*>(ptr);
    }

    template<typename T>
    T* GetComponent(uint32_t entityId) {
        size_t id = GetComponentTypeId<T>();
        auto pool = componentPools_[id].get();
        return static_cast<T*>(pool->Get(entityId));
    }

    template<typename T>
    void RemoveComponent(uint32_t entityId) {
        size_t id = GetComponentTypeId<T>();
        auto pool = componentPools_[id].get();
        T* component = static_cast<T*>(pool->Get(entityId));
        if (component) {
            component->~T();
            pool->Deallocate(entityId);
        }
    }

    void RemoveAllComponents(uint32_t entityId) {
        for (auto& pair : componentPools_) {
            pair.second->Deallocate(entityId);
        }
    }

private:
    template<typename T>
    static size_t GetComponentTypeId() {
        static size_t id = nextComponentId_++;
        return id;
    }

    class ComponentPool {
    public:
        ComponentPool(size_t componentSize) : componentSize_(componentSize) {}

        void* Allocate(uint32_t entityId) {
            if (components_.find(entityId) == components_.end()) {
                auto& block = components_[entityId];
                block.data = malloc(componentSize_);
                return block.data;
            }
            return components_[entityId].data;
        }

        void* Get(uint32_t entityId) {
            auto it = components_.find(entityId);
            return (it != components_.end()) ? it->second.data : nullptr;
        }

        void Deallocate(uint32_t entityId) {
            auto it = components_.find(entityId);
            if (it != components_.end()) {
                free(it->second.data);
                components_.erase(it);
            }
        }

    private:
        struct ComponentBlock {
            void* data;
        };

        size_t componentSize_;
        std::unordered_map<uint32_t, ComponentBlock> components_;
    };

    static size_t nextComponentId_;
    std::unordered_map<size_t, ComponentTypeInfo> componentTypes_;
    std::unordered_map<size_t, std::unique_ptr<ComponentPool>> componentPools_;
};

/// 实体类
class Entity {
public:
    uint32_t id;
    bool active = true;

    Entity(uint32_t id) : id(id) {}

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent();

    template<typename T>
    void RemoveComponent();

    template<typename T>
    bool HasComponent();

    void Destroy();

private:
    World* world_ = nullptr;
    friend class World;
};

/// 世界/场景管理器
class World {
public:
    World();
    ~World();

    Entity* CreateEntity() {
        uint32_t id = nextEntityId_++;
        auto entity = std::make_unique<Entity>(id);
        entity->world_ = this;
        entities_[id] = std::move(entity);
        return entities_[id].get();
    }

    void DestroyEntity(uint32_t entityId) {
        if (entities_.find(entityId) != entities_.end()) {
            componentManager_.RemoveAllComponents(entityId);
            entities_.erase(entityId);
        }
    }

    Entity* GetEntity(uint32_t entityId) {
        auto it = entities_.find(entityId);
        return (it != entities_.end()) ? it->second.get() : nullptr;
    }

    template<typename T, typename... Args>
    T* AddComponent(uint32_t entityId, Args&&... args) {
        componentManager_.RegisterComponent<T>();
        return componentManager_.AddComponent<T>(entityId, std::forward<Args>(args)...);
    }

    template<typename T>
    T* GetComponent(uint32_t entityId) {
        return componentManager_.GetComponent<T>(entityId);
    }

    template<typename T>
    void RemoveComponent(uint32_t entityId) {
        componentManager_.RemoveComponent<T>(entityId);
    }

    template<typename T>
    bool HasComponent(uint32_t entityId) {
        return GetComponent<T>(entityId) != nullptr;
    }

    void AddSystem(std::shared_ptr<System> system);
    void Update(float deltaTime);

    ComponentManager& GetComponentManager() { return componentManager_; }

private:
    std::unordered_map<uint32_t, std::unique_ptr<Entity>> entities_;
    std::vector<std::shared_ptr<System>> systems_;
    ComponentManager componentManager_;
    uint32_t nextEntityId_ = 1;
};

/// 系统基类
class System {
public:
    virtual ~System() = default;
    virtual void Update(float deltaTime) = 0;

protected:
    World* world_ = nullptr;
    friend class World;
};

/// 系统管理器
class SystemManager {
public:
    template<typename T>
    void AddSystem(std::shared_ptr<T> system) {
        systems_.push_back(std::static_pointer_cast<System>(system));
    }

    void UpdateAll(float deltaTime) {
        for (auto& system : systems_) {
            system->Update(deltaTime);
        }
    }

    const std::vector<std::shared_ptr<System>>& GetSystems() const {
        return systems_;
    }

private:
    std::vector<std::shared_ptr<System>> systems_;
};

// ---- Entity inline implementations (require World definition) ----

template<typename T, typename... Args>
T* Entity::AddComponent(Args&&... args) {
    return world_->AddComponent<T>(id, std::forward<Args>(args)...);
}

template<typename T>
T* Entity::GetComponent() {
    return world_->GetComponent<T>(id);
}

template<typename T>
void Entity::RemoveComponent() {
    world_->RemoveComponent<T>(id);
}

template<typename T>
bool Entity::HasComponent() {
    return world_->HasComponent<T>(id);
}

inline void Entity::Destroy() {
    active = false;
    world_->DestroyEntity(id);
}

inline void World::AddSystem(std::shared_ptr<System> system) {
    systems_.push_back(system);
    system->world_ = this;
}

inline void World::Update(float deltaTime) {
    for (auto& system : systems_) {
        system->Update(deltaTime);
    }
}

}  // namespace apollo::battle::ecs
