
# SSEngine Legacy Engine Reference (ssengine-reference)

> Analysis subject: `/home/cui/workspaces/SSEngine` — a C++17 system layer reverse-engineered from an old engine's binaries (thread sync/timers/memory pools/object pools/file IO/shared memory/thread pool/DB facade/logging/network wrappers). That repo carries its own reverse-analysis artifacts (FINAL_ANALYSIS_REPORT.md, IMPLEMENTATION_ROADMAP.md, QUICK_REFERENCE.md, etc.) plus all source under `include/ssengine/` + `src/`.
> Stance: module-by-module "is this valuable to apollo", with exactly three verdicts — **worth adopting (with a landing form) / already have and better / recommend against**.

---

## Executive Summary

1. **Most worth adopting: the async DB Command model** (`sddb.h:253-390`): `OnExecuteSql` (executes on the DB thread) → `OnExecuted` (read back in the main thread's `Session::Run()`) → `Release` — a two-phase async pattern, plus `GetGroupId()` ordering semantics. This is the standard MMO answer of "DB thread + main-thread completion callback"; apollo's `modules/data` currently has only a synchronous mock, and this model is the first-priority reference.
2. **Second: the typed object pool** (`sdobjectpool.h`): placement-new + free-list + **the lock policy as a template parameter** (`MT = CSDNonMutex`); apollo's raw-memory-pool header (`utils/memory_pool.h`) was already deleted with the orphan-header cleanup (2026-10-15, zero consumers), and per-type pooling is missing. The C++20 landing suggestion is a modernized rewrite with pmr/free-list, not porting the macros.
3. **Third: a real timer wheel.** The sdtimer.h header comment claims "Linux-style hierarchical timers", but `sdtimer.cpp:21-79` is actually **mutex + vector full-table scan** — docs not matching implementation is this repo's lesson number one; apollo currently has **no timer module**, and what the game loop needs is a real hierarchical timing wheel (O(1) insert/delete) — absorb it per the "target design", not per the "current implementation".
4. **Backpressure visibility is worth learning**: sdnet's `GetSendBufFree()` (`sdnet.h:166-169`) and `DelaySend` (cross-thread delivery) — explicitly exposing the send-queue watermark to upper layers; apollo's network abstraction layer (docs/design/net-abstraction.md) takes this class of capability into a unified interface, instead of exposing thread and buffer internals directly like the old engine did.
5. **Attribute/state sync: SSEngine does not have it.** It is a pure system-layer library (threads/memory/files/network pipes) with no entity-attribute replication mechanism at all — apollo's attribute sync must target the BigWorld/KBEngine-class models (see `docs/design/attribute-sync.md`); the only things borrowable from the old engine are transport-layer parts (sdpkg fixed-length frame header + checksum, loop_buffer ring buffer, data_queue thread-safe queue).
6. **Lessons list** (exposed by the reverse-engineering process): two analyses in the same repo contradict each other ("100% complete / production ready" vs "20 core modules unimplemented, 59.7% coverage"); header-file comments lie (the hierarchical timer); `volatile BOOL` as a thread flag (`sdthreadpool.h:140-141`); "remember to lock" comment-style thread safety (`sdmemorypool.h:29-41`); raw pointers + `Release()` manual lifetime management (`sddb.h:138`). **Adopt the ideas and the designs, not the code.**

---

## 1. Background and Reading Scope

- Origin: a binary reverse-engineering reconstruction of an old MMO engine's system layer, C++17, dual Windows/Linux stack (`win32/`, `win64/` hold prebuilt artifacts).
- Read: FINAL_ANALYSIS_REPORT.md, IMPLEMENTATION_ROADMAP.md, QUICK_REFERENCE.md, all 70+ headers under `include/ssengine/` (with focus on sdtimer/sdmemorypool/sdobjectpool/sdthreadpool/sdtime/sddb/sdidpool/sdshmem/sdpkg/sddatastream/sdnet) and key .cpp files (sdtimer.cpp etc.).
- Credibility judgment (important): the three analysis artifacts inside the repo contradict each other —
  - FINAL_ANALYSIS_REPORT.md: claims every module is "100% complete, production ready";
  - IMPLEMENTATION_ROADMAP.md: claims "20 core modules completely lack implementation, 42 modules have incomplete platform support";
  - QUICK_REFERENCE.md: implementation coverage 59.7%.
  After cross-checking the sources, the verdict: **QUICK_REFERENCE is closest to the truth** — headers are complete (72), implementations about 60%, and "has an implementation" does not mean "the implementation matches what the comments claim" (sdtimer as proof). Reverse-engineered artifacts serve only as leads; the source is always authoritative.

## 2. Legacy Engine Capability Inventory

| Module | Headers | Design highlights (code-reading conclusions) | Implementation completeness |
|---|---|---|---|
| Timer | `sdtimer.h/.cpp` | claims hierarchical; actually mutex+vector full scan each frame, supports loop counts/permanent repeat, callbacks executed outside the lock (that part is right) | header≠body, functional but O(n) |
| Time | `sdtime.h/.cpp` | dual track: `CSDDateTime` (tm wrapper, 1970-2037) + ModernDateTime/Timer (chrono lineage); GetTickCount 49.7-day wraparound comment | complete; the dual track is a migration-period form |
| Variable-size memory pool | `sdmemorypool.h/.cpp` | 16 size-class free lists + page allocation (0x80000 pages), returned in bulk only when the pool is destroyed; **comments explicitly "not thread-safe, lock it yourself"** | complete |
| Fixed-size memory pool | same as above | fixed-size units per page + per-page free head | complete |
| Object pool | `sdobjectpool.h` + `detail/sdobject_allocator*.h` | placement-new construct/explicit destruct; constructor args hand-overloaded up to 5 (Boost PP up to 20); **the lock policy is a template parameter** | complete |
| Thread pool | `sdthreadpool.h/.cpp` | min/max threads, max pending tasks, two termination semantics (TerminateQuick/TerminateWaitJobs); `volatile BOOL` flags (a UB pattern) | complete |
| Sync primitives | `sdmutex/sdcondition/sdlock/sdatomic` | cross-platform CRT/POSIX wrappers + RAII lock templates | complete (pointless under modern C++) |
| ID pool | `sdidpool.h` | array + in-chain next pointers for O(1) Alloc/Free (`next==-1` reused as the "occupied" flag, neat); apollo's `base/id_pool.hpp` is its modern version | complete |
| Queue/ring buffer | `sddataqueue/sdfifo/sdloopbuffer` | cross-thread delivery queue, ring read/write buffer | complete (apollo utils already has the port) |
| Files/dirs/mapping | `sdfile/sddir/sdfilemapping/sdcsvfile` | synchronous file IO + mmap wrappers + CSV parsing | complete |
| Shared memory | `sdshmem.h/.cpp` | unified Windows file-mapping / POSIX shm wrapper (Open/Create/Close) | complete |
| DB facade | `sddb.h/.cpp` | **async command model** (see §4.1): sessions built from connection strings, core/max connection pool, GroupID ordering, two-phase callbacks | complete (MySQL/ODBC/ADO + Mock) |
| Logging | `sdlogger/` (7 cpps) | multi-backend: file/TCP/UDP, cross-platform file splitting | complete |
| Network | `sdnet` (win/linux dual implementations) | callback-style ISSSession (OnRecv/Send/DelaySend/GetSendBufFree/SetBufferSize), IOCP/Epoll, GATE variant | complete |
| Packet format | `sdpkg.h` | 16/32-byte fixed frame header: mark+len+checksum (`(len^0xBBCC)&0x88AA`), network byte order | header-only |
| Serialization | `sddatastream.h` | byte-stream read/write primitives | complete |
| Others | sdcrc/sdmd5/sddes/sdstrencrypt/sdtranslate(i18n)/sdconsole/sdsysteminfo/sdgate/sdpipe/sddebugviewer | utilities and periphery | varies |

**Conclusion: there is no entity/attribute/state-sync layer.** The reverse-engineering covers the engine's "system layer"; the game-object replication logic lived in the original engine's game layer and never entered this repo. apollo's attribute-sync design cannot learn from here (see `docs/design/attribute-sync.md`; the target model is BigWorld/KBEngine).

## 3. Comparison with apollo's Current State

| Capability | SSEngine | apollo today | Verdict |
|---|---|---|---|
| Thread pool | `sdthreadpool` (min/max, manual termination semantics) | `modules/base/thread_pool.hpp` (function+future, hardware_concurrency default) | **already have, more modern**; but lacks "cap rejection / queue cap" semantics — could be added |
| Locks/condvars | sdmutex/sdcondition/sdlock | std::mutex/condition_variable (orphan header `utils/lock.h` already deleted, 2026-10-15) | std primitives suffice; do not adopt |
| Time | dual-track DateTime + chrono | `modules/base/time.hpp` (chrono lineage) | already have and better |
| Timer | sdtimer (O(n) scan) | **no timer module** (rg across all modules finds no timer) | **missing**: build new per the target design (hierarchical timing wheel), see §4.3 |
| Raw memory pool | 16 size-class pool | `utils/memory_pool.h` (fixed-block pool, orphan header deleted 2026-10-15) + `modules/base/memory.hpp` (ByteBuffer) | the size-class variable-pool idea can be reused in network-buffer scenarios |
| Typed object pool | sdobjectpool (placement-new + MT template parameter) | **no per-type object pool** | **missing**: §4.2 |
| ID pool | sdidpool | `modules/base/id_pool.hpp` / `utils/id_pool.h` | already have (modern version) |
| Cross-thread queue/ring buffer | sddataqueue/sdloopbuffer | `include/apollo/utils/data_queue.h` / `loop_buffer.h` | already have (these are the ports) |
| Async DB | sddb two-phase commands + connection pool + ordering | `modules/data`: MemoryConnection/SimpleDataSource/SqlTemplate **synchronous mock** | **biggest gap**: §4.1 |
| Shared-memory IPC | sdshmem/sdshm | none (rg finds no shmem/mmap) | **missing but deferred**: §4.4 |
| File mapping/CSV/ini | sdfilemapping/sdcsvfile/sdiniconfig | the new config system (priority + provenance) is stronger; CSV has `utils/serializer` + the serialization module | already have and better |
| Logging | multi-backend (file/TCP/UDP) | `utils/logging` + `core/log` (async, multi-level) | already have; the **remote sink (TCP/UDP direct to collector)** is worth referencing |
| Network | sdnet (IOCP/Epoll, callback-style) | `include/apollo/net` (Session/Connection/dual-adapter embryos) + README claims IOCP/Epoll | skeleton exists; the old engine's **backpressure visibility** and GATE pattern are worth abstracting into net-abstraction (docs/design/net-abstraction.md) |
| Packet frame format | sdpkg fixed header + checksum | `modules/protocol` (codec/messages/nng_wrapper) | already have; frame-header field design can align (mark+len+checksum+sequence) |
| Attribute sync | **none** | docs/03 draft + `game/attributes` (AttributeContainer embryo) | see `docs/design/attribute-sync.md` (targeting BigWorld) |
| Algorithms (CRC/MD5/DES) | sdalgorithm | `utils/crypto.h` + protocol | already have; DES/MD5 are legacy crypto, banned in new code |
| Gateway/pipe/console | sdgate/sdpipe/sdconsole/sddebugviewer | apps/gateway-app planned; utils/terminal | direction exists; no module adoption needed |

## 4. Recommended Adoption List (ordered by value)

### 4.1 The Async DB Command Model (value ★★★★★)

- **Old engine design** (`sddb.h:253-390`):
  - `ISSDBCommand`: `OnExecuteSql(conn)` executes on the **DB thread**; `OnExecuted()` executes in order inside the **user thread**'s `ISSDBSession::Run(n)`; then `Release()` reclaims synchronously — "heavy work off-stage, read-back to the main thread".
  - `GetGroupId()`: same-group commands keep order, `-1` means unordered — ordering made an explicit per-command declaration instead of global serialization.
  - `Session` core/max connection counts (`GetDBSession(account, coreSize, maxSize)`); the synchronous path carries timeout parameters.
- **Why apollo needs it**: `modules/data` is currently the synchronous form of `MemoryConnection` + `SqlTemplate`; an MMO main loop cannot inline-wait on the DB. apollo's existing data docs (docs/08 Player_Data_Storage_and_BI) agree on the direction — what's missing is settling the threading model.
- **Landing form (modernized C++20, no interface porting)**:
  - `DbCommand = { sql/proc + completion callback }`, submitted into per-shard work queues; the completion callback is queued back onto the **owning scene thread**'s task queue (executed at the same tick boundary as attribute sync, guaranteeing a lock-free read side).
  - The ordering key becomes an explicit `entityId` (writes to the same entity ordered, cross-entity parallel) — closer to game semantics than GroupId.
  - Connection pool core/max + health checks + disconnect backoff; keep the Mock backend (for unit tests — apollo's MemoryConnection idea aligns).
  - Write strategy and crash safety per `docs/design/attribute-sync.md` §persistence (snapshot + write-behind journal); the DB threading model reuses this section.
- **Cost**: a module-level new build (inside `modules/data`), leaving the existing synchronous API untouched (can coexist as a backend).

### 4.2 The Typed Object Pool (value ★★★★)

- **Old engine design**: `CSDObjectPool<T, MT>`: free-list allocates raw memory → placement-new constructs → explicit destruct returns; **the lock-policy template parameter** (single-threaded pools pass `CSDNonMutex` for zero overhead) is the single most copy-worthy design decision in the whole library.
- **apollo gap**: the raw byte-pool header was deleted with the orphan cleanup (2026-10-15), no object semantics; hot-path high-frequency objects (packets, AOI events, attribute delta batches, skill projectiles) currently go through global new/delete.
- **Landing form**: C++20 needs none of the old engine's macro/overload tricks — `std::pmr::monotonic_buffer_resource` (frame-scoped arena) covers "per-tick temporaries"; cross-tick resident objects get a thin `free-list + std::construct_at/destroy_at` wrapper (~100 lines), with thread safety likewise a template parameter or an injected pmr memory_resource. Allocation points align with the per-tick arena in `docs/design/attribute-sync.md`.
- **Cost**: low; do the arena first (biggest payoff, smallest risk).

### 4.3 The Hierarchical Timer Wheel — Adopt per the Target Design (value ★★★★)

- **Old engine status**: header claims hierarchical, implementation is a vector scan (`sdtimer.cpp:48-74`) — **it merely "works"**.
- **Why apollo must have it**: the game loop's buff expiry, skill CDs, AI think cadence, attribute regen ticks, reconnect timeouts, DB retries — all timers; without a timer module these will later scatter into private `next_fire_time` scans in every system.
- **Landing form**: a hierarchical timing wheel (e.g. 5 levels × 64 slots, ms/10ms/100ms/1s/10s granularity), O(1) insert/cancel; **the driver sits at a fixed phase of the main loop** (not a standalone thread — established principle: the scheduler only provides the data structure, the driving right belongs to the game loop, nothing inside the high-frequency tick 〔originally citing docs/34 §15, that document was deleted with the 2026-09-29 Spring/IoC deprecated-doc cleanup, traceable in git history〕); due callbacks split into "main-thread callback / worker-thread callback" classes. The "collect inside the lock, execute outside" detail of SSEngine's sdtimer (`sdtimer.cpp:48-73`) is correct — keep that semantic.
- **Cost**: medium (one standalone module + unit tests); see §6 for the tie-in with Lua hot-reload.
- **Ownership (architecture-review §16.8.3-④)**: lands as a **new component in modules/base (data structure + unit tests)** — three precedents: BW TimeQueue sits in the lowest-level common library lib/cstdmf with its own unit_test (time_queue.hpp:60/:74, unit_test/test_time_queue.cpp); KBEngine Timers in lib/common (timer.h:101/:108); skynet as the core thread fleet (skynet_start.c:209-211); the module only provides the O(1) structure and due-callback collection, with the driving right per this section staying in a fixed game-loop phase. **Not modules/bigworld** — that is the legacy compatibility layer (BigWorld.h:3-9), timer.cpp is a forwarding stub (:3-5, self-declares the implementation comes from apollo::bw::Runtime, runtime.h:40 addEntityTimer), used only as semantic reference, not in the production chain.

### 4.4 Shared-Memory IPC (value ★★, deferred)

- **Old engine design**: `SDOpenShmem/SDCreateShmem` unifying Windows mapping / POSIX shm.
- **apollo scenario judgment**: once BigWorld-ized (base-app/world-app as separate processes), **same-machine inter-process** bulk read-only data (space grid snapshots, monitoring metrics, publish/subscribe hotspots) genuinely benefits from shm; but apollo today is mostly single-process, and the cross-process communication protocol is unsettled — adopting now would be premature optimization.
- **Recommendation**: list it among the inter-process channel candidates in net-abstraction.md (compared in the same frame as Aeron's IPC transport) — **already wired (2026-09-29)**: a formal row in net-abstraction §6's decision table + §7 P3 settled in the same batch as the self-built bus (adopting the design, not the code); the G-2 hot-standby mirror stream (same-machine shm SPSC ring) is its point-to-point exemplar scenario; implementation deferred to the multi-process landing phase.

### 4.5 The Rest (explicitly not adopted)

- sdmutex/sdcondition/sdthread/sdatomic: std fully replaces them.
- sdtime's `CSDDateTime` dual track: apollo stays single-track chrono.
- sdpkg's checksum algorithm (`(len^0xBBCC)&0x88AA`): too weak and predictable; apollo's frame format should use CRC32 + sequence (see attribute-sync.md §protocol).
- DES/MD5: legacy crypto, only for compatibility with old data.
- sdtranslate i18n/sdconsole/sddebugviewer: low overlap with apollo's existing plans, small payoff.

## 5. Lessons Exposed by the Reverse Engineering (Pitfall Records)

1. **Docs diverging from implementation is the norm**: sdtimer.h's "hierarchical timer" comment vs the vector-scan implementation; FINAL_ANALYSIS_REPORT's "100% production ready" vs ROADMAP's "20 modules unimplemented". → apollo's constraint: module READMEs must declare a three-state "implemented / partial / planned", and CI runs a capability-inventory assertion (docs are code too).
2. **`volatile BOOL` as a thread flag** (`sdthreadpool.h:140-141`): volatile has no synchronization semantics in C++; borderline UB. → new code uses `std::atomic`, always.
3. **Comment-style thread safety** ("remember to lock", `sdmemorypool.h:29-41`): offloading correctness onto every call site — guaranteed to leak at scale. → thread policy goes into the type (sdobjectpool's MT template parameter was right) or is documented as "single-thread type" with a debug-mode assertion.
4. **Raw pointers + `Release()` manual lifetimes** (ISSDBRecordSet/ISSDBSession/ISSBase AddRef/QueryRef): COM-style turns every return value into a leak point in a GC-less language. → unique_ptr/shared_ptr boundaries made explicit.
5. **The cost of a Windows-centric API facing migration**: `SSAPI/CHAR/BOOL/Hungarian naming` plus hand-rolled platform macros — exactly the hardest parts to align during reverse reconstruction. → apollo insists on stdlib-first, with platform differences locked inside adapters (one of net-abstraction.md's layering motives).
6. **Fixed header + checksum is old but right** (sdpkg's mark+len structure points the right way; the algorithm is just too weak): apollo's frame format can stand on its shoulders, adding sequence numbers and CRC.

## 6. Intersections with the Lua Hot-Reload Plan

- **The read substrate for script-resource hot loading**: file-change detection (the old engine's sdfile/sdfilemapping mtime approach; apollo already has `utils/config/FileWatcher.h` inotify/poll) + `loop_buffer` double-buffered copy + **the §4.3 timer wheel's low-frequency scan** (200ms in dev, off in production) = the hot-load pipeline's three pieces; loading/compiling on worker threads, atomic switch at the main-thread tick boundary (see `docs/design/scripting-lua.md` §hot-swap protocol for details).
- **The async DB model ↔ script async**: §4.1's "submit → callback on main thread" aligns naturally with Lua's coroutine/yield model — a script issuing `db.query()` suspends the coroutine, and the completion callback resumes at a tick boundary; this exposes the same DB threading-model substrate to both C++ and Lua.
- **Object pools/arena ↔ script GC**: once the per-tick arena takes apollo's high-frequency C++ temporaries, the Lua side only manages its own incremental GC step; neither side drags the other (scripting-lua.md §sandbox and performance budget).

---

*Reading baseline: SSEngine @ working copy (the reverse-engineering reconstruction repo); apollo main @ 35a9c528. 2026-09-29 added the §4.3 ownership line (pasted from architecture-review §16.9.4/§16.10.1).*
