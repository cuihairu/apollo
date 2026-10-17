
# Apollo Time and Clock Model (clock-and-time)

> Status: design draft (under review). Goal: pin down the three forms of "time" in apollo and each one's forbidden zones — the monotonic clock only measures, the tick number is the only ordering for decisions, the wall clock is only for human readability — and answer tick scheduling (fixed step/catch-up/pause), the client time-sync boundary, cross-process clock non-trust, and the timer's time interface. Cross-referenced with `docs/design/attribute-sync.md` (§10 six phases/§5.1 budget/§8.2 amortized writes — all scaled by this file's tick number; §10.1 already argues "why tick, not message-driven" G-7, not repeated here), `docs/analysis/ssengine-reference.md` §4.3 (timer wheel), and `docs/analysis/mmo-mechanism-deep-dive.md` §8 (fixed-tick and thread fleet: three-family precedents).

---

## Executive Summary

1. **The three-clock division of labor is the entirety of this design**: `steady_clock` (monotonic clock) — **the only measurement entry** (frame time/timeout/backoff/budget-refill duration); the **tick number** (uint64 logical clock) — **the only ordering for world-state decisions** (attribute seq container/timer scale/journal site anchor/budget-collection cadence); `system_clock` (wall clock) — **the only human-readable, cross-system alignment** (log lines/persisted created_at/GM audit/reports). Each clock has its forbidden zone: wall differences never measure duration (NTP jumps produce negative values), tick never converts to real time (replay semantics), steady never enters game decisions (cross-client inconsistency).
2. **Tick numbers decide; timestamps never decide**: replay determinism (replay = seed + an input sequence aligned by tick — battle-determinism's timeline is exactly this), NTP/leap-second immunity, monotonic with no regression, zero cross-client clock-sync cost. BW/KBE are all tick-paced (`DEFAULT_GAME_UPDATE_HERTZ = 10`, lib/server/common.hpp:19; `gameUpdateHertz 10`, kbengine_defaults.xml:5 — deep-dive §8 measured).
3. **Fixed step (design scope 100ms/10Hz, implementation default 50ms/20Hz), a process-level single TickSource**: the main loop = a message-processing segment + a tick segment (BW time-slice pattern, cellapp.cpp:946-948); catch-up is bounded (if behind by ≤ catch_up_max, run consecutive ticks to catch up; beyond that, skip ticks + alarm + record dropped_ticks — unbounded consecutive running that starves IO is not allowed); the tick number is monotonic within a process, resets to zero on restart, and cross-restart continuity is carried by the journal's persistent sequence number. The implementation default is tickRateMs=50 (20Hz); the design scope of 100ms (10Hz) awaits calibration.
4. **Clock injection discipline**: game logic never calls the OS clock directly — only through the read-only ClockService facade injected at construction via core::di (three clock readings + tick count); unit tests/replay inject a fake clock. The `GameClockService` at `apps/game-server/src/main.cpp:20` is currently a demo shell (just `std::string name = "clock"`, no clock semantics at all) — to be rewritten per this design at the code stage or removed with the demo code; the design does not depend on its current state.
5. **Client time is display-only**: the tick number is delivered with snapshots/key messages (client progress display/interpolation alignment); the time-sync protocol is P3 and affects only the display layer; the movement channel's "timestamp, take the newest" semantics unchanged (net-abstraction §4.1). Any proposal that "the client clock participates in server decisions" is rejected outright per this design.
6. **Cross-process clocks do not trust each other**: inter-process "who came first" comparisons always use seq/site/epoch (net-abstraction §5.7's handshake triple already carries authority_epoch; the G-2 takeover anchor = the journal site, not a point in time). Time values are never compared across processes — NTP skew between two machines can reach milliseconds, while the tick decision granularity is 100ms.

---

## 1. Current-State Inventory (code-reading conclusions)

| Piece | Location | Current state | Verdict |
|---|---|---|---|
| `apollo::base::Time` | modules/base/include/apollo/base/time.hpp:12-60 | steady ms/µs/ns (:16-29) + `unix_time` (:31-33) + formatting family (:35+) — monotonic/wall already separated | **keep as the primitive layer**; missing the tick clock and the injection shape |
| `GameClockService` | apps/game-server/src/main.cpp:20-22 | `struct { std::string name = "clock"; }` — pure demo shell; `LoginPipeline` holds a reference (:24-27) only to demo DI assembly; tick() just increments a counter (:51-53) | rewrite as a real facade at the code stage or remove with the demo (registered: next code batch) |
| legacy timer wheel | include/apollo/core/timer/{timer.h,timer_manager.h} + src/apollo/core/timer | C-23/C-24 already ruled dead (never fires past 1 tick/unlocked-unlock UB/cross-thread races) | delete-first (already noted in the register); the new wheel's time interface belongs to this design §8 |
| `include/apollo/utils/time.h` | utils | ported piece (old tree) | handled with the attribute/legacy-tree convergence batch; no new dependencies |
| tick scheduling | none | the main loop has a fixed step (world_app.cpp:421 gameLoop); implementation default 20Hz (tickRateMs=50), design scope 10Hz awaiting calibration | newly built by this design (§4) |

Negative space: none of the six design documents contains "clock/monotonic clock/steady_clock/time sync" in writing (gap inventory #1 evidence); BW/KBE likewise have no standalone clock design (deep-dive §3/§8 shows only the updateHertz constants) — **this document is self-defined beyond the precedents; the precedents provide only the "10Hz + tick pacing" shape as corroboration**.

## 2. The Three-Clock Division of Labor (core decision table)

| | Monotonic clock steady_clock | tick number (logical clock) | wall clock system_clock |
|---|---|---|---|
| Essence | OS monotonic clock (the `Time::now()` family, time.hpp:16-29) | a uint64 counter, process-level single-source increment | OS calendar clock (`Time::unix_time()`, :31-33) |
| Semantics | "how long has passed" | "how many beats has the world advanced" | "what time is it in human society" |
| Sole legitimate use | measuring: frame time, timeout decisions, reconnect backoff, token refill duration, performance counters | decision order: attribute seq advancement, ACK timeout ("3 broadcast ticks", attribute-sync §3.3), priority delay (INSTANT=0/LOW=20 ticks, §5.2), timer deadlines, journal site anchor, write-behind per-tick quota dequeue (§8.2), Lua instruction budget per frame | display: log-line timestamps, persisted created_at, GM audit "who when", operations reports, directory naming |
| Forbidden zones | game decisions (cross-client/cross-process inconsistency); persistence alignment | conversion to real time (the tick-time mapping exists only inside the scheduler, never leaks out); client display clock | measuring differences (jumps/leap seconds produce negative values or jumps); any game-logic branch |
| Jump behavior | immune (monotonic guarantee) | immune (self-incrementing) | accepted (owned by OS/ops; this layer does not correct) |
| Cross-process | not mutually trusted | not mutually trusted (each process counts independently; cross-process order uses seq/epoch, §6) | not mutually trusted (NTP skew can exceed one tick granularity) |

Three usage mantras (written into code-review red lines, corresponding to architecture-review's "discipline assertion" family):

1. **Measure duration — steady only** — `Time::now()` differences are the repo-wide sole legitimate source of duration; a `time(nullptr)` difference appearing in a diff is a defect.
2. **Make decisions — tick only** — any clock reading appearing in the branch conditions of attributes/timers/budgets/sites is a defect.
3. **For humans — wall only** — log and persistence lines carry wall; the same line also carries the tick number (§7 dual-write), but the two duties are never interchanged.

## 3. The Tick Number: the Sole Decision Order

- **Production**: a process-level single `TickSource` (attached to the main loop, §4) increments by one per step; uint64 @10Hz overflow is impossible (58.4 billion years). The first complete tick after process start counts as 1; 0 is reserved as the "not started/invalid" sentinel.
- **In-process broadcast**: multiple scene threads consume **the same tick value** — each scene thread has its own timer wheel/budget instances, but they share the global tick scale. Rationale: journal sites, budget statistics, and G-5 frame-time aggregation must be comparable across scenes; per-scene self-counting would give "which beat of this process" multiple answers, complicating every ops query and replay alignment. BW's same-process single updateHertz pace is the same pattern.
- **Lifecycle**: monotonic within the process; **resets to zero on restart**; cross-restart world-timeline continuity is carried by the persistence layer — the journal's persistent sequence number (attribute-sync §8.2) is the cross-restart anchor; the tick number is only an in-process scale. After a client resumes, tick re-syncs (the current value is delivered with the enter-view snapshot).
- **The tick-time mapping does not leak**: internally the scheduler uses the steady clock to decide "which beat to run", but that mapping (when to accelerate/skip) is an implementation detail; any module converting "tick × 100ms" into wall time is in breach — when ops needs real time, read wall directly.

## 4. Tick Scheduler: Fixed Step, Bounded Catch-Up, Pausable

**Shape** (attached to the modules/runtime main loop, BW/KBE pattern — messages processed on arrival + fixed-cadence advancement, both coexisting):

```
loop:
  process_messages(until next_tick_deadline)   # message segment: uplink dispatch/async callbacks (attribute-sync §10 phase 0)
  tick = TickSource.next()                     # decide the tick boundary has arrived (steady clock's sole use: computing deadlines)
  for scene in scenes: scene.tick(tick)        # each scene thread advances per §10's six phases (phases 1-6)
  sleep_until(next_tick_deadline += step)      # yield the remaining budget
```

- **Step**: default 100ms (10Hz) — BW `DEFAULT_GAME_UPDATE_HERTZ = 10` (common.hpp:19, registered at server_app_config.cpp:28), KBE `gameUpdateHertz 10` (kbengine_defaults.xml:5), the two families agree; the step goes into configuration (core::config), **never changed at runtime** (changing the step = changing the semantics of every system-wide timeout constant; that goes through a restart).
- **Bounded catch-up**: after a beat overruns (e.g. breakpoint/long GC), if behind by ≤ `catch_up_max` (default 2 beats), run consecutive ticks to catch up — preserving the "tick semantics ≈ 100ms of game world" approximation; if further behind, **skip ticks**: TickSource jumps straight to the current beat, the skipped beats are recorded in a `dropped_ticks` counter (reported by G-5 detection primitives) + a WARN log. **Unbounded consecutive running is not allowed** — catching up 20 beats starves IO for a full RTT; skipping lets the world "slow down" rather than "freeze".
- **Pause (tick pause ≠ process pause)**: three legitimate pause points — (1) online-debug eval/state sampling (scripting-lua §7, tick-boundary sandbox); (2) the script hot-replacement atomic table-swap window (scripting-lua §3.2); (3) manual ops suspension. While paused, TickSource stops while wall keeps running; on resume **no catch-up** (record `gap_ticks = wall difference / step` for observation) — a world modified by the debugger must not fast-forward instantly through catch-up. The shutdown sequence (attribute-sync §10.2) does not go through the pause path — shutdown must flush the journal clean; pause only freezes.
- **Frame-budget monitoring**: each beat's steady difference (six-phase time) goes into G-5 detection primitives (frame time/queue watermark family); "N consecutive beats over 80% of budget" is an overload signal, wired to the admission gate (BW LoginConditions pattern).

## 5. Client Time: Display Alignment, Never Decisions

- **Tick delivery**: the enter-view snapshot and key control messages carry the current server tick number; the client uses it for — progress/cooldown display alignment, replay alignment (#2 battle determinism shares this axis), and a "how many beats behind" self-check (movement interpolation drift detection).
- **Time sync (P3, optional)**: handshake/heartbeat carries a server wall timestamp; the client computes an RTT/2 offset — **the product goes only into the display layer** (chat timestamps, event countdowns). Zero consumption on the decision plane: AOI interpolation uses the local clock + the server position sequence (movement carries "timestamp, take the newest", net-abstraction §4.1 — that timestamp is an "ordering" use, not wall alignment).
- **Red line**: no time value reported by the client enters server decisions (anti-cheat surface: the client clock is fully controllable). Uplink carries only seq/intent, never "what time I think it is".

## 6. Cross-Process Clocks: No Mutual Trust

- The comparison primitives already exist, without using time: inter-process before/after = seq/site (InterServerLink ReliableEvent resumption, RO_MIRROR journal-site ack); "did the peer restart" = authority_epoch mismatch (net-abstraction §5.7 handshake triple); "where does takeover resume" = the journal site (G-2).
- Cross-process messages carrying a tick number (an optional envelope field, InternalMessageEnvelope — net-abstraction:351 envelope family) serve **observation alignment only** (lining up two processes' log lines by tick when investigating), and never participate in routing/retry/takeover decisions.
- The cluster introduces no global clock service (NTP/PTP precise synchronization is outside the design surface): scenarios needing "global before/after" go to a single-point owner for ordering (G-1 fleet events sequenced through the mgr) — the time version of the single-writer discipline.

## 7. Dual Persistence Timestamps

journal rows / snapshot rows / GM audit rows uniformly dual-write **(tick, wall_ms)**:

- tick = replay positioning and site semantics (journal replay range is by site); wall = ops queries ("what happened at 3am last night" via direct SQL, no tick conversion needed).
- The two are never interchanged: deriving sites from wall (a clock jump misaligns) or converting wall from tick (pause/skip drifts) are both breaches.
- BW precedent corroborates the maturity of "tick as the budget unit": BackupSender carries a fractional remainder across ticks (backup_sender.cpp:114-117, `numToBackUpFloat = bases.size()/periodInTicks + backupRemainder_`) — the cadence conversion exists only inside the mechanism, the same discipline as §3's "the mapping does not leak".

## 8. The Timer Wheel's Time Interface (connecting to ssengine-reference §4.3)

- **The deadline unit = tick number, not milliseconds**: `after(n_ticks)` / `at(tick)`; the wheel's slots are scaled by the tick step (skynet's five-layer wheel on a 2.5ms grid is the companion of an independent-thread paradigm — apollo's wheel hangs on the main loop, so its scale is naturally the tick). The ms→tick conversion is done by the **caller** at registration (`after_ms(x)` is `after(ceil(x/step))`, rounding up — better a beat late than a beat early), and no clock ever appears inside the wheel.
- **Expiry callbacks run at the owning scene thread's tick boundary** (§4.3 as established); the callback signature carries the tick value — inside the callback, "which beat is it now" reads the parameter, not a clock.
- While paused, the wheel freezes (the scale is tick; §4 pause = the wheel stops); after resume, deadlines are unchanged (the world slows down; timer semantics follow the world, not the wall) — the same semantics as BW's `secondsToTicks` conversion (baseapp_config.cpp:84 backupPeriodInTicks).
- The legacy core/timer is not wired to this interface before its deletion (C-24 already ruled dead).

## 9. Intersections with the Other Designs

| Related design | Landing point |
|---|---|
| attribute-sync | §10 six phases/§5.1 token bucket/§5.2 priority tick table/§8.2 per-tick amortized writes — all consume this file's tick number; §10.1 (G-7 paradigm argument) is the prerequisite of this document's §4, not repeated |
| ssengine-reference §4.3 | timer-wheel deadline = tick number (this document's §8 supplies the time interface); game-loop drive ownership unchanged |
| net-abstraction | §3 thread model: tick-boundary flush follows the tick boundary; movement's "timestamp, take the newest" is ordering semantics, not clock alignment; InterServerLink/envelope optional tick field (§6) |
| scripting-lua | §3.2 hot-replacement window = a legitimate pause point (this document's §4); §6 instruction budget per frame = per tick; §7 debug eval at the tick boundary, pause semantics defined here |
| logging | log lines dual-write (wall, tick) — the crash-forensics kit's logs can be lined up with journal sites |
| sdk-contract | snapshots/control messages carrying the tick number = contract fields (finalized in P2 when delivered to clients; into messages.xml internal/client domains) |
| battle-determinism (gap #2, with the battle batch) | the replay timeline = this file's tick number; the four determinism constraints take "a single decision order" as the first |
| architecture-review | the GameClockService rewrite/cleanup and the legacy core/timer deletion are registered in the code-impact items (§16.10.2 register, appended to existing rows) |

## 10. Phased Landing

- **P1**: `TickSource` + the `ClockService` read-only facade (modules/base — above the Time primitives, injected at construction via core::di; game-server's demo rewritten as a real assembly); the main loop's fixed step + bounded catch-up/skip land in modules/runtime (ServiceHost run_once upgraded); the timer wheel's new interface (deadline=tick) goes with ssengine §4.3's implementation batch; the three mantras enter the code-review red lines.
- **P2**: the tick number delivered with snapshots/control messages (contract field finalized); client display alignment (movement interpolation self-check).
- **P3**: the time-sync protocol (optional, display layer); cross-process tick observation alignment (envelope optional field); the G-2 takeover/journal-site and tick dual-write database verification (the capacity batch §5 baseline includes a "tick-skip rate" metric).

---
