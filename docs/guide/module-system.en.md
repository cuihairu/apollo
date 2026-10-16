---
title: Module System
icon: puzzle
order: 5
prev: /guide/concepts
next: /guide/configuration
---

[English](module-system.en.md) | [中文](module-system.md)

# Module System

Apollo adopts a layered modular architecture; every module has a clear responsibility and can be used independently.

## Module layers

```
┌─────────────────────────────────────────────────────────────────┐
│                        Applications                              │
└─────────────────────────────────────────────────────────────────┘
                                ↓
┌──────┬──────┬──────┬───────┬────────┬─────────┬─────────┬──────┐
│  net │ data │ game │protocol│contract│bigworld │ runtime │ core │
└──────┴──────┴──────┴───────┴────────┴─────────┴─────────┴──────┘
                                ↓
┌─────────────────────────────────────────────────────────────────┐
│                        Base                                      │
└─────────────────────────────────────────────────────────────────┘
```

## Using modules

### CMakeLists.txt setup

```cmake
add_subdirectory(path/to/apollo)

target_link_libraries(my_app
    apollo::core
    apollo::net_core
)
```

### Conditional loading

```cmake
if(APOLLO_BUILD_GAME_MODULE)
    target_link_libraries(my_app apollo::game_world)
endif()
```

## Module dependencies

**Important rules**:
- Upper-layer modules may depend on lower-layer modules
- Lower-layer modules must not depend on upper-layer modules
- Same-layer modules should avoid depending on each other

```
net ← runtime
data ← core
game ← core
```

## Custom modules

### 1. Create the module directory

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

### 2. Write CMakeLists.txt

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

## Next steps

Learn about the [Configuration system](./configuration.en.md).
