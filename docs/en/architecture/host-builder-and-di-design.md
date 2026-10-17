---
title: Host Builder and DI Design
icon: sitemap
order: 18
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - HostBuilder
  - DI
  - Bootstrap
---

# Host Builder and DI Design

> **Status (ruled and on record 2026-09-30, C-76)**: **reference piece** — for the implementation status of containers/lifecycle/assembly and the conclusions in force, `docs/analysis/architecture-review.md` §17/§21 is authoritative (HostBuilder/ServiceCollection are neither implemented nor scheduled; the repo's sole container consumer = the game-server demo — the §20 ten-item comparison); §16 of this piece, "choosing multi-script backends by profile", has been superseded by `docs/design/scripting-lua.md` §2 (Lua single language, decision #12). Status table = tier A of `docs/architecture/README.md`.

This document addresses a very core question in getting Apollo's assembly system to land further:

`once starter, profile, manifest, and bootstrap all exist, how do modules and objects actually get assembled?`

If this layer is left undefined, the earlier assembly designs will ultimately degrade back into:

- a pile of objects hand-newed in main
- module initialization order hardcoded
- dependencies shoved down through constructor after constructor

That would keep Apollo from ever truly becoming a composable framework.

## 1. Design Goals

The design of this layer must solve six problems:

1. how objects are uniformly built when an app starts.
2. how modules register services and factories.
3. how the host assembles runtime / ops / platform / game / domain.
4. how modules obtain their dependencies.
5. how lifecycle objects are hosted.
6. how to avoid an overly complex IoC container in C++.

## 2. Reference Sources

### 1. The Host Builder idea

Reference points:

- build the host first at startup
- the host then carries the services and modules
- configuration, logging, and lifecycle come before the business domains

### 2. The dependency injection idea

Reference points:

- modules do not new their dependencies everywhere directly
- dependencies are injected through the container and the builder

Points not copied wholesale:

- no Java-style runtime reflection injection
- no large-scale implicit auto-wiring

### 3. KBE's host boundaries

Reference points:

- capabilities such as login, connection, world, script, and entity must hang off explicit hosts

## 3. Why This Design

Apollo is a C++ framework and is not suited to:

- heavy-reflection IoC
- heavy annotation containers
- vague runtime dependency injection

But it is very well suited to:

- an explicit Host Builder
- a lightweight DI container
- lifecycle hosting
- module registration factories

In other words, the more reasonable route for Apollo is not:

- "build a heavyweight runtime container"

but:

- "build a C++-style explicit host builder"

## 4. Strengths

- dependency relationships become clearer
- the startup process becomes more verifiable
- module assembly becomes more stable
- app `main` can get noticeably thinner
- it pairs better with manifest / starter / bootstrap

## 5. Costs and Risks

- the container and registration APIs must be designed up front
- module authoring has to be constrained — no more casual newing
- if made too heavy, it turns a C++ project into an unmaintainable "pseudo-Java IoC"

## 6. Why the Other Options Were Not Chosen

### Not "fully manual construction"

Because modules and apps quickly spiral out of control as they grow.

### Not "a heavyweight IoC container"

Because C++ is a poor fit for that road; debugging and maintainability both deteriorate.

### Not "every module keeping its own local singletons"

Because that makes dependency boundaries ever more hidden.

Apollo is better served by:

- an explicit builder
- lightweight service registration
- lifecycle hosting

## 7. Core Concepts

## 8. `HostBuilder`

`HostBuilder` is the core object of the whole startup assembly.

It owns:

- creating the host
- assembling configuration, logging, and the event bus
- loading module registrations
- building the service container
- advancing the lifecycle stages

Suggested structure:

```text
HostBuilder
    ├── BootstrapContext
    ├── ServiceCollection
    ├── ModuleRegistry
    ├── HostParts
    └── BuildResult
```

### Why have a `HostBuilder`

Because eventually every Apollo app should go through one unified build path, rather than each app doing its own thing.

## 9. `ServiceCollection`

This tier is Apollo's lightweight registration container.

It owns:

- registering singletons
- registering scoped/app-lifetime services
- registering factories
- registering interfaces to implementations

But it should not take on:

- complex runtime reflection injection
- vague auto-resolution magic

### Recommended registration styles

```text
addSingleton<T>()
addSingleton<TInterface, TImpl>()
addFactory<T>()
addInstance<T>()
```

### Recommended principles

- explicit first
- lifecycles kept clear
- type safety first

## 10. `ServiceProvider`

`ServiceProvider` is the read-only container once the build completes.

It owns:

- resolving services
- providing dependency queries
- keeping lifecycle objects alive

### Why separate `Collection` and `Provider`

Because:

- the registration phase and the resolution phase should be separated

That makes it easier to do:

- startup validation
- duplicate registration checks
- missing dependency checks

## 11. The Recommended Lifecycle Model

Apollo should support only a few well-defined lifecycles here.

### 1. `Singleton`

Fits:

- config manager
- logger
- telemetry bridge
- module registry

### 2. `HostScoped`

Fits:

- world host
- runtime ops host
- script runtime host

### 3. `Factory`

Fits:

- domain object creator
- entity builder
- repository builder

### Why no more complex scopes

Because Apollo's current focus is process-level assembly, not web-request-scoped lifecycles.

## 12. How Modules Plug into DI

Each module should provide one explicit registration entry.

Logically close to:

```cpp
void registerRuntimeServices(ServiceCollection& services);
void registerPlatformRedisServices(ServiceCollection& services);
void registerWorldServices(ServiceCollection& services);
```

### The rationale for this design

- module boundaries stay clear
- no runtime scanning needed
- it suits C++ building and debugging better

## 13. The Recommended Build Order

`HostBuilder` should assemble in at least the following order:

1. core services
2. runtime host
3. runtime ops host
4. platform foundation
5. game foundation
6. domain components
7. distributed world extension (if enabled)

### Why this order

Because:

- ops and observability should come before business domains
- the platform layer should come before the game domain
- game foundations should come before business components

## 14. The Recommended Object Model

```text
HostBuilder
    ├── buildCore()
    ├── buildRuntime()
    ├── buildOps()
    ├── buildPlatform()
    ├── buildGameFoundation()
    ├── buildDomain()
    └── buildDistributed()
```

### `BuildResult`

Suggested to contain:

```text
BuildResult
    service_provider
    application_host
    runtime_host
    runtime_ops_host
    initialized_modules
```

## 15. The Relationship to starter / manifest / bootstrap

### `Manifest`

Tells the builder:

- which modules exist
- what the dependency graph is

### `Starter`

Tells the builder:

- which modules are selected by default

### `Bootstrap`

Drives the builder:

- to build the host and services at the right stages

So `HostBuilder` is in effect:

- the execution hub of starter / manifest / bootstrap

## 16. The Relationship to the Script Layer

Script backends should also plug in through the same registration style.

For example:

- `registerLuaScriptServices`
- `registerPythonScriptServices`

### Principles

- one project usually enables only one primary script backend
- the builder picks the backend from profile and the conflict rules

## 17. Direct Implications for Apollo Today

Apollo already has some host and lifecycle foundations, but nothing official yet for:

- `HostBuilder`
- `ServiceCollection`
- `ServiceProvider`

The more realistic next steps would be:

1. first define the minimal service registration API
2. first bring the runtime / ops / platform modules on board
3. then let app `main.cpp` thin out step by step

## 18. Conclusion

If Apollo is to truly land as a "composable framework", `HostBuilder + lightweight DI` is an indispensable layer.

It exists not to imitate Java, but so that:

- starter
- profile
- manifest
- bootstrap

these designs can finally actually execute.

## Related Reading

- [Starter and Module Assembly Design](./starter-and-module-assembly-design.md)
