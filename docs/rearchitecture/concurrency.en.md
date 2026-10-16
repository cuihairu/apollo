[English](concurrency.en.md) | [中文](concurrency.md)

# Apollo Concurrency Model (concurrency)

> Belongs to: companion piece 5 of the phase-one read-only architecture review. Companions: audit.md, architecture.md, object-model.md, lifecycle.md, persistence.md, improvement-plan.md.
> Basis: task book §26 (concurrency model: who may modify Player/Scene/Instance/Entity/Persistence, with an explicit Owner + explicit execution context), §17 (Tick/Scheduler), §32 (thread/lock complexity). Terminology follows existing code wording (BigWorld/KBE concepts reused first); the differences list is at the end.

---

## 1. Thread Inventory (measured current state)

| Thread | Created by | Runs what | Current problems |
|---|---|---|---|
| Main thread | each app's main | login/gateway/base/cell main logic + message handling | game-server uses the ServiceHost run_once loop; cell-app's main loop = gameLoop (handleMessages + map_instance_manager_.update + worker_pool submission + tick+sleep) |
| Autosave thread | base-app ctor | SaveQueue::workerLoop (100ms wait_for polling the queue) | only callback(true); no stop protocol; the 60s autoSaveLoop only prints |
| Network send/recv threads | net library components | RpcClient/RpcServer send/recv loops (10ms sleep idle-spin placeholders); HttpServer spawns a detached thread per connection | detached threads never reclaimed; stop hangs |
| WorkerPool | cell-app (ThreadPool) | no game-logic tasks (idling) | submitted but no consumption semantics |
| WorldHost tick | called directly from the cell-app main loop | CellWorldService::tick: EntityManager.update + MapInstanceManager.update (**holding locks**) | see §3 |
| IPC-related | APOLLO_ENABLE_IPC=OFF | — | not compiled |

**Absent**: scene threads, a real persistence-write thread, directory threads, multi-scene parallel threads. **All business runs on the main thread + mutex-protected in-memory containers**.

---

### 1.1 Post-hoc errata (2026-10-07 measured update)

This section is a post-hoc supplement to the §1 snapshot — the P0-3-and-later
refactor changed the thread topology, and the "all business on the main thread"
of when this document was written no longer holds:

