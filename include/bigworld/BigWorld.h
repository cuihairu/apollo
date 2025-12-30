#pragma once

#include "apollo/bw/bigworld.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace BigWorld {

using apollo::bw::Entity;
using apollo::bw::EntityId;
using apollo::bw::TimerId;

inline double time() { return apollo::bw::BigWorld::time(); }
inline uint64_t timeMs() { return apollo::bw::BigWorld::timeMs(); }
inline void update() { apollo::bw::BigWorld::update(); }

inline void registerEntityFactory(const std::string& typeName,
                                  apollo::bw::Runtime::EntityFactory factory) {
    apollo::bw::BigWorld::registerEntityFactory(typeName, std::move(factory));
}

inline std::shared_ptr<Entity> createEntity(const std::string& typeName) {
    return apollo::bw::BigWorld::createEntity(typeName);
}

inline std::shared_ptr<Entity> entity(EntityId id) {
    return apollo::bw::BigWorld::entity(id);
}

inline std::vector<std::shared_ptr<Entity>> entities() {
    return apollo::bw::BigWorld::entities();
}

inline bool destroyEntity(EntityId id) {
    return apollo::bw::BigWorld::destroyEntity(id);
}

inline TimerId callback(double delaySeconds, std::function<void()> fn) {
    return apollo::bw::BigWorld::callback(delaySeconds, std::move(fn));
}

inline bool cancelCallback(TimerId callbackId) {
    return apollo::bw::BigWorld::cancelCallback(callbackId);
}

inline TimerId addTimer(double initialOffsetSeconds,
                        double repeatOffsetSeconds,
                        std::function<void(TimerId, int32_t)> fn,
                        int32_t userArg = 0) {
    return apollo::bw::BigWorld::addTimer(
        initialOffsetSeconds, repeatOffsetSeconds, std::move(fn), userArg);
}

inline bool delTimer(TimerId timerId) {
    return apollo::bw::BigWorld::delTimer(timerId);
}

} // namespace BigWorld
