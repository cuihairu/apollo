---
title: BigWorld API
icon: bigworld
prev: /api/README.md
---

# BigWorld API

> 2026-10-10 对账：本页按 HEAD 实况重写（兼容面 = `include/bigworld/BigWorld.h`
> 头库 + 模块层 `modules/bigworld`，底层 `apollo::bw::BigWorld`）。旧稿的
> `BigWorld::SPACE_ID` 双参 `createEntity`、`getEntity`、`ICallback`/
> `registerCallback`、`Runtime()->addTimer(1000, fn, true)` 等在仓库中不存在。

## 兼容面（include/bigworld/BigWorld.h，namespace BigWorld）

现有 BigWorld 风格代码的薄迁移面——头库形态，无链接依赖，映射到 `apollo::bw`：

```cpp
#include <bigworld/BigWorld.h>

// 时间
BigWorld::time();    // 秒（double）
BigWorld::timeMs();  // 毫秒
BigWorld::update();  // 推进定时器帧

// 实体
auto entity = BigWorld::createEntity("Player");        // 单参：类型名
auto e2     = BigWorld::entity(id);                    // 按 EntityId 取
auto list   = BigWorld::entities();                    // 全部实体
bool dead   = BigWorld::destroyEntity(id);
BigWorld::registerEntityFactory("Player", factory);    // 自定义构造

// 回调/定时器（秒制延迟）
auto cid = BigWorld::callback(0.5, []() { /* 一次性 */ });
BigWorld::cancelCallback(cid);
auto tid = BigWorld::addTimer(initial, repeat,
                              [](BigWorld::TimerId, int32_t userArg) { /* 周期 */ },
                              userArg);
BigWorld::delTimer(tid);
```

## 实体（apollo::bigworld::Entity）

`Entity(EntityID id)`，属性面为模板化键值：

```cpp
#include <apollo/bigworld/entity.hpp>

Entity e{id};
e.set("hp", 100);                 // set(name, value)
int hp = e.get("hp", 0);          // get(name, default_value)
bool alive = e.is_valid();        // 经 Runtime::entity_exists 核对
```

无 `position()/teleport()` 等 BigWorld 世界几何方法（空间几何未接线，
Witness/Ghost 分布式空间为规划态）。

## 迁移对照

| 旧 BigWorld API | 现状 |
|----------------|------|
| `BigWorld::createEntity(SPACE_ID, "Type")` | `BigWorld::createEntity("Type")`（单参） |
| `BigWorld::getEntity(id)` | `BigWorld::entity(id)` |
| `BigWorld::destroyEntity(id)` | 同名支持 |
| `BigWorld::Runtime()->addTimer(...)` | `BigWorld::addTimer(initial, repeat, fn(TimerId,int32), userArg)` |
| `ICallback` / `registerCallback` | 不存在（改 `callback(delay, fn)` 一次性或 `addTimer` 周期） |
| `Proxy` / `CellApp` 概念 | 进程面见 [服务器应用](/apps/)；Witness/Ghost 规划态 |

## 相关文档

- [BigWorld 架构详解](/architecture/bigworld)
- [BigWorld 生命周期](/architecture/bigworld-lifecycle)
- [模块总览](/modules/)（BigWorld 模块页）
