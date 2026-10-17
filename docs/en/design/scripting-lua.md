# Lua Scripting and Updates

modules/script provides ScriptRuntime with native Lua C API bindings. The vcpkg manifest pins Lua 5.5.1; runtime and precompilation tools must remain compatible.

## Executive summary

Module preparation, commit, rollback, fault attribution, sandboxing, contract-table loading, and async handoff primitives exist. Reliable cross-World rollout, real business backends, and multi-instance private-state isolation still need application integration.

## 1. Responsibility

Lua hosts dynamic gameplay. C++ owns networking, Scene, AOI, attributes, and persistence. Inject named services at startup instead of allowing arbitrary container lookup. Simulation writes run on the owning logic thread.

## 2. Hosting

Use each ScriptRuntime sequentially through its owner, including serialized prepare/commit operations. Do not execute one Lua state concurrently on multiple World threads.

WorldApp currently assembles one script state for its default Scene. Private script/task isolation across Instances remains a complete-activity requirement; shared globals do not establish isolation.

## 3. Module updates

### 3.1 Identity

Modules carry names, versions, and dependencies. Validate bindings, dependencies, and resource constraints before preparation/commit; reject missing or incompatible content.

### 3.2 Prepare, commit, and rollback

`prepare_module` produces a prepared result; `commit_module` switches at the owning tick boundary; `rollback_module` restores an available previous version. Observe canary faults using primitives such as `module_faults_since`. These APIs do not establish distributed delivery, acknowledgments, retries, or rollback orchestration.

Code-only changes preserve wire contracts; C++ uses process replacement. Active-instance continuity, state compatibility, and drain deadlines require separate acceptance.

### 3.3 State and versions

Define old-coroutine, private-state, and rollback write policies. Persistent version primitives need host wiring and recovery validation, not only an in-memory current version.

### 3.4 Contracts

`load_contract` consumes generated contract.lua routing/permission metadata. Loading a table does not upgrade all clients. Follow [SDK compatibility rules](./sdk-contract.md) for contract changes.

### 3.5 Bytecode

Precompilation/cache primitives exist. Bytecode and manifests must match Lua version, platform, and source identity. Reject mismatches rather than treating arbitrary build outputs as valid patches.

## 4. Injected services

Expose limited named C++ interfaces, validating arguments, lifetime, session, and authority. Do not expose arbitrary addresses, unrestricted persistent writes, or cross-thread Scene pointers.

## 5. Attribute hooks

Run hooks in defined simulation phases with write allowlists and budgets. Prevent unbounded recursive recalculation and bypasses of visibility, ChangeSink, or settlement deduplication.

WorldApp has prototype Scene/script wiring. Validate gameplay mutations, client delivery, and persistent saving through real business integration.

## 6. Sandbox and faults

Runtime instruction/memory budgets and fault/circuit-breaker primitives need tests for loops, allocation limits, errors, budget recovery, and rollback. Fault attribution and logs do not establish successful recovery.

### 6.1 Fault events

Use fixed runtime observation interfaces. Script faults must not trigger synchronous external HTTP. Application load tests establish latency-budget compliance.

## 7. Debugging and observation

Controlled eval enters at tick boundaries, with separate read-only and mutating permissions. Observe coroutines, versions, memory, and execution cost through their defined surfaces rather than granting arbitrary execution as operations access.

Pausing a logic thread affects every hosted Instance. Breakpoint suspension is not an online availability guarantee. Use external sampling separately from runtime statistics.

## 8. Asynchronous tasks

### 8.1 Backend boundary

`register_async_service` is a host extension point, not implemented Redis, SQL, or complete cross-server business integration. Handlers return promptly and place blocking work on suitable executors/services.

### 8.2 Handoff

`complete_async` / `fail_async` enqueue completions; `drain_async` handles delivery, timeouts, and coroutine resume at the owning boundary. Same-thread completion still follows the handoff point.

### 8.3 Validation and cancellation

Use `set_async_validator` to revalidate lifetime, session, and authority, and `cancel_pending` for cooperative cancellation. Late completions after destruction/transfer cannot revive a writable old object.

## 9. Acceptance

Validate local replacement, canary failure/rollback, completion/cancellation, private-state isolation, and cross-World rollout separately. Code replacement, protocol upgrade, TCP continuity, and active-match continuity are distinct outcomes. Current tasks live only in the root todo.md.
