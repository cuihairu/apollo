---
title: BigWorld 模块
icon: bigworld
order: 8
category:
  - 模块
tag:
  - bigworld
  - 兼容层
---

# BigWorld 模块

BigWorld 模块提供 BigWorld API 兼容层，允许现有 BigWorld 风格代码平滑迁移。
> 2026-10-10 对账：本页按 HEAD 实况重写——兼容面 = `include/bigworld/BigWorld.h`
> 头库，模块层 `modules/bigworld`（Entity/Runtime/Timer/Callback），底层
> `apollo::bw::BigWorld`。旧稿的 `ICallback`/`registerCallback`/`SPACE_ID`
> 双参 createEntity/`getEntity` 等在仓库中不存在。

## 概述

```
BigWorld 风格调用（namespace BigWorld，include/bigworld/BigWorld.h 头库）
    ↓ 内联映射
apollo::bw::BigWorld（modules/bigworld + include/apollo/bw/bigworld.h）
    ↓ 消费
Apollo 模块 (base 的 TimerWheel 驱动、game 的实体域等)
```

## 基本用法

```cpp
#include <bigworld/BigWorld.h>

// 实体：工厂注册 + 创建/获取/销毁（EntityId 路由）
BigWorld::registerEntityFactory("Player", factory);
auto entity = BigWorld::createEntity("Player");      // 单参：类型名
auto found  = BigWorld::entity(id);                  // 按 EntityId 取
auto all    = BigWorld::entities();
BigWorld::destroyEntity(id);

// 实体属性（apollo::bigworld::Entity：模板化键值面）
entity->set("hp", 100);
int hp = entity->get("hp", 0);

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

## 迁移对照

| 旧 BigWorld API | 现状 |
|----------------|------|
| `createEntity(SPACE_ID, "Type")` | `createEntity("Type")`（单参，无空间 ID 参） |
| `getEntity(id)` | `entity(id)` |
| `destroyEntity(id)` | 同名支持 |
| `Runtime()->addTimer(ms, fn, repeat)` | `addTimer(initialSec, repeatSec, fn(TimerId, int32_t), userArg)`（秒制） |
| `ICallback` / `registerCallback` | 不存在——一次性用 `callback(delay, fn)`，周期用 `addTimer` |
| `Proxy` / `CellApp` 概念映射 | 进程面见[服务器应用](/apps/)；Witness/Ghost 分布式空间规划态 |

## 不兼容的部分

- 实体无 `position()/teleport()` 等世界几何方法（空间几何未接线）
- Witness/Ghost/分布式空间为规划态语义（对照 [架构参考](/architecture/bigworld)）
- 自定义 Python 脚本需迁移到 C++（Lua 面见 scripting-lua 设计件，未入主线）

## 依赖

- apollo::game_core（实体域消费）
- apollo::runtime（宿主运行时）

## 链接

```cmake
find_package(apollo-bigworld REQUIRED)
target_link_libraries(my_app apollo::bigworld)
```

## 相关文档

- [BigWorld API](/api/bigworld) —— 兼容面 API 明细
- [BigWorld 架构详解](/architecture/bigworld)
- [BigWorld 生命周期](/architecture/bigworld-lifecycle)
