---
title: Core Concepts
icon: lightbulb
order: 4
prev: /en/guide/quick-start
next: /en/guide/module-system
---

# Core Concepts

## Application

Application is Apollo's basic unit of execution, representing a startable/stoppable service. The business side implements `IHostedService` (start/stop/tick); the host `ApplicationHost`/`ServiceHost` drives it frame by frame.

```cpp
class MyService : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override { return "my-service"; }
    bool start() override { running_ = true; return true; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    void tick() override { /* 定帧业务 */ }

private:
    bool running_ = false;
};
```

## Module

Apollo uses a modular architecture; each module provides a specific capability.

| Module | Purpose |
|------|------|
| `apollo::base` | Base utilities |
| `apollo::core` | Core framework |
| `apollo::runtime` | Runtime host |
| `apollo::net` | Networking |
| `apollo::data` | Data access |
| `apollo::game` | Game logic |

## Dependency Injection (DI)

Apollo provides the constructor-injection container `apollo::core::di`: a type-keyed bean graph, topologically ordered assembly, and only two scopes, Singleton/Prototype. Assembly happens only at the application entry (apps/).

```cpp
#include <apollo/core/di/application_context.hpp>

apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<GameClockService>().name("game_clock");
builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
auto context = builder.build();
if (!context.initialize()) {
    return 1;
}
```

## Config

`apollo::core::config::ConfigRegistry` is a key-value registry (string/int64/bool);
Configuration snapshot replacement at tick boundaries is not integrated; see [configuration](../guide/configuration.md).

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
int64_t port = config.get_int64("server.port");
```

## Log

Leveled logging (`apollo::core::log`, in-house multi-level; file/console output).

```cpp
#include <apollo/core/log/log_manager.hpp>

APOLLO_LOG()->info("这是一条信息");
APOLLO_LOG()->warning("这是一条警告");
APOLLO_LOG()->error("这是一条错误");
```

## Entity

The basic object of the game world: `apollo::game::core::Entity` (strongly typed `EntityId`;
`PlayerId`/`EntityId` never implicitly convert), with component attachment support (`IEntityComponent`).

```cpp
#include <apollo/game/core/entity.hpp>

apollo::game::core::Entity entity(apollo::game::core::EntityId{12345}, "Player");
entity.add_component<HealthComponent>();
auto health = entity.get_component<HealthComponent>();
```

## AOI (Area of Interest)

Interest management: `apollo::game::world::SceneAoi` 3×3 grid (one instance owned exclusively by each Scene,
naturally isolated per scene); `ViewerState` records the per-viewer vision watermark, and the
Enter/Sync/Leave event surface is dispatched through a sink (the gateway delivery side is left to P3-2).

```cpp
#include <apollo/game/world/scene_aoi.hpp>

apollo::game::world::SceneAoi aoi(1000.0f, 1000.0f, 100.0f, 200.0f);
aoi.enter(apollo::game::core::EntityId{1001}, {100.0f, 0.0f, 100.0f});
auto viewers = aoi.viewers_at({100.0f, 0.0f, 100.0f});
```

## Actor model

> **Not implemented** (2026-10-04 reconciliation): there is no Actor code in the repository. The current concurrency model =
> frame-driven scene threads + the single-writer discipline (concurrency C-4: tick belongs to the scene thread, IO holds no
> game data locks); the "one independent message queue per Actor" sample code is a legacy draft remnant, to be cleaned up with P4-1.

## Next steps

Learn about the [Module System](./module-system.md).
