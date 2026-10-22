---
title: Application Hosts and Dependency Injection
---

# Application Hosts and Dependency Injection

Use existing runtime and container types rather than defining synonymous hosts. Business process boundaries follow the [active architecture decisions](./adr.md).

## Current components

| Component | Ownership and responsibility |
|---|---|
| `ApplicationContextBuilder` | Startup registration of types, constructor dependencies, names, and scopes |
| `ApplicationContext` | Creates and owns injected objects, initializes dependencies, releases in reverse order |
| `ApplicationHost` | Hosted service lifecycle, stop reasons, and shutdown hooks |
| `ServiceHost` | Convenience application-host wrapper |
| `WorldHost` | Fixed-tick World service execution |
| `IHostedService` | start / stop / is_running / tick interface |

Dependency injection lives in `modules/core`; hosting lives in `modules/runtime`. Constructing dependencies does not start business services. Applications must coordinate both lifecycles explicitly.

## Registration and lookup

ApplicationContextBuilder supports Singleton and Prototype. Constructor dependencies use type keys; Singleton initialization follows dependency order. `get<T>()` and `get_named<T>()` resolve dependencies, but business hot paths and Lua should not use the container as an arbitrary service locator.

```cpp
apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<GameClockService>().name("game_clock");
builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
auto context = builder.build();
if (!context.initialize()) return 1;
```

The application supplies these example types. Current validation includes assertions for missing, ambiguous, and cyclic dependencies; this does not establish complete production diagnostics, configuration validation, or uniform assembly across applications.

## Startup and shutdown

Entry points load their configuration, compose services, register hosts, initialize logging/runtime facilities, and define shutdown order. ApplicationHost owns the lifecycle of registered services; other services remain explicitly application-owned.

Current WorldApp uses WorldHost for simulation; move execution into cell and retain World management. LoginServer manages OnlineDirectory/admission. Persistent-player and storage components must be hosted by dedicated baseServer/database entries. They are not fully containerized applications, and consistent terminology does not require additional unimplemented HostBuilder or ServiceCollection types.

Shutdown closes admission and intake before completing accepted work, settlement, persistence, and resource cleanup. Dependency facilities stop last. Deadlines and active-instance recovery require business acceptance rather than inference from destructor order.

## Scripting and updates

Reuse Lua ScriptRuntime with startup-injected named services. Scripts access controlled APIs; simulation writes run on the owning logic thread. Module replacement does not modify the C++ dependency graph at runtime. C++ changes use process replacement.

See the [architecture overview](../guide/architecture.md) for the two update modes and their boundaries.

## Related documentation

- [Module assembly](./starter-and-module-assembly-design.md)
- [Runtime module](../modules/runtime.md)
- [Core module](../modules/core.md)
