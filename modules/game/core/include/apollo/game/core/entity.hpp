#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace apollo::game::core {

class EntityId {
public:
    constexpr EntityId() : id_(0) {}
    constexpr explicit EntityId(uint64_t id) : id_(id) {}

    constexpr uint64_t value() const { return id_; }
    constexpr bool is_valid() const { return id_ != 0; }
    constexpr explicit operator bool() const { return is_valid(); }

    constexpr bool operator==(const EntityId& other) const { return id_ == other.id_; }
    constexpr bool operator!=(const EntityId& other) const { return id_ != other.id_; }
    constexpr bool operator<(const EntityId& other) const { return id_ < other.id_; }

    static constexpr EntityId invalid() { return EntityId{}; }

private:
    uint64_t id_;
};

class IEntity {
public:
    virtual ~IEntity() = default;

    virtual EntityId get_id() const = 0;
    virtual std::string get_type() const = 0;

    virtual void on_spawn() {}
    virtual void on_despawn() {}
    virtual void on_update(float delta_time) {}
};

using EntityPtr = std::shared_ptr<IEntity>;

class IEntityComponent {
public:
    virtual ~IEntityComponent() = default;

    virtual std::string get_type_name() const = 0;

    virtual void on_attach(IEntity* entity) {}
    virtual void on_detach() {}
    virtual void on_update(float delta_time) {}
};

using ComponentPtr = std::shared_ptr<IEntityComponent>;

class Entity : public IEntity {
public:
    explicit Entity(EntityId id, std::string type = "Entity")
        : id_(id), type_(std::move(type)) {}

    EntityId get_id() const override { return id_; }
    std::string get_type() const override { return type_; }

    void on_spawn() override;
    void on_despawn() override;
    void on_update(float delta_time) override;

    template <typename T>
    std::shared_ptr<T> add_component() {
        auto component = std::make_shared<T>();
        add_component(component);
        return component;
    }

    void add_component(ComponentPtr component);
    void remove_component(const std::string& component_type);
    template <typename T>
    std::shared_ptr<T> get_component() const;

private:
    EntityId id_;
    std::string type_;
    std::unordered_map<std::string, ComponentPtr> components_;
};

} // namespace apollo::game::core
