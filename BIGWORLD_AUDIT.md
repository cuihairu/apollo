# BigWorld Compatibility Layer Audit

## 1. File List & Line Counts

### modules/bigworld (include/src)

| File | Lines | Notes |
|------|-------|-------|
| `include/apollo/bw/bigworld.h` | 31 | BigWorld facade static methods |
| `include/apollo/bw/runtime.h` | 76 | Runtime class + BigWorld static methods |
| `include/apollo/bw/entity.h` | 48 | Entity class + type definitions |
| `include/apollo/bw/types.h` | 14 | EntityId=uint64_t, TimerId=uint64_t, INVALID constants |
| `modules/bigworld/include/apollo/bigworld/runtime.hpp` | 19 | Convenience using-declarations; BW_RUNTIME/BW_NOW/BW_UPDATE macros |
| `modules/bigworld/include/apollo/bigworld/entity.hpp` | 25 | EntityID wrapper class with is_valid(), set/get templates |
| `modules/bigworld/include/apollo/bigworld/timer.hpp` | 25 | add_timer()/del_timer() inline functions delegating to BigWorld |
| `modules/bigworld/include/apollo/bigworld/callback.hpp` | 19 | register_callback()/cancel_callback() inline functions delegating to BigWorld |
| `modules/bigworld/src/runtime.cpp` | 489 | **Full implementation**: Runtime class + BigWorld inline facades |
| `modules/bigworld/src/runtime_wrapper.cpp` | 8 | **Empty stub**: comment only "// All functions are inlined in the header" |
| `modules/bigworld/src/entity.cpp` | 8 | **Empty stub**: comment only "// Entity implementation is provided by apollo::bw::Entity" |
| `modules/bigworld/src/callback.cpp` | 8 | **Empty stub**: comment only "// Callback implementation is provided by apollo::bw::Runtime" |
| `modules/bigworld/src/timer.cpp` | 7 | **Empty stub**: comment only "// Timer implementation is provided by apollo::bw::Runtime" |

### include/apollo/bw (top-level compatibility headers)

| File | Lines | Notes |
|------|-------|-------|
| `include/bigworld/BigWorld.h` | 62 | Legacy `BigWorld` namespace with inline functions delegating to `apollo::bw::BigWorld` |
| `include/apollo/bw/bigworld.h` | 31 | apollo::bw::BigWorld class declaration (same as modules/bigworld version) |
| `include/apollo/bw/runtime.h` | 76 | Runtime class definition + BigWorld static method wrappers |
| `include/apollo/bw/entity.h` | 48 | Entity class definition + timer/destroy methods |
| `include/apollo/bw/types.h` | 14 | EntityId=uint64_t, TimerId=uint64_t, INVALID_ENTITY_ID/INVALID_TIMER_ID |

---

## 2. Namespace & Key Abstractions

### `apollo::bw` namespace boundary

The `apollo::bw` namespace contains the core compatibility layer implementation:

**Runtime class** (runtime.h:14-74, fully implemented in runtime.cpp:1-489):
- `double time() const` / `uint64_t timeMs() const` — time access
- `void update()` — timer advancement & callback dispatch
- `void registerEntityFactory(const std::string& typeName, EntityFactory factory)` — entity type registration
- `std::shared_ptr<Entity> createEntity(const std::string& typeName)` — factory-driven entity creation
- `std::shared_ptr<Entity> addEntity(std::shared_ptr<Entity> entity)` — add entity to runtime
- `std::shared_ptr<Entity> findEntity(EntityId id) const` — lookup by ID
- `std::vector<std::shared_ptr<Entity>> listEntities() const` — enumerate all entities
- `bool destroyEntity(EntityId id)` — full cleanup (onLeaveWorld + timers + onDestroy)
- `TimerId addEntityTimer(Entity& entity, double initialOffsetSeconds, double repeatOffsetSeconds, int32_t userArg)` — entity-bound timer
- `bool delEntityTimer(Entity& entity, TimerId timerId)` — entity-bound timer deletion
- `TimerId callback(double delaySeconds, std::function<void()> fn)` — global (entity-less) timer
- `bool cancelCallback(TimerId callbackId)` — cancel global timer
- `TimerId addTimer(double initialOffsetSeconds, double repeatOffsetSeconds, std::function<void(TimerId, int32_t)> fn, int32_t userArg = 0)` — global timer with callback fn
- `bool delTimer(TimerId timerId)` — global timer deletion