- cell-app split out `gameThread_` (cell_server.cpp:142) running
  `worldHost_->tick()`; message handlers execute on `protocol::RepSocket`'s
  self-owned worker thread (socket.cpp:124 workerLoop) — **handlers writing the
  world (create/destroy/move/cross-border transfer/combat) and the tick
  reading/writing the world run concurrently across threads, with no protection
  whatsoever between the two threads for Scene (the post-P1-3 lock-free
  single-writer form) and the World tree** — a real race (not triggered in
  production only because the load is hollow; same root cause as architecture
  §6.1's "model hollowness"). WorldSessionManager's mutex protects only the
  session table itself, not the world tree.
- base-app: autoSaveThread_ + the SaveQueue worker thread really exist (already
  recorded in the §1 table); the calling-thread attribution of
  AnchorManager/SessionLocator awaits the P3-2 batch B map close-out.

The fix direction follows the §3 target model ("other threads modify via
message delivery") — batch B's charter is in docs/todo P3-2; the scheme fork
(queued async acceptance / queued + sync wait / minimal-mutex fallback) is an
RPC-semantics tradeoff, listed as a decision item — **the prerequisite design
is chartered** in docs/design/cell-single-writer.md (2026-10-10: A-level
real-read of the race + three-way semantics/risk comparison + six decision
points, candidate lean = (a) queued async acceptance, (c) disproof veto).
**Decision ① ruled** (2026-10-10 / ADR-015): adopt (a) — the acceptance-face
batch (functional code) awaits boundary release; ② watermark / ③ fast path /
④ Manager / ⑤ shutdown are all ruled (ADR-016, 2026-10-10: two-tier watermark
+ callback error codes + tick-budget anchoring / no fast path / Manager mutex
retained / shutdown G-3 mirror; the watermark decision's prerequisite design is
docs/design/queue-watermark.md). **Decision ⑥ ruled (ADR-017, 2026-10-10)**:
ack-on-accept is the current form (receipt at the acceptance point, the
change-class empty ack matches the current wire semantics) + deferred replies
upgrade with M1 InterServerLink (the acceptance-face skeleton is retired
wholesale) + blocking adaptation vetoed (a queue depth permanently ≤1 makes
the watermark machine dead code / throughput collapses to the tick rate /
conflicts with the original (b) rejection) — the crux itself = the nng_rep0
transport lockstep[socket.cpp:139-172] being incompatible with deferred
replies; see ADR-017 for details.

---

## 2. Lock Inventory (the list that actually exists)

| Lock | Protects what | Hold duration/risk |
|---|---|---|
| AnchorManager::mutex_ | player→anchor map | short (one critical section each for activate/find/deactivate/count) — **healthy** |
| SessionLocator::mutex_ | the bidirectional binding map | short — healthy |
| MapInstanceManager::mutex_ | the instances_ map | **update() runs the entire tick tree inside the lock** (map_instance_manager.cpp:25-32) — long critical section |
| WorldSessionManager::mutex_ | the sessions_+player_index_ double map | short (one critical section per operation, no deadlock structure) |
| EntityManager (cell) | the entity map | update traverses entities under the lock calling on_update (cell_server.cpp:38-42 chain) |
| cell::AOIManager | the fixed-length grid | grid updates short |
| AttributeManager/Container (two sets) | attribute containers | **returns references/pointers inside the lock, used outside it** — race window (attribute.cpp:91-94,107-111) |
| CacheManager/PrimitiveCache | the in-memory cache | short — healthy |
| ConnectionPool×2 | the idle deque+cv | correct (the standard GetConnection/ReleaseConnection pattern), but CreateConnection is an empty function |
| DatabaseService::cacheMutex_ | the player map | short — healthy (but no medium writes) |
| SaveQueue mutex+cv | the task queue | correct |
| SessionManager::mutex_ (net) | the session table | **self-deadlock**: onDisconnected calls getSession under the lock, locking again (session_manager.cpp:273-280) — non-recursive lock |
| Reactor::mutex_ | the socket table | short |
| io_context / queue family | inherited pieces | short |
| RedisDistributedLock | (fake) | 90% random success, zero consumers |

**Verdict**: about 13 locks (uncompiled pieces like Timer/Channel not counted);
"short critical sections" are the majority and structurally healthy; the
problems concentrate in 3 places: running a business tree inside a lock
(MapInstanceManager), callbacks inside a lock (EntityManager), references used
outside a lock (Attribute) + 1 deadlock (SessionManager) + 1 place with **no
lock at all** (concurrent DI get is UB, relying on the "build before startup,
read-only at runtime" convention — callers currently comply, but nothing
enforces it).

---

## 3. The "Who May Modify Whom" Matrix (the answer to task book §26 — current state)

| Modified object | Currently allowed by | Actual entry path | Problem |
|---|---|---|---|
| PlayerAnchor | the AnchorManager holder (base-app request thread) | activate/bind/unbind/deactivate all on the request handler thread | single thread + lock = gray zone; no "changer identity" concept (no guardrail when multi-threaded later) |
| WorldSession | the cell-app main thread (after holding the WorldSessionManager lock) | attach/detach/transfer | single writer is good; but the lock does bookkeeping, no scene-thread boundary |
| Scene.entities_ | the cell-app main thread | Scene::update single-threaded | zero entities in production — model hollowness |
| Entity (cell) | update under lock | on_update inside the lock | callback inside a lock: a callback re-entering the manager is a deadlock risk |
| MapInstance tree | tick inside the lock | the update chain | long critical section mixed with broadcast |
| Attribute containers | any thread gets a reference | AttributeManager (fetch inside the lock, write outside it) | race window |
| Session table (net) | SessionManager | OnDisconnected re-enters under the lock | self-deadlock |
| Cache | pool lock | — | healthy |
| DB fake implementation | the request thread | savePlayer synchronous | no real writer |

**Target model (contract/design line + task book §26 recommendation)**:

```text
Explicit Owner + explicit execution context:
  PlayerAnchor: Home baseapp "home thread" single writer (other threads modify via
       message delivery)
  Scene/Instance/Entity/AOI: the scene thread (one execution context per scene; the tick is
       fixed-step 10Hz per the design line; the current WorldHost 20Hz default needs
       aligning with clock-and-time's 10Hz — see differences list item 5)
  Session/Connection: the gateway connection thread (one-way downstream buffering)
  Persistence: writes async (write-behind drains on a dedicated thread); reads sync warm-up
  Manager layer: fully de-locked (after single-writer, mutexes become assertion tools
       rather than synchronization mechanisms, or are removed from hot paths outright)
```

---

## 4. Tick/Scheduler Current State and Target (task book §17)

| Layer | Current state | Target |
|---|---|---|
| World | WorldHost 20Hz single loop (not compiled) | the baseapp world clock (10Hz per the design line; clock-and-time's three clocks: steady tick / wall / now) |
| Scene | no independent tick (Scene::update only traverses the empty entities_, running with MapInstanceManager::update) | scene tick: simulate→recalc→aoidecay→collect→budget+flush→persist-batch (attribute-sync's six phases) |
| Instance | none (same tree as scene) | gameplay tick (each of the waiting/running/settlement phases at its own pace) |
| Battle | none (two layers of empty shells) | the battle runtime inside the scene tick (or an independent fixed rate) |
| Multi-threading | single-threaded main loop | multiple scenes parallelizable (P2/P3 boundary; single-scene single-threaded correctness first) |

**Over-complexity check (the second half of §17)**: there is no complex scheduler today — that is good; the risk is on the other side: **the framework layer has no fixed-rate abstraction available** (WorldHost uncompiled + game-server has no loop); the only thing running is cell-app's hand-written `gameLoop` (tick + 100ms sleep mixed, the sleep unrelated to the frame-rate budget).

---

## 5. Concurrency Problem List (severity-ordered)

| # | Problem | Evidence | Level |
|---|---|---|---|
| C-1 | MapInstanceManager/EntityManager runs the business tree inside a lock (tick callbacks inside the critical section) | map_instance_manager.cpp:25-32 | P0 (the foundation of scene concurrency correctness) |
| C-2 | SessionManager self-deadlock (re-entry inside a non-recursive lock) | session_manager.cpp:273-280 | P1 |
| C-3 | Attribute containers fetch references/pointers inside the lock, use them outside (race window) | attribute.cpp:91-94,107-111 | P1 |
| C-4 | No "execution context" concept: every object lacks an owner-thread declaration (nothing documents or enforces which thread runs what) | the whole codebase | P0 (task book §26's first priority) |
| C-5 | HttpServer detached threads never reclaimed + stop hangs (concurrency resource leak) | http.cpp:506-549 | P1 |
| C-6 | Concurrent DI get is UB (no lock, relies on convention) | application_context.hpp | P2 (record, do not change semantics) |
| C-7 | RepSocket::stop joins before closing the socket, deadlocking (module being retired; the fix is listed in the transport convergence) | modules/protocol/src/socket.cpp:127-133 | P2 |
| C-8 | No TSAN/sanitizer gate, no concurrency stress testing | full CI layout check | P3 |

---

## 6. Terminology Differences (concurrency level)

1. **"Scene-thread single writer"**: original line §1.5's tick entry (fixed-tempo total-order advancement, scene-thread single writer at 10Hz). The code has no scene-thread concept (§4) and no 10Hz (WorldHost defaults to 20Hz, world_host.hpp:17 — clock-and-time's 10Hz and the code default disagree, registered); the "single writer" current fact is "main thread + lock", semantics matching but the naming surface does not exist. Difference registered.
2. **Tick number (tick sequence)**: the original line requires the tick number to be totally ordered and unique; the WorldTickContext{tick_index, delta_seconds, now} header has the field (exactly the original line's design) but WorldHost is uncompiled — the implementation side awaits landing. Registered.
3. **The "lock count" complexity-review verdict** (task book §32's line): the existing 13+ locks are **excessive** for the target scale of "single process, 1..N scenes" (the target model's lock count ≈ 2: the save queue + the directory mirror); the P0 de-locking target is folded into improvement-plan's convergence criteria.
4. The remaining original-line words (four-channel seq, acked_seq, ChangeHistory) are not covered by this document; see architecture.md §1.5's gap table.

---

(end)
