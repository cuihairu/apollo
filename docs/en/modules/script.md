---
title: Script Module
---

# Script Module

The Lua sandbox host (`apollo::script`); design in [scripting-lua.md](/en/design/scripting-lua).
Dynamic goes to Lua, hard real-time to C++; this module carries only the runtime foundation —
the gameplay logic surface comes in later batches.

## Dependencies

```cmake
find_package(unofficial-lua CONFIG REQUIRED)
target_link_libraries(my_target PUBLIC apollo::script)
```

- `unofficial::lua::lua` (the vcpkg lua port, locked to the 5.5 line — design §2 version
  discipline: lock the 5.5 mainline, patch level follows vcpkg, no self-held overlay port).
- Zero internal apollo dependencies; fault events are raised through an injected sink, with
  no direct coupling to the logging library.

## ScriptRuntime (sandbox host)

```cpp
apollo::script::ScriptConfig cfg;
cfg.instruction_budget = 200000;   // per-call 指令预算（0 = 不限）
cfg.memory_limit_bytes = 1 << 22;  // 内存上限（0 = 不限）
apollo::script::ScriptRuntime rt(cfg);

rt.set_fault_sink([&](const apollo::script::ScriptFaultEvent& e) { /* ERROR 旁路日志 + 计数 */ });
rt.register_module("apollo.test", kApiFuncs);  // §4.1 启动期注入具名模块表
rt.call_string("boot.lua", source);            // pcall 隔离，失败返回 false 不崩进程
rt.get_global_bool("ready");
```

### Delivered Surface

- **One state, one lua_State** (§2: one state per logical thread, no interlocking); a custom
  allocf enforces the memory ceiling (allocations beyond it are refused → Lua memory error).
- **Standard library whitelist**: base/string/table/math/utf8/coroutine; io/os/debug are not
  opened.
- **Dangerous base surface removed**: `dofile`/`loadfile`/`collectgarbage` set to nil;
  `load` replaced with a `mode "t"` text-only loader (binary chunks rejected).
- **Instruction budget**: a COUNT hook coarsely checks every 10k steps; over budget,
  `lua_error` aborts the current call (infinite loops covered naturally).
- **Fault isolation**: all `call_*` go through `lua_pcall`; errors are classified
  `kSyntax`/`kRuntime`/`kMemoryLimit`/`kInstructionBudget` into `ScriptFaultEvent` (chunk
  name + error string), and the state remains usable.
- **Named module injection**: `register_module(name, luaL_Reg*)` goes through a global named
  table (§4.1 whitelist; scripts may not look up the service container by name).
