---
title: BigWorld Module
icon: bigworld
order: 8
category:
  - Modules
tag:
  - bigworld
  - compatibility
---

[English](bigworld.en.md) | [中文](bigworld.md)

# BigWorld Module

The BigWorld module provides a BigWorld API compatibility layer, allowing existing BigWorld-style code to migrate smoothly.
> 2026-10-10 reconciliation: this page was rewritten against the actual HEAD — the compatibility
> surface is the `include/bigworld/BigWorld.h` header library, the module layer
> `modules/bigworld` (Entity/Runtime/Timer/Callback), and the lower layer
> `apollo::bw::BigWorld`. The old draft's `ICallback`/`registerCallback`/`SPACE_ID`
> two-arg createEntity/`getEntity` and friends do not exist in the repository.

## Overview

```
BigWorld 风格调用（namespace BigWorld，include/bigworld/BigWorld.h 头库）
    ↓ 内联映射
apollo::bw::BigWorld（modules/bigworld + include/apollo/bw/bigworld.h）
    ↓ 消费
Apollo 模块 (bw 自实现堆定时器、game 的实体域等)
```

## Basic Usage

```cpp
#include <bigworld/BigWorld.h>

// 实体：工厂注册 + 创建/获取/销毁（EntityId 路由）
BigWorld::registerEntityFactory("Player", factory);
auto entity = BigWorld::createEntity("Player");      // 单参：类型名
auto found  = BigWorld::entity(id);                  // 按 EntityId 取
auto all    = BigWorld::entities();
BigWorld::destroyEntity(id);

// 实体属性：未接线（apollo::bigworld::Entity 无 set/get，属性面挂 attributes 模块）
// entity->set("hp", 100);
// int hp = entity->get("hp", 0);

// 回调与定时器（秒制延迟；TimerId 可撤销）
auto cid = BigWorld::callback(0.5, []() { /* 一次性 */ });
BigWorld::cancelCallback(cid);
auto tid = BigWorld::addTimer(/*initial=*/1.0, /*repeat=*/0.1,
                              [](BigWorld::TimerId, int32_t userArg) { /* 周期 */ },
                              /*userArg=*/0);
BigWorld::delTimer(tid);

// 时间与帧推进
double t = BigWorld::time();     // 秒
uint64_t ms = BigWorld::timeMs();
BigWorld::update();              // 驱动定时器帧
```

## Migration Mapping

| Old BigWorld API | Current state |
|----------------|------|
| `createEntity(SPACE_ID, "Type")` | `createEntity("Type")` (single arg, no space ID argument) |
| `getEntity(id)` | `entity(id)` |
| `destroyEntity(id)` | supported under the same name |
| `Runtime()->addTimer(ms, fn, repeat)` | `addTimer(initialSec, repeatSec, fn(TimerId, int32_t), userArg)` (seconds) |
| `ICallback` / `registerCallback` | does not exist — one-shot uses `callback(delay, fn)`, periodic uses `addTimer` |
| `Proxy` / `CellApp` concept mapping | for the process surface see [Server Apps](/apps/); Witness/Ghost distributed spaces are planned state |

## Incompatible Parts

- Entities have no `position()/teleport()` or other world-geometry methods (spatial geometry not wired)
- Witness/Ghost/distributed spaces are planned-state semantics (see the [architecture reference](/architecture/bigworld))
- Custom Python scripts must migrate to C++ (for the Lua surface see the scripting-lua design doc, not yet on mainline)

## Dependencies

- apollo::game_core (entity domain consumer)
- apollo::runtime (host runtime)

## Linking

```cmake
add_subdirectory(path/to/apollo)
target_link_libraries(my_app apollo::bigworld)
# 模块默认 OFF，需 APOLLO_BUILD_BIGWORLD=ON 开启
```

## Related Docs

- [BigWorld API](/api/bigworld) — compatibility surface API details
- [BigWorld Architecture Deep Dive](/architecture/bigworld)
- [BigWorld Lifecycle](/architecture/bigworld-lifecycle)
