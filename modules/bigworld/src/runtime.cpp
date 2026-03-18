#include "apollo/bw/bigworld.h"

#include <algorithm>
#include <chrono>
#include <queue>

namespace apollo::bw {

namespace {

using Clock = std::chrono::steady_clock;

uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}

struct TimerEvent {
    uint64_t dueMs = 0;
    TimerId timerId = INVALID_TIMER_ID;

    bool operator>(const TimerEvent& other) const {
        if (dueMs != other.dueMs) return dueMs > other.dueMs;
        return timerId > other.timerId;
    }
};

struct TimerState {
    EntityId ownerId = INVALID_ENTITY_ID;
    TimerId timerId = INVALID_TIMER_ID;
    uint64_t dueMs = 0;
    uint64_t intervalMs = 0;
    int32_t userArg = 0;
    bool cancelled = false;
    std::function<void()> callback;
    bool isCallback = false;
};

} // namespace

struct Runtime::TimerEntry {
    std::priority_queue<TimerEvent, std::vector<TimerEvent>, std::greater<>> heap;
    std::unordered_map<TimerId, TimerState> timers;
    uint64_t startMs = nowMs();
};

Runtime::Runtime() : timers_(std::make_unique<TimerEntry>()) {}

Runtime::~Runtime() {
    // 先销毁实体，保证 onDestroy 等回调在定时器状态销毁前执行。
    std::vector<EntityId> ids;
    ids.reserve(entities_.size());
    for (const auto& [id, _] : entities_) {
        ids.push_back(id);
    }
    for (EntityId id : ids) {
        destroyEntity(id);
    }
}

double Runtime::time() const {
    return static_cast<double>(timeMs()) / 1000.0;
}

uint64_t Runtime::timeMs() const {
    if (!timers_) {
        return 0;
    }
    uint64_t current = nowMs();
    return current >= timers_->startMs ? (current - timers_->startMs) : 0;
}

void Runtime::update() {
    if (!timers_) {
        return;
    }

    const uint64_t current = nowMs();

    while (!timers_->heap.empty() && timers_->heap.top().dueMs <= current) {
        const TimerEvent ev = timers_->heap.top();
        timers_->heap.pop();

        auto it = timers_->timers.find(ev.timerId);
        if (it == timers_->timers.end()) {
            continue;
        }

        TimerState& st = it->second;
        if (st.cancelled || st.dueMs != ev.dueMs) {
            continue;
        }

        if (st.callback) {
            auto cb = st.callback;
            try {
                cb();
            } catch (...) {
                // Ignore exceptions from user callbacks to keep the runtime stable.
            }
        } else {
            auto entIt = entities_.find(st.ownerId);
            if (entIt == entities_.end()) {
                timers_->timers.erase(it);
                continue;
            }

            std::shared_ptr<Entity> entity = entIt->second;
            if (!entity) {
                timers_->timers.erase(it);
                continue;
            }

            try {
                entity->onTimer(st.timerId, st.userArg);
            } catch (...) {
                // Ignore exceptions from entity code to keep the runtime stable.
            }
        }

        // 回调内可能取消定时器或销毁实体，需要重新校验状态。
        it = timers_->timers.find(ev.timerId);
        if (it == timers_->timers.end() || it->second.cancelled) {
            continue;
        }

        TimerState& updated = it->second;
        if (updated.intervalMs == 0) {
            if (!updated.callback) {
                auto entIt = entities_.find(updated.ownerId);
                if (entIt != entities_.end() && entIt->second) {
                    entIt->second->timers_.erase(updated.timerId);
                }
            }
            timers_->timers.erase(it);
            continue;
        }

        updated.dueMs = current + updated.intervalMs;
        timers_->heap.push(TimerEvent{updated.dueMs, updated.timerId});
    }
}

void Runtime::registerEntityFactory(const std::string& typeName, EntityFactory factory) {
    if (typeName.empty() || !factory) {
        return;
    }
    factories_[typeName] = std::move(factory);
}

std::shared_ptr<Entity> Runtime::createEntity(const std::string& typeName) {
    auto it = factories_.find(typeName);
    if (it == factories_.end()) {
        return nullptr;
    }
    auto entity = it->second();
    if (!entity) {
        return nullptr;
    }
    return addEntity(std::move(entity));
}

std::shared_ptr<Entity> Runtime::addEntity(std::shared_ptr<Entity> entity) {
    if (!entity) {
        return nullptr;
    }

    const EntityId id = nextEntityId_++;
    entity->attach(*this, id);
    entities_[id] = entity;

    entity->onEnterWorld();
    return entity;
}

