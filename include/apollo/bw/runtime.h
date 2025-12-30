#pragma once

#include "apollo/bw/entity.h"
#include "apollo/bw/types.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::bw {

class Runtime {
public:
    using EntityFactory = std::function<std::shared_ptr<Entity>()>;

    Runtime();
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&) = delete;
    Runtime& operator=(Runtime&&) = delete;

    double time() const;
    uint64_t timeMs() const;

    void update();

    void registerEntityFactory(const std::string& typeName, EntityFactory factory);

    std::shared_ptr<Entity> createEntity(const std::string& typeName);
    std::shared_ptr<Entity> addEntity(std::shared_ptr<Entity> entity);

    std::shared_ptr<Entity> findEntity(EntityId id) const;
    std::vector<std::shared_ptr<Entity>> listEntities() const;
    bool destroyEntity(EntityId id);

    TimerId addEntityTimer(Entity& entity,
                           double initialOffsetSeconds,
                           double repeatOffsetSeconds,
                           int32_t userArg);
    bool delEntityTimer(Entity& entity, TimerId timerId);

    TimerId callback(double delaySeconds, std::function<void()> fn);
    bool cancelCallback(TimerId callbackId);

    TimerId addTimer(double initialOffsetSeconds,
                     double repeatOffsetSeconds,
                     std::function<void(TimerId, int32_t)> fn,
                     int32_t userArg = 0);
    bool delTimer(TimerId timerId);

private:
    struct TimerEntry;

    TimerId addTimerInternal(EntityId ownerId,
                             double initialOffsetSeconds,
                             double repeatOffsetSeconds,
                             int32_t userArg,
                             std::function<void()> callback,
                             bool isCallback);
    bool delTimerInternal(TimerId timerId);

    EntityId nextEntityId_ = 1;
    TimerId nextTimerId_ = 1;

    std::unordered_map<std::string, EntityFactory> factories_;
    std::unordered_map<EntityId, std::shared_ptr<Entity>> entities_;

    // 定时器状态的具体实现放在 src 中，避免头文件膨胀。
    std::unique_ptr<TimerEntry> timers_;
};

} // namespace apollo::bw
