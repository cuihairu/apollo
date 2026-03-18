---
title: 模块系统
icon: puzzle
order: 5
prev: /guide/concepts
next: /guide/configuration
---

# 模块系统

Apollo 采用分层模块化架构，每个模块职责清晰，可以独立使用。

## 模块分层

```
┌─────────────────────────────────────────────────────────────────┐
│                        Applications                              │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌──────┬──────┬──────┬──────┬──────────┬──────────┬────────────┐
│  net │ data │ game │ actor│ framework│  runtime │   core     │
└──────┴──────┴──────┴──────┴──────────┴──────────┴────────────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                        Base                                      │
└─────────────────────────────────────────────────────────────────┘
```

## 使用模块

### CMakeLists.txt 配置

```cmake
find_package(apollo-core REQUIRED)
find_package(apollo-net REQUIRED)

target_link_libraries(my_app
    apollo::core
    apollo::net
)
```

### 条件加载

```cmake
if(ENABLE_GAME_MODULE)
    find_package(apollo-game REQUIRED)
    target_link_libraries(my_app apollo::game)
endif()
```

## 模块依赖

**重要规则**：
- 上层模块可以依赖下层模块
- 下层模块不能依赖上层模块
- 同层模块尽量避免相互依赖

```
base ← core ← runtime ← net/data/game
```

## 自定义模块

### 1. 创建模块目录

```
modules/
└── mymodule/
    ├── include/
    │   └── apollo/
    │       └── mymodule/
    │           └── api.hpp
    ├── src/
    │   └── impl.cpp
    └── CMakeLists.txt
```

### 2. 编写 CMakeLists.txt

```cmake
add_library(apollo_mymodule STATIC
    include/apollo/mymodule/api.hpp
    src/impl.cpp
)

target_include_directories(apollo_mymodule
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(apollo_mymodule
    PUBLIC apollo::base
    PRIVATE apollo::core
)

add_library(apollo::mymodule ALIAS apollo_mymodule)
```

## 下一步

了解 [配置系统](./configuration.md)。