- **Module hot swap** (§3.2): `swap_module(ModuleSpec{name, version, deps}, source)` —
  dependency check → compile (`mode "t"`) → smoke test (if the new table has `smoke()` it
  must return true) → only when everything passes is `_G[name]` atomically swapped; any
  failing step leaves the old table untouched; the old active table goes to a freezer slot
  (only the most recent one kept), and `rollback_module` restores it and discards the bad
  version; a successful swap calls the new table's `onReload(old)` hook (a hook failure does
  not roll back, it records a fault) and broadcasts `ModuleReloaded` via
  `set_reload_listener`; `active_modules()` is the §7.2 version matrix. Zero migration of
  module-private state, per §3.2 (state lives on the C++ authoritative side). Load-path
  structure probing is fully raw reads (smoke/onReload/contract structure probes never go
  through metatables — a malicious `__index` error can no longer throw past the protected
  region and crash the process). Host-side global reads/writes and field probing are fully
  raw (`raw_getglobal`/`raw_setglobal`/`raw_getfield`, stack semantics identical to the
  native ones — `_ENV` and module tables can have malicious-error `__index`/`__newindex`
  metamethods attached at runtime by scripts; the call path and the global access surface
  likewise never crash the process). **Two-phase hot swap (§3.2 Phase A/B)**:
  `prepare_module(ModuleSpec, source)` runs the full swap protocol in an independent
  staging sandbox (a second ScriptRuntime built lazily) — compile → evaluate → smoke → load
  into the staging mirror, returning `PreparedModule{name, version, deps, bytecode,
  bytecode_hash, error}` (the artifact = `lua_dump` strip=0 own bytecode + an fnv1a64 hash,
  same convention as the §3.5 `lua_precompile`; separate lua_States cannot xmove, so the
  handoff is in-process bytecode that never passes the sandbox `load`);
  `commit_module(prepared)` at the owning thread's tick boundary does hash integrity
  verification (empty artifact/tampering rejected with fault + audit entry) → loads with
  `"b"` (Lua self-verifies the artifact header signature) → evaluates → verifies the table →
  shares the swap tail with `swap_module` (atomic swap + freezer + breaker ledger reset +
  onReload); **smoke is not re-run** (already proven on the staging side — re-running
  self-checks would carry the cost Phase A saved back into Phase B and introduce a
  second-execution difference surface); staging mirrors accumulate in topological order
  (dependency modules must have been prepared first; a module swapped across staging
  surfaces but missing from the mirror reports `missing dependency on preparer`; the
  owning-side commit likewise checks dependencies first); the control plane serializes the
  calls (prepare/commit are not concurrent); host-thread selection belongs to the control
  plane (`apollo_script` has zero apollo dependencies and pulls in no thread pool).
  **Current-version pointer persistence** (§3.2, guards against the bad-version crash
  loop): `ScriptConfig.version_store_path` (the patch directory version manifest; empty =
  off) — a successful swap and a successful rollback each do a whole-manifest
  read-modify-write + atomic rename of `<name>\t<version:016x>` (when the Windows CRT rename
  refuses to replace an existing target, it falls back to delete-then-rename — the tmp file
  is fully written before the delete, so a half file never appears at the target location);
  `current_version_manifest()` lets the control plane read the "latest known healthy
  version". Write timing has exactly two sites (swap/commit success, rollback success) —
  prepare does not write (the version is not yet active), and the staging sandbox gets its
  own config with the path cleared to avoid accidental writes. The manifest shares the
  format family of the §3.5 cache manifest but with the opposite access pattern (written at
  runtime, read by the control plane), in a separate file `module_version_store`.
  **Patch directory read + hash verification control plane has landed** (Phase A engineering
  step 1, "read the patch directory → verify hashes"): `load_patch_directory(dir)`
  (`patch_loader`, manifest-driven entry by entry) — the source hash must pass (mismatch =
  tampering/corruption, rejected into errors); a bytecode hash / format_sig mismatch = drop
  the bytecode, keep the source (the artifact falls back and remains usable); a missing
  manifest = a whole-load error. It only does "read + verify + carry artifacts out";
  compile/smoke/swap belong to `prepare_module`, and dependency topological ordering belongs
  to control-plane orchestration. **The Phase C canary verdict primitive for the
  gray-release observation window**: `ModuleCanary` (`module_canary`) — after commit,
  `begin()` takes the runtime tick as the window start, the control plane calls `tick()`
  each beat, and in-window faults are attributed through the per-module ledger booked by
  `call_module` (`module_faults_since`, independent of the breaker sliding window — the
  window N is a control-plane parameter); over threshold it flags bad early (without waiting
  for the window to fill); promote/rollback is driven by the control plane per verdict
  through the existing `commit_module`/`rollback_module`.