std::shared_ptr<Entity> Runtime::findEntity(EntityId id) const {
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : it->second;
}

std::vector<std::shared_ptr<Entity>> Runtime::listEntities() const {
    std::vector<std::shared_ptr<Entity>> out;
    out.reserve(entities_.size());
    for (const auto& [_, ent] : entities_) {
        out.push_back(ent);
    }
    return out;
}

bool Runtime::destroyEntity(EntityId id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        return false;
    }

    std::shared_ptr<Entity> ent = std::move(it->second);
    entities_.erase(it);

    if (ent) {
        ent->onLeaveWorld();
        ent->cancelAllTimers();
        ent->onDestroy();
        ent->detach();
    }

    // 兜底清理：移除所有仍指向该实体的定时器。
    if (timers_) {
        for (auto jt = timers_->timers.begin(); jt != timers_->timers.end();) {
            if (jt->second.ownerId == id) {
                jt = timers_->timers.erase(jt);
            } else {
                ++jt;
            }
        }
    }

    return true;
}

TimerId Runtime::addEntityTimer(Entity& entity,
                               double initialOffsetSeconds,
                               double repeatOffsetSeconds,
                               int32_t userArg) {
    if (entity.id() == INVALID_ENTITY_ID) {
        return INVALID_TIMER_ID;
    }
    return addTimerInternal(entity.id(), initialOffsetSeconds, repeatOffsetSeconds, userArg, {}, false);
}

bool Runtime::delEntityTimer(Entity& entity, TimerId timerId) {
    if (entity.id() == INVALID_ENTITY_ID || timerId == INVALID_TIMER_ID) {
        return false;
    }

    if (timers_) {
        auto it = timers_->timers.find(timerId);
        if (it == timers_->timers.end() || it->second.ownerId != entity.id()) {
            return false;
        }
    }

    return delTimerInternal(timerId);
}

TimerId Runtime::addTimerInternal(EntityId ownerId,
                                  double initialOffsetSeconds,
                                  double repeatOffsetSeconds,
                                  int32_t userArg,
                                  std::function<void()> callback,
                                  bool isCallback) {
    if (!timers_) {
        return INVALID_TIMER_ID;
    }

    if (ownerId == INVALID_ENTITY_ID && !callback) {
        return INVALID_TIMER_ID;
    }

    const double initial = std::max(0.0, initialOffsetSeconds);
    const double repeat = std::max(0.0, repeatOffsetSeconds);

    const uint64_t initialMs = static_cast<uint64_t>(initial * 1000.0);
    const uint64_t intervalMs = static_cast<uint64_t>(repeat * 1000.0);

    TimerId id = nextTimerId_++;
    const uint64_t due = nowMs() + initialMs;

    TimerState st;
    st.ownerId = ownerId;
    st.timerId = id;
    st.dueMs = due;
    st.intervalMs = intervalMs;
    st.userArg = userArg;
    st.callback = std::move(callback);
    st.isCallback = isCallback;

    timers_->timers.emplace(id, st);
    timers_->heap.push(TimerEvent{due, id});

    return id;
}

bool Runtime::delTimerInternal(TimerId timerId) {
    if (!timers_ || timerId == INVALID_TIMER_ID) {
        return false;
    }

    auto it = timers_->timers.find(timerId);
    if (it == timers_->timers.end()) {
        return false;
    }

    it->second.cancelled = true;
    timers_->timers.erase(it);
    return true;
}

TimerId Runtime::callback(double delaySeconds, std::function<void()> fn) {
    if (!fn) {
        return INVALID_TIMER_ID;
    }
    return addTimerInternal(INVALID_ENTITY_ID, delaySeconds, 0.0, 0, std::move(fn), true);
}

bool Runtime::cancelCallback(TimerId callbackId) {
    if (!timers_) {
        return false;
    }

    auto it = timers_->timers.find(callbackId);
    if (it == timers_->timers.end() || !it->second.isCallback) {
        return false;
    }

    return delTimerInternal(callbackId);
}

