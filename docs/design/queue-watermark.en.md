[English](queue-watermark.en.md) | [中文](queue-watermark.md)

# Acceptance Queue Watermark and Overload Prerequisite Design (Decision ② decision surface)

> Status: **prerequisite design draft (2026-10-10); all three decision points ruled 2026-10-10 (ADR-016: two-tier watermark / overload callback error codes / tick-budget anchoring)**. Positioning: after cell-single-writer Decision ① was ruled ADR-015 ((a) queued asynchronous acceptance), this is the decision-surface expansion of Decision ② "queue watermark and overload semantics" — pinning the three-way fork of watermark form, pinning the overload response, pinning the watermark numeric semantics; implementation details are out of scope. Genre follows net-abstraction §7 / battle-instance-offload / cell-single-writer. Authoritative semantics = net-abstraction §4.2/§5.7 (the watermark/backpressure parent piece) + cell-single-writer §5 Decision ② candidate lean (two-tier watermark, no silent dropping of old entries).

---

## 1. Baseline and Current State (A-level firsthand reads + parent-piece restatement, 2026-10-10)

**Parent-piece semantics (net-abstraction §5.7)**:

- The **four-level watermark** (§4.2, client side) serves the client-experience tradeoff that "low-priority traffic may be dropped"; **server-to-server never trims** — the drop eligibility of every internal message belongs to its message-class declaration, and RemoteEntityCall is non-droppable by default.
- **The offer return-code philosophy**: on BACK_PRESSURED the caller decides (queue cap / downshift / assert); the framework never decides for it.
- **The bounded-queue discipline**: "queueing while disconnected" and "backpressure" are two names for the same mechanism, and **both must be bounded** — unbounded queueing = deferring backpressure into a memory bomb.
- Per-Link watermarks are independent (a slow process drags only its own Link).

**The in-process acceptance surface (cell-single-writer §3(a)/§5)**:

- (a)'s costs already list "the queue watermark surface must be defined (aligned to the §5.6 backpressure-watermark semantics: reject / drop-old / watermark alert)" — this piece is the expansion of that surface.
- Decision ② candidate lean: two-tier watermark (high-watermark alert + at-cap rejection with an overload error response), **no dropping of old entries, no silence**.
- The calling surface is already the `sendRequestAsync` callback form (socket.hpp:96-100), and workerLoop is single-threaded (socket.cpp:124) — the evaluation surface is one depth read per enqueue, O(1).
- Shutdown sequencing: the G-3 mirror "cut the flow → drain the queue → gameThread join" (Decision ⑤ candidate lean; this piece does not handle drain, only records the intersection).

**Budget anchor**: tick 100ms@10Hz (clock-and-time §4, settled); the capacity §2.1 three-model measured anchor points are the same form (frame mean/peak/Sync per frame/budget share, i9-10880H under load 18-24 noise period).

## 2. Target Form (authoritative semantics)

The overload semantics of the acceptance queue have three parts: **observable** (alert when the watermark crosses the line) + **rejection** (an overload error returned at the cap, the caller decides for itself — the in-process version of the §5.7 offer philosophy) + **no dropping of old entries and no silence** (requests already at the head of the queue are unaffected by watermark actions; overload is always answered explicitly). Watermark numbers are anchored to the tick budget, not made-up constants.

## 3. The Three-Way Fork Expanded (watermark form)

### (a) Two-tier watermark — alert line + at-cap rejection

- **Semantics**: depth past the alert line → an alert row (a structured row into the log stream, cat=system level); depth at the cap → newly enqueued requests rejected (callback error code), existing queue-head entries keep running.
- **Cost**: the watermark numbers must be set (§4 Decision 3); the rejection handling surface on the caller side depends on the existing callback error semantics.
- **Relationship to §5.7**: the in-process version of "server-to-server never trims" — RemoteEntityCall is non-droppable by default, rejection = letting the caller retry/downshift, and the queue itself never silently drops old entries.

### (b) Four levels copied wholesale from §4.2

Evaluate all four levels by message class (low priority droppable / deferred aggregation / …).

- **Why not**: the four levels serve a client-experience tradeoff (priority triage); the in-process acceptance surface carries only RPC intents (cell write operations/queries), with no consumer family needing tiering — a wholesale copy imports complexity into the acceptance surface, against the "converged surface" intent of the single-writer closure. Moreover §5.7 has already ruled that server-to-server never trims.

### (c) No watermark, unbounded queue

- **Disproof**: unbounded queueing = a memory bomb (explicit in §5.7), deferring backpressure into a crash — worse than rejection; the G-1 packet-receipt enqueue precedents (death events / mirror packets / recovery re-reports) are also designed around bounded queueing. No.

## 4. Overload Response Form (Decision ② second question)

- **(A) Callback error code (leaning)**: the `sendRequestAsync` callback carries a typed error (overload=queue-full), zero new protocol — reusing §5.7's "typed reasons, no guessing on the caller's behalf" philosophy; the caller retries/downshifts/reports inside the callback, the same form as BACK_PRESSURED.
- (B) A new internal-domain message (overload error packet into the contract internal domain): an in-process single hop needs no wire surface; it is only needed if the M1 receiving end reuses the acceptance surface across processes — evaluate then with the M1 contract batch — **leave the seam, do not pre-spend it**.
- (C) Silent drop / silent reject: **disproof** — the Decision ② candidate lean states "no silence" in writing; silence makes the caller troubleshoot timeouts as network failures.

