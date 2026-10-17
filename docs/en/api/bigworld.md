---
title: BigWorld API
icon: bigworld
prev: /api/README.md
---

# BigWorld API

> 2026-10-10 audit: this page was rewritten against the actual state of HEAD
> (compatibility surface = the `include/bigworld/BigWorld.h` header library +
> the module layer `modules/bigworld`, backed by `apollo::bw::BigWorld`). The
> old draft's `BigWorld::SPACE_ID` two-arg `createEntity`, `getEntity`,
> `ICallback`/`registerCallback`, `Runtime()->addTimer(1000, fn, true)` etc.
> do not exist in the repository.

## Compatibility surface (include/bigworld/BigWorld.h, namespace BigWorld)

A thin migration surface for existing BigWorld-style code — header-library
form, no link dependency, mapping onto `apollo::bw`:

```cpp
#include <bigworld/BigWorld.h>

// time
BigWorld::time();    // seconds (double)
BigWorld::timeMs();  // milliseconds
BigWorld::update();  // advance the timer frame

// entities
auto entity = BigWorld::createEntity("Player");        // single arg: type name
auto e2     = BigWorld::entity(id);                    // by EntityId
auto list   = BigWorld::entities();                    // all entities
bool dead   = BigWorld::destroyEntity(id);
BigWorld::registerEntityFactory("Player", factory);    // custom construction

// callbacks/timers (second-based delays)
auto cid = BigWorld::callback(0.5, []() { /* one-shot */ });
BigWorld::cancelCallback(cid);
auto tid = BigWorld::addTimer(initial, repeat,
                              [](BigWorld::TimerId, int32_t userArg) { /* periodic */ },
                              userArg);
BigWorld::delTimer(tid);
```

## Entities (apollo::bigworld::Entity)

`Entity(EntityID id)`; the property surface is templated key-value:

```cpp
#include <apollo/bigworld/entity.hpp>

Entity e{id};
e.set("hp", 100);                 // set(name, value)
int hp = e.get("hp", 0);          // get(name, default_value)
bool alive = e.is_valid();        // verified via Runtime::entity_exists
```

There are no BigWorld world-geometry methods such as `position()/teleport()`
(spatial geometry is not wired; Witness/Ghost distributed space is planned,
not built).

## Migration Cross-reference

| Old BigWorld API | Current state |
|----------------|------|
| `BigWorld::createEntity(SPACE_ID, "Type")` | `BigWorld::createEntity("Type")` (single arg) |
| `BigWorld::getEntity(id)` | `BigWorld::entity(id)` |
| `BigWorld::destroyEntity(id)` | supported under the same name |
| `BigWorld::Runtime()->addTimer(...)` | `BigWorld::addTimer(initial, repeat, fn(TimerId,int32), userArg)` |
| `ICallback` / `registerCallback` | does not exist (use `callback(delay, fn)` one-shot or `addTimer` periodic) |
| `Proxy` / `RoomServer` concepts | for the process surface see [Server Applications](/en/apps/); Witness/Ghost are planned, not built |

## Related Docs

- [BigWorld Architecture](/en/architecture/bigworld)
- [BigWorld Lifecycle](/en/architecture/bigworld-lifecycle)
- [Module overview](/en/modules/) (the BigWorld module page)
