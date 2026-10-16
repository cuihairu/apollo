---
title: Core Module
icon: core
order: 2
category:
  - Modules
tag:
  - core
  - kernel
---

[English](core.en.md) | [中文](core.md)

# Core Module

The Core module provides the framework kernel and defines the common runtime semantics.

## Components

### Dependency Injection (DI)

`apollo::core::di` is a minimal constructor-injection container: a type-keyed bean graph,
topologically ordered assembly, only the Singleton/Prototype scopes. Assembly happens only
at the application entry point (apps/); no annotations, no registration macros.

```cpp
#include <apollo/core/di/application_context.hpp>

// 定义服务（普通类，依赖走构造函数）
class DatabaseService {
public:
    void connect() { /* ... */ }
};

// 构建：builder 声明 bean，build 产出上下文，initialize 按拓扑序装配
apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<DatabaseService>().name("database");
auto context = builder.build();
if (!context.initialize()) {
    // 装配失败
}

// 解析：get<T>() 取按类型键；get_named<T>("...") 按名取
auto& db = context.get<DatabaseService>();
db.connect();
```

### Configuration (Config)

`apollo::core::config::ConfigRegistry`, a key-value registry (string/int64/bool); hot reload
is planned to swap a ConfigSnapshot at tick boundaries (architecture-review §17.6, not implemented).

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));

int64_t port = config.get_int64("server.port");
std::string host = config.get_string("server.host");
bool ok = config.has("server.port");
```

### Logging (Log)

`apollo::core::log`: the `LogManager` singleton (`initialize(config)` / `getLogger(name)` /
`createLogger(name, level)` / `removeLogger` / `hasLogger` / `flushAll` /
`shutdown`), with the config factory `LogManagerConfig::createDefault/createConsoleOnly/
createCombined/createFileOnly`. **There are only two convenience macros**: `APOLLO_LOG()`
(the default logger) and `APOLLO_LOG_GET(name)` (a named logger) — formatting goes through
the logger method chain; macros like `APOLLO_LOG_INFO/LOG_WARN_F` do not exist (invented in
an old draft; the doxygen samples at the top of log.h are stale comments too).

```cpp
#include <apollo/core/log/log_manager.hpp>

APOLLO_LOG()->info("这是一条信息");
APOLLO_LOG()->warn("端口 {} 已变更", 8888);
APOLLO_LOG_GET("network")->error("timeout");

auto& mgr = apollo::core::log::LogManager::instance();
mgr.initialize(apollo::core::log::LogManagerConfig::createCombined("logs/server.log",
               apollo::core::log::LogLevel::Info));
auto logger = mgr.getLogger("MyLogger");
```

### Lifecycle

The `IApplicationLifecycle` six hooks (the host calls back per phase): `on_application_boot` /
`on_application_config_loaded` / `on_application_initialized` /
`on_application_ready` / `on_application_reload` / `on_application_stop`.
The `ApplicationHost` six-phase state machine (Boot→…→Stopped) lives in the runtime module.

```cpp
#include <apollo/core/application_lifecycle.hpp>

class MyApplication : public apollo::core::IApplicationLifecycle {
public:
    void on_application_ready() override {
        APOLLO_LOG()->info("应用就绪");
    }
    void on_application_stop() override {
        APOLLO_LOG()->info("应用停止");
    }
};
```

### Events (Event)

> **Not implemented** (2026-10-04 reconciliation): `apollo/core/event_bus.hpp` does not exist;
> the repository has no generic EventBus. The existing event surfaces are domain-specific:
> PlayerDirectory reports online-directory events (session_up/down/moved/kicked) through
> `EventSink`, and `SceneAoi::EventSink` reports the three AOI events — new events first enter
> the glossary, then land as per-domain interfaces; no generic bus.

### Timers (Timer)

The timing wheel lives in the base module: `apollo::base::TimerWheel` (the G-4 library piece,
hierarchical hash, driven by feeding the clock with `advance(now_ms)`, see the
[Base module](/modules/base)) — the core module proper contains no timers. The top-level
legacy piece `include/apollo/core/timer/timer_manager.h`
(`TimerManager::setTimer/killTimer`) is referenced only by the legacy GTest batch, is not part
of the gate, and must not be used in new code.

## Dependencies

- apollo::base

## Linking

```cmake
find_package(apollo-core REQUIRED)
target_link_libraries(my_app apollo::core)
```