- **Per-scene-thread gray-release delivery orchestration** (§3.2 Phase A engineering,
  "multi-runtime delivery wiring"): `ModuleRollout` (`module_rollout`) — one staging pass
  serves all targets (the orchestrator holds its own staging sandbox, prepares the whole set
  in Kahn topological order, dependency cycles report `cyclic dependency in module set`; the
  same `PreparedModule` is handed to N `commit_module` calls); verdict progression is a pure
  state machine, and swap/rollback go only through the existing surfaces. Per-target stepped
  queues `pending_canary`/`pending_promote`/`pending_rollback` (the Phase B commit must run
  at the target owning thread's tick boundary — the control plane enqueues into the
  corresponding scene thread's task queue per queue; a single-gameThread host drives it
  sequentially); the phase machine runs kCanaryPending → kObserving → kPromoting /
  kRollingBack → kPromoted / kRolledBack, with illegal phases or role mismatches silently
  idling. Observation window T0 = all gray-surface commits complete (each target's window
  start = its own runtime tick), one `ModuleCanary` per (gray target, module); any window
  flagged bad fails early into rollback (without waiting for the other windows), and all
  windows full and healthy moves to promotion; a healthy verdict is not rewritten by a
  promote failure (promote failure = a per-target error, the target stays on its pre-patch
  version), and a gray-surface load failure = flagged bad into rollback; rollback runs in
  reverse topological order and rewrites the current pointer. The same instance runs
  multiple rounds serially (only terminal states allow another prepare; the staging sandbox
  is kept across rounds, and the handoff surface is cleared per round).
- **Attribute hook window** (§5.1/§5.2): `set_attr_hook("onAttrChanged")` registers the
  global hook function (signature `(entity, attr, old, new)`, returns an integer revision
  value; nil/non-integer = no modification); `run_attr_hook_window(changes, whitelist,
  frame_budget)` is called entry by entry after recalculation and before broadcast —
  out-of-whitelist entries are skipped and counted, once the frame budget is exhausted the
  remainder is skipped (`reset_attr_frame()` restores it next frame), hook errors keep the
  original value, record a fault, and the state stays usable; the payload is a C++-side
  snapshot copy (`ScriptRuntime::AttrChange`, int64 pairs, lock-free — same owning thread);
  with no hooks the window makes zero calls.
- **Module circuit breaker** (§6.1): `call_module(module, func)` invokes module handlers
  (faults attributed to the module); M FAULTs within a sliding window of W ticks →
  quarantine for Q ticks (W/M/Q/max configured at startup, defaults 1000/3/600/4800); calls
  during quarantine short-circuit to false without entering Lua (counted in
  `stats().circuit_rejects`); when quarantine expires it half-opens for one probe — a
  successful probe clears the ledger and lifts it, another FAULT doubles the quarantine span
  (capped at max); **hot reload resets automatically** (a successful swap/rollback means a
  new version with a fresh ledger); the host feeds `set_tick(t)` every tick; the status
  surface subset is `is_quarantined`/`quarantine_remaining`/`quarantined_modules` (list +
  remaining ticks).
- **Breaker state persistence** (§6.1, guards the bad-version crash loop):
  `export_breaker_state()`/`import_breaker_state(blob)` — quarantined entries (remaining
  span converted from absolute ticks) + the WARN debounce ledger survive restarts
  (preventing a restart alert storm); little-endian TLV (magic + version); malformed input
  (magic/version/truncation/trailing bytes) is rejected with the state untouched; the host
  manages its own persistence (journal/state file) and restart injection.
- **Contract table loading** (§3.4): `load_contract(source)` — contract.lua (the
  server-side dual form emitted in the same apollo_gen batch) is compiled →
  structure-verified (a routes table is required) → route completeness gate (if the bin has
  a handler but Lua does not, the swap is rejected; a handler is a global name or a
  one-level `module.func` dotted path) → `_G["contract"]` and the C++ data surface are
  swapped atomically (any failing step leaves the old table untouched); the data surface:
  `contract_routes()` (msg_id → handler), `contract_write_whitelist()` (§3.4 write whitelist
  as data, fed straight into `run_attr_hook_window`), `contract_version()`;
  `call_route(msg_id)` dispatches by contract route (unregistered id / unreachable handler /
  execution error returns false). **Generator artifact load surface**: the three pieces
  apollo_gen emits into contract.lua — the flat `routes` table, the integer
  `write_whitelist` id array, and `version` — match the loader's consumption shape (the
  scripting-lua §3.4 load-surface form), and a `script_tests` integration case reads the
  in-repo golden artifact through the full load path to pin it — a writer change or a
  contract forgetting the shape turns red.
