
# Apollo Capacity Model and Benchmark Facilities (capacity-and-benchmark)

> Status: design draft (under review). Positioning: gather the capacity numbers scattered across the six design documents (5000 CCU / 25k msg/s / movement <100B / 32KB/s token bucket …) into a **single-process capacity budget table with its derivation chain**; answer "where do the benchmarks of net-abstraction §5.6's 'M2+ every item has a benchmark before you touch it' come from"; fill in the C++-side memory and object-pool budgets (the Lua side already has its caps in scripting-lua §6; the C++ side had no counterpart before). Cross-referenced with `clock-and-time.md` (tick 10Hz semantics / skip-rate metrics), `attribute-sync.md` (§5 bandwidth / §10 six phases / §11 acceptance table), `net-abstraction.md` (§4 watermarks / §4.3 upstream rate limiting / §5.6 evolution ladder), `scripting-lua.md` (§5.2 hook budget / §6 sandbox caps), `sdk-contract.md` (§10.6 performance quantification), and `battle-determinism.md` (§5 recording format — reused as benchmark load).

---

## Executive Summary

1. **Capacity target in one sentence**: single process, 5000 CCU steady state, and single scene with 100 players on screen in battle hitting p99 (attribute-sync §11 full table) — every other number derives from this one; no more each-for-itself.
2. **Derivation chain in four steps**: CCU → entity scale and AOI density → change/message rates (upstream 25k/s whole-server = 5000×5 msgs/s) → frame budget and bandwidth allocation (10Hz × 100ms/frame; downlink per-client 32KB/s token bucket) → memory budget (bytes per entity × scale + Lua state + net rings).
3. **Semantics unified**: main-loop tick = **10Hz** (settled in clock-and-time §4); movement **20Hz is the client-report cadence** (net-abstraction §4.1), not the main-loop frequency — the "20Hz main thread" in sdk-contract §10.6's quantification passage is an old estimate; this document restates per clock-and-time (the frame-share conclusion unchanged; sdk-contract :500's 8%/4% recomputed at 10Hz is ~4%/2%, still within the frame budget's 10%).
4. **Frame budget table** (§2): each 100ms frame allocated across the message segment + tick six phases + slack; the existing budgets in other designs (script-hook window ≤0.5ms/frame, collect/flush timeout truncation) are placed into the same table; the budget-overrun truncation mechanism follows attribute-sync §10.
5. **Memory and object pools (#7)**: pool the three known hot spots (frame objects / attribute-collect buffers / send-ring slices), free-lists hung on the owning thread (single writer, lock-free); **jemalloc/tcmalloc not introduced** — allocation hot spots are already converged by single-writer + pooling; a global allocator swap is premature optimization; listed as an M2+ benchmark-gated candidate (evaluate only if allocation exceeds 5% of frame).
6. **Three benchmark forms** (§5): micro-benchmarks (single mechanism, ctest, PR-level gate) / scenario benchmarks (examples/benchmark, synthetic intent streams — deterministic-RNG derived, reproducible) / recording-replay (production-sampled intent sequences = the same format as battle-determinism §5; a recording is both replay-verification load and stress-test load). The M2+ admission benchmark = the scenario benchmark's before/after.
7. **System-level acceptance metric set** (§5 final table): the §11-type acceptance items of other designs aggregated + new system-level items (skip rate / dropped_ticks — clock-and-time §10's P3 reference to "the capacity batch §5" means this table; watermark trigger rate; violation_score disconnect rate; recording size and hash-chain divergence rate — battle-determinism §8's P3 integration point).

## 0. Current State: Scattered Numbers and Blanks (gap evidence)

- Scattered numbers (sources read directly): 5000 CCU and 25k msg/s (sdk-contract :476/:500), movement <100B (sdk-contract :395), 32KB/s per-client token bucket (attribute-sync §5.1), 100-players-on-screen stress test (attribute-sync §11/§12 P1 acceptance), zstd >256B threshold (sdk-contract :395 — "benchmark it at P1 landing, together with attribute-sync §11's measurement" is the debt this document pays), hook instruction budget 200k≈0.5ms (scripting-lua §5.2), per-state Lua memory cap (scripting-lua §6 — mechanism present, number absent).
- Blanks: "object pool / memory pool / pmr" zero hits across the six design documents (#7 evidence); system-level capacity model and benchmark harness — zero as a document (#5 evidence) — "M2+ every item has a benchmark before you touch it" is empty words without facilities.

## 1. Semantics and Top-Level Targets

| Item | Value | Source/notes |
|---|---|---|
| Main loop | tick 20Hz, frame 50ms | world-app config tickRateMs=50 (design semantics 10Hz, calibration pending) |
| Capacity target | 5000 CCU/process steady state | sdk-contract :476 incumbent semantics; this document promotes it to the top-level target |
| Extreme scenario | single scene, 100 players on screen in battle | attribute-sync §11 stress-test semantics |
| Entity scale | 5000 players + ~20k NPCs/monsters (per-scene quota) | estimate semantics, configurable |
| Upstream profile | 5 msgs/s per player → 25k msg/s whole-server | sdk-contract :500 incumbent estimate |
| Attribute change rate | 20 changes/s per player (after AOI broadcast amplification, ~200 frames/s per client downlink) | estimate semantics, benchmark-calibrated |
| Deployment shape | single-process stage: single machine 8GB; after the P3 fleet, per-process-role tables (this document uses the game process) | — |

## 2. Frame Budget Table (100ms per frame, single-scene-thread semantics)

| Segment | Budget (initial allocation; enters config constants after benchmark calibration) | Basis/mechanism |
|---|---|---|
| Message segment (process_messages: upstream dispatch / async callbacks) | ~20ms (run-to-completion if insufficient, remainder yielded) | clock-and-time §4 main loop |
| simulate (business/script writes attributes) | ~30ms (incl. script-hook window ≤0.5ms, scripting-lua §5.2) | attribute-sync §10 phase 1 |
| recalc (derived recomputation, incl. Lua formula calls) | ~10ms | phase 2 |
| aoidecay (viewer-set deltas) | ~5ms | phase 3 |
| collect (delta/snapshot assembly) | ~10ms | phase 4 |
| flush (budget packing + send) | ~10ms | phase 5 |
| persist-batch (journal handoff to the DB thread) | ~5ms (measured anchor §2.4: resident-handle batch landing 1.6–2.4ms — 0.32–0.48× of budget; the per-line flush durability semantics unchanged) | phase 6 |
| Slack / long-tail absorption | ~10ms | N consecutive beats >80% = overload signal (clock-and-time §4) |

- Overrun handling follows the existing mechanism: phases 4/5 truncate immediately on budget timeout, remainder deferred to the next tick (attribute-sync §10); whole-frame overrun goes through bounded catch-up/skip (clock-and-time §4 catch_up_max=2).
- Multiple scene threads: each thread independently 100ms (§10 single writer), the frame budget table reused as-is; whole-process CPU budget = scene-thread count × frame budget + IO threads + DB thread (deployment parameter: scene threads ≤ physical cores − 2).

### 2.1 Measured anchors (P3-2 batches A/C, 2026-10-07)

**Semantics**: scene-local load (not whole-frame-segment cost) — `examples/benchmark --models`, the three model stubs (arch §6 semantics): all players drifting at constant speed (5 units/s) + SceneAoi::move (ViewerState diff → Enter/Sync/Leave event counts) + Scene::tick's six-phase real path, single-threaded drive; golden-ratio scatter; 10 warm-up frames excluded from statistics; frame means take the 3-run **min** (lower-bound semantics — host load 18-24 during measurement, means swing with noise; min is the statistic closest to true cost). Machine: i9-10880H @2.30GHz (laptop-grade baseline, not deployment-target hardware; cross-machine comparison requires re-measurement).

| Model | Entity distribution | Frame mean (min) | Budget share | Sync events/frame | Event determinism |
|---|---|---|---|---|---|
| A big scene | 1×1000 (1000² world) | 2.84ms | 2.8% | 1973 | 3 runs bit-identical |
| B multi-scene high density | 10×100 (100² world) | 5.90ms | 5.9% | 19543 | ditto |
| C instance-based | 100×10 (100² world) | 0.82ms | 0.8% | 1721 | ditto |

**Conclusion directions** (magnitude semantics, lower-bound observations under noise):

- All three models' scene-local load fits the 100ms budget (heaviest B ≈6-8%) — **the current bottleneck is not on the scene compute face**; against the §2 segment table, the simulate segment (~30ms) has 5-10× headroom.
- B (multi-scene high density) is heaviest: the broadcast event face ~19543 events/frame ≈ 195k events/s @10Hz — the AOI diff + ViewerState watermark maintenance is the first artificial-load hot-spot candidate.
- Event counts bit-deterministic (movement pattern fixed → diff sequence deterministic) — the benchmark can serve as a regression signal (event-count drift = an AOI/ViewerState behavior change).
- Stub-scope limits: no pathfinding/skills/cross-scene/network-send face; frame cost is a lower bound; making the §2 whole-frame segments empirical goes by batches — the flush segment is done (§2.3, 2026-10-13), the persist-batch segment is done (§2.4, 2026-10-13).
- After Batch B's closure (world-app single-writer-ization, plan pending decision) + multi-scene parallel threading, re-measure against this anchor.

(Batch A's first-run 20Hz-semantics data voided — the main loop at 10Hz is the clock-and-time §4 settlement; benchmark kFrameDelta/budget constants errata'd with Batch C to 10Hz/100ms.)

### 2.2 Acceptance-face measured anchor (P3-2 batch D, 2026-10-11)

**Semantics**: acceptance-face local cost — `examples/benchmark --acceptance`: the acceptance path (parseHeader + envelope construction [raw frame copy] + enqueue) and the execution path's queue side (dequeue + parseHeader + decodeBody) measured as separate micro-segments; the burst = cap-max 1024 real movement-intent frames (MessageCodec full encoding); 3-run min (lower-bound semantics, same discipline as §2.1); world-change/broadcast face not in this segment (belongs to the §2.1 scene anchor; the two segments bracket the full cost). Machine as §2.1 (i9-10880H, load noise during measurement; the magnitudes sit far below budget so noise does not affect the conclusions).

| Segment | Measured (min) | Semantics |
|---|---|---|
| Acceptance path (worker: header + envelope + push) | ~45 ns/intent | 3-run min, noise band 45-70 |
| Execution path queue side (tick: pop + header + decode) | ~55 ns/intent | ditto |
| Total | **~0.1 µs/intent** | ADR-016 #3's starting assumption ~50µs; measured ~**500× better** |
| cap-max 1024 full drain | **≈0.10 ms** | message-segment budget 20ms; headroom **~194×** |

**Calibration conclusions** (ADR-016 #3 landed; numbers backfilled into acceptance_queue.hpp):

- **The drain side is far from the constraint**: starting constant cap 1024 × measured per-intent cost ≈ 0.1ms ≪ 20ms message-segment budget — "queue depth × per-request handling time ≤ tick budget" has two orders of magnitude of headroom at measured cost; **the capacity side is the constrained dimension**.
- **Scale-tier comparison** (upstream profile 5 msgs/s/player @10Hz = 0.5 intents/beat/player): single instance (10 players) 5 intents/beat ≈ 0% of cap; single scene (100 players) 50 ≈ 5%; big scene (1000 players) 500 ≈ 49%; **full capacity (5000 CCU) 2500 intents/beat = 244% of cap** — under single-process full-capacity steady state, queue saturation is the inherent shape, and max-out rejection is exactly its designed consumption face (ADR-016: no silent dropping of the old); upstream rate-limit backstop goes with net M1 (net-abstraction §4.3).
- **Numbers kept**: cap 1024 / warning line 768 stay at starting values — ~20× headroom for the single-scene tier, ~2× for the big-scene tier; if the full-capacity saturation level is to be raised later, one constant changes (the drain side's 194× headroom supports it) — a capacity decision item, not privately changed in this batch.
- Re-measurement semantics: the `benchmark --acceptance` standalone segment (no waiting on the scene segments) — the comparison-baseline re-measurement entry of the §5 environment discipline.

### 2.3 flush-segment measured anchor (P3-2 ① whole-frame segments made empirical, 2026-10-13)

**Semantics**: the measurable part of the collect+flush segments — the `examples/benchmark --models` frame loop, after the scene-local segment (movement + AOI diff + tick), encodes every AOI event of the frame into real protocol frames (Enter→CellCreateEntity / Sync→CellEntityMove / Leave→CellDestroyEntity, MessageCodec full encoding, session_id 0 = system broadcast); the encoding-timing segment is separated from the scene-local segment. **Network socket sending not counted** — the gateway delivery face is still a stub (world_app `broadcastToViewers` only logs); this segment measures packing cost; 3-run min (same discipline as §2.1). Machine as §2.1.

| Model | Scene frame mean (min) | flush mean (min) | Whole-frame total | Whole-frame share | Msgs/frame | KB/frame |
|---|---|---|---|---|---|---|
| A big scene | 1.93ms | 0.156ms | **2.09ms** | 2.09% | 2035 | 118.3 |
| B multi-scene high density | 4.50ms | 1.75ms | **6.25ms** | 6.25% | 20261 | 1176.3 |
| C instance-based | 0.66ms | 0.171ms | **0.83ms** | 0.83% | 1786 | 103.7 |

**Conclusion directions**:

- **The flush segment is model B's biggest single-item segment**: 1.75ms ≈ 39% of the scene-local segment (4.50ms) — under high-density multi-scene, broadcast packing cost exceeds any single scene segment; the §2 budget table's collect ~10ms + flush ~10ms allocation points the right way; under the B shape the two segments' measured total ~1.75ms (~6× headroom within budget).
- Per-message encoding cost ~80ns stable (A 0.156ms/2035 ≈ 77ns, B 1.75ms/20261 ≈ 86ns) — linear in message count, no super-linear knee.
- **Event counts bit-deterministic, unchanged**: Sync/frame across the three models' three runs bit-identical with the prior batch (1972.8/19542.5/1721.3) — the regression signal kept; Msgs/frame and KB/frame likewise bit-deterministic (encoding-path determinism).
- **Scope boundary (faithful)**: the whole-frame total is still a lower bound — recalc/collect assembly/network send/persistence segments not counted; the boundary between collect assembly (delta-structure building) and encoding is not separated in this segment (event collection into the vector is included in the scene segment, encoding in the flush segment); the persist segment is done (§2.4, 2026-10-13 — the first test's per-line open/close exceeded budget; the resident-handle batch closure, see §2.4).

(The event-collection face entered the benchmark with this batch: sink counting + per-frame staging vector — encoding done uniformly after tick; the scene-local timing excludes encoding.)

### 2.4 persist-segment measured anchor (P3-2 ① whole-frame-empirical closure, 2026-10-13)

**Semantics**: the measurable face of the §2 budget table's persist-batch row (journal handoff to the DB thread) — the `examples/benchmark --models` frame loop, after the flush segment, appends one PersistJournal line per entity (write-ahead to disk) + quota drain (sink = handover counting; the DB-thread serialization face not counted); under the single-writer posture, append+drain are consecutive on the same thread, the timing segment separated from scene/flush. Load model: dirty-entity semantics = one line per entity per frame (~30B JSON payload standing in for HP/exp-type L2 deltas — movement itself is L3 volatile, never journalled; the payload is a load model, not a semantic claim), 1000 lines per frame for all three models. **Medium = host /tmp (tmpfs)**; the to-disk face includes real syscalls; compact (snapshot thinning) is an independent cadence, not in this segment; 3-run min (same discipline as §2.1); machine as §2.1.

| Model | Rows/frame | persist segment (min) | Per-line cost (min) | 3-run range |
|---|---|---|---|---|
| A big scene | 1000 | 2.39ms | ~2.4µs | 2.4–2.5ms |
| B multi-scene high density | 1000 | 2.39ms | ~2.4µs | 2.4–2.6ms |
| C instance-based | 1000 | 1.61ms | ~1.6µs | 1.6–2.5ms |

(Table values = anchors after the resident-handle batch (2026-10-13); the first test on the P1-4 per-line open/close skeleton — 6-run min A 10.06 / B 6.13 / C 8.74ms — is archived as a mechanism comparison, see below.)

**Conclusion directions**:

- **This segment is not a function of scale; it is mechanism cost × row count**: Rows/frame is 1000 for all three models (dirty-entity = everyone semantics), segment costs the same magnitude; the 1.6–2.4ms min spread is to-disk-face jitter, not scale difference.
- **Mechanism closure (persist durable-face batch, 2026-10-13)**: append went from the P1-4 skeleton's per-line `ofstream` open+write+flush+close (durability-first posture, measured 6–22µs/line at a thousand lines/frame, 1–3× over budget) to a **resident handle** — open once with `app|binary`, per-line flush, durability semantics unchanged; on write failure close the handle, reopen at next append (self-healing same as the old per-line-open semantics); compact's tmp+rename atomic rewrite changes the inode — close the handle before thinning, reopen after (otherwise post-thinning appends silently write into the deleted file). Measured min 1.6–2.4ms all inside the ~5ms budget (0.32–0.48×) — **group commit (amortizing flush) is no longer needed**; that ledger item closed.
- **Scope boundary (faithful)**: append may hang on the SaveQueue worker thread (skeleton single-writer note), in which case the game-thread face only does enqueue handover and this segment's cost leaves the frame; the DB-thread serialization and compact faces not counted; the text-line format's encode_line + deque-enqueue body cost is a small share (drain face is microsecond-level).
- **Regression signal**: Rows/frame = 1000 bit-constant (load model deterministic); persist_ms is on the IO face (host jitter), not a regression signal.

## 3. Message and Bandwidth Budget (aggregating the existing + deriving)

| Face | Value | Source |
|---|---|---|
| Total upstream rate | 25k msg/s whole-server (5/s per player); per-session rate limiting in the four-channel parameter table | sdk-contract :500; net-abstraction §4.3 |
| Downlink per-client | token bucket 32KB/s (dynamically lowered), steady-state target ≤80% | attribute-sync §5.1/§11 |
| movement frames | 20Hz × <100B ≈ 2KB/s/client steady state | net-abstraction §4.1; sdk-contract :395 |
| attributes batching | 10Hz batches, zstd only above 256B (threshold to be micro-benchmarked at P1 — the sdk-contract :395 debt) | attribute-sync §7.1 |
| Whole-server egress bandwidth | 5000 × 32KB/s = 160MB/s hard cap; actual ≤80% ≈ 128MB/s ≈ 1Gbps — within a single machine's 10Gbps NIC; split per process after the multi-process fleet | derived |
| Frame-header overhead | 16B/frame (net-abstraction §3) — ~16% of movement-class small frames; bundle coalescing (M2+) is the countermeasure | net-abstraction §3 |

## 4. Memory and Object-Pool Budget (#7)

**Per-process memory budget table (initial, magnitude semantics, benchmark-calibrated)**:

| Domain | Budget | Derivation |
|---|---|---|
| Entities and attributes | ≤1.5GB | player entity ~40KB (values bucket ~5KB + ChangeHistory 1024 entries ~20KB + components ~15KB) × 5000 ≈ 200MB; NPC ~15KB × 20k ≈ 300MB; ViewerState pairs (100 on-screen² × 16B × scene count) and peak headroom |
| Lua state | ≤64MB per state (numerifying scripting-lua §6's cap) × scene-thread count (≤8) ≈ 512MB | entity data stays out of Lua (scripting-lua §1); state holds logic only |
| net send/receive | 64KB send ring + 64KB receive buffer per session × 5000 ≈ 640MB | ring capacity = burst ceiling (the physical basis of the 95% watermark cut, net-abstraction §5.8) |
| journal/batch buffers | ≤512MB | the boundedness of write-behind's per-tick quota dequeue (attribute-sync §8.2) |
| Other (AOI grid / contract tables / observation) | ≤1GB | — |
| **Total target** | **steady-state RSS ≤4GB (8GB machine)** | warning line 6GB / circuit-breaker line 7GB (process level, ops plane) |

**Three object-pool classes** (known hot spots; free-list pools hang on the owning thread — single writer, lock-free, zero-synchronization returns):

1. **Frame objects** (MsgPtr family): parameterized by per-tick message peak; pool exhaustion = take the trim/degrade path, **no fresh allocation** (OOM protection over dropped frames — same direction as the watermark cut).
2. **Attribute-collect buffers** (per-viewer batch assembly in the collect phase): reserved per (viewer × channel), reused across ticks.
3. **Send-ring slices**: the ring itself is pre-allocated (§5.8 send-ring submission semantics); pooling refers to slice handles/view objects.

- **Pool size = capacity parameter** (config constant, derived from the §1 target); allocated at startup, no growth at runtime — the memory ceiling is fixed at startup; no trading budget for throughput.
- **jemalloc/tcmalloc: not introduced** — the single-writer discipline + three pool classes have already converged allocation hot spots to countable places; a global allocator swap changes whole-process behavior (fragmentation policy/statistics/ops planes) and is same-family with the "no coexistence" discipline (§15.2) (a second allocator = a dual behavior face). Listed as an **M2+ benchmark-gated candidate**: initiate evaluation only if scenario benchmarks show allocation >5% of frame.
- Lua-side cap alignment: scripting-lua §6's table (memory/instructions/errors/infinite loops/isolation — five dimensions) is the script-domain counterpart; this table fills the C++ domain — the two tables together form whole-process memory discipline.

## 5. Benchmark Facilities (latter half of #5)

**Three forms**:

| Form | Content | Home | Gate use |
|---|---|---|---|
| a micro-benchmark | single mechanism: zstd threshold (the sdk-contract :395 debt) / RNG substreams / timer wheel / send-ring throughput | ctest-registered (tests/) | PR-level regression visibility (red on number degradation) |
| b scenario benchmark | N scenes × M entities × synthetic intent streams (deterministic-RNG derived — same source as battle-determinism §4; load reproducible, results comparable) | apps/bench (ops tool, apps→modules one-way; BW server/tools, KBE load nonexistent, self-built same-shape) | M2+ admission benchmark (before/after comparison), capacity acceptance (running the §1 profile) |
| c recording-replay | production-sampled intent sequences = **the same format as battle-determinism §5** — a recording is both replay-verification load and stress-test load (real packet shapes / real rhythms) | same batch as battle-determinism's P3 replayer | real-load calibration (packet-shape distributions synthetic streams cannot calibrate) |

- **Where "M2+ every item has a benchmark" lands** (the facilitization of net-abstraction §5.6's discipline): admission of M2+ items like the io_uring switch, bundle coalescing, allocator evaluation = form b's same-load before/after + significance criteria (merge only if tail-latency p99 improves ≥X%); benchmark topology (scene count/entity distribution/packet shapes) goes into config and the repo.
- **Environment discipline**: benchmark machine config declared (CPU model/kernel/NIC/NUMA balancing off); single-process runs (distributed stress testing established separately after the P3 fleet); **comparison baseline golden-ized into the repo** (against "compared with last local run" drift — same philosophy as contract goldens).
- **System-level acceptance metric set** (aggregating the scattered acceptance items of other designs + additions; the "capacity batch §5 benchmark" cited by clock-and-time §10's P3 is this table):

| Metric | Target | Source |
|---|---|---|
| Attribute change → client p99 / INSTANT | ≤150ms (NORM) / ≤1 tick | attribute-sync §11 |
| Single-client steady-state bandwidth | ≤80% of budget (100 on screen) | attribute-sync §11 |
| Skip rate / dropped_ticks | steady state <0.1% of frames; consecutive skips alarm | clock-and-time §4/§10 (newly quantified) |
| watermark soft+ trigger rate | steady state <1% of sessions·min (a trigger = an overload-anomaly signal) | net-abstraction §4.2 (newly quantified) |
| violation_score disconnect rate | steady state <0.01% of sessions·min | net-abstraction §4.3 (newly quantified) |
| Reconnect recovery / crash rollback | ≤1 RTT + snapshot (≤50KB) / ≤2s journal zero loss | attribute-sync §11 |
| Whole process-recovery chain time | kill -9 → re-report convergence ≤3s / mirror convergence ≤3.5s (skeleton two-run measured 1.3-1.8s / 2.3-2.8s, **full-load noise-period ceiling semantics** — load 75-90 machine-shared period; idle should be better; the dominant item = restart backoff 1s; RPO/RTO target-value tiering see current process-recovery behavior decision point 4) | current process-recovery behavior §6 batch-0 drill (scripts/drill_kill9.sh, 2026-10-09, i9-10880H local fleet) |
| Frame-time percentiles | tick segments p99 ≤70ms (§2 table total); 3 consecutive frames >80ms overload alarm | clock-and-time §4 (newly quantified) |
| Recording size / hash-chain divergence rate | ≤5MB per key battle / recomputation divergence = 0 (within a version) | battle-determinism §5/§8 (integration point) |
| RSS | steady state ≤4GB, alarm 6GB | §4 (this table) |

## 6. Phased Landing

- **P1**: capacity parameter set into config (CCU/pool sizes/budget-table constants/alarm lines) + first micro-benchmark batch (zstd threshold benchmarked — the sdk-contract :395 debt paid; send ring; RNG) + skip/watermark-trigger counters into MetricRegistry (metrics first, benchmarks later).
- **P2**: scenario benchmark v1 (apps/bench: synthetic intent streams + frame-time percentiles/memory/bandwidth sampling into MetricRegistry) + the three object-pool classes + first-round calibration of the frame budget constants.
- **P3**: recording-replay load integration (same batch as battle-determinism's P3 replayer) + capacity acceptance test (5000 CCU profile running the full §5 metric table) + the allocator M2+ evaluation point (if the benchmark initiates it).

## 7. Intersections with the Other Designs

| Related design | Landing |
|---|---|
| clock-and-time | source of the tick 10Hz semantics; skip-rate/gap_ticks metricization; §10 P3's "capacity batch §5" = this document's §5 metric table |
| attribute-sync | §11 acceptance table merged into this metric set; §5.1/§10 budget/truncation mechanisms reused; §8.2 write-amortization quantified (journal buffer budget) |
| net-abstraction | facilitization of §5.6's M2+ admission benchmark (this document's §5 form b); §4.2/§4.3 watermark/rate-limit trigger rates metricized; §5.8 ring capacity into the memory table |
| scripting-lua | §5.2 hook budget into the frame budget table; §6 Lua state cap numerified (64MB/state) alongside the C++ table |
| sdk-contract | §10.6 quantification semantics unified (old 20Hz → 10Hz); the :395 zstd-threshold benchmarking debt paid in this document's P1 |
| battle-determinism | §4 deterministic RNG = the source of reproducible benchmark load; §5 recording format = the replay-load format; §8 P3 metrics (divergence rate/size) integrated |
| architecture-review | the execution of 16.8.3's placement criterion: apps/bench (aggregation tool) ↔ modules detection primitives, two halves; benchmark goldening shares the contract-golden philosophy |

---
