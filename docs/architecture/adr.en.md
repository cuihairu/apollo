---
title: Architecture Decision Records (ADR-001..017)
icon: gavel
order: 2
category:
  - Architecture
tag:
  - ADR
  - Decision Records
---

[English](adr.en.md) | [中文](adr.md)

# Architecture Decision Records (ADR-001..017)

> Purpose (task brief §36): write the architecture rulings that are decided and landed as citable records, **to keep the architecture from running off the rails again in the future**. Each entry has four segments: Status / Background / Decision / Consequences. The "actual artifact" column gives repository anchors — reconcile against the ADR before changing code.

---

## ADR-001 Player Runtime Authority

**Status**: landed (P0-2 Player object model delivered).

**Background**: if player data can be rewritten by multiple parties — instance/battle/gateway — long-term-state ownership diverges, and there is no single truth on disconnect/instance transfer/rollback.

**Decision**: the sole authority over a player's long-term state is `PlayerAnchor` (`modules/game/session`). Login activates presence idempotently via `AnchorManager::activate`, and long-term progress accumulates only on the anchor (`progress` + `mark_dirty`, dirty on every change); instances and battles hold no long-term state — battle settlement settles onto the anchor **one-way** through `IRewardSink`, forgetting as soon as it lands. Logout reclaims it via `deactivate`.

**Consequences**: any module that wants to change a player's long-term state can only settle onto the anchor (a one-way opening); there is no second write point. The cost is that settlement paths must take the explicit `AnchorRewardSink` hop.

---

## ADR-002 Scene as Runtime Boundary

**Status**: landed (P0-3 Scene model delivered).

**Background**: if spatial authority dangles (in a global service and in process-local state at the same time), visibility and entity ownership produce two sets of truth.

