#pragma once

#include "apollo/bw/runtime.h"

namespace apollo::bw {

class BigWorld {
public:
    static Runtime& instance();

    static double time();
    static uint64_t timeMs();
    static void update();

    static void registerEntityFactory(const std::string& typeName, Runtime::EntityFactory factory);
    static std::shared_ptr<Entity> createEntity(const std::string& typeName);
    static std::shared_ptr<Entity> entity(EntityId id);
    static std::vector<std::shared_ptr<Entity>> entities();
    static bool destroyEntity(EntityId id);

    static TimerId callback(double delaySeconds, std::function<void()> fn);
    static bool cancelCallback(TimerId callbackId);

    static TimerId addTimer(double initialOffsetSeconds,
                            double repeatOffsetSeconds,
                            std::function<void(TimerId, int32_t)> fn,
                            int32_t userArg = 0);
    static bool delTimer(TimerId timerId);
};

} // namespace apollo::bw
