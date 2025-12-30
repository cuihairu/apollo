/**
 * @file test_bigworld_api.cpp
 * @brief BigWorld 兼容层 API 测试
 */

#include "bigworld/BigWorld.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace BigWorld;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

namespace {

using Clock = std::chrono::steady_clock;

void pumpFor(int durationMs, int tickMs = 5) {
    const auto start = Clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count() < durationMs) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(tickMs));
    }
}

bool pumpUntil(const std::function<bool()>& condition,
               int timeoutMs,
               int tickMs = 5) {
    const auto start = Clock::now();
    while (!condition()) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(tickMs));
        const auto elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
        if (elapsedMs > timeoutMs) {
            return false;
        }
    }
    return true;
}

void clearWorld() {
    auto list = entities();
    for (const auto& ent : list) {
        if (ent) {
            (void)destroyEntity(ent->id());
        }
    }
    update();
}

class TestEntity final : public Entity {
public:
    TestEntity() : Entity("TestEntity") {}

    std::atomic<int> enterCount{0};
    std::atomic<int> leaveCount{0};
    std::atomic<int> destroyCount{0};
    std::atomic<int> timerCount{0};
    std::atomic<TimerId> lastTimerId{0};
    std::atomic<int32_t> lastUserArg{0};

    void onEnterWorld() override { enterCount.fetch_add(1); }
    void onLeaveWorld() override { leaveCount.fetch_add(1); }
    void onDestroy() override { destroyCount.fetch_add(1); }

    void onTimer(TimerId timerId, int32_t userArg) override {
        timerCount.fetch_add(1);
        lastTimerId.store(timerId);
        lastUserArg.store(userArg);
    }
};

class ThrowingEntity final : public Entity {
public:
    ThrowingEntity() : Entity("ThrowingEntity") {}

    void onTimer(TimerId /*timerId*/, int32_t /*userArg*/) override {
        throw std::runtime_error("ThrowingEntity::onTimer");
    }
};

} // namespace

//==============================================================================
// 测试用例
//==============================================================================