**Decision**: the spatial runtime boundary is the `Scene` (`modules/game/world`) — entity ownership, coordinates, and visibility inside a scene are adjudicated exclusively by the Scene; Apollo does not build a cross-process continuous big world (seamless maps/cell sharding/ghost/entity migration were all rejected, architecture ruling #9); the world's natural boundaries are rooms/scenes/instances/matches.

**Consequences**: entity transfer across Scenes must go through explicit scene transfer (ADR-004); there is no implicit roaming across boundaries. Worlds larger than a single Scene are expressed by orchestrating multiple Scenes/multiple instances.

---

## ADR-003 Instance as First-Class Object

**Status**: landed (P0-4 / P4-2 batch A, verified across the full chain).

**Background**: if "a match" is scattered across callbacks and temporary containers, nobody minds its lifecycle, and leaks and half-dead states abound.

**Decision**: one match, one instance — `Instance` (`modules/game/world`) is an eight-state one-way state machine `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`; only adjacent transitions are legal (the `is_transition_valid` check table) and the Destroyed terminal state is sealed. The Waiting window carries the gameplay payload and pre-entry players; `start` validates that combatants are non-empty internally before entering Running.

**Consequences**: instance creation/scheduling/recovery/reclamation has a single handle to manage. The cost is that all advancement must pass through the state machine (no jump-around side doors); a capability added during the skeleton phase must first locate its own state window.

---

## ADR-004 Scene Transfer

**Status**: landed (P1-1 scene_transfer delivered).

**Background**: if cross-scene movement is done by "editing coordinates in place", there is nowhere to put the target scene's entity registration, view rebuilding, or failure rollback.

**Decision**: every crossing of a Scene boundary goes through the four scene-transfer segments `prepare → detach → attach → resume`; a failure in any segment rolls back to the source scene (`modules/game/world/scene_transfer`).

**Consequences**: scene transfer is transactional — either arrive in full or stay put. The cost is that the scene-transfer path has higher latency than ordinary movement, and during a transfer the entity is in a transitional state and cannot interact.

---

## ADR-005 AOI Ownership

**Status**: landed (P1-3 / P3-2 delivered).

**Background**: if interest management is separated from the scene (a global AOI manager), the two coordinate systems — scene_id and spatial grids — inevitably drift apart.

**Decision**: AOI belongs to the scene — the single `SceneAoi` implementation is owned exclusively by the Scene (held via `scene.aoi()`, isolated by scene_id), 3×3-grid diffing + `ViewerState` per-observer watermarks, with the Enter/Sync/Leave event face delivered through `set_event_sink` (the sink is silent by default).

**Consequences**: the visibility authority is unique (the Scene says who sees whom); the broadcast-volume metric = the count of Sync diff events; the gateway delivery face just hooks the event sink, and the AOI core is no longer polled by multiple parties.

---

## ADR-006 Battle Runtime

**Status**: landed (P2-1 / P4-2 batch A verified).

**Background**: if battle edits player data directly, rulings cannot be replayed and settlement has no receipts; if it shares one timeline with the instance tick, two drivers race for the beat.

**Decision**: the battle ruling domain is independent — `BattleRuntime` (`modules/game/battle`) has five phases (Created→Entering→Battling→Rewarding→Finished), strictly increasing ticks + ordered inputs + `CombatRoll` substream random numbers (the determinism fingerprint = a hash chain), and rulings settle one-way through `IRewardSink` (summed per-player Hit damage). The ruling domain's timeline is driven internally by `Instance::tick`, and battle input goes through the explicit `BattleInput` injection face — **the two timelines must never be mixed or allowed to race for the beat**. The participant collection window = the instance's Created/Entering window (forwarded by `Instance::enter`); from Battling onward, entrants may only spectate.

**Consequences**: the same seed with the same input sequence always yields the same battle outcome (auditable and replayable). The cost is that inputs must be injected explicitly and kept ordered; any side-door state change breaks determinism.

---

## ADR-007 Persistence Model

**Status**: landed (P1-4 / P1-5 delivered; audit A8's configuration closure shares the same semantics).

**Background**: an external DB dependency raises the bar for "getting it running", and wiring a DB while storage semantics are still undefined in the skeleton phase is a negative asset.

**Decision**: today there are zero external storage dependencies — player profiles = JSON files (atomic tmp+rename writes) + `PersistJournal` write-behind (append to disk → quota drain → snapshot compaction → crash replay resumes); a load miss fails outright (no default-profile bootstrap); MySQL/Redis/ClickHouse are all in the planned state and not wired.

**Consequences**: dev/CI environments run the full chain with no storage service at all. The cost is that single-machine capacity and query capability are limited by the file format, and the external-DB wiring batch must preserve equivalent replay semantics.

**Proxy-decision note (2026-10-16, inspection progress ticket FW1016D)**: external storage wiring (MySQL/Redis/ClickHouse) remains uninitiated by proxy decision during inspection — this ADR's basis still holds (wiring an external DB before storage semantics are settled is a negative asset and raises the "getting it running" bar), and the repo still shows no production-deployment or capacity demand signals (the same factual footing as ADR-010's G-2 closure); the unlock trigger = a production deployment being initiated or a user-named list. Inspections do not initiate external storage dependencies on their own.

---

## ADR-008 Recovery Model

**Status**: landed (P1-5 Recovery / Reconnect delivered).

**Background**: disconnect and process crash are two kinds of recovery — the former has a live process to return to, the latter must rebuild from on-disk state; mixing one mechanism for both does neither cleanly.

**Decision**: two recovery lines. **Reconnect** goes through the Session-layer reconnect window + the Player anchor's session ownership (the `BaseAppMgr` directory ruling "which world to return to, via which gateway"); **crash recovery** goes through `PersistJournal` replay continuation (replaying the write-ahead log to the last consistent point), with no cross-process snapshot migration.

**Consequences**: a disconnected player's long-term state and in-progress instance are adjudicated solely by the directory, and reconnecting returns them to their place; the recovery point after a process crash = the journal's last consistent point, with a loss window ≤ the drain quota.

---

## ADR-009 Concurrency Ownership

**Status:** landed (skeleton-phase semantics, evolving batch by batch).

**Background**: multithreaded simulation voids determinism (ADR-006) and the state machine (ADR-003) entirely, and a spreading lock surface is unauditable.

**Decision**: the simulation domain is single-thread driven — `Instance`/`BattleRuntime` are driven by a single-thread tick, and the clock-feed injection spins up no internal threads (discovery shares the same semantics); cross-thread faces converge to a minimal set: logging `LogContext` (proc mutex + tick atomics), `MetricRegistry` find-or-create under lock + an owning-reference lock-free hot path, and the machined main loop's non-blocking `waitpid(WNOHANG)` reaping; the service host `ApplicationHost` is frame-driven, single-thread run_once.

**Consequences**: simulation logic has zero locks and zero contention (the precondition for determinism to hold). The cost is that the throughput ceiling = single-thread tick capacity, and multi-core scaling goes through horizontal instance orchestration rather than in-domain multithreading.

**Aftermath errata (2026-10-10)**: after P0-3, cell-app split out a `gameThread_` to run the worldHost tick while message handlers execute on `RepSocket`'s self-held worker thread — handler writes to the world and tick reads/writes of the world are concurrent across threads, exceeding this ADR's "cross-thread minimal set" (recorded in the concurrency §1.1 aftermath errata; production never triggered it only because of the load vacuum). The closure prerequisite design is on the books as docs/design/cell-single-writer.md (a three-way comparison table + six decision points, candidate lean = (a) queued asynchronous acceptance, decision ① pending ruling — ADR-015/016/017 ruled everything the same day). This ADR's skeleton-phase semantics stay in force and evolve batch by batch once the closure batch lands.

---

## ADR-010 G-2 Topology: Restart-Only First (disaster-recovery topology: plan A first, standby not initiated)

**Status:** ruled (2026-10-10 inspection-authorized batch, all six decision points adjudicated together; the adjudication basis = backup-revive's counter-evidence section + batch 0 field measurements).

**Background**: the G-2 prerequisite design (backup-revive) laid out three plans (A cold restart / B hot standby / C dual-write without standby) and left all six decision points pending. The counter-evidence section already records: in a single-machine fleet a standby dies on the same machine anyway; under the 5000-CCU single-machine 8GB target, the mainstream of contemporary small/mid deployments is exactly "process-manager auto-restart + persistence + client reconnect" (a D-tier industry-wide common shape); the hot standby's real increment is only RTO seconds → sub-second plus volatile-state retention, at the cost of a resident standby machine + mirror-stream bandwidth + dual-master arbitration complexity. The batch 0 kill -9 full-chain drill is delivered (scripts/drill_kill9.sh; both runs' assertions passed).

**Decision**:
1. **Plan A first (restart-only)** — machined's supervision face does backoff restarts + PersistJournal persistent state + client reconnect; **standby is not initiated**; no phased reservation is made for plans B/C. If production operating data raises a sub-second RTO demand, initiate separately (relying at that point on net M1 InterServerLink).
2. **Takeover granularity splits into three tiers by process type** (not one unified semantics): the stateful-rebuild tier = baseappmgr (full re-reporting, existing) + baseapp (cold restart + journal replay + re-report convergence, measured in batch 0); the stateless-restart tier = login-app / verifier (worker-pool restart restores service); the crash-invalidates tier = Battle instances (ADR-012).
3. **Arbitration face: plan A has no dual master and invents no new arbitration protocol**; the epoch reuses the directory face's existing semantics (monotonically increasing + stale epochs rejected — the guard genuinely exists in the directory-face core, player_directory.cpp:117-124 §3-③ race-window guard: `next_epoch_` self-increments to issue numbers, and resume rejects the old by double-anchoring on token and epoch, while the directory_mirror layer only carries wire serialization), with baseappmgr's single-point serialization maintained.
4. **RPO/RTO are pinned**: RPO = 0 (journal write-ahead, zero loss; the existing attribute-sync §11 metric is upgraded); RTO = re-report convergence ≤3s / mirror convergence ≤3.5s (pinned as the measured upper bounds from batch 0's full-load noise window; idle load does better).
5. **No mirror stream**: the journal's cross-process consumption rework is not done (the current single-process SaveQueue worker stays); the two candidate shapes — same-machine shm SPSC / InterServerLink — are archived on file, to be ruled on if B/C is ever initiated.
6. **The acceptance chain = the batch 0 kill -9 drill chain** (delivered); dual-master injection drills do not enter the gate (plan A has no dual master). The four G-2 glossary terms are settled by this ruling: the hot-standby/takeover entries are annotated "not initiated (plan A first)".

**Consequences**: G-2 batches 1-4 (the position face/mirror stream/takeover arbitration/dual-write face) are cancelled in full — plan A's complete form is batch 0's terminal state, already delivered; G-2 is closed (reopen conditions = production data raising sub-second RTO / a multi-machine form being initiated). Costs: seconds-scale (not sub-second) service interruption while a process is dead, and volatile-state loss (reconnect falls back to on-disk state) — both are explicitly registered in the capacity §5 metric set.

**Proxy-decision note (2026-10-15, inspection progress ticket R1015F)**: this ADR's closed state was re-reviewed by proxy decision during inspection and stands — plan A first, standby not initiated, G-2 batches 1-4 stay cancelled; it does not reopen without an ADR counter-ruling or production data (sub-second RTO demand / a multi-machine form being initiated).

---

## ADR-011 net M1 Chain: Minimal Gateway Fix First (gateway chain: minimal fix first, topology-survey conclusion takes effect)

**Status:** ruled (2026-10-10 inspection-authorized batch).

**Background**: gateway-app today is a half-finished shell (three causes from the sounding: at startup every backend is a hard dependency with no retry / default ports are misplaced — BaseApp 9002 = its own port, CellApp 9100 misplaced / the ChatApp ghost dependency points at a port no process listens on; compounded by ingress not being wired). "When to fix it, and via the minimal-fix prescription or by waiting for net M1 to wire it directly" hangs on the net M1 decision chain; the gateway-topology-survey conclusion "keep an independent gateway-app + a homegrown inter-process bus" is an advisory piece not yet in effect.

**Decision**:
1. **The gateway-topology-survey conclusion "keep an independent gateway-app" takes effect** (topology settled: a connection-face process + baseapp never sees clients + a homegrown bus; not the nats middleware family).
2. **The minimal-fix prescription goes first** (three steps): ① `MessageRouter::start` downgrades connection failure to a warning + `available=false` (no more throw→exit 1; reuse the forwarding face's existing degradation semantics) ② the dev_fleet roster passes the three backend URLs (removing the port misplacement) ③ the ChatApp dependency is removed (the repo has no chat-app process).
3. **ingress client wiring does not ride this batch** (the real gap is ingress; it belongs to P3-2 gateway surface with the net M1 batch); protocol convergence / the P4-4 code-layer three clusters advance along the M1 ladder (§5.6: the M1 minimal correct kernel = frame delimitation + contract_route dispatch + backpressure watermark + unicast topology + InterServerLink).

**Consequences**: once the three steps land, the dev_fleet fleet no longer gateway crash-loops (the exit=1 ×5 give-up is eliminated); gateway remains a process shell with no client entrance (the expected state, not a defect) — the real client face waits for P3-2 + M1. All three steps are startup-semantics and parameter-passing corrections; forwarding-face behavior is untouched.

**Implementation status (aftermath, 2026-10-10)**: all three steps landed — ② the dev_fleet roster passes the three backend URLs (commit 5ccfc651); ①③ the connection-failure degradation warning + ChatApp ghost-dependency removal (commit d7f21831: `connectBackendChannel` changed to a warning + empty-object return instead of throw, chatAppUrl/chatAppClient_ removed in four places, gateway_route_tests gained a degradation regression); the live-machine smoke test of standalone boot showed all three backend degradation warnings and exit=124 surviving to timeout (the old exit=1 written off) — the crash-loop is gone. gateway remains a process shell with no client entrance (the expected state; ingress wiring belongs to P3-2 with net M1).

**Implementation status addendum (2026-10-11, net M1 kernel batch)**: the §5.6 M1 minimal correct kernel landed — five self-held components in modules/net/link (frame delimitation / contract_route dispatch by id / backpressure watermark / InterServerLink / TCP base + connector + manager), with LinkTests 33 cases green on both ON/OFF trees; correctness first, no performance metrics set. Decision 3's M1-ladder kernel line is fulfilled; gateway ingress wiring still belongs to P3-2 (kernel components ≠ ingress wiring); the nng base-swap migration hangs on the upper-layer wiring batch (synced in the P4-4 batch record).

**Implementation status addendum (2026-10-12, nng migration batches ⓪+①)**: work began on migrating the five nng consumption faces — batch ⓪ the linkrpc correlation primitives (contracts 940/941 correlation envelopes + corr_id correlation + a call_async/poll/wait two-state wait bridge; payloads are opaque = full MessageCodec frames, business semantics belong to each process's handler; modules/net/rpc is the legacy old stack, unusable, and the new face is modules/net/linkrpc) + batch ① gateway admission re-outfitted to M1 (login `--m1-port` passive acceptance face + gateway `--login-m1-port` link admission service, both ends carrying their own pump threads — the admission synchronous-wait semantics and the host pump are mutually exclusive; the degradation semantics keep the same seam as in the stub era: an unreachable transport is not amplified into a rejection, only login success=false rejects). 5ae86969 + ef69fccc; fleet live-fire proof 9103 ESTAB (see the P4-4 batch record). The admission service's synchronous signature is unchanged — zero caller-face change when P3-2's ingress mainline wiring lands.

---

## ADR-012 Instance Offload: On-Demand Spawn, Stub-First, Crash-Invalidates (the three instance-offload rulings)

**Status:** ruled (2026-10-10 inspection-authorized batch; all three decision points settled per battle-instance-offload's existing candidate leans).

**Background**: the battle-instance-offload prerequisite design left three decision points pending: the lifecycle protocol (static pre-pool vs on-demand spawn), the transport dependency order (M1 first vs in-memory stub transition), and the crash ruling (invalidate vs keyframe-replay recovery).

**Decision**:
1. **Lifecycle = (b) the on-demand spawn request protocol** (the requester has machined's control face launch it; visible through the directory once ready; the control face reuses the G-1 fleet-event shape — a same-direction extension of the Query/Advertise precedent; a static pre-pool is allowed as a transitional form until (b) is ready).
2. **Transport dependency order = (b) an in-memory stub transition**: offload first lands its lifecycle + internal-domain envelope protocol + fault-handling skeleton on same-machine pipes (pipe/shmem SPSC), swapping the base once M1 is ready (one base swap decouples design risk from transport).
3. **Crash ruling = (a) crash invalidates**: the team battle restarts, and players are told via the error face; (b) keyframe-replay recovery keeps a seam (ReplayTuple is already keyframe material; the protocol face can be added later, not pre-spent).

**Consequences**: the post-ruling path starts: a protocol message contract batch (internal domain, committed with the implementation) → an offload skeleton batch → an M1-ready base swap → integration at the baseappmgr-domain placement. The Battle instance's failure domain is independent of baseapp's (battle-instance-offload §7); the invalidated tier of ADR-010's three-tier granularity table is carried by this.

---

## ADR-013 LoggerApp In, VerifierApp Deferred (logger process initiated, verifier process deferred)

**Status:** ruled (2026-10-10 inspection-authorized batch).

**Background**: two process names were finalized: the log-collection process `LoggerApp` (dual log outlets: the local-file source of truth + push) and the battle-verification process `VerifierApp` (server-side recomputation reconciliation for client-authoritative battle, gap #17). The logging batch C delivered the structured line format (six keys in fixed order + the FileAppender structuredOutput switch, off by default, to be flipped by the apps wiring batch); VerifierApp's recomputation engine depends on the Lua face (scripting-lua not yet in the mainline).

**Decision**:
1. **LoggerApp is initiated**: an apps/LoggerApp process shell + every app's FileAppender structuredOutput switch flipped on — the local-file source of truth is maintained (dual outlets for human reading and machine reading), while the push outlet (an upstream collector) keeps an interface without an implementation (the collector face comes after M1).
2. **VerifierApp is deferred**: its recomputation engine depends on the Lua face, which is not in the mainline; the existing battle-verification-service §5 semantics (a stateless worker pool, no G-2 hot-standby requirement) stand as a design piece; initiation rides the Lua/gameplay batch.

**Consequences**: the structured line format gains a process-level consumption face (batch C's leftover "apps wiring not done" is written off); the VerifierApp gap (gap #17) stays registered.

**Landing note (2026-10-10)**: decision 1's "switch flipped on" has one more precondition — the apps wiring status = zero (all seven apps write straight to `std::cout`, with zero log-module integration; the sole touchpoint is game-server's crash-visibility line), so apps must first integrate the log module (L1) and only then flip the switch (L2); the landing-form prerequisite design is on the books as docs/design/logger-app.md (a three-way v1 collection-form choice + five decision points, candidate lean = tail pull).

**Batch record (2026-10-10, three batches delivered in succession the same day)**: L1+L2 (`7ad64938`) — all seven app mains integrated the log module (initLogging: processIdentity + fileEnabled + structuredOutput, console kept human-readable; placed before init_crash_capture — write()'s lazy default initialization is console-only, and reversing the order would clobber the file-face configuration), and the app-library runtime lines moved wholesale to the logger (domain sub-logs recovery/mirror/window/death = §5.1 cat= retrieval keys; the signal path and the fork pre-exec path keep writing straight to console/stderr — the logger face is lock-bearing, and signal contexts and the fork-snapshot lock-holding window do not enter it); the kill -9 drill's full-chain assertion regression is green (the message-body substring contracts all preserved). L3 (the logger shell) — apps/logger + the `logger` binary + class `LoggerApp`; read-side parsing (a mirror of the write-side structured.cpp: quote/escape restoration, six-key completeness determination) + the retrieval subcommand `scan` (proc/level/ts/cat filtering, stdout query face / stderr summary separation) + `follow` tailing (the fleet aggregation face, incremental reads + partial-line retention) + the push seam `IPushSink`/`NullPushSink` (interface kept, not implemented; swaps to InterServerLink after M1); logger_tests 9 groups entered the gate (42/42); the dev_fleet roster gained a logger row (follow, no port — the three bans), and the fleet smoke test was green on all 7 members (103 structured aggregate lines in machined.log, scan actually searched 12 files, all parsed). L4 (the push channel + disconnect degradation) hangs on M1. Archiving/rotation follows the existing FileAppender; the logger is read-only.

**Batch record (2026-10-12, L4 delivered)**: L4 (`8d8a1dc0`/`6dd9445f`/`b8111f2a`/`d8499672`) — the push channel + disconnect degradation in full form (the logging.md §5 collector landed): kernel LinkManager passive fan-out (staging→peers migrated by each peer's self-reported name, same-name reconnect replaces, assembly attached in the on_up beat) + contract 930 logger_line_push + the sending face modules/net/logpush LogPushClient (an internal pump thread as single writer, a bounded queue of 4096 dropping new entries at cap, kReliable reconnect-exactly-once redelivery, stop-drain keeps the queue) + PushAppender/LogManagerConfig.pushSink wiring + the logger serve subcommand (a passive "apps" mode receives 930 original lines into the archive, flushed line by line — fixed after ofstream buffering + kill -9 data swallowing was proven empirically) + one-line APOLLO_LOG_PUSH_ADDR wiring in all seven apps (unset = off; v1's single outlet unchanged) + the dev_fleet logger row switched to serve. The disconnect-degradation fleet live-fire drill: SIGKILL killed collector+gateway together → machined death/rebirth lines fell in the disconnect window → the queue buffered → the reborn collector redialed and redelivered, with everything arriving (delta=20) — decision 1's "push outlet keeps an interface" is hereby written off, and the IPushSink retrieval face is unchanged. See todo P3-3 / logger-app §6 for the detailed batch record.

**Proxy-decision note (2026-10-16, inspection progress ticket FW1016D)**: decision 2's deferral originally rested on "the recomputation engine depends on the Lua face, which is not in the mainline", which is now outdated (the Lua runtime base's thirteen batches have been in the mainline since 2026-10-15), but "rides the gameplay batch" stands — by proxy decision during inspection it is not initiated early: the mainline has zero gameplay content to recompute (no skill/effect authoring), BattleRuntime determinism is already covered by the battle_runtime/rng_substream tests, and initiating early would only yield an empty-shell worker pool recomputing the existing determinism test surface; the unlock trigger = the gameplay batch being initiated.

---

## ADR-014 Process Rename: Manager and Zone (process rename: baseappmgr→manager, base-app→zone-app)

**Status:** revoked (by user order, 2026-10-12) — ruled in the 2026-10-10 inspection-authorized batch, landed 2026-10-11 as a5750eda, and renamed back to the BW names the same day. Basis for the reversal: the naming contract the ruling rested on ("no cellapp/baseapp") was internally drafted, never reviewed by the user, and the file has been deleted, so the basis does not stand; the semantics in force = reuse ready-made BigWorld words first.

**Background**: P3-1 left a leftover — "rename the base/baseappmgr process names (go through the new-term process for naming; no renaming before that)". **Basis correction (2026-10-12)**: the naming contract originally cited ("no cellapp/baseapp process names") was internally drafted and **never reviewed by the user**, and that file has been deleted; the semantics now = **reuse existing BigWorld/KBE concepts first to lower comprehension cost** (new terms must come with a comparison and a rationale). Whether the process names revert to BW vocabulary is ruled on separately.

**Decision**:
1. **baseappmgr → manager**: process name / binary / roster identity / log identity become `manager` (the BaseAppMgr class-name domain code is tidied for within-batch consistency; the CTest suite name does not change semantics — BaseAppMgrTests keeps its existing name to avoid pointless assertion renames).
2. **base-app → zone-app**: process name / binary / roster identity become `zone-app` (Zone = the finalized name for the logical-server process; the code identifier `Zone` is not conflated with the process name). cell-app is the world-runtime prototype and keeps its name (its merger/fate rides the gameplay batch, outside this ruling).
3. The rename surface = the apps directory + CMake targets + the dev_fleet/drill rosters and document references; a pure process-identity change with zero behavior change (ports/parameters/supervision semantics untouched).

**Consequences**: the finalized names from the rename batch land everywhere, and the baseappmgr/base-app process names no longer appear in the repo (other-engine comparison terms survive only in document evidence citations); after the rename batch, full-chain re-verification on dev_fleet/drill is the acceptance face.

**Landing (2026-10-11, a5750eda)**: apps/baseappmgr → apps/manager (binary manager, class ManagerApp, namespace manager, log identity and file name manager, crash db crashdumps/manager); apps/base-app → apps/zone-app (binary zone-app, log identity and file name zone-app, RecoveryCoordinator identity zone-app; internal base/BaseServer code identifiers untouched — decision 2's "code identifiers are not conflated with the process name"); the CLI parameter surface unchanged (--base-app backend hook parameters/ports/supervision semantics as-is); the CTest suite name BaseAppMgrTests kept (decision 1); the dev_fleet/drill rosters renamed and assertion substrings (child death zone-app / restart zone-app) updated in batch; other-engine comparison citations (BW baseappmgr.cpp:588-599 etc.) are exempt and retained per this ADR. Acceptance = all three trees' gates green (ON 42/42, Examples 40/40, OFF 36/36) + the dev_fleet smoke test (born manager/zone-app lines in the logs, structured proc= lines under the new names in log/manager_*.log and log/zone-app_*.log, crashdumps/manager|zone-app isolation) + the drill_kill9 full-chain assertions all passing (death detection 45ms → backoff restart 1.05s → recovery ready 1.07s → full re-reporting 1.70s → mirror convergence 2.71s). Document face: current-fact docs renamed in batch (guides / api / overview / Directory_Structure / the apps implementation docs / the design pieces' status connections and parts tables / delivery-note paths — design-piece cited line numbers were checked and did not drift; all renames were equal-length line replacements); point-in-time historical docs are not rewritten (the seven-piece audit set / analysis / qa / other-engine comparison pieces / the starter reference piece's target-form example / the baseline footer's existing firsthand-read list) — point-in-time evidence is preserved, and this note plus the todo batch record are the authoritative aftermath.

**Revocation (2026-10-12, by user order)**: both process names reverted to the ready-made BigWorld words — apps/manager → **apps/baseappmgr** (binary baseappmgr, class BaseAppMgr, namespace baseappmgr, log identity and file name baseappmgr, crash db crashdumps/baseappmgr); apps/zone-app → **apps/base-app** (binary base-app, log identity and file name base, crash db crashdumps/base-app, RecoveryCoordinator identity base-app; internal base/BaseServer code identifiers untouched). The M1 link's self_name/peer_name and CLI (--baseappmgr-m1-*), the CTest suite names (BaseAppMgrM1ControlTests/BaseM1Tests), and the dev_fleet/drill rosters and assertion substrings (child death base / restart base) were reverted in batch. A pure process-identity change with zero behavior change. The document narrative layer reverted in sync: the Zone/zone process → baseapp (same logic as apps/base-app), manager in the process sense → baseappmgr; code identifiers (zone_id, zone_hint, home_zone_id, etc.) and other-engine comparison facts (EQEmu's zone process system, SmartFoxServer's Zone, etc.) unchanged. Acceptance = all three trees' gates green + the dev_fleet smoke test (born base/baseappmgr lines in the logs, crashdumps isolation) + the drill_kill9 full-chain assertions passing. This ADR's earlier text and point-in-time historical docs (the seven-piece audit set / analysis / qa / other-engine comparison pieces) are not rewritten; point-in-time evidence is preserved.

---

## ADR-015 Cell Single-Writer: Async Queue Acceptance First (single-writer main plan: queued asynchronous acceptance)

**Status:** ruled (2026-10-10 inspection follow-on r2 authorization; decision ① adjudicated per the cell-single-writer prerequisite design's candidate lean; ② watermark / ③ read-only fast path / ④ Manager merge / ⑤ shutdown sequencing remain pending).

**Background**: the P3-2 batch B race evidence — cell-app's `gameThread_` tick reads/writes the World/Scene graph while `RepSocket` worker-thread handlers touch `world_` directly, with zero protection between the two threads (cell-single-writer §1, A-level firsthand read). The prerequisite design's three-way fork: (a) queued asynchronous acceptance / (b) queueing + synchronous wait / (c) minimal-mutex fallback; the candidate lean is (a); (c) was rejected on counter-evidence (mid-tick container mutation: iterator invalidation + a broken battle-determinism iteration order), and (b) on head-of-queue blocking (workerLoop is single-threaded, and queue depth × tick duration crossing the bound collapses throughput).

**Decision**:

1. **Main plan = (a) queued asynchronous acceptance**: the RPC worker only decodes + enqueues (intent envelopes), and `gameThread_` consumes, executes, and sends replies at tick boundaries. All changes are tick-aligned (attribute-sync §10.1 G-7: "visible, ordered, and sequence-numberable within a fixed boundary"); reply latency ≤1 tick (a 100ms bound at 10Hz); the calling face is already the `sendRequestAsync` callback style, so the latency increment breaks no semantics; the acceptance face is exactly the net M1 receiving-end skeleton (under M1, (b)'s synchronous face is scrapped and redone).
2. **(b) is retained as the escalation plan for when (a)'s watermark semantics fall short**; (c) stays rejected. **Aftermath errata (2026-10-10 / ADR-017)**: the (b) synchronous-wait mechanism was thoroughly rejected via decision ⑥ — the escalation path for short watermark semantics is watermark retuning/capacity, not synchronous waiting; M1's delayed replies, the correct solution, cover (b)'s reply-immediacy demand.
3. **Decisions ②-⑤ remain pending** (watermark form / read-only fast path / Manager merge / shutdown sequencing) — the watermark-and-overload decision face gets its own prerequisite design (acceptance-queue watermark semantics), for later inspections to rule on. **Ruled (2026-10-10 / ADR-016)**: ② a two-tier watermark + overload callback error codes + tick-budget anchoring; ③ no fast path; ④ the Manager mutexes are kept; ⑤ shutdown mirrors G-3.

**Consequences**: the acceptance-face batch (intent envelopes + queue + tick consumption + pending reply, cell-app feature code) waits for the docs/scripts boundary to be opened; the watermark-and-overload batch follows right behind (the decision-face design landed with ADR-015); the Manager-merge re-review batch follows decision ④; the net M1 receiving-end wiring only swaps the transport base (the acceptance face = the skeleton, reused directly). battle-instance-offload's spawn request protocol reuses the same acceptance form for its control face (ADR-012). **Aftermath errata (2026-10-10 / ADR-017)**: the acceptance-face batch's reply face = the acceptance-point receipt (ack-on-accept), with pending reply as the M1 upgrade increment (the transport lockstep snag and the correction to this ADR's decision 1 "send replies" wording are in ADR-017).

---

## ADR-016 Cell Acceptance: Watermark, Overload, and Companion Rules (acceptance-queue watermark and overload, including the ②-⑤ convergence)

**Status:** ruled (2026-10-10 inspection follow-on authorization; decision ② adjudicated per the queue-watermark prerequisite design's candidate lean; the ③ read-only fast path / ④ Manager merge / ⑤ shutdown sequencing companion semantics converge together).

**Background**: ADR-015 decision 3 left four items pending (② watermark / ③ fast path / ④ Manager / ⑤ shutdown). The watermark-and-overload decision face already has the queue-watermark prerequisite design on the books (a three-way fork: two-tier / four-tier full set / unbounded; overload responses: callback error codes / a new contract message / silence; numeric semantics: tick-budget anchoring).

**Decision**:

1. **Watermark form = (a) two tiers** (an alarm line + reject at cap; no dropping of old entries, no silence) — (b) the four-tier full set is client-experience semantics misplaced into a process (the acceptance face has no consumer-family tiering demand) and (c) unbounded queueing = a memory bomb (net-abstraction §5.7's explicit discipline); neither is taken.
2. **Overload response = (A) callback error codes** (a typed overload=queue-full, reusing §5.7's "typed reasons, no guessed throws on the caller's behalf" philosophy, zero new protocol); (B) a new contract internal-domain message is left for evaluation in the M1 batch (one in-process hop needs no wire face; a seam is left but not pre-spent); (C) silence is rejected on counter-evidence.
3. **Watermark numbers = tick-budget anchored** — starting from the ratio queue depth × per-request handling time ≤ the tick budget (100ms@10Hz, clock-and-time §4); the static cap takes a conservative value first, calibrated to the same shape as capacity §2.1's three-model measured anchors once the benchmark hooks into the acceptance face.
4. **③ the read-only fast path is not built** — full enqueueing is the single form; a 1-tick delay for PING/queries is harmless, and every extra fast path is one more concurrency form to audit. **Aftermath errata (2026-10-10 / ADR-017)**: before M1, PING is answered with a Pong at the acceptance point (a pure wire echo on the worker side with no world access face — not a fast path) — under the current transport lockstep, a Pong that is enqueued has no send path to go out on; once M1's delayed replies land, the Pong is enqueued with the queue.
5. **④ the three Manager mutexes are kept** — WorldSessionManager/AnchorManager are already in the "cross-thread minimal set" form (a small data face, low contention intensity), and the migration gain does not outweigh the reshape risk; SessionLocator is untouched; the single-writer closure covers only the World/Scene graph.
6. **⑤ the shutdown sequencing = the cell-app mirror of G-3's five phases** — stop accepting (cut the stream, reply "under maintenance", the same shape as base-app's `dispatchRequest` maintenance gate) → queue drain → gameThread join → worldHost stop.

**Consequences**: when the acceptance-face batch (intent envelopes + queue + tick consumption + pending reply, cell-app feature code) lands it already carries the ③④⑤ companion semantics; the watermark numbers' benchmark calibration rides the acceptance face's entry into the capacity anchor system; the M1 receiving-end wiring only swaps the transport base (the acceptance face = the skeleton, reused directly). **Aftermath errata (2026-10-10 / ADR-017)**: the acceptance-face batch's reply face = the acceptance-point receipt (ack-on-accept, including the PING answered-with-Pong-at-the-acceptance-point special case — see the decision 4 note), with pending reply as the M1 upgrade increment.

---

## ADR-017 Cell Acceptance Reply Wiring: Ack-on-Accept Now, Deferred Reply at M1 (acceptance-face reply wiring: the acceptance-point receipt comes first, delayed replies follow M1)

**Status:** ruled (2026-10-10 inspection follow-on authorization, adjudicated per the cell-single-writer decision ⑥ candidate lean — two-stage wiring: ack-on-accept now + delayed replies as the M1 upgrade; (iii) the blocking adaptation is rejected).

**Background**: as the acceptance-face batch (ADR-015 (a)) began its sounding, it hit a transport-layer snag — (a) requires "the worker only decodes + enqueues, and `gameThread_` consumes, executes, and **sends replies** at tick boundaries" (delayed replies), but the current transport cannot realize this:

- `RepSocket::workerLoop` (socket.cpp:139-172, nng_rep0) is strictly lockstep: `nng_recvmsg` → `handler_(data)` synchronously returns the reply bytes (socket.hpp:119, RequestHandler = `std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>`) → send → the next recv round. The reply must be produced synchronously by the same thread that recv'd, and the worker cannot take the next request before the handler returns — delayed replies cannot be realized.
- **Stub-transport facts**: :165 `if (!response.empty())` skips sending on an empty reply — today's change-class handlers return `{}`, and under a real nng_rep0 the REP state machine deadlocks when owed a reply; the whole repo runs the stub branch (:431-488) for exactly this reason (after the nng-dependency-unban assessment concluded it would be a transport-rewrite-scale break, it was rolled back and the module stays disabled; the M1 homegrown kernel awaits landing — contract-difference debt clearing §34.3 / the P1-6 batch record).
- The consumption face today: real consumers = gateway forwarding (one-way `sendPayload`, the ADR-011 half-finished shell) + protocol unit tests + liveness; change classes return `{}` and PING is answered with a Pong; **no result-carrying reply** exists.

Three candidate leans (cell-single-writer §5 decision 6): (i) wait for the M1 transport / (ii) the ack-on-accept adaptation / (iii) the blocking adaptation ((b)'s synchronous wait in a new skin).

**Decision**:

1. **The wiring in force = (ii) ack-on-accept**: after the worker decodes + validates + enqueues (an intent envelope, through ADR-016's two-tier watermark gate) the **acceptance point returns a receipt immediately**; gameThread_ consumes and executes at tick boundaries, and results do not land in replies (no result-carrying caller exists today). Change classes return an empty ack (= the current handler return value; byte-identical to today under the stub transport); overload returns an overload error packet (ADR-016 #2; the loop closes at the acceptance point). The race closure is decoupled from the transport rewrite — the race is the P3-2 batch B fix target and does not wait for M1.
2. **(iii) the blocking adaptation is rejected**: the worker blocking on a condvar for the reply after enqueueing = plan (b)'s mechanism. Three counter-evidence items: (a) with a single request in flight → queue depth is permanently ≤1, making the entire ADR-016 watermark machinery (two-tier alarm/reject at cap) dead code; (b) RPC throughput collapses to the tick rate (≤10/s) — the original ADR-015 §3(b)/§4 verdict that abandoned (b) over "head-of-queue blocking × tick duration"; (c) it directly conflicts with ADR-015's already-ruled "take (a), drop (b)".
3. **PING's Pong is answered at the acceptance point** (a pure wire echo on the worker side; it does not touch world_) — ③'s "no fast path" ban targets concurrency faces that bypass the queue to touch the world; the Pong echo has zero world access and does not constitute a second concurrency form. Moreover, if the Pong were enqueued, gameThread_ would have no send path to answer on (:165 + the lockstep double lock). Errata to ADR-016 #4's "full enqueueing" for the Ping special case: once M1's delayed replies land, the Pong is enqueued with the queue.
4. **Delayed replies (the original (a) wiring) come with the M1 upgrade**: once InterServerLink decouples recv/send, add the pending-reply association face (carried by header.sessionId) and the gameThread_-side send path; the acceptance-face skeleton (intent envelopes/queue/watermark/tick consumption/single writer) is discarded at zero — cell-single-writer §6's "M1 only swaps the transport base" semantics unchanged. ack-on-accept is not a replacement endgame for (i); it is the preceding stage of (i); a real-transport REP lockstep demands one non-empty reply frame per request, and the ack frame type is defined with the M1 contract batch.
5. **Errata to the candidate-lean semantics**: decision ⑥'s recorded original lean — "(i) is the true master copy (the transport-layer root cause; adaptation is a detour)" — targets the **endgame wiring**; the ruling keeps (i) as the endgame at M1. The "detour" charge does not stand against ack-on-accept: it closes the race, preserves wire semantics, and its skeleton is reused by M1 — it is decoupling, not a detour.

**Consequences**: the acceptance-face batch (intent envelopes + queue + tick consumption + the acceptance-point receipt, cell-app feature code) has a closed design and starts once the docs/scripts boundary opens (the decision face fully adjudicated: the ADR-015 main plan + the ADR-016 companions + this ADR's reply wiring); the watermark-and-overload batch rides the same batch as the acceptance-face batch; the M1 wiring batch gains a "delayed-reply upgrade" increment (the pending-reply association face + the gameThread_ send path) with zero rework of the acceptance face; the gateway forwarding / unit-test / liveness consumers' wire behavior stays byte-identical. The other RepSocket consumers (login-app/base-app/baseappmgr) with synchronous reply semantics are unaffected, and the M1 upgrade path applies in the same shape across all protocol modules.

**Evidence**: modules/protocol/src/socket.cpp:139-172 (workerLoop lockstep), :165 (empty reply skips sending), :431-488 (the stub branch), :101 (nng_rep0_open); modules/protocol/include/apollo/protocol/socket.hpp:119 (RequestHandler returns synchronously); apps/cell-app/src/cell_server.cpp:157-210 (acceptRequest's acceptance-point receipt: change classes `{}` / PING Pong); docs/design/cell-single-writer.md §3(a) / §5 decision 6; todo.md's P1-6 batch record (nng not installed, both trees run the stub) and contract-difference debt clearing §34.3 (the nng-unban assessment rollback, M1 re-estimated with the P3-2 batch); ADR-015 §3(b)/§4 (the original verdict abandoning (b) over head-of-queue blocking).

---

## Related Docs

- [Architecture Overview](/architecture/overview) — the seven-layer downward data flow (the ADR's layering perspective)
- [Architecture Audit and Current State](/analysis/architecture-review) — the audit's full evidence base
