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

BigWorld 模块提供 BigWorld API 兼容层，允许现有 BigWorld 代码平滑迁移。

## 概述

BigWorld 兼容层将 BigWorld 风格的 API 映射到 Apollo 的模块化架构上：

```
BigWorld API
    ↓
BigWorld 兼容层
    ↓
Apollo 模块 (game, net, data, ...)
```

## 基本用法

### 实体操作

```cpp
#include <bigworld/BigWorld.h>

// 创建实体
BigWorld::EntityID id = BigWorld::createEntity(
    BigWorld::SPACE_ID,  // 空间ID
    "Player"            // 实体类型
);

// 获取实体
BigWorld::IEntity* entity = BigWorld::getEntity(id);

// 操作实体
entity->position({100, 200, 0});
entity->teleport({300, 400, 0});

// 销毁实体
BigWorld::destroyEntity(id);
```

### 回调系统

```cpp
#include <bigworld/BigWorld.h>

// 定义回调
class MyCallback : public BigWorld::ICallback {
public:
    void onEntityEnter(BigWorld::IEntity* entity) override {
        LOG_INFO("BigWorld", "实体进入: {}", entity->id());
    }

    void onEntityLeave(BigWorld::IEntity* entity) override {
        LOG_INFO("BigWorld", "实体离开: {}", entity->id());
    }

    void onSpaceGeometryChanged(BigWorld::SpaceID spaceID) override {
        LOG_INFO("BigWorld", "空间几何变化: {}", spaceID);
    }
};

// 注册回调
BigWorld::Runtime()->registerCallback(new MyCallback());
```

### 定时器

```cpp
#include <bigworld/BigWorld.h>

// 一次性定时器
BigWorld::Runtime()->addTimer(1000, []() {
    LOG_INFO("BigWorld", "1秒后执行");
});

// 周期定时器
BigWorld::Runtime()->addTimer(100, []() {
    // 每100ms执行
}, true);
```

## 迁移指南

### 从 BigWorld 迁移

1. **替换头文件**

```cpp
// 旧代码
#include "BigWorld.h"

// 新代码
#include <bigworld/BigWorld.h>
```

2. **API 映射**

| BigWorld API | Apollo 兼容层 |
|-------------|--------------|
| `BigWorld::createEntity()` | ✅ 支持 |
| `BigWorld::destroyEntity()` | ✅ 支持 |
| `BigWorld::getEntity()` | ✅ 支持 |
| `BigWorld::Runtime()` | ✅ 支持 |
| `BigWorld::registerCallback()` | ✅ 支持 |

3. **重新编译**

```cmake
find_package(apollo-bigworld REQUIRED)
target_link_libraries(my_app apollo::bigworld)
```

### 不兼容的部分

以下功能需要适配：

- BigWorld 的 `Proxy` 概念映射到 Apollo 的 `GatewayApp`
- BigWorld 的 `CellApp` 概念映射到 Apollo 的 `CellApp` 进程
- 自定义 Python 脚本需要迁移到 C++ 或使用脚本引擎

## 依赖

- apollo::game
- apollo::net
- apollo::data

## 链接

```cmake
find_package(apollo-bigworld REQUIRED)
target_link_libraries(my_app apollo::bigworld)
```

## 相关文档

- [BigWorld 架构详解](/architecture/bigworld.md)
- [BigWorld 生命周期](/architecture/bigworld-lifecycle.md)