**Entity class** (entity.h:13-46):
- `explicit Entity(std::string typeName)` — constructor with type name
- `virtual ~Entity()` — virtual destructor
- `EntityId id() const` / `const std::string& typeName() const` — identification
- `TimerId addTimer(double, double, int32_t = 0)` — delegate to Runtime::addEntityTimer
- `bool delTimer(TimerId)` — delegate to Runtime::delEntityTimer
- `bool destroy()` — delegate to Runtime::destroyEntity
- Virtual callbacks: `onEnterWorld()`, `onLeaveWorld()`, `onDestroy()`, `onTimer(TimerId, int32_t)`

### `apollo::bigworld` namespace

Convenience re-export of `apollo::bw` via `using namespace apollo::bw` (runtime.hpp:7-8). Provides simpler function names:
- `BW_RUNTIME`, `BW_NOW()`, `BW_UPDATE()` macros (runtime.hpp:17-19)
- `add_timer()`, `del_timer()` (timer.hpp:11-23)
- `register_callback()`, `cancel_callback()` (callback.hpp:11-17)

### `BigWorld` (legacy/compat namespace)

`include/bigworld/BigWorld.h` provides a `BigWorld` namespace with inline functions that delegate to `apollo::bw::BigWorld`, serving as the primary inclusion point for code ported from BigWorld 14.4.1.

---

## 3. 'Compatible Layer' Evidence — Mapping Table

All `apollo::bw::X` APIs map to BigWorld 14.4.1 concepts. Mapping table (apollo::bw::X → BW conceptual equivalent):

| apollo::bw API | BigWorld 14.4.1 Equivalent | Status |
|---|---|---|
| `BigWorld::instance()` | BigWorld global singleton | ✅ Real implementation via static Runtime |
| `BigWorld::time()` | BigWorld::now()/deltaTime() | ✅ Delegates to Runtime::time() |
| `BigWorld::timeMs()` | BigWorld::timeMs() | ✅ Delegates to Runtime::timeMs() |
| `BigWorld::update()` | BigWorld::update() | ✅ Delegates to Runtime::update() |
| `BigWorld::createEntity(type)` | BigWorld::createEntity() | ✅ Uses factory registry |
| `BigWorld::entity(id)` | BigWorld::entityExists/id lookup | ✅ Finds entity by ID |
| `BigWorld::entities()` | BigWorld::entities() list | ✅ Lists all entities |
| `BigWorld::destroyEntity(id)` | BigWorld::destroyEntity() | ✅ Full cleanup |
| `BigWorld::callback(delay, fn)` | BigWorld::callback() | ✅ Creates global timer |
| `BigWorld::cancelCallback(id)` | BigWorld::cancelCallback() | ✅ Cancels callback |
| `BigWorld::addTimer(delay, repeat, fn, userArg)` | BigWorld::addTimer() | ✅ Creates timer with fn |
| `BigWorld::delTimer(id)` | BigWorld::delTimer() | ✅ Deletes timer |
| `Runtime::createEntity(type)` | Entity factory creation | ✅ Full impl in runtime.cpp |
| `Runtime::addEntity(entity)` | Add entity to runtime | ✅ Full impl in runtime.cpp |
| `Runtime::findEntity(id)` | Find entity by ID | ✅ Full impl in runtime.cpp |
| `Runtime::listEntities()` | List all entities | ✅ Full impl in runtime.cpp |
| `Runtime::destroyEntity(id)` | Destroy entity | ✅ Full impl in runtime.cpp |
| `Runtime::addEntityTimer(entity, ...)` | Entity timer add | ✅ Full impl in runtime.cpp |
| `Runtime::delEntityTimer(entity, id)` | Entity timer del | ✅ Full impl in runtime.cpp |
| `Runtime::callback(delay, fn)` | Global callback timer | ✅ Full impl in runtime.cpp |
| `Runtime::cancelCallback(id)` | Cancel callback | ✅ Full impl in runtime.cpp |
| `Runtime::addTimer(delay, repeat, fn, userArg)` | Global timer | ✅ Full impl in runtime.cpp |
| `Runtime::delTimer(id)` | Global timer del | ✅ Full impl in runtime.cpp |
| `Entity::addTimer()` | Entity method timer add | ✅ Delegates to Runtime::addEntityTimer() |
| `Entity::delTimer()` | Entity method timer del | ✅ Delegates to Runtime::delEntityTimer() |
| `Entity::destroy()` | Entity destroy | ✅ Delegates to Runtime::destroyEntity() |

