# Time and Clocks

Reuse Time, WorldHost, and TimerWheel, separating duration measurement, simulation order, and display time without another synonymous clock service.

## Executive summary

Use monotonic time for durations and timeouts, ticks for simulation order, and wall time for readable records. Cross-process authority uses epochs, sequences, and durable records rather than comparing machine clocks.

## 1. Current implementation

| Component | Responsibility |
|---|---|
| `apollo::base::Time` | Monotonic readings, wall time, and formatting helpers |
| `WorldHost` | Fixed steps, injected monotonic source, bounded catch-up, and dropped_ticks |
| `WorldTickContext` | tick_index, delta_seconds, and local monotonic instant |
| `apollo::base::TimerWheel` | Single-writer wheel advanced by caller-supplied monotonic milliseconds |

The example GameClockService is an assembly demonstration, not a delivered framework clock facade.

## 2. Three time domains

| Time | Purpose | Restriction |
|---|---|---|
| steady | Durations, timeouts, backoff, budget refill | Not a comparable cross-machine timestamp or deterministic gameplay input |
| tick | Input attribution, simulation order, replay | Interpreted within its process/activity; independent World ticks are not comparable |
| wall | Logs, audits, presentation, reports | Not timeout arithmetic or authority ordering |

Do not format a steady-clock epoch value as a real wall-clock date.

## 3. Fixed steps

WorldHost defaults to 10 Hz. WorldApp defaults tickRateMs to 50, or 20 Hz. These are defaults at different configuration entry points, not two simultaneous simulation schedules.

WorldHost catches up at most 5 steps per call and records excess backlog as dropped_ticks, avoiding unbounded catch-up. Constructor injection supports deterministic clock tests. State the actual tick configuration in capacity evidence.

## 4. Pause and execution boundaries

Distinguish paused simulation, stopped admission, and process shutdown. Heartbeats, admission, drain, and async completion during a pause require explicit application policy. Stopping ticks alone does not establish availability.

Script commits, input consumption, and background completion run at the owning logic boundary. Completion timing cannot change a finished tick's decisions.

## 5. Client time

Client timestamps support presentation, interpolation, and defined experience policies. Server authority determines execution order. Client-reported time does not grant write authority or permission to rewrite history.

## 6. Cross-World ordering

Worlds keep independent clocks. Transfers use process incarnation, player authority epoch, transfer identity, and conditional directory commit. Timeout triggers reconciliation; it does not prove absent destination commit or authorize source writes automatically.

## 7. Logs and persistence

Logs include wall/tick context. Journal sequences, settlement IDs, and authority epochs provide recovery and idempotency. Readable timestamps cannot replace them. Local ticks may restart; business order across restart needs durable evidence.

## 8. Timers

The owning thread calls TimerWheel.advance(now_ms). The wheel does not read OS time or create threads. Defaults are 10 ms precision, 64 slots per level, and 5 levels; hosts choose appropriate configuration.

Schedule/cancel and callbacks follow single-writer ownership. Callbacks can schedule/cancel other work but cannot reenter advance. Object destruction must cancel related work or validate object generations before callbacks touch an expired Instance.

The current API uses monotonic milliseconds, not World tick IDs. Hosts must choose and convert explicitly and validate pause/catch-up semantics.

## 9. Related rules

- [Attribute synchronization](./attribute-sync.md): input, hooks, and output boundaries.
- [Lua](./scripting-lua.md): commit and asynchronous completion.
- [Cross-server scenes](./cross-server-scenes.md): authority commit and timeout reconciliation.
- [Crash capture](./crash-capture.md): build and failure evidence correlation.

## 10. Acceptance

Validate injected clocks, zero-instant startup, fixed steps, catch-up limits, dropped ticks, backwards input, and cancellation. Test reconnect, drain, and transfer with independent clocks instead of using similar local timestamps as protocol evidence.