TimerId Runtime::addTimer(double initialOffsetSeconds,
                          double repeatOffsetSeconds,
                          std::function<void(TimerId, int32_t)> fn,
                          int32_t userArg) {
    if (!timers_ || !fn) {
        return INVALID_TIMER_ID;
    }

    const double initial = std::max(0.0, initialOffsetSeconds);
    const double repeat = std::max(0.0, repeatOffsetSeconds);

    const uint64_t initialMs = static_cast<uint64_t>(initial * 1000.0);
    const uint64_t intervalMs = static_cast<uint64_t>(repeat * 1000.0);

    TimerId id = nextTimerId_++;
    const uint64_t due = nowMs() + initialMs;

    TimerState st;
    st.ownerId = INVALID_ENTITY_ID;
    st.timerId = id;
    st.dueMs = due;
    st.intervalMs = intervalMs;
    st.userArg = userArg;
    st.isCallback = false;
    st.callback = [fn = std::move(fn), id, userArg]() mutable {
        fn(id, userArg);
    };

    timers_->timers.emplace(id, st);
    timers_->heap.push(TimerEvent{due, id});

    return id;
}

bool Runtime::delTimer(TimerId timerId) {
    if (!timers_ || timerId == INVALID_TIMER_ID) {
        return false;
    }

    auto it = timers_->timers.find(timerId);
    if (it == timers_->timers.end()) {
        return false;
    }

    // Not a global timer (entity timers have no callback; callbacks use cancelCallback()).
    if (!it->second.callback || it->second.isCallback) {
        return false;
    }

    return delTimerInternal(timerId);
}

Entity::Entity(std::string typeName) : typeName_(std::move(typeName)) {}

Entity::~Entity() {
    cancelAllTimers();
}

TimerId Entity::addTimer(double initialOffsetSeconds, double repeatOffsetSeconds, int32_t userArg) {
    if (!runtime_) {
        return INVALID_TIMER_ID;
    }

    TimerId id = runtime_->addEntityTimer(*this, initialOffsetSeconds, repeatOffsetSeconds, userArg);
    if (id != INVALID_TIMER_ID) {
        timers_.insert(id);
    }
    return id;
}

bool Entity::delTimer(TimerId timerId) {
    if (!runtime_ || timerId == INVALID_TIMER_ID) {
        return false;
    }

    bool ok = runtime_->delEntityTimer(*this, timerId);
    timers_.erase(timerId);
    return ok;
}

bool Entity::destroy() {
    if (!runtime_ || id_ == INVALID_ENTITY_ID) {
        return false;
    }
    return runtime_->destroyEntity(id_);
}

void Entity::attach(Runtime& runtime, EntityId id) {
    runtime_ = &runtime;
    id_ = id;
}

void Entity::detach() {
    runtime_ = nullptr;
    id_ = INVALID_ENTITY_ID;
    timers_.clear();
}

void Entity::cancelAllTimers() {
    if (!runtime_) {
        timers_.clear();
        return;
    }

    auto ids = timers_;
    timers_.clear();

    for (TimerId id : ids) {
        runtime_->delEntityTimer(*this, id);
    }
}

//==============================================================================
// BigWorld facade
//==============================================================================

Runtime& BigWorld::instance() {
    static Runtime runtime;
    return runtime;
}

double BigWorld::time() {
    return instance().time();
}

uint64_t BigWorld::timeMs() {
    return instance().timeMs();
}

void BigWorld::update() {
    instance().update();
}

void BigWorld::registerEntityFactory(const std::string& typeName, Runtime::EntityFactory factory) {
    instance().registerEntityFactory(typeName, std::move(factory));
}

std::shared_ptr<Entity> BigWorld::createEntity(const std::string& typeName) {
    return instance().createEntity(typeName);
}

std::shared_ptr<Entity> BigWorld::entity(EntityId id) {
    return instance().findEntity(id);
}

std::vector<std::shared_ptr<Entity>> BigWorld::entities() {
    return instance().listEntities();
}

bool BigWorld::destroyEntity(EntityId id) {
    return instance().destroyEntity(id);
}

TimerId BigWorld::callback(double delaySeconds, std::function<void()> fn) {
    return instance().callback(delaySeconds, std::move(fn));
}

bool BigWorld::cancelCallback(TimerId callbackId) {
    return instance().cancelCallback(callbackId);
}

TimerId BigWorld::addTimer(double initialOffsetSeconds,
                           double repeatOffsetSeconds,
                           std::function<void(TimerId, int32_t)> fn,
                           int32_t userArg) {
    return instance().addTimer(initialOffsetSeconds, repeatOffsetSeconds, std::move(fn), userArg);
}

bool BigWorld::delTimer(TimerId timerId) {
    return instance().delTimer(timerId);
}

} // namespace apollo::bw
