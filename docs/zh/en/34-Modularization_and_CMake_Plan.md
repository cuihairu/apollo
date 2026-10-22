
# Apollo Modularization and CMake Split Implementation Plan

> **Historical-artifact note (reconciled 2026-10-04)**: this is an early modularization
> plan. Its `data/redis` split direction was rejected by the P1-4 convergence (the whole
> redis family and `modules/data/redis` were removed, and the five existing sets were
> converged into one); the current directory state follows the actual `modules/` repo.
> The remaining split ideas still serve as historical reference.

## Goal

The project already has directions such as `starter`, `net`, `database`, `redis`, and
`game`, but the build layer is still dominated by the monolithic `apollo` static library,
and the source still has old and new paths coexisting, for example:

- `src/network` and `src/apollo/net`
- `src/storage/database` and `src/apollo/database`
- `src/apollo/storage/redis` and `src/apollo/redis`

This brings three direct problems:

1. Module boundaries are unclear, making on-demand assembly difficult.
2. New capabilities can only be piled further into the monolithic target.
3. Applications, framework, and business domains cannot be layered stably.

The goal of this plan is not a one-shot rewrite, but to introduce a modular skeleton
that can be migrated incrementally while keeping the existing buildable trunk.

## Layering Principles

### `base`

Positioning: pure foundational capabilities, carrying no Apollo framework semantics.

Contents:

- Time, strings, error codes, result types
- buffer, memory pool, lock-free queue
- Thread pool, basic concurrency primitives
- Low-level terminal adaptation for terminal/console

Constraints:

- Knows nothing of application, module, service, or starter concepts
- Depends on none of `core`, `runtime`, `game`

### `core`

Positioning: the Apollo framework kernel, responsible for process-internal common runtime semantics.

Contents:

- `ApplicationContext`
- Configuration management
- Logging management
- Lifecycle
- Event bus
- Module management
- Metrics and service-discovery abstract interfaces

Constraints:

- May depend on `base`
- Does not depend on `game`
- Does not directly depend on concrete MySQL/Redis/HTTP implementations

### `runtime`

Positioning: the host runtime, responsible for "how the program runs".

Contents:

- `ApplicationHost`
- `ServiceHost`
- Tick loop
- Shutdown hooks
- Signal / console event loop
- Process-level health checks

Constraints:

- Depends on `core`
- Does not directly host concrete business gameplay

### `net`

Positioning: the networking capability layer, responsible for sending/receiving, protocols, and connection management.

Suggested split:

- `net/core`
- `net/tcp`
- `net/http`
- `net/websocket`
- `net/rpc`

Constraints:

- `net/core` holds only abstractions and common mechanisms
- Each concrete transport protocol stands on its own
- Convergence of `src/network` and `src/apollo/net`

### `data`

Positioning: the data access layer, responsible for database, cache, and serialization integration.

Suggested split:

- `data/core`
- `data/orm`
- `data/redis`

Semantic boundaries:

- `data/core` defines `DataSource`, `Session`, `Transaction`, `Repository`
- `data/orm` provides the SQL/ORM/MySQL implementation
- `data/redis` provides the Redis client, cache primitives, leaderboards, and distributed-lock primitives

### `game`

Positioning: the game domain layer, split by business semantics rather than technical type.

Suggested split:

- `game/core`
- `game/world`
- `game/battle`
- `game/social`
- `game/economy`

Examples:

- AOI, Scene, Map, Movement go to `game/world`
- Attribute, Skill, Buff, the Damage pipeline go to `game/battle`
- Chat, Guild, Team go to `game/social`

### Application assembly

Applications explicitly compose CMake modules and existing runtime hosts. Shared application services live in apps/common. No separate runtime assembly container is required. See [module assembly](./architecture/starter-and-module-assembly-design.md).

### `apps`

Positioning: the final executable programs.

Suggested directories:

- `apps/gateway-app`
- `apps/login-app`
- `apps/game-server`
- `apps/world-app`

Responsibilities:

- Composing the required starters/modules
- Providing the process entry point
- Registering program-specific commands and operations interfaces

## Dependency Direction

The recommended fixed dependency direction:

```text
base
  ^
core
  ^
runtime

net/* ----\
data/* ----> game/*

starter/* -> core/runtime/net/data/game
apps/*    -> starter/* + a few direct modules
```

Hard constraints:

- `base` depends on no other Apollo layer
- `core` does not depend on `game`
- `data` does not depend on `game`
- `game` does not reach back into `core`

## Directory Plan

The target directory layout:

```text
apollo/
  modules/
    base/
    core/
    runtime/
    net/
      core/
      tcp/
      http/
      websocket/
      rpc/
    data/
      core/
      orm/
      redis/
    game/
      core/
      world/
      battle/
      social/
      economy/
    starter/
      core/
      net/
      mysql/
      redis/
      game-server/
  apps/
    gateway-server/
    login-server/
    game-server/
  tests/
```

Notes:

- `modules/` is the new modular build entry
- The current root-level monolithic `apollo` target is kept for now
- As each module is migrated out, the corresponding sources are removed from the root target

## CMake Target Naming Rules

Stable, predictable target names are recommended:

- `apollo_base`
- `apollo_core`
- `apollo_runtime`
- `apollo_net_core`
- `apollo_net_tcp`
- `apollo_net_http`
- `apollo_net_websocket`
- `apollo_net_rpc`
- `apollo_data_core`
- `apollo_data_orm`
- `apollo_data_redis`
- `apollo_game_core`
- `apollo_game_world`
- `apollo_game_battle`
- `apollo_starter_core`
- `apollo_starter_mysql`
- `apollo_starter_redis`

This has three benefits:

1. Target names map one-to-one onto directories.
2. Dependency relations are readable.
3. The final app's link list *is* the assembly list.

## Top-Level CMake Evolution Strategy

The top-level `CMakeLists.txt` should converge on:

1. Defining global options and compile standards
2. Providing the existing monolithic `apollo` target
3. Introducing the two new entries `modules/` and `apps/`

The current phase uses a "skeleton first" strategy:

- Keep the existing `apollo` target to avoid breaking current development all at once
- Add `APOLLO_ENABLE_MODULAR_LAYOUT`
- Add `APOLLO_BUILD_APPS`
- Create `modules/CMakeLists.txt`
- Create `apps/CMakeLists.txt`

This lets modules be migrated one by one rather than through a one-shot big-bang rewrite.

## Module Migration Order

Recommended order, lowest to highest risk:

1. `base`
2. `core`
3. `runtime`
4. `data/core`
5. `data/orm`
6. `data/redis`
7. `net/core`
8. `net/tcp/http/websocket/rpc`
9. `game/core`
10. `game/world`
11. `game/battle`
12. `starter/*`
13. `apps/*`

Reasoning:

- Stabilize the low-level common capabilities first
- Then extract the host and access layers
- Migrate the strongly business-semantic layers last

## Acceptance Criteria per Phase

### Phase 1

- The top-level CMake supports `modules/` and `apps/`
- Architecture boundaries are documented
- The existing monolithic target still works

### Phase 2

- The first batch of real module targets is extracted, e.g. `apollo_base`, `apollo_core`
- Unit tests can link module targets directly

### Phase 3

- `apps/game-server` runs via starter/module composition
- The root monolithic `apollo` target shrinks to a compatibility aggregation target or is retired

## Ownership of console / terminal / cli

To avoid mixing terminal adaptation, console events, and operations commands, split into
three layers:

- `modules/base/terminal`
  - Terminal output, colors, key input, ANSI/Windows compatibility
- `modules/runtime/console`
  - stdin listening, Ctrl+C, window close, the console event loop
- `modules/core/cli`
  - Command registration, argument parsing, command dispatch

Concrete business commands do not go in common modules — they go in the specific app:

- `apps/game-server/console`

## Recommended Next Steps for the Current Repo

The immediate follow-up work:

1. First create empty targets for `modules/base`, `modules/core`, `modules/runtime`.
2. Migrate from the old code, prioritizing:
   - General utilities into `base`
   - DI, lifecycle, configuration, logging into `core`
3. Create `apps/game-server` as a minimal host program only, carrying no business implementation.
4. Demote the existing `GameServer` from "framework main entry" to part of the runtime/game-server host.