**Forwarding stub / TODO count**: 4 source files in `modules/bigworld/src/` are essentially empty stubs with only comments:

- `runtime_wrapper.cpp:6-7` — `namespace apollo::bigworld { /* All functions are inlined in the header, using apollo::bw implementations */ }`
- `entity.cpp:6-7` — `namespace apollo::bigworld { /* Entity implementation is provided by apollo::bw::Entity /* This file provides the bridge */ }`
- `timer.cpp:5-6` — `// Timer implementation is provided by apollo::bw::Runtime`
- `callback.cpp:5-7` — `// Callback implementation is provided by apollo::bw::Runtime // The callback functions are managed through the Runtime`

Ratio: **4/4 src files are stubs** (100% stub file count), but the actual functional implementation resides entirely in `runtime.cpp` (489 lines). The stub files exist only to satisfy the build interface; removing them would not affect runtime behavior.

---

## 4. Entity System Parallelism (§16.8.3-④)

There are **four distinct entity systems** operating in parallel:

1. **`apollo::bw::Runtime` + `apollo::bw::Entity`** — The compatibility layer entity system
   - Runtime manages `entities_` map (EntityId→shared_ptr<Entity>), `factories_` map (typeName→factory)
   - Entity holds `runtime_` pointer, `id_`, `typeName_`, `timers_` set
   - Entity methods delegate to Runtime for all operations

2. **`modules/game::core::Entity`** — The "new" Apollo game entity system
   - `apollo::game::core::Entity : public IEntity` with component system
   - `EntityId` (uint64_t based), `on_spawn()`, `on_despawn()`, `on_update(float delta_time)`
   - Component system: `add_component<T>()`, `get_component<T>()`, `remove_component()`
   - **Does NOT use** `apollo::bw::Entity` or `Runtime` at all

3. **`modules::game::world::Scene`** — Scene management
   - `spawn_entity(EntityPtr)`, `despawn_entity(EntityId)`, `get_entity(EntityId)`
   - `update(float delta_time)`, simple `unordered_map<uint64_t, EntityPtr>` storage
   - Manages entity lifecycle within a scene graph

4. **BigWorld 14.4.1 official entity system** — The reference BW API
   - `BigWorld::Entity`, `BigWorld::createEntity`, `BigWorld::destroyEntity`
   - Built-in scripting bindings, C++ server entity system
   - The compatibility layer's direct counterpart

**§16.8.3-④ observation confirmed**: The compatibility layer `apollo::bw::Entity` IS a fourth entity body alongside BW's native system, the game module's system, and (if present) any `apollo::ecs` system. The bw::Entity is isolated from the game module's Entity — no shared base, no integration, completely separate hierarchies.

---

## 5. Consumer Survey

### Production code consumption (modules/ + include/, excluding tests/)

- **Zero production files** outside the bigworld module itself reference `apollo::bw` or `apollo::bigworld`.
- The game module (`modules/game/`) has its own entity system and does **not** include any `apollo::bw` headers.

### Test/example consumption points