- **Bytecode cache** (§3.5): at build time the `lua_precompile` tool produces `<name>.luac`
  + a manifest triple (source hash / artifact hash / format signature — Lua version + the
  type size set, necessarily different across minor versions); at runtime an env-gated
  read-only load (`APOLLO_LUA_BYTECODE_CACHE`, empty/unset = off). The three load points
  `run_chunk`/`swap_module`/`load_contract` share `load_source` — when the whole triple
  verifies, the `luaL_loadbufferx` "b" branch is taken (counted in
  `stats().cache_hits`/`cache_misses`, no counting when the cache is off); any failure
  (source changed / signature changed / artifact missing or corrupt / manifest miss) falls
  back to source compilation — loading never fails because of the cache. The invalidation
  rules (Lua version changed / source changed / loader logic changed) all rerun the pipeline
  at build time. The red line stands: the sandbox `load` still rejects bytecode for runtime
  strings (`mode "t"`); the cache path serves only our own build artifacts and never passes
  through `load`.
- **§6.1 event surface**: three-level events through the fault sink — WARN (a single call
  consumes ≥ budget × `warn_threshold_percent` (default 80; 0 = off) without exceeding it,
  debounced 256 ticks per chunk, not counted as a fault), FAULT (the existing classified
  counting), TRIP/RECOVER (breaker trip/release: all three release paths — half-open
  settlement, hot reload, rollback — emit); the `ScriptFaultEvent` structured field set is
  closed: `kind/chunk/message + ts_wall_ms/tick/consumed/budget` (filled in uniformly by the
  engine side; downstream routes by field instead of parsing text).
- **attach eval** (§7.1): `eval_debug(source, result)` — delivered through the admin single
  entry, called by the host at the owning thread's tick boundary; the same sandbox surface
  (whitelist/instruction budget/memory ceiling all in force, infinite loops interruptible);
  expressions first with statement fallback (REPL convention); result string truncation
  (`debug_eval_result_max`, default 4KB) + per-tick rate limiting
  (`debug_eval_max_per_tick`, default 8, 0 = unlimited); **on failure result = the verbatim
  error** (rate limit/syntax/runtime error — the same out-parameter as success, so fault
  isolation does not blow up the session). The host-side execution queue shipped with the
  Scene batch (`Scene::enqueue_debug_eval` → a fixed debug point on the first tick beat →
  reply keyed by id; the cross-thread delivery holds its own mutex).
- **Observability surface**: `stats()` (calls, per-class fault counts, instruction
  coarse-check totals, reload success/failure, attr_hook call and skip counts, WARN debounce
  counts, breaker rejection counts, cache hit and fallback counts), `memory_used()`,
  `reset_budget()`.
- **Read-only state introspection** (§7.2): `gc_state()` — the host allocf counter
  `used_bytes` (the enforcing ground truth for the ceiling) + `limit_bytes` + Lua's own
  view `lua_gc_kib` + the incremental parameters `gc_pause/stepmul/stepsize` (a read-only
  `LUA_GCPARAM` query with value = -1; the host is always in incremental mode, with no
  public mode-reading API); `reload_history()`, the hot-swap audit ring (cap 64, oldest
  dropped) — swap success/rejection and rollback all book entries (a rejection is also an
  event: missing dependency/compile/evaluate/non-table/smoke rejection, reason truncated to
  256), recording `LuaReloadRecord{name, from_version(0=first load), to_version, tick,
  success, reason}`, so in-process you can answer "which version is each module running,
  and how did it get there"; `env_snapshot(module, max_entries)`, a depth-1 top-level
  snapshot of the module env — raw traversal via `lua_next` (never `__pairs`/`__index`),
  typed value extraction (no in-place `lua_tolstring` conversion), calling no Lua functions,
  entry-count truncation (the `truncated` flag), per-value truncation past 512 bytes; a
  module name not in the global surface = not found. Called at the owning thread's tick
  boundary (same §7.1 eval window discipline); the aggregated presentation surface belongs
  to a later observability batch.
