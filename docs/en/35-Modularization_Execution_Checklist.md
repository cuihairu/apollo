
# Apollo Modularization Execution Checklist

> **Historical note (reconciled 2026-10-19)**: this checklist is a historical record of the modularization work; application directories and executable targets were later unified (`apps/{gateway,login,base,world}`, no `-app`/`-server` suffixes), and `apps/game-server` was removed in that unification — unchecked items that reference it are kept as written and no longer executed.

This checklist tracks the construction order for migrating from the monolithic
`apollo` target to modular targets.

## Phase 1: Skeleton Completion

- [x] Top-level `CMakeLists.txt` wires up `modules/`
- [x] Top-level `CMakeLists.txt` wires up `apps/`
- [x] Add `modules/base`
- [x] Add `modules/core`
- [x] Add `modules/runtime`
- [x] Add `apps/game-server`
- [x] Clarify target naming rules
- [x] Clarify dependency direction

## Phase 2: Base Module Extraction

- [x] Inventory the general capabilities in `src/utils` and `src/apollo/utils` that can be pushed down into `base`
- [x] Extract the first batch of real source code into `apollo_base`
- [x] Verify `apollo_base -> apollo_core -> apollo_runtime -> apollo_game_server` via an independent smoke build
- [x] Add basic unit tests to `apollo_base`
- [ ] Define the boundary between `base/terminal` and `runtime/console`
- [ ] Gradually switch the existing callers of `Time` and `IdPool` over to `apollo/base/*`
- [x] Land the new public entry for `ThreadPool` in `apollo/base/thread_pool.hpp`

## Phase 3: Core Module Extraction

- [x] Merge and migrate DI capabilities into `apollo_core`
- [x] Merge and migrate configuration management into `apollo_core`
- [x] Merge and migrate logging management into `apollo_core`
- [ ] Unify the lifecycle and module-management model
- [x] Add a minimal runnable test for `apollo_core`

## Phase 4: Runtime Extraction

- [x] Redefine `GameServer` as a runtime host rather than the framework's main entry
- [x] Add `ApplicationHost`
- [ ] Add `ServiceHost`
- [ ] Add signal / shutdown hooks
- [x] Add the console event loop abstraction boundary

## Phase 5: Data Extraction

- [x] Extract `apollo_data_core`
- [x] Migrate ORM / SQL Template / datasource
- [ ] Migrate RedisTemplate / cache primitive / distributed lock primitive
- [ ] Remove the duplicated database and redis paths

## Phase 6: Net Extraction

- [ ] Converge `src/network` and `src/apollo/net`
- [ ] Extract `apollo_net_core`
- [ ] Separate tcp / http / websocket / rpc

## Phase 7: Game Extraction

- [ ] Extract `apollo_game_core`
- [ ] Migrate AOI / scene / movement into `game/world`
- [ ] Migrate attribute / buff / skill / ecs into `game/battle`
- [ ] Identify the independent boundaries of social / economy

## Phase 8: Starter and App Assembly

- [ ] Provide starters for core/net/mysql/redis
- [ ] Refactor `apps/game-server` into a starter-composition entry point
- [ ] Introduce `apps/gateway-server`
- [ ] Introduce `apps/login-server`

## Exit Criteria

- [ ] The top-level monolithic `apollo` target serves only as a compatibility aggregation layer, or is fully retired
- [ ] All new apps are built via module target + starter composition
- [ ] Module boundaries are stable, with tests covering the basic paths