bool test_time_monotonic() {
    std::cout << "Running: test_time_monotonic..." << std::endl;

    const uint64_t t1 = timeMs();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const uint64_t t2 = timeMs();

    TEST_ASSERT(t2 >= t1, "timeMs monotonic");
    TEST_ASSERT(time() >= 0.0, "time non-negative");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_factory_and_entity_lifecycle() {
    std::cout << "Running: test_factory_and_entity_lifecycle..." << std::endl;

    clearWorld();

    TEST_ASSERT(createEntity("UnknownType") == nullptr, "Unknown entity type returns null");

    registerEntityFactory("TestEntity", []() {
        return std::make_shared<TestEntity>();
    });

    auto ent = createEntity("TestEntity");
    TEST_ASSERT(ent != nullptr, "Entity created");

    auto testEnt = std::dynamic_pointer_cast<TestEntity>(ent);
    TEST_ASSERT(testEnt != nullptr, "Entity type matches factory");

    const EntityId id = ent->id();
    TEST_ASSERT(id != 0, "EntityId assigned");
    TEST_ASSERT(ent->typeName() == "TestEntity", "typeName matches");
    TEST_ASSERT(testEnt->enterCount.load() == 1, "onEnterWorld called");

    TEST_ASSERT(entity(id).get() == ent.get(), "entity(id) returns same instance");

    auto list = entities();
    bool found = false;
    for (const auto& e : list) {
        if (e && e->id() == id) {
            found = true;
            break;
        }
    }
    TEST_ASSERT(found, "entities() contains the created entity");

    TEST_ASSERT(destroyEntity(id), "destroyEntity succeeds");
    TEST_ASSERT(entity(id) == nullptr, "entity(id) returns nullptr after destroy");
    TEST_ASSERT(entities().empty(), "entities() empty after destroy");
    TEST_ASSERT(testEnt->leaveCount.load() == 1, "onLeaveWorld called");
    TEST_ASSERT(testEnt->destroyCount.load() == 1, "onDestroy called");

    TEST_ASSERT(!destroyEntity(id), "destroyEntity returns false when missing");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_entity_timer_api() {
    std::cout << "Running: test_entity_timer_api..." << std::endl;

    clearWorld();

    registerEntityFactory("TestEntity", []() {
        return std::make_shared<TestEntity>();
    });

    auto orphan = std::make_shared<TestEntity>();
    TEST_ASSERT(orphan->addTimer(0.01, 0.0, 1) == 0, "Orphan addTimer returns INVALID_TIMER_ID");

    auto ent = createEntity("TestEntity");
    TEST_ASSERT(ent != nullptr, "Entity created");

    auto testEnt = std::dynamic_pointer_cast<TestEntity>(ent);
    TEST_ASSERT(testEnt != nullptr, "Entity type matches");

    const TimerId onceId = ent->addTimer(0.05, 0.0, 123);
    TEST_ASSERT(onceId != 0, "Entity addTimer returns id");

    TEST_ASSERT(pumpUntil([&]() {
        return testEnt->timerCount.load() >= 1;
    }, 1500), "Entity timer fired");

    TEST_ASSERT(testEnt->lastUserArg.load() == 123, "Entity timer userArg");
    TEST_ASSERT(testEnt->lastTimerId.load() == onceId, "Entity timerId delivered");

    // One-shot timer should be auto-removed after firing.
    TEST_ASSERT(!ent->delTimer(onceId), "delTimer returns false after oneshot fired");

    const int startCount = testEnt->timerCount.load();
    const TimerId repeatId = ent->addTimer(0.01, 0.02, 7);
    TEST_ASSERT(repeatId != 0, "Repeated timer created");

    TEST_ASSERT(pumpUntil([&]() {
        return testEnt->timerCount.load() >= (startCount + 3);
    }, 1500), "Repeated timer fired multiple times");

    TEST_ASSERT(ent->delTimer(repeatId), "delTimer cancels repeated timer");

    const int baseline = testEnt->timerCount.load();
    pumpFor(200);
    TEST_ASSERT(testEnt->timerCount.load() == baseline, "No more callbacks after delTimer");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_entity_destroy_method() {
    std::cout << "Running: test_entity_destroy_method..." << std::endl;

    clearWorld();

    registerEntityFactory("TestEntity", []() {
        return std::make_shared<TestEntity>();
    });

    auto ent = createEntity("TestEntity");
    TEST_ASSERT(ent != nullptr, "Entity created");

    auto testEnt = std::dynamic_pointer_cast<TestEntity>(ent);
    TEST_ASSERT(testEnt != nullptr, "Entity type matches");

    const EntityId id = ent->id();
    TEST_ASSERT(id != 0, "EntityId assigned");

    TEST_ASSERT(ent->destroy(), "Entity::destroy succeeds");
    TEST_ASSERT(entity(id) == nullptr, "Entity removed from world");
    TEST_ASSERT(testEnt->leaveCount.load() == 1, "onLeaveWorld called");
    TEST_ASSERT(testEnt->destroyCount.load() == 1, "onDestroy called");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bigworld_callback_api() {
    std::cout << "Running: test_bigworld_callback_api..." << std::endl;

    clearWorld();

    std::atomic<int> called{0};
    const TimerId cbId = callback(0.05, [&]() { called.fetch_add(1); });
    TEST_ASSERT(cbId != 0, "callback returns id");

    TEST_ASSERT(pumpUntil([&]() {
        return called.load() == 1;
    }, 1500), "callback executed");

    TEST_ASSERT(!cancelCallback(cbId), "cancelCallback returns false after executed");
    TEST_ASSERT(!delTimer(cbId), "delTimer cannot delete callback timers");

    std::atomic<int> cancelledCalled{0};
    const TimerId cb2 = callback(0.2, [&]() { cancelledCalled.fetch_add(1); });
    TEST_ASSERT(cb2 != 0, "callback returns id");
    TEST_ASSERT(!delTimer(cb2), "delTimer cannot delete callback timers (pending)");
    TEST_ASSERT(cancelCallback(cb2), "cancelCallback succeeds");

    pumpFor(350);
    TEST_ASSERT(cancelledCalled.load() == 0, "Cancelled callback not executed");

    TEST_ASSERT(!cancelCallback(0), "cancelCallback(INVALID_TIMER_ID) returns false");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bigworld_global_timer_api() {
    std::cout << "Running: test_bigworld_global_timer_api..." << std::endl;

    clearWorld();

    std::atomic<int> fired{0};
    std::atomic<TimerId> firedId{0};
    std::atomic<int32_t> firedArg{0};

    const TimerId onceId = addTimer(0.05, 0.0,
        [&](TimerId timerId, int32_t userArg) {
            fired.fetch_add(1);
            firedId.store(timerId);
            firedArg.store(userArg);
        }, 42);

    TEST_ASSERT(onceId != 0, "addTimer returns id");
    TEST_ASSERT(pumpUntil([&]() { return fired.load() == 1; }, 1500), "Global oneshot timer fired");
    TEST_ASSERT(firedId.load() == onceId, "Global timerId delivered");
    TEST_ASSERT(firedArg.load() == 42, "Global timer userArg delivered");

    std::atomic<int> repCount{0};
    const TimerId repId = addTimer(0.01, 0.02,
        [&](TimerId, int32_t) {
            repCount.fetch_add(1);
        });

    TEST_ASSERT(repId != 0, "Repeated global timer created");
    TEST_ASSERT(!cancelCallback(repId), "cancelCallback does not cancel global timers");

    TEST_ASSERT(pumpUntil([&]() { return repCount.load() >= 3; }, 1500), "Repeated global timer fired");

    TEST_ASSERT(delTimer(repId), "delTimer cancels global timer");

    const int baseline = repCount.load();
    pumpFor(200);
    TEST_ASSERT(repCount.load() == baseline, "No more callbacks after delTimer (global)");

    TEST_ASSERT(!delTimer(0), "delTimer(INVALID_TIMER_ID) returns false");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_factory_invalid_inputs_and_factory_returns_null() {
    std::cout << "Running: test_factory_invalid_inputs_and_factory_returns_null..." << std::endl;

    clearWorld();

    registerEntityFactory("", []() {
        return std::make_shared<TestEntity>();
    });
    TEST_ASSERT(createEntity("") == nullptr, "Empty typeName factory is ignored");

    registerEntityFactory("TestEntity", []() {
        return std::make_shared<TestEntity>();
    });
    TEST_ASSERT(createEntity("TestEntity") != nullptr, "Factory registered");

    registerEntityFactory("TestEntity", apollo::bw::Runtime::EntityFactory{});
    TEST_ASSERT(createEntity("TestEntity") != nullptr, "Empty factory does not override existing one");

    registerEntityFactory("NullEntity", []() -> std::shared_ptr<Entity> {
        return nullptr;
    });
    TEST_ASSERT(createEntity("NullEntity") == nullptr, "Factory returning nullptr produces nullptr");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_update_required_for_callback_dispatch() {
    std::cout << "Running: test_update_required_for_callback_dispatch..." << std::endl;

    clearWorld();

    std::atomic<int> called{0};
    const TimerId id = callback(0.05, [&]() { called.fetch_add(1); });
    TEST_ASSERT(id != 0, "callback returns id");

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    TEST_ASSERT(called.load() == 0, "callback not executed without update()");

    TEST_ASSERT(pumpUntil([&]() { return called.load() == 1; }, 1500), "callback executed after update()");
    TEST_ASSERT(!cancelCallback(id), "cancelCallback returns false after executed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_callback_empty_and_exceptions_swallowed() {
    std::cout << "Running: test_callback_empty_and_exceptions_swallowed..." << std::endl;

    clearWorld();

    TEST_ASSERT(callback(0.0, {}) == 0, "callback(empty) returns INVALID_TIMER_ID");
    TEST_ASSERT(!cancelCallback(0), "cancelCallback(INVALID_TIMER_ID) returns false");

    const TimerId throwId = callback(0.0, []() {
        throw std::runtime_error("callback throws");
    });
    TEST_ASSERT(throwId != 0, "callback created");

    std::atomic<int> ok{0};
    const TimerId okId = callback(0.0, [&]() { ok.fetch_add(1); });
    TEST_ASSERT(okId != 0, "callback created");

    // 如果异常未被 swallow，这里会直接崩溃
    update();
    update();
    TEST_ASSERT(ok.load() == 1, "runtime continues after callback throws");

    registerEntityFactory("ThrowingEntity", []() {
        return std::make_shared<ThrowingEntity>();
    });

    auto ent = createEntity("ThrowingEntity");
    TEST_ASSERT(ent != nullptr, "ThrowingEntity created");

    const TimerId tid = ent->addTimer(0.0, 0.0, 0);
    TEST_ASSERT(tid != 0, "ThrowingEntity timer created");

    std::atomic<int> after{0};
    const TimerId afterId = callback(0.0, [&]() { after.fetch_add(1); });
    TEST_ASSERT(afterId != 0, "callback created");

    update();
    update();
    TEST_ASSERT(after.load() == 1, "runtime continues after entity onTimer throws");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_addTimer_invalid_clamp_and_cross_cancel() {
    std::cout << "Running: test_addTimer_invalid_clamp_and_cross_cancel..." << std::endl;

    clearWorld();

    TEST_ASSERT(addTimer(0.0, 0.0, {}) == 0, "addTimer(empty) returns INVALID_TIMER_ID");

    // 负数 offset 会被 clamp 到 0，repeatOffset<0 会被 clamp 到 0（一次性）
    std::atomic<int> fired{0};
    const TimerId clampId = addTimer(-1.0, -1.0, [&](TimerId, int32_t) { fired.fetch_add(1); }, 7);
    TEST_ASSERT(clampId != 0, "addTimer returns id");
    TEST_ASSERT(pumpUntil([&]() { return fired.load() == 1; }, 500), "clamped timer fired");
    pumpFor(80);
    TEST_ASSERT(fired.load() == 1, "clamped timer is one-shot");

    registerEntityFactory("TestEntity", []() {
        return std::make_shared<TestEntity>();
    });

    auto ent = createEntity("TestEntity");
    TEST_ASSERT(ent != nullptr, "Entity created");

    auto testEnt = std::dynamic_pointer_cast<TestEntity>(ent);
    TEST_ASSERT(testEnt != nullptr, "Entity type matches");

    TEST_ASSERT(!ent->delTimer(0), "Entity::delTimer(INVALID_TIMER_ID) returns false");

    const TimerId entityTimer = ent->addTimer(0.05, 0.0, 1);
    TEST_ASSERT(entityTimer != 0, "Entity timer created");
    TEST_ASSERT(!delTimer(entityTimer), "BigWorld::delTimer does not cancel entity timers");
    TEST_ASSERT(ent->delTimer(entityTimer), "Entity::delTimer cancels its own timer");
    pumpFor(120);
    TEST_ASSERT(testEnt->timerCount.load() == 0, "Cancelled entity timer not executed");

    std::atomic<int> globalCalled{0};
    const TimerId globalTimer = addTimer(0.05, 0.0, [&](TimerId, int32_t) { globalCalled.fetch_add(1); });
    TEST_ASSERT(globalTimer != 0, "Global timer created");
    TEST_ASSERT(!ent->delTimer(globalTimer), "Entity::delTimer does not cancel global timers");
    TEST_ASSERT(delTimer(globalTimer), "BigWorld::delTimer cancels global timers");
    pumpFor(120);
    TEST_ASSERT(globalCalled.load() == 0, "Cancelled global timer not executed");

    auto orphan = std::make_shared<TestEntity>();
    TEST_ASSERT(!orphan->destroy(), "Orphan Entity::destroy returns false");
    TEST_ASSERT(!orphan->delTimer(1), "Orphan Entity::delTimer returns false");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_runtime_edge_cases() {
    std::cout << "Running: test_runtime_edge_cases..." << std::endl;

    apollo::bw::Runtime runtime;

    TEST_ASSERT(runtime.addEntity(nullptr) == nullptr, "Runtime::addEntity(nullptr) returns nullptr");

    auto orphan = std::make_shared<TestEntity>();
    TEST_ASSERT(runtime.addEntityTimer(*orphan, 0.01, 0.0, 1) == 0, "Runtime::addEntityTimer requires attached entity");
    TEST_ASSERT(!runtime.delEntityTimer(*orphan, 1), "Runtime::delEntityTimer requires attached entity");

    auto ent = std::make_shared<TestEntity>();
    auto added = runtime.addEntity(ent);
    TEST_ASSERT(added != nullptr, "Runtime::addEntity returns entity");
    TEST_ASSERT(ent->enterCount.load() == 1, "onEnterWorld called");

    const TimerId tid = ent->addTimer(1.0, 0.0, 5);
    TEST_ASSERT(tid != 0, "Entity timer created");

    const EntityId id = ent->id();
    TEST_ASSERT(id != 0, "EntityId assigned");

    TEST_ASSERT(runtime.destroyEntity(id), "Runtime::destroyEntity succeeds");

    std::cout << "  PASSED" << std::endl;
    return true;
}

int main() {
    std::cout << "=== BigWorld Compatibility Layer Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_time_monotonic);
    run(test_factory_and_entity_lifecycle);
    run(test_entity_timer_api);
    run(test_entity_destroy_method);
    run(test_bigworld_callback_api);
    run(test_bigworld_global_timer_api);
    run(test_factory_invalid_inputs_and_factory_returns_null);
    run(test_update_required_for_callback_dispatch);
    run(test_callback_empty_and_exceptions_swallowed);
    run(test_addTimer_invalid_clamp_and_cross_cancel);
    run(test_runtime_edge_cases);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