- **Performance attribution buckets** (§7.3, Lua side): the second duty of the same COUNT
  hook — per-chunk instruction counts (bucketed on cost the hook already paid, 10k-step
  quantization matching the stats convention) + wall-clock totals (steady-clock timing at
  host call boundaries, microseconds); booking points = six: `call_string`/`call_module`
  (by module name)/`call_route` (by handler name)/`call_global`/the attr hook window (by
  registered name)/`eval_debug` — a failed lookup neither executes nor books; the query
  surface is `profile_snapshot()` (returns a copy, chunk names in lexicographic order) +
  `profile_reset()`; the over-budget event status surface: on instruction-budget abort the
  `over_budget_history()` ring (cap 64, oldest dropped) records `{chunk, tick, consumed,
  budget}` (the `ScriptFaultEvent` event surface still emits on the same beat). The C++
  side (frame markers / unstripped symbols) and MetricRegistry aggregation/presentation
  belong to the observability/owning wiring batch.
- **Async task model** (§8, L1 handoff primitives + the L2 coroutine surface):
  `register_async_service(name, handler, timeout_ms)` is the L0 execution-layer extension
  point (the host plugs in real Redis/DB/cross-server backends per batch; acceptance
  returns immediately); `call_coroutine(module, func)` runs handlers as business coroutines
  (cross-yield business carried by coroutines; the breaker-quarantine short-circuit is the
  same as `call_module`); on the Lua side `apollo.async.call(svc, ...)` takes a typed
  snapshot of the arguments (bool/int/double/string/nil; tables/functions are not captured
  and no metamethods are touched) then yields and suspends (unknown service / main-thread
  call / unregistered coroutine = an immediate error fail-fast); the completion surface
  `complete_async`/`fail_async` enqueues from any thread + `drain_async()` delivers
  uniformly at the tick boundary (the handoff is a message, not a future — never inlined
  even on the same thread, completion only enqueues; late/duplicate/ownerless completions
  are dropped + counted in `stats().async_late`); the timeout ceiling is configurable per
  service (0 = the `async_default_timeout_ms` global default); when the timeout lands first,
  it resumes with a timeout error (late results dropped + counted); `cancel_pending(reason)`
  is cooperative cancellation (the next drain terminates with a cancellation error —
  shutdown/player-offline finalization); `set_async_validator` is the re-validate hook
  (validates the suspended coroutine's context before completion delivery; on invalidation
  it terminates with an error instead of injecting a zombie result); **injected errors
  (timeout/cancel/invalidation) still emit faults but do not count toward module breakers**
  (the operational end state belongs to the host — a shutdown cancellation does not push
  business modules into quarantine; genuine execution errors still attribute to the breaker
  as usual); all three disciplines have landed: resume only at the fixed drain-boundary
  point / re-validate after resume / timeout ceiling + cooperative cancellation. The Lua
  longjmp discipline: host-side RAII locals are released before the yield/error throw point
  (`lua_yieldk`/`lua_error` longjmps do not run C++ destructors — the argument vector's
  scope closes before the suspension point, and the error string is copied into Lua before
  the swap releases it).
