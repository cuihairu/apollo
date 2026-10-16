[English](backup-revive.en.md) | [中文](backup-revive.md)

# Backup / Disaster Recovery and Outage Takeover Design (backup-revive)

> Status: **design draft (preliminary design, G-2 initiation batch, 2026-10-09; all six decision points ruled 2026-10-10 — Option A goes first, see §4 and ADR-010)**. Positioning: the process-level high-availability surface — candidate topologies, arbitration semantics, and takeover sequencing for "service stays up" after an authority process dies; the boundary with attribute-sync §8.2 follows the existing scope statement: the journal answers "data is not lost", this document answers "the process does not fall over". Gap registration: **G-2 backup/disaster-recovery and outage takeover** (leftover registration in architecture-review §16.4; the "P3 preliminary design" G-2 paragraph in net-abstraction §7 is the existing skeleton semantics, which this document expands into decidable candidate topologies). Cross-references: net-abstraction (§7 G-2 paragraph / §5.7 InterServerLink / §6 sdshmem decision row), attribute-sync (§8.2 scope statement / §10.2 shutdown sequence / §11 acceptance table), session-and-online-directory §6 (recovery matrix), clock-and-time (§6 cross-process clocks not mutually trusted / §7 dual timestamps), capacity-and-benchmark (§1 capacity targets / §5 metric set), crash-capture (forensics cross-verification surface), concept-glossary "hot standby and takeover" entry, architecture-review (§16.2/§16.4/C-51/C-52/§30 three simplification cuts), doc 36 (§0 evidence grading / question 5 / question 7), battle-instance-offload §7 (independent decision on the instance failure domain).

---

## Executive Summary

