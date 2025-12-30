# BigWorld Compatibility Layer (Apollo)

Status: **Incremental**. This is a BigWorld-style C++ API facade built on top of Apollo, not a full BigWorld engine/runtime.

## Goal

Provide a familiar `BigWorld::*` C++ API surface for game/server logic while reusing Apollo infrastructure. The compatibility layer is designed to be:

- **KISS**: a small, explicit runtime with an update loop
- **YAGNI**: only the APIs we actually use are implemented
- **Testable**: every exposed API has a runnable test case

## Implemented APIs (current)

Headers:

- `include/bigworld/BigWorld.h`: public `BigWorld::*` facade
- `include/apollo/bw/*`: implementation types (`Runtime`, `Entity`, ids)

Implementation:

- `src/apollo/bw/runtime.cpp`

### Global facade (`BigWorld::*`)

- Time:
  - `double BigWorld::time()`
  - `uint64_t BigWorld::timeMs()`
  - `void BigWorld::update()`
- Entity registry:
  - `registerEntityFactory(typeName, factory)`
  - `createEntity(typeName)`
  - `entity(id)`
  - `entities()`
  - `destroyEntity(id)`
- Timers (global):
  - `addTimer(initialOffsetSeconds, repeatOffsetSeconds, fn, userArg)`
  - `delTimer(timerId)`
- One-shot callbacks:
  - `callback(delaySeconds, fn)`
  - `cancelCallback(callbackId)`

### Entity base class (`BigWorld::Entity`)

- Identity:
  - `EntityId id() const`
  - `const std::string& typeName() const`
- Entity timers:
  - `TimerId addTimer(initialOffsetSeconds, repeatOffsetSeconds, userArg)`
  - `bool delTimer(timerId)`
- Destruction:
  - `bool destroy()`
- Lifecycle hooks:
  - `onEnterWorld()`
  - `onLeaveWorld()`
  - `onDestroy()`
  - `onTimer(timerId, userArg)`

## Semantics (important)

### Update loop

Timers are only dispatched when `BigWorld::update()` is called. You must call it periodically (e.g. per tick/frame) from your main loop.

### Timer units and behavior

- Time parameters are **seconds** (`double`).
- Negative offsets are clamped to `0`.
- `repeatOffsetSeconds == 0` means **one-shot** timer.

Timer cancellation rules (intentionally separated):

- `Entity::delTimer()` cancels **entity timers** created by that entity.
- `BigWorld::delTimer()` cancels **global timers** created by `BigWorld::addTimer()`.
- `BigWorld::cancelCallback()` cancels **callback timers** created by `BigWorld::callback()`.

Cross-type cancellation returns `false` by design.

### Threading / safety

Current implementation is **single-threaded** (no locks). Call `BigWorld::*` APIs from one thread.

### Exceptions

Exceptions thrown from user callbacks or entity `onTimer()` are caught and ignored to keep the runtime stable.

## Quick start

```cpp
#include "bigworld/BigWorld.h"

struct Avatar final : BigWorld::Entity {
    Avatar() : Entity("Avatar") {}
    void onEnterWorld() override { /* ... */ }
    void onTimer(TimerId id, int32_t arg) override { /* ... */ }
};

int main() {
    BigWorld::registerEntityFactory("Avatar", [] {
        return std::make_shared<Avatar>();
    });

    auto e = BigWorld::createEntity("Avatar");
    e->addTimer(0.1, 1.0, 7);

    for (;;) {
        BigWorld::update();
        // sleep/tick...
    }
}
```

## Tests

All exposed APIs are covered by:

- `tests/test_bigworld_api.cpp`

CTest target:

- `BigWorldApiTests`

### API coverage mapping

- `time()/timeMs()/update()` → `test_time_monotonic`, `test_update_required_for_callback_dispatch`
- Entity lifecycle APIs → `test_factory_and_entity_lifecycle`, `test_factory_invalid_inputs_and_factory_returns_null`
- Entity timer APIs → `test_entity_timer_api`, `test_addTimer_invalid_clamp_and_cross_cancel`
- Callback APIs → `test_bigworld_callback_api`, `test_callback_empty_and_exceptions_swallowed`
- Global timer APIs → `test_bigworld_global_timer_api`, `test_addTimer_invalid_clamp_and_cross_cancel`

## Not implemented yet (roadmap)

This compatibility layer does **not** yet provide:

- BigWorld Base/Cell split, mailboxes, and distributed entity routing
- EntityDefs/IDL and automatic property replication
- Python scripting compatibility
- Network protocol compatibility with official BigWorld clients

When you specify the exact BigWorld API surface/version you want (BaseApp/CellApp, mailbox semantics, scripting language), the next step is to extend this layer incrementally with the same “API + tests” rule.