- **Coroutine inventory** (§7.2, the presentation surface of the §8.2 coroutine registry):
  `coroutine_snapshot()` returns a copy of all coroutine rows `LuaCoroutineInfo{handle,
  status, suspend_reason, origin, born_tick, age_ticks, pending_request, timeout_ms,
  leaked}` — status `suspended` (suspended: `await <svc>` or a bare `yield`)/`ready`
  (registered, not yet first-run; running/normal are resume transients, invisible at the
  snapshot point); the origin attribution key = `module:func` (started via
  `call_coroutine`) or `coroutine.create`/`coroutine.wrap` (script-created —
  `create/wrap/resume` are synchronously substituted into the registry, so the inventory
  has no blind spots and the instruction hook and budget stay in force across yields); lazy
  terminal-state sweeping (finished/errored coroutines are dropped from the snapshot point;
  the registry anchors them against GC); over-age detection `async_leak_threshold_ticks`
  (0 = off) flags `leaked=true` on a hit (the §8.3 discipline-3 leak-detection object);
  called at the owning thread's tick boundary (same §7.1 discipline).

### Boundaries (left to later batches)

- Hot-swap engineering surface: host-side wiring of coroutine upgrade semantics (the
  per-scene-thread admin command transport follows the admin/control batches)
- Version partial-order enforcement (contract version ≥ script patch version)
- Runtime consumption: upstream business messages dispatched by the C++ frame router to Lua
  handlers via `contract_route` (`call_route` wiring) — with the gameplay Lua batch (host
  startup loading has landed: the cell `--script-handlers`/`--script-contract` startup
  order runs handler surface → contract → boot source; the handler placeholder module ships
  at `apps/world-app/scripts/contract_handlers.lua`, with the golden contract sync gate
  `contract_handler_tests`)
- Formulas as hooks (§5.3)
- ScriptFaultEvent host context fields (scene/entity_id/module+version — filled with the
  scene wiring batch)
- attach transport (admin single entry / control-channel forwarding to the target process —
  an admin/control batch), the GM command surface (§7.5)
- §8 L0 execution-layer real backends (Redis/DB/cross-server — the
  `register_async_service` extension point is in place; plugged in per their own batches);
  the `apollo.async.all` concurrency combinator (API shape decided with the consumer batch)
- The `require`/package surface is not opened (module loading currently goes through the
  host `call_string` surface)

## Tests

`tests/test_script_runtime.cpp` (58 cases): whitelist availability, sandbox defaults,
binary chunk rejection, instruction-budget abort and state recovery, per-call budget
refill, memory ceiling, pcall error isolation, fault sink, named module injection; hot-swap
atomic table replacement, failure keeps the old table, dependency rejection, smoke gate,
onReload hook, freezer rollback, reload broadcast; attribute hook revision values,
out-of-whitelist skipping, errors keep the original value, frame-budget skipping and
next-frame recovery, nil means no modification, empty window with no hooks; breaker triple
trip, sliding-window expiry, half-open recovery, half-open doubling, hot-reload ledger
reset, configurable M, call_module dispatch, missing handler books a fault; contract
loading data surface, missing handler rejects the swap, bad structure rejected, failure
keeps the old table, contract route dispatch, contract whitelist feeding the hook window,
2 generator-artifact load-surface cases (reading the in-repo golden contract.lua through
the full load path: shape probes + handler stub registration + data-surface values asserted
equal to the probes; direct loading without the handler surface is rejected through the
consistency gate); WARN 80% trigger, 256-tick debounce, silence under threshold, silence
after raising the threshold, trigger after lowering it, 0 = off, TRIP/RECOVER events,
hot-reload ledger reset emits RECOVER, event structured fields; eval expression evaluation
and statement execution, error isolation, budget jurisdiction, result truncation, same-tick
rate limiting and next-tick recovery; breaker state export/import round trip, quarantine
surviving restarts, tick conversion, no persistence without quarantine, debounce ledger
across restarts, bad blob rejection. Three-tree gate (GAME_MODULE ON/OFF) + sanitizer all
green.