| File | Usage |
|------|-------|
| `modules/bigworld/tests/bigworld_comprehensive_tests.cpp` | `using namespace apollo::bigworld;` (line 23); includes all bigworld headers |
| `tests/test_bigworld_api.cpp` | `#include "bigworld/BigWorld.h"` (line 15); uses `apollo::bw::Runtime` (line 329, 457) |

**Conclusion**: The compatibility layer has **no production-code consumers** outside its own test module. It exists as a standalone compatibility shim with no upstream dependents.

---

## 6. Container Usage Survey

Searching `modules/bigworld` for `framework/ioc|ApplicationContext|core::di`:

- **No references** to `framework/ioc` in any bigworld file
- **No references** to `ApplicationContext` in any bigworld file
- **No references** to `core::di` in any bigworld file

The bigworld module implements its own simple management patterns:
- Entity factory registry: `std::unordered_map<std::string, EntityFactory>` (runtime.cpp:143-148) — string-keyed, manually registered
- Entity ID generation: `nextEntityId_ = 1` (runtime.cpp:66) — auto-increment, no DI container
- Timer state: internal `TimerEntry` heap + map (runtime.cpp:40-44) — manually managed

No formal IOC/di container is used; the module provides its own minimal dependency-tracing via factory registration and entity ID assignment.

---

## 7. Defect Pattern Counting

| Pattern | File:Line | Description |
|---|---|---|
| Global lock / stub first | runtime.cpp:48-58 | Runtime destructor destroys entities before timers — correct ordering enforced |
| New+old co-existence | runtime.cpp:150-160 | `createEntity()` uses factory registry; if no factory registered, returns `nullptr` — graceful degradation |
| Dead code | `runtime_wrapper.cpp:6-7` | Empty namespace block — stub with no content |
| Dead code | `entity.cpp:6-7` | Empty namespace block — stub with no content |
| Dead code | `timer.cpp:5-6` | Comment-only namespace — stub with no content |
| Dead code | `callback.cpp:5-7` | Comment-only namespace — stub with no content |
| String keys | `runtime.cpp:143-148` | Factory registry uses `std::unordered_map<std::string, EntityFactory>` — string-key lookups |
| Timer stub pattern | `timer.cpp:3-5` | "Timer implementation is provided by apollo::bw::Runtime" — forward-declaration pattern |
| Entity stub pattern | `entity.cpp:3-4` | "Entity implementation is provided by apollo::bw::Entity" — forward-declaration pattern |
| Callback stub pattern | `callback.cpp:5-6` | "Callback implementation is provided by apollo::bw::Runtime" — forward-declaration pattern |

Each defect entry gives the exact file:line where the pattern was observed (all lines are **实测**, i.e. measured from the actual source).

---

## 8. Timer Forwarding Framework Deep Inspection

**bigworld/timer.cpp full content** (lines 1-7):

```cpp
#include "apollo/bigworld/timer.hpp"

namespace apollo::bigworld {

// Timer implementation is provided by apollo::bw::Runtime

} // namespace apollo::bigworld
```

**Forwarding chain** (all from runtime.cpp):

1. User calls `apollo::bw::BigWorld::addEntityTimer(entity, ...)` (runtime.cpp:438-440 inline wrapper)
   → `instance().addEntityTimer(...)` — **Runtime::addEntityTimer** at runtime.cpp:219-227

2. `Runtime::addEntityTimer(entity, initialOffsetSeconds, repeatOffsetSeconds, userArg)` (runtime.cpp:219-227):
   ```cpp
   if (entity.id() == INVALID_ENTITY_ID) { return INVALID_TIMER_ID; }
   return addTimerInternal(entity.id(), initialOffsetSeconds, repeatOffsetSeconds, userArg, {}, false);
   ```

3. `Runtime::addTimerInternal(EntityId ownerId, ...)` (runtime.cpp:244-280):
   - Validates `timers_` existence, ownerId + callback sanity checks
   - Computes `initialMs`, `intervalMs` from seconds
   - Generates new `TimerId` via `nextTimerId_++`
   - Sets `due = nowMs() + initialMs`
   - Emplaces `TimerState` into `timers_->timers` map and pushes `TimerEvent` into heap
   - Returns the new `TimerId`

