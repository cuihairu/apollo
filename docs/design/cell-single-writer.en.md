[English](cell-single-writer.en.md) | [中文](cell-single-writer.md)

# Cell Single-Writer Closure (the concurrency form of RPC acceptance and the simulation thread)

> Status: **prerequisite design draft (2026-10-10); all six decision points ruled (ADR-015 main plan (a) queued asynchronous acceptance + ADR-016 companions: ② two-tier watermark/③ no fast path/④ Manager mutexes retained/⑤ shutdown G-3 mirror + ADR-017 reply wiring: ack-on-accept stays current/delayed replies follow M1/blocking adaptation rejected; ADR-015's (c) stays rejected, (b) thoroughly rejected via decision 6)**. Positioning: the decision-face expansion of todo P3-2 batch B "cell-app single-writer closure first" — pinning the race evidence, pinning the three-way semantics, pinning the decision points, without writing implementation details. The genre follows net-abstraction §7 "P3 prerequisite design"/battle-instance-offload. Terminology is governed by this piece (cell-app keeps its current name); the authoritative concurrency semantics = ADR-009 + the attribute-sync §10.2 single-writer discipline note + concurrency.md §1.1 aftermath errata (this piece is the direct expansion of its "plan forks listed as decision items").

---

## 1. Status Declaration (Grade-A direct read, 2026-10-10)

**The race itself: the World/Scene graph has two writers and zero locks.**

- The simulation writer: `gameThread_` (cell_server.cpp:142) → `worldHost_->tick()` → `CellWorldService::tick` → `world_.tick(...)` (cell_server.cpp:56) — `Scene::tick` walks `entities_` (scene.cpp:153, a `std::unordered_map`).
- The RPC writer: `protocol::RepSocket`'s self-held worker thread (socket.cpp:124 `workerLoop`) calls back into `setRequestHandler` (cell_server.cpp:98) → `handleCellCreateEntity/handleCellDestroyEntity/handleCellEntityMove/handleCellCrossBorder/handleCombatSkillCast` touch `world_` directly — `find_scene` (:176), `spawn_entity` (:191), `despawn_entity` (:210-211), `aoi().move` (:225-231), `find_session`+`execute_scene_transfer` (:253-287). Zero protection between the two threads.
- Production has not triggered it only because of the load vacuum (already recorded in concurrency §1.1) — the race is structural, not probabilistic.
- The calling-thread ownership map (closed out in the second sounding): **WorldSessionManager** has a mutex on all methods (world_session_manager.cpp:8-159) but it only guards the session table, not the world tree; **AnchorManager** has a mutex (anchor_manager.hpp:26) across the base-app acceptance line + autoSaveThread_ (snapshot base_server.cpp:496); **SessionLocator** is single-threaded (dedicated to the base-app main line). The two pieces whose locks are already in place are not this piece's race face — but their form ownership is unified with decision 1 (see §5 decision 4).
- The RPC consumers today: real-process consumption = the gateway forwarding path (a half-finished shell, the ADR-011 prescription on the books) + protocol unit tests; the calling face is already the `sendRequestAsync` callback style (socket.hpp:96-100) — **asynchronizing the replies does not break the callers' shape**.
- Future integration: the receive faces of the net M1 receiving end (InterServerLink, ADR-011) and the gateway ingress (P3-2) are naturally of the "receive packet, enqueue" form — the acceptance face defined by this decision is exactly their in-process skeleton.

## 2. Target Form (authoritative semantics)

ADR-009 (concurrency ownership): the simulation domain is driven by a single thread; cross-thread faces converge to a minimal set. The attribute-sync §10.2 single-writer discipline note: **"an entity belongs to its scene/spatial thread; cross-thread access always delivers intents/messages"**; the write semantics require "a batch of changes becomes visible, ordered, and sequence-numberable within a fixed boundary" (§10.1 G-7 argument two). That is: all changes to simulation state happen at tick boundaries and are executed exclusively by the simulation thread; the other threads only deliver intents and collect results.

## 3. The Three-Way Choice Expanded

### (a) Queued asynchronous acceptance

The RPC worker only decodes + enqueues (intent envelopes); `gameThread_` consumes, executes, and sends back replies at tick boundaries.

- **Semantics**: all changes tick-aligned; reply latency ≤1 tick (10Hz = a 100ms upper bound); callers are callback-style, no semantic breakage.
- **Cost**: a pending-reply association face is added (replies must be sent back from the tick boundary, an association key must be kept — header.sessionId can carry it); the queue watermark face must be defined (aligned with the net-abstraction §5.6 backpressure watermark semantics: reject/drop-old/watermark alarm); the shutdown sequence adapts to the G-3 five phases (stop the stream first → queue drain → gameThread join).
- **Relation to existing semantics**: isomorphic with the G-1 closing-batch precedent — death events, mirror packets, recovery re-reports all go "receive packet, enqueue; the main loop consumes"; this decision normalizes RPC requests into the same form, homogenizing the in-process hop.

### (b) Queued + synchronous wait

The worker blocks after enqueueing (condvar/future), waits for gameThread to finish, takes the result and replies immediately — synchronous-reply semantics preserved.

- **Semantics**: changes still tick-aligned (gameThread consumes at the boundary); replies immediate.
- **Cost**: **head-of-queue blocking** — the RepSocket workerLoop is single-threaded and receives no next request until the handler returns; when gameThread's tick is slow (50ms+), all subsequent requests pile up, and the backpressure propagates into the nng buffers until senders time out. Once the arrival-rate × tick-duration product crosses the line, RPC throughput collapses.
- **Association face**: the condvar wait face is itself a cross-thread face; the shutdown join order must have the handlers stop the stream before gameThread stops (the G-3 mirror order in cell-app), otherwise the join hangs.

### (c) Minimal-mutex fallback

Add one mutex to `world_` (or a scene-granularity lock); both threads keep touching directly.

- **Semantics**: zero structural adjustment, minimal diff; consistent with the current shapes of WorldSessionManager/AnchorManager.
- **The substantive rejection face (this piece's counter-proof)**: handlers mutate containers **mid-tick** — while `Scene::tick` is walking `entities_` (an unordered_map), the worker thread calls `spawn_entity`/`despawn_entity` (scene.cpp:43-56) → iterator invalidation = a UB crash; and even without crashing it breaks the iteration order = **a substantive threat to battle-determinism's four-constraint "iteration order"** (the entity tick order inside the determinism domain gets scrambled by RPC insertions). The only way to save (c) is to make the lock granularity "lock everything at tick boundaries" — which is already (b)'s semantics, circling back to head-of-queue blocking.
- **Conclusion**: (c) is not a candidate but a negative control — the lock face enters the simulation-domain hot path and does not solve determinism, violating the very body of ADR-009's "simulation logic zero locks" decision.

## 4. Honest Comparison (risk table of the three-way choice)

| Dimension | (a) async acceptance | (b) synchronous wait | (c) mutex fallback |
|---|---|---|---|
| Single writer (ADR-009) | ✓ | ✓ | ✗ (lock enters the simulation domain) |
| Deterministic iteration order | ✓ (changes at tick boundaries) | ✓ | ✗ (mid-tick mutation) |
| Reply latency | ≤1 tick | immediate | immediate |
| Throughput-collapse risk | none (the worker never blocks; the watermark rejects) | yes (head-of-queue blocking × tick duration) | none (but the hot path is contended) |
| Implementation cost | pending-reply association + watermark | condvar + shutdown order | minimal diff (a false solution) |
| net M1 receiving-end reuse | direct reuse (acceptance face = receiving skeleton) | redo (the synchronous face is scrapped) | redo |
| Shutdown sequence (G-3) | stop stream → drain → join | stop stream → (careful join order) → join | nothing new |

## 5. Decision Points (this piece decides nothing on the user's behalf; candidate leanings recorded)

1. **The main-plan three-way choice** — [decided 2026-10-10 / ADR-015: adopt **(a) queued asynchronous acceptance**] The §10.2 discipline note verbatim is (a)'s direct lineage; the G-7 argument "visible, ordered, sequence-numberable within a fixed boundary" = tick-boundary acceptance; the calling face is already callback-style, the latency increment breaks no semantics; the acceptance face is exactly the M1 receiving-end skeleton ((b)'s synchronous face would be scrapped and redone under M1). (b) is kept as the upgrade plan if (a)'s watermark semantics prove insufficient, (c) rejected. ((b) as the upgrade plan was later thoroughly rejected via decision 6, see ADR-017 decision 2)
2. **Queue watermark and overload semantics** — [ruled 2026-10-10 / ADR-016: watermark **two-tier** (high-water alarm + reject-at-cap replying an overload error packet), no drop-old no silence; overload response = callback error codes; values = tick-budget anchored] The decision-facing prerequisite design is docs/design/queue-watermark.md (watermark-form three-way choice + overload response + budget anchoring).
3. **The read-only fast path** — [ruled 2026-10-10 / ADR-016: **no fast path**] Everything enqueues, the acceptance face stays one form; PING being delayed one tick is harmless, and every extra fast path is one more concurrency form to review.
4. **The three Manager pieces' merged disposition** — [ruled 2026-10-10 / ADR-016: mutexes **retained**] WorldSessionManager/AnchorManager are already in the "cross-thread minimal set" form, small data faces, low contention intensity; the migration gain does not outweigh the shape-change risk; SessionLocator untouched; the single-writer closure refers only to the World/Scene graph. Consistent with the second sounding's "the locks must stay unless the persistence line changes shape".
5. **The shutdown sequence** — [ruled 2026-10-10 / ADR-016: the G-3 five phases mirrored in cell-app] stop accepting (stop the stream, return to maintenance; the same shape as base-app's `dispatchRequest` maintenance gate) → queue drain → gameThread join → worldHost stop.
6. **Reply wiring (the transport-layer blocker)** — [ruled 2026-10-10 / ADR-017: **ack-on-accept stays current + delayed replies upgrade with M1; (iii) blocking adaptation rejected**] The blocker itself: ADR-015 (a) requires "the worker only decodes + enqueues; gameThread_ consumes, executes, and **sends back the reply** at tick boundaries" (delayed replies), but the current transport `RepSocket` (nng_rep0) `workerLoop` is strict lockstep (`nng_recvmsg` → `handler_(data)` synchronous return → `socket_.send(response)` → the next recv round, socket.cpp:139-172/socket.hpp:119) — the reply must be produced synchronously by the same thread that received, delayed replies are unimplementable; and :165's skip-send-on-empty-reply (current mutation-type handlers return `{}`) under a real REP deadlocks as soon as a reply is owed — that is exactly why the whole repo takes the stub branch (:431-488). The ruling's key points: after the worker decodes + validates + enqueues (intent envelope, the ADR-016 two-tier watermark gate), **the acceptance point acknowledges immediately** (an empty ack for mutation types = byte-for-byte identical to the current return values; overload replies an overload error packet); gameThread_ consumes and executes at tick boundaries, results never become replies; PING's Pong is replied at the acceptance point (a pure wire echo with no world access, not the fast path that prohibition 3 refers to); delayed replies (the pending-reply association face + the gameThread_ send path) upgrade with M1's InterServerLink, the acceptance-face skeleton is scrapped zero. (iii) blocking adaptation = (b) reskinned, with three counter-proofs (queue depth forever ≤1, the watermark machinery becomes dead code / throughput collapses to the tick rate / conflicts with ADR-015's original ruling to abandon (b)) — rejected. The three-candidate leaning analysis ((i) wait for M1 / (ii) ack-on-accept / (iii) blocking adaptation) and the leaning-semantics errata (the original "(i) is the canon" addressed the endgame wiring; ack-on-accept is (i)'s prerequisite stage, not a detour) are detailed in ADR-017.

## 6. Post-Decision Path

The acceptance-face batch (intent envelopes + queue + tick consumption + acceptance-point acknowledgment, cell-app) — **delivered (2026-10-10, 90d8ebb7)**: intent envelopes + AcceptanceQueue (bounded FIFO + two-tier watermark + reject at cap) + acceptance-point acknowledgment (ack-on-accept) + tick-boundary drain + the shutdown G-3 tail sweep, cell_acceptance_tests six groups all green; reply wiring per ADR-017 (ack-on-accept stays current). The watermark-and-overload batch (aligned with §5.6 semantics) landed in the same batch as the acceptance face. The Manager-merger review batch (if decision 4 changes anything) → the net M1 receiving-end wiring (the acceptance face is the skeleton; M1 only swaps the transport base, adding the delayed-reply upgrade increments: the pending-reply association face + the gameThread_ send path). battle-instance-offload's spawn-request protocol control face reuses the same acceptance form (ADR-012).

## 7. Intersections with Other Designs

- **attribute-sync §10.1/§10.2**: the G-7 scheduling-paradigm argument + the single-writer discipline note — this piece's authoritative-semantics source.
- **concurrency.md §1.1**: the aftermath errata of the race evidence — this piece is the expansion and closure carrier of its "plan fork".
- **battle-determinism**: the iteration order of the four constraints — plan (c)'s substantive rejection face.
- **net-abstraction §5.6**: the M1 watermark/backpressure semantics — decision 2's alignment baseline; §5.7 InterServerLink — the acceptance face's future transport base.
- **ADR-009/ADR-011/ADR-012**: the concurrency-ownership decision proper; the timing of the gateway surface vs M1; the offload control face's same-shape reuse.
- **session-and-online-directory §6**: the recovery phase's re-report acceptance on the cell-app side — likewise following the "receive packet, enqueue; the main loop consumes" precedent (G-1 increment 3).

## 8. Evidence Source List (Grade-A direct read, 2026-10-10)

- `apps/cell-app/src/cell_server.cpp`: :56 (the tick writer), :98-127 (handler registration), :142 (gameThread_), :176/:191/:210-211/:225-231/:253-287 (the worker touching world_ directly), :344 (gameLoop tick)
- `modules/protocol/include/apollo/protocol/socket.hpp`: :116-138 (RepSocket), :96-100 (sendRequestAsync callback style), :119 (RequestHandler synchronously returning `std::vector<uint8_t>`); `src/socket.cpp`: :124/:139 (workerThread_/workerLoop), :139-172 (recv→handler→send strict lockstep)
- `modules/game/world/src/scene.cpp`: :43-56 (spawn/despawn), :153 (tick walking entities_); `include/.../scene.hpp`: :85-87 (the unordered_map container)
- `modules/game/world/src/world_session_manager.cpp`: :8-159 (mutex on all methods)
- `modules/game/session/include/apollo/game/session/anchor_manager.hpp`: :26 (mutex)
- `apps/base-app/src/base_server.cpp`: :231 (setRequestHandler→dispatchRequest), :299-340 (the maintenance gate, same shape), :496 (autoSaveThread_ snapshot)
- `docs/rearchitecture/concurrency.md` §1.1, `docs/design/attribute-sync.md` §10.1/§10.2, `docs/architecture/adr.md` ADR-009/ADR-011/ADR-012/ADR-015/ADR-016/ADR-017

*Baseline: apollo main @ 8cab82d5. This piece is a prerequisite design — all six decision points ruled (ADR-015/ADR-016/ADR-017, 2026-10-10); the acceptance-face batch has landed (2026-10-10, 90d8ebb7).*