1. **Why G-2**: death of a single-writer process = the authority vanishes instantly — the journal guarantees zero data loss (attribute-sync §8.2, P1-4/P1-5 delivered), but "data present, service down": between process death and recovery, every session and scene it carried is stalled, and volatile state (L3) has no handling semantics. Of the three precedents only BigWorld has a complete layer here (C-51/C-52: the real generation gap between those two engines lies in the **backup and takeover chain**, not the row-store shape); KBE's cost of lacking this layer = rollback to the last archive window on outage (300s-class), skynet is single-node with no fleet.
2. **What already exists (this design only consumes, does not rebuild)**: the skeleton of Option A (cold restart + persistent state + reconnect) is **already in the repo** — machined supervision surface (roster + linear-backoff restart + give-up-over-limit + four-type death events), cross-process death-event reporting (G-1 closing batch increment ①), cross-process recovery phase (FleetRecoveryCoordinator: exclusive phase + full re-report + dual-criteria convergence), directory mirror ("APD2" wire family), PlayerDirectory (anchor_epoch adjudication key + Suspended window + batch reverse lookup), PersistJournal (append/drain/replay/compact), RecoveryCoordinator (restore→admission→ready), baseappmgr admission gate and `--recovery-*` wiring. G-2's real increment is in only two places: **how state reaches the takeover party** (mirror stream / dual-write) and **who adjudicates the takeover** (arbitration surface).
3. **Mechanism dissection of the BW precedent (A-grade direct source reading, 2026-10-09 this round)**: the backup sender spreads framed writes across ticks (`backupRemainder_` fractional remainder carried across ticks) + two-generation backup-hash coexistence with switch-over (`entityToAppHash_`/`newEntityToAppHash_` + `isUsingNewBackup_`) + `BackupHashChain` generational redistribution (fleet scale-up/down never drops backups) + reviver as an independent process (launched on request via machined, machined tags restrict revivable component types) + secondary db backfill. BW built the backup chain into an independent subsystem deeply coupled with scaling — twenty years of evolution left both the benefit and the complexity on the record.
4. **apollo skeleton semantics (existing registration, expanded by this document into candidates)**: hot-standby stream = the second read-only consumer of PersistJournal (journal serves three uses from one write path — recovery position / hot-standby mirror / persistence queue; no separate backup protocol); the standby's acked journal position is the takeover starting point; takeover = replay from position, then hand over authority at a tick boundary (the mirror path of the attribute-sync §10.2 shutdown sequence); **no independent reviver process** — reviver semantics fold into the baseappmgr domain (architecture-review §30 three semantic-merge cuts, existing registration). Whether the existing net-abstraction §7 skeleton line "backup-hash chain + reviver first, secondary db / dynamic scaling deferred" holds is for decisions 1/2 to confirm or rewrite — **rewritten (ADR-010: no backup-hash chain first, no reviver; see §4)**.
5. **Anti-evidence must be honest**: apollo's capacity targets are 5000 CCU per process and 8GB per machine (capacity-and-benchmark §1/§4); the P3 fleet is a small topology of seven process classes — BW's hot-standby chain addresses routine single-point death in seamless-world cross-machine large fleets (World of Tanks 190k CCU scale, C/D); its blast radius is a different order of magnitude from an apollo single-baseapp death. Contemporary small/mid-deployment mainstream practice (D-grade, industry pattern) = process-manager auto restart + persistent state + client reconnect; hot-standby adoption concentrates in seamless large worlds and strong-continuity gameplay. Option A's RTO = seconds-class, RPO = zero (journal zero-loss is already a §11 metric); hot standby's real increment = RTO seconds→sub-second + volatile-state retention, at the cost of a resident standby + mirror-stream bandwidth + dual-primary arbitration complexity. **In a single-machine fleet the standby dies on the same machine — decision 1 is in substance a deployment-shape decision, not a pure technology choice.**
6. **Three candidate topologies** (§3): A cold restart + persistent state + reconnect (fully reuses delivered pieces, increment ≈ 0); B hot-standby process + state sync (same shape as BW backup-hash: backup stream + position ack + takeover arbitration); C no standby, state dual-write (journal remote dual-write / replay rebuild — no resident standby, RPO decided by the sync policy; active-active replicas conflict with this repo's single-writer discipline, honestly recorded as a not-adopted shape). The three options share one foundation: journal position anchor + epoch arbitration + recovery-phase exclusivity + machined launch — the difference is only the path by which state reaches the takeover party.
7. **Six decision points** (§4, ruled 2026-10-10 / ADR-010): Option A first (restart-only), three takeover tiers, Option A has no dual primary and creates no new arbitration, RPO=0 / RTO≤3s (batch 0 measurement pinned the numbers), no mirror stream, acceptance chain = batch 0 drill. The original "no deciding on others' behalf, no candidate preference" discipline was in force until the ruling; the adjudication record is in §4 and ADR-010.
8. **Phasing** (§6): batch 0 (decision-independent) delivered — kill -9 drill acceptance chain + measured RTO figures into the books, i.e. the complete form of Option A; **decision = Option A; B/C's three later batches (position surface / mirror stream / takeover arbitration) are all cancelled, G-2 closes** (reopening condition = production data demanding sub-second RTO / multi-machine shape initiation).

## 0. Scope and Non-Scope

- **Status statement (explicit, following net-abstraction §7 semantics)**: in apollo's single-process stage there is **no process-level high availability** — crash safety = write-behind journal zero data loss + restart relaunch; backup/takeover/migration starts from zero. This is phasing, not omission.

| Dimension | In scope (this design owns) | Out of scope (existing pieces / other designs own) |
|---|---|---|
| Crash post-mortem forensics | only consumes machined death rows (exit 128+signo) cross-checked with dumps on disk | collection and symbolization proper = crash-capture.md (delivered) |
| Process relaunch/restart | linkage semantics between Supervisor events and the recovery phase | fork/exec + backoff restart proper = apps/machined (batch F delivered) |
| Data not lost | the **consumption semantics** of the journal position as takeover anchor | write-behind/journal/replay proper = attribute-sync §8.2 (delivered) |
| Single-machine dev fleet | Option A already covers — this design only adds the drill acceptance chain (batch 0) | scenarios with no standby benefit (the standby dies on the same machine) |
| Multi-machine production fleet | Option B/C mirror stream and arbitration surface (pending decisions 1/5) | cross-machine transport substrate = net-abstraction §5.7/§7 (M1, not landed) |
| Single-player disconnect/reconnect | only bounds the window interaction with process-level takeover (decision 3) | resume/grace window = net-abstraction §3 + session-and-online-directory §4 (delivered) |
| gateway death | consumption-surface confirmation of directory window handling | window handling proper delivered (§6 death-row leftover batch) |
| Seamless-world cross-process entity takeover | **not applicable** — ghost vetoed (doc 36 #9), no cell-boundary migration problem | — |
| Battle instance processes | one row in decision 2's granularity table | instance crash adjudication = battle-instance-offload §7 (independent decision, neither swallows the other) |
| Stateless processes (login/verifier/interfaces) | one row in decision 2's granularity table (restart = recovery) | verifier has no G-2 hot-standby need = battle-verification-service §5 existing semantics |

## 1. Precedent Evidence and Anti-Evidence (A/B/C/D grading per doc 36 §0)

### 1.1 BigWorld's full backup/takeover suite (A-grade local source reading)

- **Backup sender** `server/baseapp/backup_sender.hpp`: `autoBackupBase`/`backupBase` framed sending (architecture-review §16.2 verified :52-61); each tick backs up only `offloadPerTick_` bases, the fractional remainder `backupRemainder_` carried across ticks (backup_sender.cpp:114-117, already cited in clock-and-time §7); `handleBaseAppDeath`/`restartBackupCycle`/`setBackupBaseApps` — the hooks for primary-death awareness and backup-cycle rebuild all live in the sender (this round's reading of the same file, 40-70 region).
- **Backup chain data plane** `lib/server/backup_hash.hpp`: `MiniBackupHash` (prime/size/virtualSize, `hashFor(EntityID)` — the header comment self-describes "handles changing hash sizes nicely…keep most values hashing to the same bucket") deriving `BackupHash` (entity id → baseapp address); `backup_hash_chain.hpp`'s `BackupHashChain` — the chained structure for hash generational redistribution on fleet scale-up/down (baseapp.hpp:38/:237/:450 hold the chain). **Two-generation hash coexistence with switch-over**: backup_sender.hpp's `entityToAppHash_`/`newEntityToAppHash_` + `isUsingNewBackup_` + `ackNewBackupHash()` — during redistribution the old and new mappings run in parallel, switched wholesale after per-entity confirmation (the double buffer that keeps backups alive across scaling; this round's reading).
- **Standby side** `server/baseapp/backed_up_base_app` and `offloaded_backups`, two families — the mirrored-entity records held by standbys and the "after offload" backup ledgers (file inventory, this round's reading).
- **Takeover side** `server/reviver/` independent process (same level as baseappmgr/cellappmgr/dbappmgr in the server/ top-level inventory; component_reviver/reviver/reviver_config/reviver_interface + main, five pieces): `Reviver::revive` requests the dead process's launch via machined (reviver.cpp:439-446); `queryMachinedSettings` reads machined tags to restrict the revivable component types on this machine (:272-296); `handleTimeout` polls by `ReviverPriority` type (:300 region). Takeover semantics = after the primary dies, the standby promotes its mirrored entities to authority; on cell death, cellappmgr rebuilds the cell on surviving CellApps (B-grade registration: architecture-review §16.4 G-2 row and the baseappmgr domain block).
- **Persistence secondary plane**: the secondary db task family + dbappmgr scaling hash redistribution (dbappmgr.cpp:472/:637, B-grade verified).

### 1.2 KBE/skynet negative precedents (B-grade registration)

- **KBEngine lacks this layer** (C-51): no journal, no hot standby, no takeover — outage recovery = the Archiver's last archive window (default 300s, not dirt-driven); the player loss window = the archive interval, all sessions drop; of the nine process classes (`kbe/src/server/` inventory) none corresponds to a reviver (architecture-review :2689 authoritative row). KBE's lesson is "the cost of lacking this layer", **not a simplification to copy** — what it simplified away is precisely G-2 itself.
- **skynet**: single-node service orchestration framework; fleet/takeover has zero built-in support; cluster is a message layer, not a disaster-recovery layer (B-grade registration).

### 1.3 Anti-evidence (honest record)

- **Capacity semantics**: 5000 CCU per process, 8GB per machine, steady-state RSS ≤4GB (capacity-and-benchmark §1/§4); the P3 fleet = a small topology of seven process classes (machined/baseappmgr/login/gateway/baseapp/interfaces/verifier, mostly one instance per class). The blast radius of a single baseapp death = that baseapp's scenes and sessions; the recovery chain of machined backoff restart + journal replay + directory full re-report takes seconds (directory rebuild <1s @ 5000 entries, session-and-online-directory §6). BW's hot-standby chain is built for the shape where a seamless-world cross-machine large fleet runs double-digit baseapp instances and single-machine death is a routine event (190k CCU scale, C/D) — different shape premises, different orders of magnitude.
- **Industry pattern (D-grade, doubt flagged)**: contemporary small/mid-deployment mainstream = process-manager auto restart + persistent state + client reconnect (same shape as the container-orchestration restart-on-fail convention); instance/zone-based game server ecosystems generally accept "restart + re-login" (EQEmu/TrinityCore ecosystem semantics, D). Hot-standby adoption in MMOs concentrates in seamless large worlds and strong-continuity gameplay. apollo's target gameplay is instance/zone-based MMORPG (doc 36 #9).
- **BW's own complexity on the record (A-grade)**: two-generation hash coexistence switch + BackupHashChain generational change + dbappmgr hash redistribution = the engineering cost of a backup chain deeply coupled with scaling. If apollo takes Option B, the scaling surface can follow the existing registration and stay deferred (secondary db / dynamic scaling postponed), but "mirror stream + position ack + arbitration" are Option B's irreducible core three.
- **In a single-machine shape the standby dies on the same machine**: hot standby only has incremental value across machines; decision 1 (deployment shape) logically precedes decision 5 (transport shape).

## 2. Existing-Piece Consumption Surface (this design redesigns nothing; mapped piece by piece)

| Existing piece | Location | How G-2 consumes it |
|---|---|---|
| machined Supervisor | apps/machined/src/supervisor.hpp/.cpp | roster roster (`name \| component_id \| zone_id \| cmd args...`) + fork/exec launch + linear-backoff restart (give up over limit) + four-type death events — Option A/C's execution half is delivered; if Option B initiates = roster gains a standby role column, the supervision surface itself untouched |
| Cross-process death-event reporting | modules/net/discovery (G-1 closing batch increment ①, op 6/7) | DeathSubscribe/DeathNotify — the **sole event source** triggering takeover; G-2 adds no heartbeat/death-detection surface |
| FleetRecoveryCoordinator | modules/game/session/fleet_recovery.hpp/.cpp | Normal↔Recovering exclusive phase + FullReport fragment reassembly + dual-criteria convergence (all reported or timeout-open) — the baseappmgr-death recovery phase is delivered; a baseapp-death takeover window, if initiated = same-shape phase reuse |
| DirectoryPublisher/Mirror | modules/game/session/directory_mirror.hpp/.cpp | "APD2" delta/snapshot wire + seq resume — after takeover the directory surface converges automatically with the baseappmgr rebuild, zero G-2 change |
| PlayerDirectory | modules/game/session/player_directory.hpp/.cpp | anchor_epoch (idempotency key + contention adjudication key) + Suspended window + `mark_suspended_by_zone/gateway` batch reverse lookup — the adjudication basis for rejecting the old primary's late messages by epoch after takeover |
| PersistJournal | modules/data/orm/persist_journal.hpp/.cpp | append/drain/replay/compact + `next_sequence()` — the source of the hot-standby mirror stream (second read-only consumer, single-writer discipline unbroken) and the takeover replay position (journal sequence is the position ruler) |
| SaveQueue/autoSaveLoop | apps/base-app (base_server) | write-behind consumption surface + five-stage shutdown (attribute-sync §10.2 delivered: CAS idempotency + producers stop first + drain timeout releases) — the authority-handover sequence = the mirror path of the shutdown sequence, part-level reuse |
| RecoveryCoordinator | modules/game/session/recovery.hpp/.cpp | restore→admission→ready startup sequence + three-tier recovery checklist (lifecycle §3 matrix in code form) — one startup order for restarters and takers-over alike |
| baseappmgr admission gate | apps/baseappmgr (`--recovery-port/--recovery-expected/--recovery-timeout-ms`, `--suspend-window-ticks`, `set_admission_gate`) | recovery-phase exclusivity wiring delivered — the takeover period reuses the same gate to reject new; convergence-open immediately emits the rebuild snapshot to splice into the mirror surface |
| TimerWheel | modules/base/timer_wheel | clock-feed drive / single writer — the timing piece for takeover timeouts, mirror-lag alarms, and arbitration deadlines (deadline = tick number, clock-and-time §8) |
| InterServerLink | net-abstraction §5.7 (design state) | authority_epoch handshake triple = peer-restart criterion (≠ disconnect); in-flight on disconnect partitioned by invoke_mode — G-2's mirror stream is already named as an existing consumer of that facility; no second API surface opened |

## 3. Candidate Topologies

### 3.0 Common foundation (shared by all three options)

Takeover anchor = journal position, **not a time point** (clock-and-time §6: cross-process clocks are not mutually trusted — dual-primary determination and takeover sequencing forbid wall-clock deltas; determination order goes epoch/position/baseappmgr serialization); authority handover = tick boundary (reusing the §10.2 shutdown-sequence mirror path: first stop the old primary's write recognition, then hand over); event source = machined death events (sole); launch = machined; recovery-period exclusivity = the recovery phase (delivered). **The three options differ in exactly one thing: how state reaches the takeover party.**

### 3.1 Option A: cold restart + persistent state + reconnect (restart-only)

- **Shape**: machined detects death → backoff restart → RecoveryCoordinator restore (journal replay) → admission → ready → baseapp/gateway full re-report convergence → client resume/re-login. New players rejected during the recovery phase.
- **Dependencies**: full reuse of delivered pieces; zero new processes, zero new protocol surface.
- **Cost**: ≈0 (only the batch 0 drill acceptance chain and RTO observation metrics remain).
- **Coverage**: persistent state zero loss (journal); directory rebuild <1s @ 5000 entries; volatile state (L3: position/temporary buffs) zeroed per existing semantics — player-side appearance = disconnect reconnect, position back to the last persisted state.
- **Failure semantics**: RTO = backoff (1s-class) + restore + re-report ≈ seconds~tens-of-seconds (order-of-magnitude semantics, calibrated by batch 0 measurement); during recovery the process's carried surface is stalled (the exclusive phase prevents the topology changing again); restart over the limit (5 attempts) gives up = manual-intervention surface.

### 3.2 Option B: hot-standby process + state sync (same shape as BW backup-hash)

- **Shape**: the standby resides, consuming the backup stream (PersistJournal's second read-only consumer, RO_MIRROR shape) → periodically acks the backup position; baseappmgr detects primary death (death event) → arbitrates the standby's takeover → the standby replays the tail difference from its own position → hands over authority at a tick boundary → epoch advances → SessionUp re-reports into the directory.
- **Dependencies**: InterServerLink M1 (mirror-stream carrier, not landed); journal cross-process consumption rework (currently a single-process SaveQueue worker); position-ack protocol (internal-domain id 900+ segmentation); dual-primary arbitration surface (decision 3); machined roster standby role column.
- **Cost**: resident standby compute (× number of process types within the takeover granularity); mirror-stream bandwidth (journal write path amplified one order); position protocol + arbitration development; if scaling joins later = the counterpart of BW's two-generation hash switch (deferred surface, deferred by existing registration).
- **Coverage**: RPO → 0 (provided the mirror stream covers the journal in full and acked positions are not lost); volatile state can be included (extending the mirror range to L3 raises bandwidth again — decision 4); RTO sub-second~seconds (tail replay is very short).
- **Failure semantics**: **dual primary is a new failure mode** (old primary false-death return / network partition) — arbitration must precede takeover with a unique criterion (decision 3); changes within the mirror-stream lag window are invisible to the takeover party (RPO upper bound measured by the acked-position gap; metricization goes to batch 1); the standby's own death degrades to Option A (the supervision surface relaunches and rebuilds it; full mirror-stream replay rebuilds the standby — unprotected during rebuild).

### 3.3 Option C: no standby but state dual-write

- **C1 journal dual-write**: journal appends mirrored synchronously or asynchronously to a remote persistent surface (peer process / shared storage — deployment shape per decision 5); the takeover party (a surviving process or a machined-launched new process) replays from the remote journal. No resident standby, no mirror-ack surface.
- **C2 cross-process key-state replicas (active-active state-machine replication)**: directly conflicts with this repo's single-writer discipline (two writable places = dual authority — single-writer's inter-process corollary, same argument as session-and-online-directory §1) — honestly recorded as a **not-adopted shape**, listed only for completeness.
- **Dependencies (C1)**: remote persistent surface selection (a deployment decision); replay surface = existing replay; position reconciliation (local/remote journal sequence consistency check).
- **Cost**: medium — no resident standby, but the journal write path gains a hop; synchronous dual-write brings remote latency into the tick budget (persist-batch segment, capacity §2); asynchronous dual-write leaves an RPO window.
- **Coverage**: RPO decided by the dual-write sync policy (sync = 0, async = window); RTO same class as Option A or slower (replay distance may exceed the local journal's).
- **Failure semantics**: no dual primary (writes only in the journal owner); **a new remote-storage failure surface**; local/remote journal consistency rests on position reconciliation, mismatch handling needs definition (snapshot reset, or refuse takeover).

### 3.4 Three-option comparison

| Dimension | A cold restart | B hot standby | C dual-write |
|---|---|---|---|
| New processes | 0 | standby ×N | 0 |
| New protocol surface | 0 | position ack + takeover command | journal mirror writes + position reconciliation |
| RPO | 0 (journal zero-loss, existing metric) | →0 (acked position) | policy-decided (0~window) |
| RTO order | seconds~tens-of-seconds | sub-second~seconds | seconds~tens-of-seconds (with replay distance) |
| Volatile state (L3) | zeroed | includable (bandwidth extra) | zeroed |
| New failure modes | none | dual primary / mirror lag | remote-storage failure |
| Dependency maturity | all delivered | InterServerLink M1 not landed | deployment surface undecided |
| Relation to existing registered semantics | directly compatible with "explicitly no HA in the single-process stage" | unfolds in the same direction as net-abstraction §7's "backup-hash chain + reviver first" | no existing registration, a brand-new surface |

## 4. Decision Points (all six ruled 2026-10-10 — Option A first, standby not initiated; full ruling in ADR-010)

The six items below were originally a decision-surface enumeration (while suspended: no deciding on others' behalf, no candidate preference); the 2026-10-10 patrol-authorized batch ruled them per this document's anti-evidence section + batch 0 measurements, item by item:

1. **Deployment shape and standby necessity** → **Option A first (restart-only), standby not initiated**. In a single-machine fleet the standby dies on the same machine (§1.3 anti-evidence); under the 5000 CCU single-machine target, Option A's seconds-class RTO / zero RPO already suffice; Options B/C get no phased reservation; if production data demands sub-second RTO, initiate separately. net-abstraction §7's "backup-hash chain + reviver first" is rewritten as "no backup-hash chain first, no reviver"; architecture-review §30's "no independent reviver process" is confirmed and maintained.
2. **Takeover granularity per process type** → **three tiers by process type** (no single unified semantics): stateful-rebuild tier = baseappmgr (full re-report, existing) + baseapp (cold restart + journal replay + re-report convergence, measured in batch 0); stateless-restart tier = login-app / verifier (worker pools); crash-voids tier = Battle instances (ADR-012).
3. **Arbitration and epoch rules** → **Option A has no dual primary, no new arbitration protocol**; epoch follows directory_mirror's existing semantics (monotonic increase + stale-epoch rejection); baseappmgr's sole-arbiter serialization is maintained; recovery-phase / takeover-window exclusivity order follows the existing session-and-online-directory §6 matrix.
4. **RPO/RTO targets** → **RPO = 0** (journal write-ahead zero loss, the attribute-sync §11 existing metric promoted); **RTO = re-report convergence ≤3s / mirror convergence ≤3.5s** (batch 0 measured upper bounds under full-load noise, better when idle; dominant term = restart backoff 1s). Volatile state (L3) is not included in the takeover surface (reconnect falls back to the persisted state, capacity §5 explicit registration).
5. **Mirror-stream transport shape** → **not built**. No journal cross-process consumption rework (the current single-process SaveQueue worker is maintained); the two candidate shapes, same-machine shm SPSC / InterServerLink, stay on file, to be ruled when B/C initiate.
6. **Pilot and acceptance chain** → **acceptance chain = the batch 0 kill -9 drill chain** (delivered, scripts/drill_kill9.sh); dual-primary injection drills stay out of the gate (Option A has no dual primary). G-2's four terms are finalized with this ruling: the hot-standby/takeover entries are marked "not initiated (Option A first)".

## 5. Terminology Proposals (new words enter the glossary before the contract — this document changes neither tier)

The two naming anchors "hot standby and takeover — backup / reviver (G-2)" and "recovery phase" already exist; if this design initiates, the following **refined nouns** first enter concept-glossary for disambiguation, then get pinned (stored with the initiation batch):

| Chinese name (proposed) | English name | Code identifier (proposed) | One-sentence definition | Comparison | Banned / aliases |
|---|---|---|---|---|---|
| 热备进程 (hot-standby process) | standby | `Standby` (role enum / roster column) | a process role that resides consuming the backup stream, standing by to take over | BW = backup baseapp; KBE has none | ban: Slave/备胎/影子 (other engines' words and colloquialisms) |
| 接管 (takeover) | takeover | `Takeover` (event/message name root) | the act by which the taking-over party, after replaying from the backup position, is promoted to authoritative writer at a tick boundary | BW = backup-side authority promotion (the reviver only launches, never promotes) | ban: failover (another engine's term, not admitted); keep distinct from "restart" — restart = machined relaunch, no state migration |
| 备份位点 (backup position) | backup position | `backup_position` (u64 = journal sequence) | the journal sequence the taking-over party has confirmed caught up to on the backup stream — the replay starting point of takeover | ViewerState.acked_seq, same model (the inter-process instance of an attribute seq watermark) | ban: bare 「位点」 as a code identifier (narrative keeps using the word) |
| 热备流 (backup stream) | backup stream | `BackupMirrorStream` | PersistJournal's second read-only consumption stream (RO_MIRROR shape, writes only in the owner) | BW backup_sender framed stream; the fixed name for net-abstraction §7's existing colloquial "G-2 mirror stream" | ban: backup_sender (another engine's name); bare 「镜像流」 as a code identifier |

- **External metric vocabulary note**: RPO (recovery point objective) / RTO (recovery time objective) are industry-standard metric abbreviations — used on the document narrative plane, not admitted as code identifiers (metric semantics belong to document-narrative vocabulary; this is a boundary note). Dual primary (split-brain / dual-primary) serves as a failure-mode descriptor, same policy, document plane only.

## 6. Phasing (post-ruling path)

- **Before the ruling**: zero code — this document is a pure design piece (the source-freeze surface as usual); batch 0 does not depend on the ruling outcome.
- **Batch 0 (decision-independent) — acceptance-chain drill**: dev_fleet fleet kill -9 full chain (death onto wire → machined backoff restart → journal replay → RecoveryCoordinator ready → full re-report convergence → client resume) + measured RTO figures into the capacity-and-benchmark §5 metric set (a "process-recovery full-chain duration" sub-item added beside the "crash rollback ≤2s" row). Option A's complete form is exactly batch 0's end state — whatever the later ruling, this chain is both Option B/C's degraded fallback and the acceptance foundation. **Batch 0 delivered (2026-10-09)** — the scripts/drill_kill9.sh drill script (independent port-range fleet: machined --interval 500 --backoff 1000 + manager[recovery phase + mirror owner] + zone-app[report+demo-anchors 3], HiRes timestamps at line starts + grep polling assertions; the kill target located via `pgrep -x` exact process-name match — measured: `-f` full-command-line matching falsely hits the outer shell's command line [the roster text contains the same string]; the `ts` piped rows set $|=1 line buffering — block buffering would hold already-written lines inside stdio and time out the assertions; both pitfalls recorded as-is); both runs passed all chain assertions: death detection 14-55ms (exit=137 onto wire → [death] kind=child_died) → [window] zone death suspended=3 (suspend-window withholding) → backoff restart 1.04-1.10s (attempt 1 = 1×backoff) → recovery ready (journal replay empty + Anchor restore) → full re-report intake=3 (restore-not-kick, in-window recovery doesn't kick) → mirror entries 3→0→3 convergence (2.3-2.8s, upper bound = the quantized observation semantics of the 500ms snapshot period); honest notes: ① the initial-startup re-report races the manager UDP bind (fire-and-forget loss), convergence goes through the timeout criterion (5s) opening — a positive verification of the dual-criteria convergence design intent, not a defect; ② the journal replay/Anchor restore segment was an empty drill (demo anchors in-memory, no player files), the measured figure is a replay-time lower bound, loaded RTO re-testing pending decision 4's pinning; ③ the client-resume segment is directory-level (epoch/anchor recovery), the real session chain comes with the login/gateway batches. The RTO figures entered the capacity-and-benchmark §5 metric set's "process-recovery full-chain duration" row.
- **Batch 1 (after ruling B/C) — position surface**: journal `next_sequence` made cross-process visible (ack position messages into the internal-domain contract, id 900+ segmentation as usual, the contract stored with the implementation batch); mirror-lag position-gap metric into MetricRegistry; RPO measurement semantics into the capacity §5 metric set.
- **Batch 2 (after ruling B) — mirror stream**: transport shape per decision 5 (same-machine shm SPSC / InterServerLink cross-machine); `BackupMirrorStream` consumption wired to the journal (second read-only consumer, single-writer discipline verification into tests); standby role lifecycle (roster standby column + zero-supervision-change verification); the full-replay rebuild path for a late/restarted standby.
- **Batch 3 (after ruling B) — takeover arbitration**: dual-primary detection (epoch-mismatch criterion wiring, the criterion per decision 3); authority-handover sequence (§10.2 shutdown-sequence mirror: stop the old primary's write recognition first, then hand over; CAS idempotency reused); baseappmgr recovery-phase linkage (takeover-window exclusivity); dual-primary injection drill acceptance.
- **Batch 4 (after ruling C) — dual-write surface**: remote persistent surface wiring (shape per decision 5's deployment surface); position reconciliation (local/remote journal sequence consistency check + mismatch handling per decision 3); replay-distance metric.
- **Dependency order**: batches 1-4 all depend on net M1 (InterServerLink landing) and the outcomes of decisions 1/3/5; batch 0 has no dependencies and goes first.

## 7. Intersections with the Other Designs

| Related design | Landing points |
|---|---|
| net-abstraction | §7 G-2 skeleton paragraph = this document's upstream semantics (backup stream = journal second consumer / position as takeover start / tick-boundary handover — this document expands it into candidate topologies pending ruling); §5.7 InterServerLink (mirror-stream carrier + authority_epoch restart criterion + message classification declaration); §6 sdshmem decision row (existing candidate for the same-machine mirror stream) |
| attribute-sync | §8.2 scope statement (the "data not lost" vs "process not fallen" boundary — this document is the "process not fallen" surface); §10.2 five-stage shutdown (the mirror-path parts of the handover sequence; CAS idempotency / producers-stop-first reused); §11 crash-rollback metrics (the existing anchor of the RPO target) |
| session-and-online-directory | §6 recovery matrix, three rows (baseappmgr/gateway decided; the baseapp row's takeover semantics to decisions 2/3); anchor_epoch = the adjudication key after takeover (PlayerDirectory existing implementation) |
| clock-and-time | §6 cross-process clocks not mutually trusted (takeover anchor = position, not time point; dual-primary determination forbids wall-clock deltas); §7 journal dual timestamps (replay localization (tick, wall_ms) dual record as usual) |
| capacity-and-benchmark | §1 capacity targets (5000 CCU single machine 8GB — input to decision 1); §5 metric set (consumption surface of the RTO sub-item and RPO measurement); §4 memory budget (order re-estimate for Option B's resident standby) |
| crash-capture | dump + symbolized stack = post-hoc evidence, cross-checked with machined death rows (exit 128+signo) — G-2 only consumes death events, does not duplicate the forensics surface |
| battle-instance-offload | §7 failure domain (instance crash voids vs keyframe replay — decision 2's Battle row and that decision do not swallow each other) |
| battle-determinism | §5 recording format (the format source if Option C takes the intent-replay line — a separate line from journal replay: journal replay replays persistent changes, intent replay derives the determination order; not mixed) |
| battle-verification-service | §5 deployment semantics (stateless worker pool, no G-2 hot-standby need — existing basis for decision 2's verifier row) |
| architecture-review | §16.4 G-2 registration (this document is that gap's initiation design surface); C-51/C-52 (evidence: KBE lacks this layer / the generation gap is in the backup chain); §30 three simplification cuts "backup+reviver → journal position + manager recovery phase" (existing registration; decisions 1/2 confirm or rewrite) |
| concept-glossary | the two naming anchors, "hot standby and takeover" and "recovery phase"; the storage prerequisite of §5's terminology proposals (glossary disambiguation first) |
| scripts/dev_fleet.sh | the batch 0 drill chain's fleet driver (up/roster/status/down existing surfaces; the crashdumps/ and fleet-log directory coexistence convention per crash-capture §2.2) |

## 8. Release Update and Player Migration (added 2026-10-12)

> Positioning: answer "shipping a new version" on the existing takeover surface (§2 existing pieces + §6 phasing + ADR-010) — how players move from old-version processes to new-version processes. Two routes: route 1 exists today; route 2 hangs on later batches.

**Route 1: wait for disconnect-reconnect, then update (available today; do this first)**

- The chain = §6 batch 0's existing surface: old-version processes stop (kill) → machined backoff relaunch (once the new-version binary is in place, the restart is the new version) → recovery-phase exclusivity rejects new → all reporters converge on the full re-report (drill measured 2.71s) → anchors replayed from the journal → clients reconnect to the original process after disconnect.
- Benefit = **fast reconnect after disconnect** (reference: 火龙果/Huolongguo, the user's 2026-10-12 wording; no other benefits implied).
- Gap = during the update, WorldAssignment (placement assignment) must skip the process being updated — otherwise new players inside the restart window would be placed onto the old process about to die (machined would catch the restart, but the player suffers an extra disconnect).
- **Gap wired (2026-10-13)**: the drain gate (SIGUSR1 on / SIGUSR2 off → consumed on the 1s main-loop tick → `set_drain`; during drain, bindSession/assignWorld reject new but not the chain, apps/baseappmgr) + the fleetctl probe (against the baseappmgr M1 control plane, assign/resolve: exit 0 = admitted / 2 = drain rejects new / 3 = chain down) + an end-to-end drill script.

**Route 1 acceptance drill (measured 2026-10-13, both runs green)**

Command: `scripts/drill_update.sh` (topology = machined:19610 + baseappmgr:19013 (`--m1-port 19105`, probe surface) + base-app:19014 (`--demo-anchors 3`); seeds 3 real `player_N.json` files — flush rewrites based on the current file with roundtrip byte-identical, so per-player sha256 before/after comparison serves as the archive-consistency assertion). Segmented measurements (both runs listed):

| Assertion surface | Measured |
|---|---|
| Drain-gate signal→effective (USR1→ON / USR2→OFF) | 0.749 / 0.952s; 0.659 / 0.921s (≤1s main-loop tick; the wait must be ≥1s) |
| New-player placement during the update | fleetctl assign dual probes exit 2 (ASSIGN_REJECTED); within the drain window the mirror snapshot `entries=3 parts=1` stays constant = zero new placements |
| Online players dropped | **0** (after restart, full report `sessions=3 intake=3`) |
| Graceful stop (SIGTERM→child death exit=0) | 0.506 / 0.863s; shutdown log `journal pending: 0` (five stages: M1 line-down → shutdown_flush → journal drain) |
| Backoff restart (backoff 1s) | 1.511 / 1.868s |
| Recovery phase→full re-report convergence | 2.441 / 2.805s (compare kill -9 full chain 2.71s) |
| Directory mirror convergence (entries=3) | 2.442 / 3.808s (quantized upper bound = the 500ms snapshot period) |
| Archive consistency | per-player sha256 before/after **3/3 byte-identical** (not sampling) |
| Version-swap evidence | cp+mv atomic replacement (same-fs rename), running exe inode 14551279→14592675; after restart `/proc/<pid>/exe` inode = the swapped on-disk file. Default is a same-version redeploy (identical sha256, recorded honestly by the script) |

With this, §9.9 item 5 (placement exclusion) is fully wired; the remaining items (cross-process snapshot pull / gateway upstream buffering / epoch persistence) stay with the route 2 batch.

**Route 2: install the new version into a new baseapp, then switch players over (no disconnects / canary-capable; later batch)**

Three must-dos:

1. **Cross-process data fetch**: the new baseapp pulls the player snapshot from the old baseapp (the old side flushes first, then sends) — preserving ADR-007 zero external storage (player file = JSON atomic write + PersistJournal write-behind; no MySQL/Redis introduced).
2. **Write-authority handover**: adjudicated by the existing anchor_epoch increment — the new process holds the new epoch; the old process rejects the moved player's late receipts by the old epoch (the player_directory.hpp comment states exactly this semantics; takeover-window exclusivity prevents dual writes, §4 decision 3's existing ruling).
3. **Directory re-point + gateway routing snapshot refreshed immediately**: after the PlayerDirectory entry re-points to the new baseapp, the gateway's routing mirror must refresh immediately — current behavior = event delta pushed immediately + baseappmgr full-snapshot periodic fallback (`--mirror-snapshot-ms` default 10000, apps/baseappmgr/src/main.cpp:182); the switch semantics require the snapshot to be pushed immediately after the directory re-point, not waiting for the period to elapse (the trigger surface hangs on the later batch).

**Acceptance (this one criterion only)**: the player does not drop mid-switch, and the data is neither more nor less.

**Conclusion**: do route 1 first (can ship now); route 2 hangs on later batches, and before starting, the change list goes to the user first. — **§9 has promoted route 2 to the primary plan (user's 2026-10-12 decision); this section's historical wording is kept unchanged; the primary-plan/fallback division is in §9.1.**

## 9. Whole-Fleet Zero-Downtime Update (rolling version swap) (added 2026-10-12)

> Positioning: answer "fleet-wide version swap" on the existing takeover surface — player-level migration is the primary plan, disconnect-reconnect is the fallback. No conflict with §4's Option A (restart-only, ADR-010): the primary plan is an advance on top of Option A — players have been moved away before the update, so the swapped process holds no sessions, and Option A remains the primary plan's degraded fallback.
>
> **Primary plan (user's 2026-10-12 decision, verbatim)**: mark the old baseapp to stop taking new players → during drain, copy players to the new baseapp → gateway switches that player to the new baseapp → swap the old process's binary.
>
> **Primary plan in one sentence**: do not restart the processes carrying players; first move players one by one onto new-version processes, and only when they are all moved swap the old process's binary — when the old process swaps versions it holds no sessions, so the restart cost drops from "full re-report + player reconnect" to "empty-process relaunch".
>
> Discipline: this section adds no glossary entries (user's order); all wording is action description, not code identifiers. The only drafted code identifier (the migration-concurrency parameter, §9.3) is marked **TBD**, to be named at wiring time. All other engines' practices are "unverified" (§9.7).

### 9.1 State Machines (seven roles × five states)

Five state names (plain words given by the user 2026-10-12): **normal serving → stop taking new players → drain → swap binary & restart → recovery / full-re-report convergence**. Four columns: entry condition / exit condition / timeout / failure fallback.

**baseapp (the primary plan's protagonist)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | startup ready, or the previous recovery converged | the drain order arrives (the update order reaches this process) | — | — |
| stop taking new players | drain order effective (the directory side no longer places new players here — the gap wiring recorded in §8 route 1) | confirmed zero new-player placements (no new placements received within a snapshot period) | 10s (routing snapshot period fallback `--mirror-snapshot-ms` 10000, apps/baseappmgr/src/main.cpp:182) | order not effective = do not enter drain, return to normal serving and report up (never silently advance) |
| drain | confirmed no new-player placements | all in-flight players complete the five steps (§9.2) | no hard cap; non-convergence = escalate to manual | a single player's five-step failure = undo that player's ①②, return to the old process and keep serving, drain continues; consecutive failures over threshold = abort this round, the old process returns to normal serving |
| swap binary & restart | drain complete (no sessions in hand) | the new-version binary launched and ready | backoff × restart count (attempt N = N×backoff; at `--backoff` 1000, attempt 1 measured 1.04-1.10s), give up after 5 attempts | the new binary won't start = roll back to the old binary and relaunch (no sessions in hand; rollback cost = one relaunch) |
| recovery / full-re-report convergence | after the new version is ready, directory re-report | dual-criteria convergence holds (full re-report complete, or the timeout criterion opens) | drill measured 2.71s; timeout criterion 5s | timeout-open (the existing dual criterion — converge or timeout; batch 0 recorded it as design intent, not a defect) |

**cellapp (not covered by the primary plan; takes the fallback)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | startup ready | the update order reaches this cell (after all baseapps) | — | — |
| stop taking new players | the directory side stops placements | no new placements within a snapshot period | 10s | not effective = do not advance |
| drain | placements stopped | natural disconnects clear the field, or the wait cap is reached | **no cap by default — swap directly**. Players inside scenes do not leave on their own; the cell cannot be drained empty (the primary plan only moves baseapp anchors, not the Avatars on cells) | at cap = swap the binary directly; its players each get one auto-reconnect |
| swap binary & restart | cleared, or at cap | new version ready | backoff × n, 5 attempts then give up | roll back to the old binary |
| recovery / full-re-report convergence | new version ready | the dual criteria | 2.71s semantics | timeout-open |

cellapp is the only role among the seven that skips "drain" — its drain state is unreachable (players in scenes don't leave on their own), so when it swaps binaries its players each go through one auto-reconnect (the suspend window keeps their slots, the Avatar re-hangs from the anchor, L3 falls back to the persisted state — §4 decision 4's existing semantics). **This design's "drops = 0" figure is therefore scoped to migration-path players** (§9.8).

**gateway (untouched in this rolling round)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | untouched throughout (client TCP anchor + routing hub) | not in this rolling round | — | — |
| stop taking new players | **n/a** — the gateway does not place players; it holds every online player's client connection | — | — | — |
| drain | **n/a** — gateway drain = all online players disconnect = a downtime update | — | — | — |
| swap binary & restart | another window (outside this design) | — | — | — |
| recovery / full-re-report convergence | another window | — | — | — |

The gateway is the only role for which the "drain" state equals everyone dropping, so it takes no part in the rolling. Its version swap is another maintenance window's problem, outside this section.

**login (stateless tier, ADR-010 decision 2)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | startup ready | any time | — | — |
| stop taking new players | n/a (no placement semantics) | — | — | — |
| drain | **n/a** — login holds no sessions, it only processes login requests themselves; there is nothing to drain | — | — | — |
| swap binary & restart | any time | new version ready | seconds-class (no recovery phase, no directory re-report — a stateless restart is the recovery) | roll back to the old binary; or dual-run alternation (the new instance starts first, the old instance stops last) squeezes the window to zero — needs a double roster entry and different ports (deployment surface, noted) |
| recovery / full-re-report convergence | n/a | — | — | — |

**manager / baseappmgr (first stop)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | startup ready | the update order reaches itself (first stop) | — | — |
| stop taking new players | before the exclusive window opens, the admission gate closes (`set_admission_gate`, apps/baseappmgr) | the whole fleet quiescent (no process in a recovery phase) | — | — |
| drain | admission gate closed | confirmed no recovery phase in flight fleet-wide | 5s timeout criterion | recovery in flight = do not restart, wait for its convergence |
| swap binary & restart | inside the exclusive window | new version ready | backoff × n, 5 attempts then give up | roll back to the old binary |
| recovery / full-re-report convergence | new version ready | the dual criteria | drill measured 2.71s | timeout-open |

The manager's recovery phase (FleetRecoveryCoordinator Normal↔Recovering) is the whole fleet's **only serial gate**: at any moment at most one process may be in a recovery phase. The rolling update borrows this gate throughout — the exclusive window is the rolling order's metronome. The manager's own version swap is the first, and the only, "whole-fleet quiescent" window; after that, each role's binary swap takes its own exclusive window, serial, non-overlapping.

**machined (excluded from the rolling sequence)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | untouched throughout | not in this rolling sequence | — | — |
| stop taking new players | n/a | — | — | — |
| drain | n/a | — | — | — |
| swap binary & restart | **not feasible** — `terminate_all` SIGTERMs all living child processes (apps/machined/src/supervisor.cpp:249-255); swapping machined's version = the whole fleet dies together, in direct conflict with "zero downtime" | — | — | — |
| recovery / full-re-report convergence | n/a | — | — | — |

The two exits for swapping machined's own version: a separate downtime window (outside this design), or dual-supervisor handover (not initiated, later batch). This section does not put machined in the rolling sequence.

**logger (background, swappable anytime)**

| State | Entry | Exit | Timeout | Failure fallback |
|---|---|---|---|---|
| normal serving | throughout | any time | — | — |
| stop taking new players | n/a | — | — | — |
| drain | **n/a** — the log plane is a kReliable bounded queue + reconnect re-delivery (exactly once); a version swap = a brief stream gap, then re-delivery continues writing | — | — | — |
| swap binary & restart | any time | new version ready | backoff × n | roll back |
| recovery / full-re-report convergence | n/a (no directory) — re-delivery resuming writes is the recovery | — | — | — |

The logger is not on the player data path; players perceive nothing of its version swap.

### 9.2 Single-Player Five-Step Switch (the primary plan's core)

Every player strictly in order: **① freeze writes → ② pull new → ③ hand over write authority → ④ re-point the directory → ⑤ release**. The next step may not begin until the previous one is confirmed complete. No copy-while-playing — copy-while-playing produces both lost writes (the old process writes again during the copy) and dual writes (two processes writing the same player).

| Step | Action | Entry | Exit | Failure fallback |
|---|---|---|---|---|
| ① freeze writes | the old baseapp stops writing for this player (state writes and simulation stop; goes read-only); the gateway starts upstream buffering (player upstream held, no longer forwarded to the old baseapp) | the directory confirms the player is currently on the old baseapp | the old baseapp confirms writes have stopped | freeze ineffective = no copy starts; undo, then re-send the freeze; consecutive failures = skip this player this round |
| ② pull new | the new baseapp pulls this player's snapshot from the old baseapp (**the old side flushes first, then sends** — §8 route 2 must-do 1, preserving ADR-007 zero external storage) | the freeze is effective | the snapshot is complete and verified | pull failure/timeout = undo ① (the old process resumes writes, the gateway stops buffering), skip this player this round |
| ③ hand over write authority | `anchor_epoch + 1` handed to the new process; the old process discards the moved player's late receipts by the old epoch | the snapshot has landed on the new process | the new process confirms takeover (the new epoch is effective) | the new process refuses = undo ①② (the new process drops the snapshot, the old process resumes writes), skip this round. **After ③ is confirmed, irreversible** — the epoch is monotonic; the old epoch is already judged stale |
| ④ re-point the directory | the PlayerDirectory entry re-points to the new baseapp + **the re-point is pushed to the gateway immediately** (never wait for the 10s snapshot period) | the new epoch is effective | the gateway confirms the re-point effective (downstream routing points to the new process) | **do not revoke write authority** (③ is irreversible). Stay in the "authority handed over, routing not yet changed" state — harmless, because ①'s buffering covers the whole of ②③④ with no upstream in flight; retry until success |
| ⑤ release | the gateway stops buffering and releases toward the new baseapp (the buffered backlog drained in one go); the old process removes this player | the gateway confirms the re-point effective | the first upstream arrives at the new process and is accepted normally | same as ④: no revoke; the player stays buffered; retry cap (none by default) reached = escalate to manual |

**Two invariants** (this section's correctness argument):

1. **A unique authority at any moment**: before ③ the old process holds write authority, after ③ the new process does, with no overlap in between — hence no dual writes; the old process discards the moved player's late receipts by the old epoch (existing adjudication: token and epoch double anchor, old session/old epoch rejected, modules/game/session/src/player_directory.cpp:122-124).
2. **No lost writes**: the upstream buffering starting at ① covers the whole of ②③④⑤ — player upstream is held at the gateway, nothing lost on the floor; ⑤ drains it all to the authority at once. So the routing lag between ③→④ is **harmless** — it is exactly this buffering that makes the order "hand over authority first, change routing later" valid.

### 9.3 Migration Granularity and the Write-Freeze Window

- **Granularity = a single player** (the user's 2026-10-12 correction): one player's ①~⑤ must all complete before the next player's starts. Players are **serial by default** (concurrency = 1).
- **Concurrency is a configurable parameter, default 1** (proposed `--player-migrate-concurrency` — **TBD**, naming hangs on the wiring batch). **Only after the single-player switch duration has been measured may small concurrency be opened.**
- **Rationale (user's order)**: the finer the granularity, the shorter each player's write-freeze window, and only then is "imperceptible" truly achieved. The write-freeze window = the duration from the ① freeze moment to the ⑤ release moment; it covers only **this one player's** snapshot pull (single-player file scale), not a batch's or a scene's snapshot scale.
- **The write-freeze window is milliseconds-class** (design intent): during the window the player's upstream is buffered at the gateway and the downstream keeps flowing from the authority — the player sees no change within a frame. **The gateway currently has no upstream buffering** (`pending_` is a downstream-answer association table, apps/gateway-app/src/m1_egress.cpp:340, `request_id → session_id`); upstream buffering is a new surface of this design (§9.9-2).
- **Measured (2026-10-13, §9.10)**: the single-player ①→④ orchestration segment **123 / 153ms** (two runs; ⑤ not wired, so the full write-freeze window can only be bounded below), command-line end-to-end 0.174 / 0.237s. The measured figures are a **small two-point sample**, not a distribution (p50/p99 unmeasured — single-player file ~120B class; large files not stressed); **concurrency still defaults to 1** — measurement gives the order of magnitude (milliseconds, far below the RTO's seconds), but "opening small concurrency" still needs large-file and batch distribution data, hanging on a later batch; the parameter naming hangs on the wiring batch too (§9.9-6).

### 9.4 Update Order and Dependencies (gateway kept untouched)

| Order | Role | Depends on | Why this position |
|---|---|---|---|
| 1 | manager | none (moves first) | the exclusive window is the fleet-wide serial gate; swap the manager first so every subsequent baseapp restart happens under the new manager's stewardship, and directory re-pointing / placement exclusion also run on the new version |
| 2 | baseapp | manager online (placement exclusion + directory re-point) | the primary plan. The new baseapp (new binary) starts empty first → the old baseapp stops taking new players → five-step migration player by player → binary swapped only after drain. **Before drain completes, the old process does not swap its binary** (user's order) |
| 3 | cellapp | after baseapp (player anchors already on the new baseapp, so Avatar re-hang points at it) | the fallback road (§9.1 cellapp row): cheapest, placed last |
| 4 | login | none | stateless tier, any time; suggested before the baseapp migration parallelizes |
| 5 | logger | none | background, any time |
| — | machined | **excluded** | `terminate_all` takes down the whole fleet (supervisor.cpp:249-255) |
| — | gateway | **untouched** | the client connection anchor; drain = everyone drops |

- **How the manager's recovery-phase exclusive window is used**: it is the rolling order's metronome — at any moment at most one process fleet-wide is in a recovery phase. When the manager itself swaps versions, the whole fleet is quiescent for one RTO period inside the exclusive window (drill measured 2.71s); after that, each role's binary swap takes its own exclusive window, serial, non-overlapping.
- **baseapp before cellapp**: baseapp first (the primary plan, 0 drops), cellapp after (the fallback road; drops counted in the cellapp segment). Reason: cellapp's binary swap makes its players reconnect, and on reconnect the Avatar must hang onto a baseapp that **has already swapped versions** — only with baseapp in place first does the cellapp segment's reconnect land on the new version.
- **The new process starts first, the old process stops last**: at any moment at least one baseapp is serving new players — the structural guarantee behind "new players can log in during the update".
- **The old process swaps its binary after drain**: at that point it holds no sessions, so the restart triggers no player reconnects; the restart cost = empty-process relaunch + directory re-report (dual-criteria convergence, 2.71s semantics).

### 9.5 Data Plane

**When the journal flushes**

- **At each player's switch**: ① freeze → the old process first drains that player's journal segment → only then does ② send the snapshot (the user's primary-plan "old side flushes first, then sends" = §8 route 2 must-do 1).
- **Before the old process swaps its binary**: a full drain after drain completes (the drain stage of attribute-sync §10.2's five-stage shutdown — CAS idempotency + producers stop first + drain timeout releases).
- **After the new process restarts**: RecoveryCoordinator restore (journal replay) — already drained post-drain, short replay distance, RTO below the full-load scenario.
- **Invariant**: at any moment journal writes happen only in the authority process (the single-writer discipline unbroken) — before ③ writes land only in the old process, after ③ only in the new process.

**Anchor replay**

- The new baseapp replays anchors from the journal once as it starts empty (containing only the migrated players' anchors at that point); thereafter, one more append per migrated player. Anchor replay is part of the existing recovery sequence (restore → admission → ready), not a new mechanism.
- Batch 0's Anchor restore segment was an empty drill (demo anchors in-memory, no player files); loaded replay time hangs on the measurement batch — the existing figures are lower bounds.

**`anchor_epoch` continuity across restart (the core of the user's point 3, and the primary plan's precondition gap)**

- **Status quo**: `next_epoch_` is an in-memory increment (modules/game/session/src/player_directory.cpp:33), **no persistence, no recovery path** — it resets to zero when baseappmgr restarts.
- **Consequence**: the epoch of the same account's Nth session and its N+1th session can go backwards (a new session's epoch smaller than the epoch already persisted with the anchor). The existing adjudication (stale-epoch rejection) would discard the legitimate new session as an old one — **the same account's Nth session breaks**.
- **This design requires**: the epoch counter persisted together with the anchors (inside the journal), resuming from the recovered value after restart; hand-over during migration = an explicit +1.
- **Status**: **to be wired** (§9.9-4) — this is a precondition for the primary plan to hold, not an optional optimization.

**Player disconnect windows and the handling of Suspended entries**

- **The primary-plan path produces no Suspended**: players never disconnect throughout (millisecond write-freeze window + gateway buffering + release); directory entry states unchanged.
- **The fallback path produces Suspended**: cellapp binary swap, leftover players at the old process's binary swap, manual escalation of ④⑤ retries — these players disconnect, and entries enter the `mark_suspended` keep-alive window (modules/game/session/src/player_directory.cpp:67; `--suspend-window-ticks` default 30 = 30s at a 1s tick, apps/baseappmgr/src/main.cpp:237). In-window recovery does **not kick** (restore-not-kick, drill intake=3); a full window is terminated by the main loop's sweep into SessionDown (into the mirror surface).
- **Death-row entry points** (existing, unchanged): zone death → `suspend_zone_sessions`; gateway death → `suspend_gateway_sessions` (apps/baseappmgr/src/main.cpp:425-443).

### 9.6 Reusing Existing Mechanisms (each named with its source)

| Mechanism | Source | How this design uses it |
|---|---|---|
| recovery phase (reject new until the full re-report converges; drill measured 2.71s) | modules/game/session/fleet_recovery.hpp/.cpp (FleetRecoveryCoordinator: Normal↔Recovering exclusive phase + full-re-report fragments + dual-criteria convergence); batch 0 delivered (scripts/drill_kill9.sh) | the fleet-wide serial gate; the manager exclusive window = the rolling order's metronome |
| mark_suspended window | modules/game/session/src/player_directory.cpp:67; apps/baseappmgr/src/main.cpp:237 (`--suspend-window-ticks` default 30); the death-row entry :425-443 | the fallback path's slot-keeping window — disconnected players keep their slots, and in-window recovery doesn't kick |
| machined backoff restart | apps/machined/src/supervisor.cpp:213-226 (linear backoff, attempt N = N×backoff; drill measured attempt 1 = 1.04-1.10s); give up over the limit :218-225 | the execution half of the binary swap; after drain, the old process's restart = empty-process relaunch |
| admission gate | apps/baseappmgr (`--recovery-port` / `--recovery-expected` / `--recovery-timeout-ms`, `set_admission_gate`) | the "stop taking new players" state and the recovery period's reject-new share the same gate |
| route 1 wait-for-reconnect (gap = AssignWorld skips the process being updated) | §8 route 1; WorldAssignment (modules/game/session/world_assignment.hpp) | the cellapp segment's and leftover players' fallback; that gap is exactly what the primary plan's "stop taking new players" wires up |
| route 2: new baseapp installs first, then players switch | §8 route 2: cross-process snapshot pull (old flushes first then sends, keeping ADR-007 zero external storage) / anchor_epoch write-authority handover / directory re-point + routing snapshot refreshed immediately | the primary plan's mechanism proper; this design refines "switch" down to the single-player five steps |
| anchor_epoch double-anchor rejection | modules/game/session/src/player_directory.cpp:122-124 (token and epoch double anchor, old session/old epoch rejected) | the old process drops the moved player's late receipts by the old epoch — the existing basis for this design having no dual writes |
| directory mirror delta-immediate + full-snapshot periodic fallback | modules/game/session/directory_mirror.hpp/.cpp ("APD2" wire, delta/snapshot + seq resume); the periodic `--mirror-snapshot-ms` default 10000 (apps/baseappmgr/src/main.cpp:182) | the fallback channel for re-pointing; this design adds the "push immediately on re-point" requirement on top of it |
| RecoveryCoordinator restore→admission→ready | modules/game/session/recovery.hpp/.cpp | the new process's startup sequence after the binary swap; anchor replay sits inside this sequence |
| gateway downstream-answer association | apps/gateway-app/src/m1_egress.cpp:340 (`pending_` association table, `ack_timeout_ms` 5000) | **a contrast surface, not reuse**: it is downstream-answer association, not upstream buffering — upstream buffering is a new surface (§9.9-2) |

### 9.7 Differences (this design's own approach; all others unverified)

**This design's own approach (three items)**:

1. **Client connections never drop**: the gateway stays up untouched throughout — it is both the client TCP anchor and the routing hub. During the update, the session between player and gateway is not rebuilt; only the gateway-to-baseapp downstream pointer is.
2. **Rolling by role**: each process role walks its own state machine (§9.1) — stateless roles (login/logger) swap directly, stateful roles (baseapp/cellapp) drain then swap, and the two roles outside the rolling sequence (machined/gateway) each have their reasons (§9.4).
3. **State machine + single-player five steps**: the state machine's "drain" state unfolds on baseapp into per-player ①~⑤ (§9.2) — migration is player-level, serial, each step has a confirmation point, and there are two invariants; no copy-while-playing.

**Others' (BigWorld / KBE / commercial engines) concrete practices on "zero-downtime releases": unverified.** No local source or official documentation to cite; within this repo's docs, C-51/C-52 cover only BigWorld's backup and takeover chain (the standby taking over after process death), not its release process. Speculation forbidden. The only verifiable related contrasts are two, neither on the release face:

- BigWorld has backup/reviver processes and a backup chain (local 14.4.1 direct reading, §1.1) — that is "the standby takes over after process death", not "deliberately swapping versions"; this design does not take that road (ADR-010 already ruled no reviver).
- KBE has no journal / hot standby / takeover (§1.2, C-51) — a disaster-recovery face unrelated to releases.

### 9.8 Acceptance Metrics (measurable)

| Metric | Target | How to measure |
|---|---|---|
| new players can log in during the update | logins possible throughout (0 failures) | inject new login requests continuously during the roll and count failures; structural guarantee = the new baseapp starts first, the old baseapp stops last (§9.4). The login's own swap window is counted separately (stateless tier's seconds-class restart window; dual-run alternation can squeeze it to 0) |
| online players dropped | **0 (migration-path scope)** | count client TCP disconnects during the migration and binary-swap segments. The cellapp segment is tracked separately as "auto-reconnect count" — cellapp's drain state is unreachable (§9.1); its players each get one auto-reconnect |
| single-player switch duration | **measured 123 / 153ms** (①→④ orchestration segment, two runs; §9.10) | fleetctl migrate waits synchronously on 943; the receipt carries three segment durations (freeze_ms / handoff_ms / total_ms) — in-process ms timing, not back-derived from the wall clock. **This measured figure is the precondition for opening up concurrency (§9.3)** |
| write-freeze window duration | **the measured 123 / 153ms is a lower bound** (①→④; ⑤ gateway release not wired — without ⑤ the full window can only be bounded below) | same as above; this is the direct measure of "imperceptible". Single-player file scale (this drill = one ~120B JSON); as files grow the window grows linearly with snapshot size, unmeasured |
| archive consistency across restart | no data loss | compare anchor contents before/after migration; the old process's drain position matches the new process's recovery position (the journal position is the ruler — clock-and-time §6: cross-process distrust, use positions not wall clocks) |
| no dual writes / no lost writes | a unique authority at any moment | migration-period audit: any player's writes land in exactly one place (journal write audit + epoch monotonic increase) |
| convergence time | attached existing drill data | full re-report convergence **2.71s**; mirror convergence **2.3-2.8s** (upper bound = the quantized observation semantics of the snapshot period); death detection **14-55ms**; backoff restart **1.04-1.10s**; timeout criterion **5s**. Source: scripts/drill_kill9.sh, batch 0 measurements (§6). Route 1 update-window full chain (SIGTERM→mirror convergence) **2.44-3.81s**, drain gate effective **0.66-0.95s** — source: scripts/drill_update.sh two-run measurements (§8 acceptance drill). Route 2 single-player migration ①→④ orchestration segment **0.123-0.153s**, command-line end-to-end (including fleetctl assembly + link handshake) **0.174-0.237s** — source: scripts/drill_migration.sh two-run measurements (§9.10). The migration segment is **milliseconds** and the convergence segment is **seconds** — not the same order; the rolling update's dominant cost is in the binary-swap segment's convergence, not the single-player switch |

### 9.9 Wiring Checklist (new surfaces introduced by this design)

The primary plan's preconditions, in dependency order:

1. **Player-level snapshot pull (cross-process)** — the mechanization of route 2 must-do 1: the new baseapp pulls the single-player snapshot from the old baseapp (the old side flushes first, then sends). **Wired (2026-10-13)**: contract 933-938 six messages + orchestration command family 942/943 (internal-domain events channel) + hand-written wire (TYPE pins the generator MsgId, little-endian fixed-length truncation, out-of-range / trailing residual bytes rejected) + on-site pull by the new zone (MigrationPuller dials into the old zone 935/936, the old side flushes first then sends, ADR-007 zero external storage unbroken); real-link unit-test coverage (two zones' real M1 surfaces + mgr orchestration), two-run drill measurements in §9.10.
2. **gateway upstream buffer queue** — holds player upstream during the write-freeze window. The gateway's existing `pending_` is a downstream-answer association table (m1_egress.cpp:340), not upstream buffering. **Without it, ①~⑤'s "player imperceptible" does not hold.** Measured confirmation: the ①→④ orchestration segment is 123-153ms (§9.10), during which player upstream still arrives after the old process's write authority has stopped — **the current form relies on the old process's Transferring state to catch it (writes rejected, not buffered), with the ⑤ release surface absent**.
3. **Immediate push on re-point + the gateway's dynamic routing selection surface** — **immediate push wired (2026-10-13)**: after ④ re-points, `migratePlayer` calls `publish_snapshot` directly (not waiting for the 10s snapshot period); the drill measured `directory rehome ... snapshot_parts=1` landing in the log 0.016s after the receipt (a 30s period could not have triggered it). **The gateway dynamic routing selection surface is not done** (the gateway_server.cpp:95 comment "M1 single-cell endpoint: the routing snapshot is stored for record only; the selection surface returns with the multi-cell batch") — belongs to the multi-cell batch.
4. **`anchor_epoch` persistence** — **wired (2026-10-13)**: `attach_epoch_store` reads back the high watermark and resumes + `persist_epoch_floor` atomically persists the watermark file (`manager-data/directory-epoch`; the store holds "the next value to be assigned"); `session_up` / `advance_epoch_to` / `moved(new_epoch)` persist on each advance; no regression across restarts.
5. **Stop taking new players (placement exclusion)** — the gap recorded in §8 route 1: during the update, AssignWorld skips the process. **Wired (2026-10-13)**: drain gate + fleetctl probe + drill_update.sh drill measurements (§8 acceptance drill).
6. **Migration-concurrency parameter** (proposed `--player-migrate-concurrency`, default 1; naming **TBD**) — to be named at wiring time. Measurement has given the order of magnitude (§9.10 milliseconds), but opening small concurrency still needs large-file + batch distribution data.
7. **④⑤ retry caps and escalation path** — no cap by default; how long a stay in the buffered state counts as escalation to manual intervention is undecided.
8. **Post-③ irreversibility surface (unfreeze contract gap)** — **exposed by measurement (2026-10-13)**: when ③ hands over successfully but a later step fails, the old process stays in `Transferring` (no unfreeze message family, no automatic "skip this round" fallback surface). §9.2's "skip this round + escalate to manual" is, in implementation, manual intervention, not automatic rollback. Adding the wire (939 unfreeze_request/reply), or letting freeze support timeout self-release, hangs on a later batch.

**Conclusion**: preconditions 1/4/5 and item 3's "immediate push" half are wired (2026-10-13); **the primary plan's executable segment ①②③④ is through** (two-run drill measurements in §9.10; archives byte-identical, no dual writes, no lost writes). **Still unwired is item 2 (gateway upstream buffering)** — the ⑤ release surface is absent, and "player imperceptible" in the current form is held up by the Transferring state rather than guaranteed by buffering; item 3's "gateway dynamic routing selection surface" belongs to the multi-cell batch; item 8's unfreeze gap relies on manual intervention. Among the acceptance metrics, "single-player switch duration" is measured and pinned (§9.8); the remaining open items are **distribution data (p50/p99, large files)** and the **concurrency parameter**.

### 9.10 Acceptance Drill (measured 2026-10-13, both runs green)

Command: `scripts/drill_migration.sh` (topology = baseappmgr:19621 (`--m1-port 19625` orchestration command surface + `--recovery-port 19622 --recovery-expected 1` admission and full re-report + `--mirror-port 19626 --mirror-snapshot-ms 30000`) + old-zone base-app:19631 (`--m1-port 19632 --zone-id 2 --component-id 30 --report-to-port 19622 --demo-anchors 42`) + new-zone base-app:19641 (`--m1-port 19642 --zone-id 5`, empty and receiving). Seeds 1 real `player_42.json` file (`PlayerData::toJson` exact byte order) — flush rewrites based on the current file, and roundtrip is byte-identical, so the sha256 comparison before/after migration is the archive-consistency assertion. The port range 19621-19642 does not collide with drill_update (19610-19614/19013-19014/19105), drill_kill9 (19600-19602/19003-19004), or dev_fleet (9001-9005/9101-9107).

Driver: `fleetctl migrate --player 42 --source-port 19632 --target-port 19642 --target-zone 5` (942 → 943 synchronous receipt wait; the receipt carries three segment durations — each taken from in-process ms timing, not back-derived from the wall clock).

| Assertion surface | Measured (Run 1 / Run 2) |
|---|---|
| directory-convergence precondition (old zone full re-report → manager intake → recovery phase opens) | `sessions=42 intake=42`, convergence 0.000s (opens at intake — `--recovery-expected 1`, a single reporter; the timeout-criterion fallback never triggers) |
| ① freeze (933 freeze → 934 accepted) | **41 / 47ms**; old-side log "frozen for migration" |
| ② pull + ③ handover (937 → the new zone pulls on site 935/936 → 938 accepted) | **123 / 153ms** (including the cross-process pull and takeover); new-side log "handoff accepted (new epoch=27)" |
| ①→④ orchestration segment (943 total_ms) | **123 / 153ms** |
| ⑤ release (gateway upstream buffering → the old side unanchors) | **not wired** (§9.9-2) — the old process stays in `Transferring` (pid alive; the script's assertion surface) |
| command-line end-to-end (including fleetctl assembly + link handshake + receipt) | 0.174 / 0.237s |
| ④ immediate push on re-point (never wait for the snapshot period) | the manager log "directory rehome ... snapshot_parts=1" lands 0.016s after the receipt; the period set to 30s could not have triggered it (the script uses this criterion to distinguish "immediate push" from "periodic push", not sequence inference) |
| write-authority monotonicity | ③ handover epoch=27 (= old entry epoch+1; intake order decides the base, not always 1); the old-epoch receipt-rejection surface is covered by directory unit tests (`test_moved_epoch_bump`; mgr has no resume passthrough) |
| archive consistency | the new side's `player_42.json` sha256 `3184881d6725…` **byte-identical** before/after; the `level` field 6 preserved (roundtrip, not sampling — a single-file full comparison) |
| no dual writes / no lost writes | from ① the old side is read-only (Transferring), from ③ the new side is online; while both processes are alive simultaneously, writes land only on the new side (journal single-writer discipline); the file's byte-identity is no-lost-writes |
| failure paths | unknown player migrate exit 2 (MIGRATE_FAILED, `unknown player in directory`, request_id echoed); `--target-zone 0` rejected at the CLI layer exit 1 (never left the box) |
| process liveness | old and new sides both alive (the old side's stay = §9.2's "skip this round + manual" shape, §9.9-8) |

**Scope boundary (as-is)**: the ①②③④ orchestration segment is through; ⑤ (gateway upstream-buffer release) is not wired, so the measured "write-freeze window" of 123-153ms is a **lower bound of the ①→④ segment**, not the full window. The post-③ irreversibility surface has no automatic rollback (unfreeze contract gap, §9.9-8) — its measured exposure is the old side's stay state. The sample is **2 runs × 1 player × one ~120B file**, not a distribution: p50/p99, large-file scale, and batch migration are all unmeasured (§9.3). The drill contains no gateway; the player upstream surface did not participate — the complete evidence chain for "player imperceptible" waits for item 2's wiring.

Corresponding to §9.9: item 1 (cross-process snapshot pull), item 3's immediate-push half, item 4 (epoch persistence), and item 5 (placement exclusion) are hereby settled; item 2 (gateway upstream buffering), item 3's selection-surface half, item 6 (the concurrency parameter), item 7 (the retry cap), and item 8 (unfreeze) stay on later batches.

---

*Baseline: apollo main @ f4d56ac6. BW evidence = the official 14.4.1 package read locally (2026-10-09 this round's supplementary reading of backup_sender.hpp/backup_hash.hpp/backup_hash_chain.hpp/baseapp.hpp and the reviver-family file inventory; existing line numbers follow architecture-review §16.2/§16.6 verified records); the grading semantics follow doc 36 §0 (A local source read / B this repo's docs / C official docs / D community material — §1.3's industry-pattern lines are flagged D-grade doubtful in full). The existing-piece consumption surface is per the 2026-10-09 working-copy read (apps/machined, apps/baseappmgr, the seven modules/game/session pieces, modules/data/orm/persist_journal, apps/base-app base_server, modules/base/timer_wheel). All six decision points were suspended at the time — this document decided on no one's behalf, set no candidate preference, marked no recommendation; before the ruling, only batch 0 could go first.*
