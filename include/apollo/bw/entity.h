#pragma once

#include "apollo/bw/types.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>

namespace apollo::bw {

class Runtime;

class Entity : public std::enable_shared_from_this<Entity> {
public:
    explicit Entity(std::string typeName);
    virtual ~Entity();

    Entity(const Entity&) = delete;
    Entity& operator=(const Entity&) = delete;
    Entity(Entity&&) = delete;
    Entity& operator=(Entity&&) = delete;

    EntityId id() const { return id_; }
    const std::string& typeName() const { return typeName_; }

    TimerId addTimer(double initialOffsetSeconds, double repeatOffsetSeconds, int32_t userArg = 0);
    bool delTimer(TimerId timerId);
    bool destroy();

    virtual void onEnterWorld() {}
    virtual void onLeaveWorld() {}
    virtual void onDestroy() {}
    virtual void onTimer(TimerId /*timerId*/, int32_t /*userArg*/) {}

private:
    friend class Runtime;

    void attach(Runtime& runtime, EntityId id);
    void detach();
    void cancelAllTimers();

    Runtime* runtime_ = nullptr;
    EntityId id_ = INVALID_ENTITY_ID;
    std::string typeName_;
    std::unordered_set<TimerId> timers_;
};

} // namespace apollo::bw