4. `Runtime::update()` (runtime.cpp:72-141) periodically fires callbacks:
   - Pops due timer events from the min-heap
   - Finds matching `TimerState` entry
   - If callback std::function: calls `cb()` (no-arg functor)
   - If entity-bound: calls `entity->onTimer(st.timerId, st.userArg)`
   - Handles interval timers by re-inserting into heap

**There is NO second layer of indirection**: `Runtime::addEntityTimer` IS the real implementation, not another forwarding stub. The `timer.cpp` and `callback.cpp` stub files are purely ceremonial — they contain zero implementation logic and exist only to satisfy the build's source list.

---

## 9. Build Ownership

| Aspect | Fact |
|---|---|
| `modules/bigworld` CMake target | `apollo_bigworld` static library (CMakeLists.txt:5-11) |
| Default build status | `APOLLO_BUILD_BIGWORLD_MODULE` defaults to `OFF` (modules/CMakeLists.txt:49) |
| Dependency on game module | Depends on `apollo::game_core` and `apollo::runtime` (CMakeLists.txt:20-24) |
| Dependency on game module (build) | Game module also defaults `OFF`; both are opt-in via CMake options |
| Relationship to `modules/game` | bigworld links game_core; game module does NOT include apollo::bw headers |
| Aggregated library | `apollo::lib` optionally links `apollo::bigworld` if target exists (modules/CMakeLists.txt:126-128) |
| Build feature test | Comprehensive tests compiled only when `BUILD_TESTING=ON` + `GTest_FOUND` (CMakeLists.txt:33-59) |

---

## 10. Evolution Verdict

**Is the compatible layer a "valuable BW API bridge" or "parallel entity system to retire"?**

### Evidence for retirement:

- **Zero production consumption**: No production code outside the bigworld test module uses `apollo::bw`/`apollo::bigworld` APIs
- **4 empty stub files**: `runtime_wrapper.cpp`, `entity.cpp`, `timer.cpp`, `callback.cpp` contain zero implementation lines — only comments deferring to `Runtime`/`Entity`
- **Single real implementation point**: All functionality concentrates in `runtime.cpp` (489 lines). If that file is removed/changed, the entire compatibility layer breaks.
- **Entity system isolation**: `apollo::bw::Entity` is completely separate from `modules/game`'s Entity system — no shared base classes, no integration paths
- **No game module integration**: The game module (the natural consumer) does not include or use `apollo::bw` at all
- **Build disabled by default**: Both `APOLLO_BUILD_BIGWORLD_MODULE` and `APOLLO_BUILD_GAME_MODULE` default to `OFF`

### Evidence for retention:

- Provides a **smooth migration path** for code porting from BigWorld 14.4.1
- The `Runtime` class offers useful timer/entity management that could be valuable internally
- Mapping headers (`include/bigworld/BigWorld.h`, `modules/bigworld/include/apollo/bigworld/`) provide clean abstraction boundaries
- The `apollo::bw::BigWorld::createEntity()` factory pattern could be repurposed for entity type extension

### Verdict:

Based on the evidence, the compatible layer functions as a **BigWorld API compatibility bridge** but exhibits strong signs of being a **parallel entity system that should be marked for deprecation/retirement**:

- It has no production consumers outside its own test module
- It consists mainly of empty stub files deferring to a single implementation file
- The game module has its own self-contained, used entity system
- Both modules are build-disabled by default

The judgment from §16.8.3-④ "参考件不是生产件的家" ("the reference is not a production piece") should be **upgraded to "整体退役的并行实体体系"** ("parallel entity system overall slated for retirement"). The layer can be preserved as a legacy compatibility header for in-flight porting projects, but it is not integrated into the active codebase and should not be expanded.

---

**Audit notes**: All line references are **实测** (measured from actual source files). No source code was modified in producing this report.