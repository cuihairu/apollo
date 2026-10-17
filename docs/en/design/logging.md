
# Apollo Runtime Logging and Crash Forensics Design (logging)

> Status: design draft (under review). Goal: answer "how key error information survives across all three states — everything healthy / the logging system itself failing / process crash" — per-process local logs (**source of truth**), central collection process (**optimization**), crash forensics chain (**last line of defense**). Related: `docs/analysis/architecture-review.md` (C-25/C-26 logging status audit, G-5 observability channel, C-32 signal-handling cautionary example), `net-abstraction.md` (G-5 two-segment placement), `scripting-lua.md` (§6 script error audit boundary), `attribute-sync.md` (§8.2 BI triage, §10.2 shutdown order).

---

## Executive Summary

1. **The local file is the source of truth; collection is an optimization**. Every process writes its local log file first, unconditionally; if the central collection process (apps/logger, P3) dies, only "aggregated search" is lost, not the evidence — each process degrades to local-only writes, zero business impact. Precedent: KBE `error_msg` dual outlets (remote logger push and local console run **in parallel**, debug_helper.cpp:973-986).
2. **Tiered persistence guarantees (QoS for logs)**: FATAL = format then immediate `write`+`fsync`, **must not continue down the crash path before returning** (persist to disk before abort); ERROR = **bypass direct-write channel** (does not enter the shared queue — cannot be squeezed out even when the queue is stuffed full by lower levels) + immediate flush; WARN batched at 100ms; INFO/DEBUG batched and may drop. Same mode of thinking as the attribute-sync bandwidth budget: who gets budget, and who is non-droppable, is declared, not assumed.
3. **Startup-order discipline: if logging cannot start, the process does not start**. logger is the first hosted service to start; on failure the process exits (precedent skynet: logger is the first service created, failure `fprintf(stderr)+exit(1)`, skynet_start.c:287-291). A process whose logging is unavailable is not allowed to run — this is the first clause of "guarantee key information is left behind": there must be no window of "running sick, no evidence afterwards".
4. **The crash path depends on no living subsystem**. The moment of crash is exactly when heap/locks are most likely already blown up by the same fault — the evidence-preserving facility must be a "dead facility": `sigaltstack` alternate stack + preallocated static buffers + `backtrace()` + raw `write(2)` to a separate crash file (not through the logger pipeline, no malloc, no lock taking), then restore the default handler and re-raise to guarantee core production. Two same-pattern precedents: KBE signal_handler (SIGABRT/SIGBUS/SIGSEGV list signal_handler.cpp:22-27 + registration :112, backtrace debug_helper.cpp:1177-1180), BW signal_processor (:29-34 list) + callstack_linux.cpp:228.
5. **core dump is the last line of defense**. RLIMIT_CORE=unlimited (setrlimit early in main) + core_pattern on an independent partition/pipe + disk-quota rotation + **CI verification** (a test process deliberately raises SIGSEGV; asserts crash file and core production) — the crash path is a tested path, not "find out the config was wrong only when disaster strikes".
6. **Consolidating the status quo**: C-25 four logging stacks coexisting (① utils::logging ② core/log built-in ③ modules/core/log ④ in-memory version) + same-path dual-header ODR trap, consolidated to **one stack: modules/core/log**; C-26 orphan TU (global_log_manager has no compilation home; default-build link failure reasoned out) is the first cut. **Develop a thin layer in-house (a few hundred lines); do not adopt spdlog/log4cxx** — tiered bypass and the signal-safe crash surface are our own semantics; third-party logging libraries do not guarantee them for you (KBE's local layer adopts log4cxx, visible under conditional compilation at debug_helper.cpp:978-980; apollo does not take this path).
7. **Each link of the degradation chain depends only on more primitive facilities**: logger thread dead/queue full → ERROR bypass direct write; filesystem failure/disk full → stderr (a redirected pipe can also receive it) + stop low levels to preserve high levels; process crash → crash-handler four-piece set + core; whole machine dead → local-disk logs and core are reported as death events by a machined-style guardian (G-1 wiring), then fetched by ops.
8. **Alert outlet and webhook default implementation (§5.2, added 2026-09-30)**: two tiers — Tier A standard stack (Prometheus alerting rules + Alertmanager + community adapters, zero code in repo) is the production recommendation; Tier B exporter built-in threshold notifier is the fallback for small deployments (rule-table driven + four bot templates: WeCom/DingTalk/Feishu/Slack = JSON template + one POST, reusing net-abstraction §5.10 curl). Six rules of sending discipline: failed retries fall back to local write, silence-window aggregation, global rate cap, secrets in env, outbound target whitelist (SSRF), closed set of template variables; **game processes never send webhooks directly** (extension of the three bans; the HTTP client lives only in the exporter).

---

## 1. Problem Definition: Three Failure Depths

| Depth | State | Guarantee mechanism for key information |
|---|---|---|
| 0 Normal | All subsystems alive | Tiered buffering + local file + (P3) collector aggregation |
| 1 Logging subsystem failure | Queue full / logger thread stuck / disk full / collector down | ERROR bypass direct write (not through the queue); disk watermark stops low levels; collector down → local-only writes |
| 2 Process crash | Heap corruption / deadlock / stack overflow — **the moment evidence is needed most is exactly when every facility is least trustworthy** | crash handler (static buffer + raw write) + core dump — neither depends on heap, locks, or the logger thread |

The design principle reads straight off the table: **each additional depth of failure must leave the evidence mechanism dependent on fewer facilities**.

## 2. Three-Frame Precedents (source-code evidence)

| Mechanism | BigWorld | KBEngine | skynet |
|---|---|---|---|
| Central collection | Standalone logger process + per-process `logger_endpoint` push (architecture-review §16.2); **push side carries its own degradation semantics**: reconnect fails at most 3 consecutive times (logger_endpoint.cpp:672-703) + send buffer beyond maxBufferedSize is dropped with an error — BW's original form of "keep local when the collection channel is unavailable, don't drag the process down"; server/tools/message_logger archive tool (§16.8.2 table) | logger as an independent component process (kbe/src/server/tools/logger/: logger.cpp + logger_interface.{cpp,h} receiving side); caller `ERROR_MSG` macro → DebugHelper (debug_helper.h:217-218) | logger is the first service in the node (service_logger.c, writes file + FILE* handle :9-27) |
| Local fallback | console/file (log_msg system) | **Dual outlets**: `error_msg` = onMessage push to logger **in parallel with** printf to local console (debug_helper.cpp:973-986) — remote down, local still on | logger service is itself local (in-node); startup failure exits immediately (skynet_start.c:287-291) |
| Crash handling | signal_processor (SIGABRT/SIGBUS/SIGSEGV etc., :29-34) + callstack_linux backtrace (:228) | signal_handler.cpp:22-27 list + :112 registration; `backtrace_msg()` (debug_helper.cpp:1177-1180, glibc backtrace) | — (crash left to OS + monitor-thread version alarm, skynet_monitor.c:31-45, architecture-review §16.2) |
| Common shape | **Detection/evidence in-process; aggregation and presentation in an outboard process** — same line as G-5 two-segment placement | same | same |

apollo's cautionary example is in its own repo: gateway main.cpp:14-19 signal handler calls `stop()` directly (join threads + close socket + take locks) — not async-signal-safe (architecture-review C-32 already recorded); this design lists that as a crash-surface red line.

## 3. Per-Process Logging Subsystem (normal path)

### 3.1 Tiers and Persistence Guarantees

| Level | Buffering and persistence | Loss tolerance | Notes |
|---|---|---|---|
| FATAL | format → `write`+`fsync` **synchronously complete** → only then may the abort/exit path proceed | **zero** | "Persist before dying" is a hard order, not best-effort |
| ERROR | **bypass direct write**: does not enter the shared ring queue; direct O_APPEND append + immediate flush | **zero** (for the entire lifetime of the process) | The bypass is the dividing line between this design and the "all levels share one queue" approach: when the queue is stuffed by an INFO flood, ERROR still gets out |
| WARN | batched (100ms or 4KB trigger) | tolerable within a crash window | |
| INFO/DEBUG | batched + ring overwrite (drop oldest when full) | droppable | Low-level yielding is explicit policy (when the disk watermark triggers, DEBUG/INFO stop first) |

### 3.2 Thread Model

```
Business threads (incl. scene threads): lock-free writes to an MPSC ring (reuses utils/loop_buffer.h — same shape as the net send ring)
                        ERROR: bypass direct-write channel (business thread does its own write(2), atomic O_APPEND append)
logger thread (1): batch dequeue → format → local file; (P3) dual-outlet push to collector
Crash path: passes through none of the above (§4)
```

- Lock-free discipline is isomorphic to net-abstraction §3: business threads are producers, the logger thread is the sole consumer; log formatting (timestamp/level/thread name) happens on the logger thread — **business threads only copy bytes**, zero formatting cost on the hot path.
- Cost model of the ERROR bypass: low frequency (errors are by definition exceptional), one syscall per record is acceptable; the return is "queue state decoupled from ERROR reachability".

### 3.3 Startup and Shutdown Order

- **Startup order**: logger is the first hosted service to start (first in the IHostedService sequence); init failure (file won't open / permissions / disk full) → process exit (skynet_start.c:287-291 precedent). Crash handler and RLIMIT_CORE are installed even earlier, at the main entry (before thread creation).
- **Shutdown order**: in the reverse-module stop of attribute-sync §10.2 phase ⑤, logger is the **last stop** — drain buffers then close files (the last batch of logs on the shutdown path is exactly the most critical: each module's stop errors must reach disk).

## 4. Crash Forensics (Last Line of Defense)

### 4.1 Degradation Chain Overview

| Failure scenario | Behavior | Facilities depended upon |
|---|---|---|
| collector down (P3) | disconnect detection → local-only writes → reconnect restores | local file |
| logger thread stuck / queue full | below WARN drop oldest; ERROR unaffected via bypass direct write | O_APPEND fd |
| disk full / filesystem error | ERROR bypass reroutes to stderr; watermark alarm; DEBUG/INFO fully stopped | stderr fd (pre-opened) |
| process crash (signal) | crash handler writes crash file → re-raise → core dump | static buffer + raw write |
| whole machine dead | local-disk logs and core persist; machined-style guardian reports death event (G-1, net-abstraction §7 P3 prerequisite design) → ops fetches | local disk + external guardian |

### 4.2 Crash-Handler Four-Piece Set

1. **sigaltstack**: handler runs on a preallocated alternate stack — stack-overflow SIGSEGVs still have a stack (installed per-thread at the thread-creation wrapper in modules/base, one unified install site).
2. **Preallocated static buffer + preformatted fields**: process name/version/build time/signal/address/thread name + a dozen-or-so raw addresses from `backtrace()` — **no online symbolization** (offline addr2line; the do-not-strip-symbols discipline was already set in scripting-lua §7.3); backtrace availability in signal context is glibc practice standard (same usage in both KBE/BW precedents).
3. **Raw `write(2)` to a separate crash file** (`crash-<pid>-<time>.log`): not through the logger pipeline, no malloc, no lock of any kind, no touching stdio buffers — physically separate from the main log file (the main log fd may already be in a bad state).
4. **Restore default handler + re-raise**: after the handler finishes writing, `signal(sig, SIG_DFL)` + `raise(sig)` — guarantees a core dump is produced (swallowing the signal inside the handler = losing the core, the most common self-inflicted wound).

Red line (review-grade): the crash path forbids malloc/locks/stdio/thread join — C-32 (gateway signal handler calling stop()) is the recorded counter-example; it must not recur.

### 4.3 Core Dump Ops Surface

- **RLIMIT_CORE=unlimited**: setrlimit early in main (deployment-environment ulimit cannot be assumed).
- **core_pattern**: directory on an independent partition (or systemd-coredump pipe) — not on the log disk, let alone the data disk; disk watermark checks + rotation by count/age (cores cleaned after upload/archive).
- **CI verification**: a test target deliberately `raise(SIGSEGV)`s; asserts crash file contents (including backtrace frames) and core production — the crash path is a tested path.
- core upload/archiving belongs to apps/ ops tooling (same batch as machined death reporting, G-1 wiring).

## 5. Central Collection Process (apps/logger, P3) and the BI Boundary

> 2026-10-10: the landing prerequisite design for ADR-013's initiated split is set down as [logger-app](./logger-app.md), and **L1+L2+L3 are delivered** (seven-app dual-outlet wiring + switch flipped + logger shell scan/follow + push seam reserved; batch record in the ADR-013 landing notes). v1 collection shape = tail pull, decided. L4 (push channel + disconnect degradation) hung on M1.
>
> 2026-10-12: **L4 delivered** (push channel + disconnect degradation in full form; batch record in [logger-app](./logger-app.md) §6 / todo P3-3) — sending side LogPushClient[bounded queue 4096, drop-new at cap, kReliable reconnect-exactly-once redelivery, stop-drain keeps queue] × receiving side logger serve[passive fan-out receives 930, original-line archive flushed line by line] × wiring APOLLO_LOG_PUSH_ADDR[unset = off, v1 semantics unchanged]. §5's three bans all held: one-way no receipt, collection never a runtime dependency, local file always source of truth; fleet live-fire drill (SIGKILL collector+gateway → machined relaunch → redelivered items all archived) landed as empirical proof.

- **Shape**: independent collection process (apps/logger); each process's logger thread has dual outlets — local file (source of truth) + push to collector (via net-abstraction §5.7 InterServerLink — the inter-process stable-connection facility exposed to upper layers; same shape as BW logger_endpoint / KBE logger component / skynet logger service). **Does not occupy the game session's four channels** (net-abstraction §4.1 movement/attributes/events/control is the client session plane; InterServerLink is the inter-process domain, naturally separate).
- **collector duties**: aggregation and archiving, search by process/level/time, disk rolling; BI triage attaches on the collector side (attribute-sync §8.2's BI bypass — business processes need not feed two outlets simultaneously; the script error audit scripting-lua §6 and runtime logs converge here). **Landing (2026-10-13, BI triage tap)**: sampling-based bypass export at collection stage — cat whitelist + deterministic sampling (keep 1 of every N hits) + original-line passthrough (the BI consumer parses structured lines itself; the collector does no TLog field translation — the translation surface lands with the TLog event family batch); env wiring `APOLLO_LOGGER_BI_DIR` (unset = triage off)/`_BI_CATS`/`_BI_SAMPLE_RATE`; single-file append at `<dir>/bi.log` (external tail ships it; Kafka sits after the collector); failure semantics = out-of-budget bypass (open failure turns triage off without blocking startup; write failure counts then turns off permanently; the archive mainline is unaffected).
- **Failure semantics**: collector down → processes write local only (disconnect detection + buffer cap + reconnect), zero business impact, zero process exits — **collection never becomes a process's runtime dependency**.
- **Per-process file layout**: `log/<app>-<instance>.log` (main log) + `log/crash-<pid>-<time>.log` (crash summary) + core partition directory; instance number distinguishes multiple processes on one machine (user requirement: every process has its own logs). **Landing deviation record (2026-10-10, L1+L2)**: the actual layout uses FileAppender's own scheme `log/<proc>_<date>[_<seq>].log` (baseName=processIdentity, ByDate rolling + restart-sequence continuation, maxFiles cleanup) — the delta from the design semantics `<app>-<instance>.log` is separator and instance-number form (sequence continuation vs declared instance number); single responsibility stays with FileAppender (logger-app.md decision point 4); layout semantics equivalent (per-process per-machine independent file set); semantics unchanged in the design doc, reality aligned via this note. Crash summaries go through the crashdumps/ directory (crash-capture batch ②), not this layout surface.

### 5.1 Outsourcing Boundary: Single Exporter, Tail Collection, and the Trace Subset

G-5 and this file previously fixed only the **internal** shape (MetricRegistry/collector/two-segment placement); the "internal → external standard stack (Prometheus/Kafka/ES)" egress contract had zero design until now; this section fixes it (direction set 2026-09-30, details follow the P3 batch).

**Three bans first** (review red lines): game processes **do not connect to Kafka directly** (the message queue is a collection-side component, not a game-process dependency — the external collector tails files and decouples), **do not adopt the full OTel SDK** (OTLP exporter background threads/per-span allocations conflict with tick discipline — borrow the data model, not the implementation), **do not open per-process HTTP ports** (one exposure per process = attack surface + port-management double burden).

- **Single exporter (admin process, serving /metrics)**: admin is the already-planned ops single entry (scripting-lua §7); it consumes the per-process MetricRegistry aggregates uplinked over the G-5 control channel and serves /metrics in Prometheus exposition text format — game processes gain zero new ports. **No prometheus-cpp** (text format hand-rolled, a few dozen lines; the external standard stack's compatibility point is the text protocol, not the client library).
- **Log outsourcing = structured lines + stable directory + external tail**: line format fixed as structured key-value (`ts=wall_ms level= proc= tick= cat= msg=` + kv extension segment — key set stable, quoting rules into the P3 batch); directory = the layout of the previous section (`log/<app>-<instance>.log`); rotation rename is atomic (external collector inode-follow semantics); filebeat/fluent-bit-class collectors tail and ship to Kafka/ES — **Kafka sits after the collector**, and game processes do not know it exists. Relation of collector (apps/logger) to the external tail: collector handles inter-process aggregation/search (internal), tail handles standard-stack shipping (external) — two outlets in parallel, not mutually exclusive; the local file is source of truth for both. **Landing (2026-10-07, P3-3 batch C)**: six keys fixed order + quoting rules + LogContext (proc/tick injection) + FileAppender `structuredOutput` switch (default off; opened in the apps/LoggerApp wiring batch) — key set and escaping details take the header comment of `modules/core/log/include/apollo/core/log/structured.h` as the contract source (logfmt-family parseable, single-line guaranteed: newlines inside values escaped).
- **Trace subset (borrow the model, not the SDK)**: fields = trace_id/span_id + W3C traceparent semantics (incl. sampling flag); **generated at process entry** (client session first touch / first generation site of cross-process messages; `InternalMessageEnvelope.trace_id` is already a standard envelope field — net-abstraction §7); **span boundary = message boundary** (one message's processing span is one segment; two spans across processes share a trace_id); **head sampling at a very low rate** (default 1/10⁴, config constant, decided by the entry process — production wiring = gateway-app configure period `-DAPOLLO_TRACE_SAMPLE_RATE=<N>` pins a compile-time constant and overrides dev env; dev surface = `APOLLO_TRACE_SAMPLE_RATE` env); sampled spans enter the log stream as structured lines (shipped with the tail) — no OTLP exporter. Purpose limited: slow-link localization ("where does the 800ms of a login go"), not a full tracing system.

### 5.2 Alert Outlet and the Webhook Default Implementation Family

§5.1 fixed the **data** egress of "internal → standard stack" (/metrics + tail + trace subset); this section adds the final hop: where **alert decision and notification sending** live and what the defaults give.

**Two-tier boundary (placement ruled first)**:

- **Tier A (standard stack, production recommendation)**: Prometheus scrapes /metrics + alerting rules + **Alertmanager** — alert rules, silences, grouping, routing are Alertmanager's day job; zero code in the apollo repo; WeCom/DingTalk receivers use community adapters (the prometheus-webhook family), also not in the repo. Deployments with the standard stack **do not take Tier B**.
- **Tier B (built-in fallback, viable for small deployments)**: the exporter (admin process) carries a built-in **threshold notifier** — single-machine/dev deployments without Prometheus get alerting with zero external dependencies. Both tiers share the same event source (MetricRegistry aggregate values) but not the code path (Tier B = rule table + template rendering + outbound POST, tens of lines). **proc liveness alert (added 2026-10-13) is a built-in rule** (`apollo_proc_up < 1`, does not occupy the env rule table): proc_up is synthesized by the hub egress face and does not enter the push body; liveness state is fed to the notifier every beat via `MetricsHub::proc_states` with the same staleness criterion — fires on the first down beat (the staleness criterion itself is a sustained window), clears on recovery, re-fires on sustained down per silence window; silence/rate caps shared with the rule surface.

**Tier B shape (main body of this section)**:

- **Rule-table driven**: `{metric, op, window, threshold, period, silence_min, severity}` — e.g. `script_fault_total{module=*} rate 5min > 10 → severity=crit, silence 30min`. Rules go into config, not code; scripting-lua §6.1's three-level counters (`script_fault_total`/`script_budget_warn_total`) are typical inputs.
- **Outbound reuses net-abstraction §5.10 curl facility** (HTTP client lives only in the exporter process — a natural extension of the three bans: **game processes never send webhooks directly**; no HTTP egress capability opened at the Lua layer: attack surface + synchronous 30s timeout violating tick discipline, double veto).
- **Four default templates** (bot webhook is essentially = JSON template + one POST; fields per each platform's current open-platform docs, templates in config not hardcoded):

| Target | Outbound and authentication | Message body | Notes |
|---|---|---|---|
| WeCom | POST `…?key=<token>` (key is the credential, no signature) | `{"msgtype":"markdown","markdown":{"content":…}}` | bot rate limit 20 msgs/min |
| DingTalk | `…?access_token=…&timestamp=&sign=`; sign = HMAC-SHA256(secret, `ts\n`+ts) base64 | `{"msgtype":"markdown","markdown":{"title":…,"text":…}}` | signed bot; rate limit 20 msgs/min |
| Feishu | POST webhook URL; self-built bots attach `timestamp + sign` (HMAC-SHA256 base64) | `{"msg_type":"text"/"interactive",…}` | card messages optional |
| Slack | POST incoming webhook URL (URL is the credential, no signature) | `{"text":…}` | — |

- **Sending discipline (review red-line grade)**: ① retry with exponential backoff ≤3 times (5s/15s/60s); if still failing, **write back to local logs** — the alert is not lost, returning to §1's "local file is the source of truth" principle; ② aggregate counts within a rule's silence window, merge into one at window end (storm prevention); ③ global rate cap (default 10 msgs/min; beyond it merge into one digest — margin against each platform's 20 msgs/min limit); ④ secrets via config/env (`APOLLO_WEBHOOK_*`), never into git, never into logs; ⑤ outbound targets on a **whitelist** (domains/CIDRs in config, not addable at runtime — same discipline as net-abstraction §5.10 SSRF; the exporter also does not allow arbitrary dialing); ⑥ template variables = a closed set of event fields (§5.1 structured-line key set + scripting-lua §6.1 ScriptFaultEvent fields), no free-form expressions (injection prevention).

**Relation to §5.1**: the three bans stand — the webhook is an outbound action of the exporter process, not a new game-process dependency; Kafka still sits after the collector (if alert events are to ride Kafka, the tail stream carries them, not the notifier); alert events themselves land in local logs as structured lines as usual (when notifier sends fail, they are the only evidence left).

## 6. Consolidating the Status Quo (C-25/C-26) and Ownership

- **Status quo** (architecture-review §11.5, line numbers there): four stacks coexisting — ① legacy `apollo::utils::logging` (only consumer is examples) ② top-level `include/apollo/core/log` built-in stack ③ `modules/core/log` (vcpkg has no spdlog; actually fell back to built-in) ④ in-memory LogManager; plus the same-path dual-header ODR trap and C-26 orphan TU (`modules/core/src/log/log_manager.cpp:30-33` compiled by no target; default-build link failure reasoned out).
- **Consolidation direction**: **single stack in modules/core/log** — delete ①, demote ④ to test-only or delete; C-26 disposition (incorporate or delete) is the first cut. This design's tiering/bypass/crash surface lands inside that module.
- **Closure delivery** (2026-10-07, P3-3 batch A): stack ① deleted (header tree include/apollo/utils/logging/ + sources + legacy_compat CMake lines, zero consumers); stack ② top-level dead tree deleted (291-line dead log_manager.h not migrated; same-name dual-header ODR trap eliminated; 8 headers git mv'd into modules/core/log/include, .h/.hpp coexisting in one directory, zero include-string changes); stack ③ closure to a single stack (root/modules CMake dead-source lines removed); stack ④ already deleted earlier with P0-6. test_log.cpp passing directly against the live API = method-surface compatibility proof; gate ON/OFF 34/34, Examples 32/32. This design's tiering/bypass/crash surface still not landed — see §7 P1 remainder.
- **Ownership** (applying architecture-review §16.8.3 criteria): log API/buffers/threads/tiering/crash handler → **modules/core/log** ("linked at runtime + owns process-evidence semantics"); sigaltstack install site in modules/base thread wrappers (one place); collector process → **apps/** ("only ops runs it"; BW server/tools, KBE tools/logger precedent — §16.8.2 table "aggregation presented in outboard tools"); the dependency-direction iron law stands: apps→modules one-way.
- **Relation to G-5**: logger is one of skynet's three tools (architecture-review §16.2/G-5 already cite it); error rates/level counts enter MetricRegistry (trends); this design's files and entries are current-state evidence — the "metrics=trends, watcher/logs=current state" semantics unchanged.

## 7. Phasing

- **P1 (a single process should have it)**: consolidate four stacks into one in modules/core/log (incl. C-26 disposition) → tiered buffering + ERROR bypass + FATAL synchronous persistence → crash-handler four-piece set + RLIMIT_CORE/core_pattern ops config + CI crash test. Acceptance: a test process that `raise(SIGSEGV)`s leaves all three pieces — "crash summary file + core + last FATAL in the main log".
- **P3 (fleet)**: apps/logger collector + push channel + disconnect degradation + BI triage wiring (**landed (2026-10-13, BI triage tap; see §5 collector duties landing note)**) + core upload/archive (same batch as G-1 machined death reporting, not landed) + alert notifier (§5.2 Tier B: rule table + four webhook templates, same batch as exporter; **landed (webhook four platforms 2026-10-12 3fe6d499, proc liveness built-in rule 2026-10-13; see §5.2 notes)**).

## 8. Intersections with Other Designs

| Related design | Placement |
|---|---|
| architecture-review.md | C-25/C-26 status audit and consolidation checklist; C-32 signal-handling cautionary example (crash-surface red line); logger item among G-5's three tools; §16.8.3 ownership criteria |
| net-abstraction.md | G-5 two-segment placement (detection primitives in modules / aggregation tools in apps/ — collector is the latter); collector push = upper-layer consumer of §5.7 InterServerLink (inter-process stable connection, independent of the client session's four channels); Link bounded queueing and §4.1 "collector down → local-only writes" share the same semantics |
| scripting-lua.md | §6 script error audit goes through the BI bypass (boundary from runtime logs, converging at the collector); §7.3 error rates into metrics; do-not-strip-symbols discipline shared |
| attribute-sync.md | §8.2 BI triage (collector-side wiring); §10.2 shutdown order (logger stops last, drains then closes files); same budget-mode thinking for disk watermark and stopping low-level writes |
| ssengine-reference.md | The "header comments lie" lesson (§5) applies equally: crash-path claims must be proven by CI tests, not accepted from comments |

---