## 5. Honest Comparison

| Dimension | (a) Two-tier | (b) Four-level wholesale | (c) Unbounded |
|---|---|---|---|
| Observable (alert) | ✓ | ✓ | ✗ |
| Explicit rejection | ✓ at cap | ✓ (by class) | ✗ |
| No dropping old entries, no silence | ✓ | ✓ | ✗ |
| Message-class requirement | none | **mandatory** (no corresponding consumer family) | — |
| Complexity imported into the acceptance surface | minimal | high | zero (false solution) |
| Distance from §5.7 semantics | nearest (same form as server-to-server no-trim) | far (client semantics misplanted) | violates discipline (unbounded = bomb) |
| M1 receiving-end reuse changes | zero (watermark semantics same form) | redo (class surface discarded) | redo |

## 6. Decision Points (ruled 2026-10-10 / ADR-016)

1. **Watermark form**: **[Ruled: (a) two-tier]** (alert line + at-cap rejection) — (b) plants client semantics in-process, (c) the memory-bomb disproof (§3/§5).
2. **Overload response**: **[Ruled: (A) callback error code]** (zero new protocol, typed reasons); (B) contract new message left for M1-batch evaluation, (C) silence disproved (§4).
3. **Watermark numeric semantics**: **[Ruled: tick-budget anchoring]** — start from queue depth × per-request processing time ≤ a proportion of the tick budget (100ms@10Hz), take a conservative value for the static cap first, and calibrate on the capacity §2.1 three-model measured anchor form once the benchmark is wired into the acceptance surface. **[Calibration landed (2026-10-11, P3-2 batch D)]** — examples/benchmark --acceptance measured: the acceptance+execution queue sides combined cost ~0.1µs per intent (min of three runs, lower bound under load-noise conditions; the ~50µs starting assumption measured 500× better), full drain of a cap of 1024 entries at the cap ≈0.10ms, less than the 20ms message-segment budget (headroom ~194×) — the drain side is far from the binding constraint; the capacity side is the constrained dimension (at full capacity 5000 CCU steady state ≈2500 intents/tick = 244% of cap, saturation lands on the rejection path by design); the numbers keep their starting values (cap 1024 / alert line 768), and the anchor points were written back into acceptance_queue.hpp and capacity-and-benchmark §2.1.

## 7. Path After the Ruling

```text
水位与过载批（功能代码，随受理面批同批或紧随）——告警行结构化、
顶格拒收、回调错误码；静态顶格起步值 → benchmark 校准（已完成：
P3-2 批 D 实测锚定，数值保持，详 §6 拍板 3 校准注记）
裁后回写 cell-single-writer §5 拍板 2（已拍板标注）
```

## 8. Intersections with the Other Designs

| Piece | Relationship |
|---|---|
| cell-single-writer §3(a)/§5 Decision ②/§6 | The parent piece and the decision object — this piece is the expansion of its "watermark surface must be defined"; the second segment of the post-ruling path is this piece landing |
| net-abstraction §4.2/§5.7 | Source of the watermark/backpressure semantics (four-level watermark, offer return codes, bounded-queue discipline) |
| net-abstraction §5.6 | The M1 ladder — the watermark machine was set in stone at M0 ("watermark machine" listed as an M0 deliverable); the in-process queue is its first in-process consumer |
| ADR-015 | The decision proper — this piece is the supplier-of-rulings for its suspended Decision ② surface |
| clock-and-time §4 | The 100ms@10Hz budget anchor |
| capacity-and-benchmark §2.1 | Precedent anchor for watermark number calibration (three-model measurement) |
| session-and-online-directory §6 | Same form as the G-1 packet-receipt enqueue precedent (bounded-queue design from the same source) |
| battle-instance-offload | The spawn-request control surface reuses the acceptance form (ADR-012) — the watermark semantics apply in the same form |

## 9. Evidence Source List

- A-level (2026-10-10 local firsthand reads / parent-piece restatement): modules/protocol/include/apollo/protocol/socket.hpp:96-100 (sendRequestAsync callback form), modules/protocol/src/socket.cpp:124 (workerLoop single-threaded), docs/design/net-abstraction.md §4.2/§5.6/§5.7 (four-level watermark / server-to-server no-trim / offer return codes / bounded-queue discipline), docs/design/cell-single-writer.md §3(a)/§5 Decision ②/§6, docs/design/clock-and-time.md §4, docs/design/capacity-and-benchmark.md §2.1, docs/design/session-and-online-directory.md §6
- B-level (in-repo docs): docs/todo.md P3-2 batch B (Decision ① ruled ADR-015)

*Baseline: apollo main @ a348b7e1. This piece is a prerequisite design — all three decision points are ruled (ADR-016, 2026-10-10); the candidate lean is the ruling result.*