`tests/test_bytecode_cache.cpp` (10 cases) + `lua_precompile` smoke: manifest line parsing
(valid/bad hex/missing fields/comment lines), cache hit counting and execution semantics,
fallback on the three invalidation rules (source hash changed / format signature changed /
artifact corrupt), fallback when "b" loading fails after passing the hash gate, fallback on
a missing manifest and a manifest miss, zero counts with the env unset, contract loading
through the cache.

`tests/test_script_async.cpp` (20 cases, §8 three disciplines + the §7.2 coroutine
inventory): round-trip semantics (acceptance yield → complete enqueues → drain-boundary
delivery — dual assertions that done lands before/after drain; the handoff is a message,
not a future), five typed argument snapshot types, unknown service fail-fast, main-thread
call rejection, timeout-error resume + late-completion drop counting, duplicate completion
counted late, fail_async error string, cooperative cancellation (two coroutines terminated
at the next drain + the verbatim error visible to pcall + the snapshot still 2 rows while a
drain-effective cancellation is pending), injected errors not counting toward the breaker
(fault still emitted + no quarantine), re-validate invalidation terminating without
injecting a zombie result (the statement after it does not run), inventory fields
(born/age/timeout/pending/origin/reason), over-age leaked, script-created coroutines
registered via create/resume with lazy terminal sweeping, wrap registration and error
propagation (the pcall capture surface), instruction budget in force across yields
(infinite loops inside coroutines interruptible), call_coroutine short-circuit during
breaker quarantine, genuine execution errors attributed to the breaker (running to error
three times after completion delivery trips quarantine), cross-thread complete_async,
sequential double await re-suspension bookkeeping (reason/pending migration). ASan/UBSan/LSan
all green (zero tolerance for Lua longjmp leaks — resources released before the yield/error
throw point).

`tests/test_script_phase_a.cpp` (13 cases, §3.2 Phase A/B): the prepare→commit round trip
(artifact bytes and hash, staging never touches the active area, audit entry from=0), the
version matrix `active_modules()`; the four staging rejection reasons verbatim
(compile/evaluate/smoke/missing dependency), with staging failures not entering the owning
audit ring; commit handoff integrity (commit without prepare is rejected + fault chunk
attribution + a rejection audit entry; a single-byte-tampered artifact with a hash mismatch
is rejected); staging-mirror topological order (missing dependency rejected, modules
swapped directly do not count into the staging mirror, dependency-first double commit sees
the dependency value during evaluation, out-of-order commit is rejected while the dependency
is absent and passes on retry); **commit does not re-run smoke** (the module body runs once;
the smoke count stays at its staging value); freezer rollback (commit two versions →
rollback flips the version with three audit entries). Four-tree gate (build / sanitizer /
OFF / examples) all green.

`tests/test_script_version_store.cpp` (15 cases, §3.2 current-version pointer persistence):
manifest line parsing and whole-manifest atomic write round trip, missing file returns an
empty manifest, bad lines skipped (comments/empty lines/missing fields/bad hex/trailing
fields), no temp-file residue and a second overwrite; the runtime integration surface —
swap/commit/rollback all write back to disk, an overwrite by the same name preserves the
other modules' entries, an empty path is off and writes nothing, prepare does not write (the
version is not yet active), a rejection does not write; **the core crash-loop-guard
scenario** — version 1 healthy → version 2 flagged bad → rollback → process crash (scope
exit) → a new runtime after restart reads the manifest at the same path and current is still
1 (the flagged-bad version 2 is no longer loaded by default); cross-runtime persistence (A
writes and destructs → B reads); a pre-seeded manifest reads directly. Four-tree gate
(build / sanitizer / OFF / examples) + ASan/UBSan/LSan all green.

`tests/test_script_patch_loader.cpp` (12 cases, §3.2 Phase A engineering: patch directory
read + hash verification): manifest-driven loading (source and source hash carried out in
manifest order), a missing manifest is a whole error, source hash mismatch rejected (tamper
interception), missing source file rejected, no luac = pure source, bytecode hash mismatch
drops the bytecode and keeps the source, format_sig mismatch drops the bytecode and keeps
the source (across Lua minor versions), valid bytecode carried out, manifest order
preserved, empty directory is a whole error; control-plane end to end — patch directory →
prepare → commit lands the current manifest, a tampered patch never reaches prepare.
Four-tree gate (build / sanitizer / OFF / examples) + ASan/UBSan/LSan all green.

`tests/test_script_canary.cpp` (6 cases, §3.2 Phase C gray-release observation canary): a
healthy module judged healthy when the window fills (no verdict before it fills),
over-threshold flags bad early (without waiting for the window), faults before begin do not
enter the window, other modules' faults are not attributed, **the full gray-release loop**
— v1 healthy → swap to v2 (onTick flags bad) → canary judges unhealthy → the control plane
drives rollback → the active version returns to 1 and the handler recovers; no full verdict
without begin. Four-tree gate (build / sanitizer / OFF / examples) + ASan/UBSan/LSan all
green.

`tests/test_script_rollout.cpp` (12 cases, §3.2 Phase A engineering: per-scene-thread
gray-release delivery orchestration): a healthy gray release moves to promotion when
windows fill (multi-target, all in place), flagged bad fails early into rollback (without
waiting for windows), dependency topological-order delivery (dependencies first), zero
delivery on staging failure, illegal parameters rejected (in-flight/empty set/empty
targets/empty gray set/unknown gray key/duplicate module names), empty runtime rejected, any
module in the set flagged bad rolls back the whole set, gray-surface load failure flags bad,
promote failure recorded per target (a healthy verdict is not rewritten), rollback rewrites
the persistent current pointer (an old version read back after restart), patch directory
end to end (load_patch_directory → prepare → commit → observe → promote), out-of-phase steps
idle. Four-tree gate (build / sanitizer / OFF / examples) + ASan/UBSan/LSan all green.

`tests/test_contract_handlers.cpp` (2 cases, §3.4 host startup loading · the handler-surface
sync gate): the full load path over the in-repo handler placeholder module
(`apps/world-app/scripts/contract_handlers.lua`) + the golden contract
(`sdks/cpp/generated/contract.lua`) — if apollo_gen adds a reflect message and the handler
surface does not follow, it turns red; the contract data surface (version/routes) is
readable after loading. The cell startup load-order cases live in `world_app_script_host_tests`
(4→7: landing visibility of the startup order handler surface → contract → boot, the
missing-handler gate rejecting without shutting down, and continuing service when the
contract file is absent). Four-tree gate (build / sanitizer / OFF / examples) all green.

`tests/test_script_state.cpp` (21 cases): the GC surface's host counter sharing one source
with `memory_used` + read-only queries of Lua's own view and the incremental parameters;
reload audit success booking (first time from=0) with tick tracing, the five rejection
reasons (missing dependency/compile/evaluate/non-table/smoke) with over-long reasons
truncated, rollback from/to flipping, the audit ring cap of 64 dropping the oldest; env
snapshot raw-type rendering (string/integer/float/boolean/integer keys/table/function),
entry-cap truncation, module absent/zero cap/empty name/non-table global, malicious
metamethod tables untouched (`__pairs`/`__index`/`__tostring` error tables snapshot as
usual), per-value 512-byte truncation; a contract table with a hostile `__index` loads
normally (raw probing on the load path); a hostile `_ENV` metatable survives the whole host
path (call_module/get_global/set_global/swap commit/snapshot surface), hostile module
metatables on the call path and the contract completeness gate, host-path stack-balance
regression (`raw_setglobal` pop semantics); profile bucketing at the six booking points
(call_string instructions and wall clock, call_module lookup failure books nothing,
call_route by handler name, attr hook by registered name + eval), over-budget event ring
booking with the 64 cap dropping the oldest, snapshot copy semantics and reset.
