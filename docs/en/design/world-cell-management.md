# world and cell Management Design

> Status: review draft. This refines the agreed management/execution boundary. It does not claim that standalone cell execution, remote placement, or continuous migration exists. Review the design before revising the implementation backlog and code.

## 1. Recommendation and Scope

Use **world for placement, cell for simulation, and Base for persistent player state**. The default deployment has one World manager on a host, automatically maintaining one or more cell processes. Both roles use the `world` binary, selected at startup, with process isolation and explicit registration, commands, and authority checks.

CellApp refers to the BigWorld comparison; Apollo's execution role remains `cell`. Sharing a binary adds no `cellapp` role alias. Uppercase Cell is a spatial partition; Space maps to the spatial meaning of the existing Scene; Instance retains its eight-state gameplay lifecycle.

Initially, each Space has exactly one complete Cell. Scaling starts additional cell processes and places new Spaces on ready executors. Shrinking drains existing activities naturally. Continuous whole-Space/Instance migration comes later, followed by multiple Cells per Space, Ghost replication, and seamless boundaries when needed.

<ArchitectureDiagram kind="system" />

The [BigWorld CellAppMgr source](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp) places executor, Space/Cell, and load management in the manager; the [CellApp source](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cellapp.hpp) provides the executor reference. This reuses boundaries and terminology, not all distributed capabilities. See the [mapping](./bigworld-apollo-mapping.md).

### 1.1. Application Directories and Binaries

This draft adopts the short names discussed in this review: application directory, executable CMake target, and published program match, without `Server`, `-server`, or `-app` suffixes. The naming migration is complete; no competing startup aliases are retained. world defaults to the management role and `--role=cell` selects the executor.

| Directory / program | Responsibility |
|---|---|
| apps/gateway / gateway | Client connections, sessions, routing |
| apps/login / login | Authentication, directory, location projections |
| apps/base / base | Persistent player objects and business |
| apps/world / world | Default management role; `--role=cell` executor |
| apps/database / database | Storage and durable results |
| apps/machined / machined | Local supervision |

Keep cell assembly within the World application or shared libraries with module boundaries, without another application binary. The former host demo entry (apps/game-server) was removed; its hosting duties are carried by world and the shared libraries, not by another deployment role. Operations tools such as logger, admin, and fleetctl retain their own responsibilities and names.

C++ type names do not follow binary names mechanically: the Base part is `base::Avatar`, the Cell part `cell::Avatar`.

## 2. Ownership and Calls

| Role | Owned state | Normal path |
|---|---|---|
| world | Executor registration, Space/Cell placement, capacity reservations, topology, creation/migration decisions, draining | Base requests placement; World commands cell and publishes committed locations |
| cell | Local Entity/Avatar, AOI, combat, Lua, simulation clock, active Instance | Gateway inputs directly to cell; simulation output to gateway; business requests to original Base |
| base | `base::Avatar`, persistent player state, business rules, settlement deduplication | Basic business and settlement through Base to database |
| login | Authentication, service directory, player-location projections | Address discovery and projections, without Cell-placement authority |
| gateway | Client connection, session, Base/cell route caches | Simulation traffic bypasses the World manager |
| Local supervision / machined | Process startup, liveness, exit, restart backoff | World embeds reusable supervision by default; standalone machined can execute local lifecycle requests |

World neither holds another Avatar nor ticks Scene. cell holds no complete player profile or home-database credentials and uses no remote equivalent of a local Anchor pointer. Instance shared goals, clocks, and settlement have one executor owner: initially the executor hosting the entire Space. World stores location and lifecycle projections.

One World per host is the default deployment, not a requirement that World identity equals a machine address or that all future executors must be local. Each cell belongs to one World. Multiple Worlds manage non-overlapping configured scopes; each logical World has one active manager. A shared cross-server pool has an explicit owning World, avoiding concurrent capacity allocation by two managers.

## 3. Identity and Minimum Records

These are semantic fields, not completed wire additions. Allocate message IDs and evolve fields through the existing contract generator.

| Record | Required information | Constraint |
|---|---|---|
| Executor | Logical cell ID, process incarnation, owning World, control/simulation endpoints, versions/capabilities, readiness/drain state | PID/IP is not stable identity; stale registrations cannot overwrite a new incarnation |
| Space | Stable Space ID, map asset reference, owning World, optional Instance, lifecycle | Independent of executor, map ID, and Instance ID |
| Cell placement | Space/Cell IDs, bounds, executor/incarnation, topology version, allocation state | Initially covers the whole Space; reserves adjacency and multiple partitions |
| Entity location | Stable entity ID, Space/Cell, executor, simulation authority epoch, session/route versions | Independent of Base ownership/version |
| Creation/migration | Stable request ID, source/target, expected versions, phase, snapshot digest, decision, cleanup progress | Retries query the same operation rather than create another activity |

Space/entity/operation IDs must be unique across the communicating deployment. If player IDs are unique only within a home server, cross-server keys must include that server or adopt a validated global ID rule. Existing IDs are not assumed sufficient.

World owns placement, cell executes under valid authorization, and directory/gateway cache committed results. Process incarnation, World-management epoch, simulation authority, topology, Base write ownership, and session versions are separate concepts; one `route_version` cannot replace them all.

## 4. Startup, Registration, and Disconnection

1. World obtains exclusive ownership, restores management records, and completes recovery before allocating new Spaces.
2. cell resolves its World using configuration or directory, authenticates its service identity, and registers incarnation, protocol/snapshot capabilities, and capacity. Same-host machined is unnecessary.
3. World validates ownership, identity, and versions and returns its management epoch. cell becomes ready only after dependencies, resource/script loading capabilities, and simulation ingress are checked.
4. Only ready, non-draining executors receive placements. Heartbeats report allocations, tick delay, queues, memory, and reservations. Reject stale incarnations and replies.
5. On disconnection, stop new allocations and mark the executor uncertain. Reconcile before creating replacement authority elsewhere.

**Initial policy preserves a single writer without automatic failover.** Heartbeat timeout establishes unreachability, not death. During a bounded configured grace period, cell continues existing activities. At expiry, freeze at a tick boundary, stop new external business effects, and close input acceptance at the IO boundary. Reconnection requires allocation/epoch reconciliation before unfreezing.

Use a local monotonic clock and calibrate grace periods against heartbeat timing, jitter, and acceptable pauses. Expiry or a higher epoch alone does not prove fencing: an old process may resume from suspension while receivers retain stale caches. Activate replacement authority only after acknowledged source freezing or verified termination/access isolation. Otherwise remain blocked. Automatic failover needs separate arbitration, authorization timing, and fencing at every side-effect receiver.

### 4.1. One Binary, Two Startup Roles

Proposed CLI, **not currently implemented commands**:

```text
world
world --config=<world-config>
world --role=cell --config=<cell-config>
```

No arguments, or no `--role`, defaults to the World manager role (`--role` accepts only `manager` and `cell`; there is no `world` form). A missing role value, unknown role, or invalid configuration fails explicitly. Role is fixed at startup, never inferred from a port.

World generates each child's role, logical identity, manager endpoint, separate runtime directory, and bootstrap credentials; developers do not maintain a handwritten configuration per executor. Do not log bootstrap secrets. Executors can bind ports and report them through authenticated registration rather than race between probing and binding a free port.

World starts management and local supervision without a simulation host. cell starts simulation and registration without spawning more executors. Shared main performs validation and role assembly; management, simulation, and supervision retain module boundaries. Logs, metrics, and crash reports identify role, logical ID, and incarnation. cell opens neither World-management logs nor Base profile storage.

World chooses desired executor count and invokes local supervision by default. Extract reusable functionality from `apps/machined` and add dynamic process membership. The existing supervisor is principally POSIX with a fixed roster; cross-platform autoscaling is not implemented. Use structured argv, a spawn/exec wrapper appropriate for a multithreaded POSIX host, and separately implemented Windows process handles and cleanup.

With standalone machined, World requests desired lifecycle changes and machined executes them. Each process has exactly one restart supervisor. The World manager itself is supervised by a system service, container, or machined. Remote startup requires a supervisor on the target machine or external orchestration; local spawning does not provide remote startup.

### 4.2. Automatic Growth and Natural Shrinking

| Decision | Rule |
|---|---|
| Limits | Default minimum one executor; maximum bounded by CPU/memory and host reserves |
| Growth | Sustained capacity pressure/high watermark starts executors within cooldown and maximum; starting processes count toward desired capacity |
| Readiness | Spawn success is only starting; allocation waits for authenticated registration and readiness; startup timeout cleans up with bounded backoff |
| Shrinking | Sustained low watermark selects an executor for draining; exit only after active/prepared Spaces, migrations, and unconfirmed settlements are cleared |
| Drain cancellation | Cancel before exit is issued if capacity pressure returns; never allocate to an exiting process |
| Blocked draining | Persistent Spaces or unfinished activities keep the process alive; timeout reports blocked shrink rather than killing gameplay |
| Restart | Distinguish intended shrink from crash; retries are bounded, incarnation changes, and old activity restoration is not assumed |

Use sustained windows, separate high/low thresholds, and cooldown. More processes distribute independent Spaces; they do not accelerate one hot Space with a single Cell. At host resource limits, reject new activities or add hosts through deployment management.

Persist executor identity/incarnation, startup/drain/exit intent, and supervision ownership. After World restart, reconcile surviving executors before filling deficits. Do not spawn a duplicate fleet merely because surviving processes are not children of the new manager. Validate startup identity and authenticated registration rather than PID alone.

Publish immutable version directories or equivalent release artifacts. World starts the selected release and records build/protocol capabilities without overwriting running files. Old/new cell processes may use different versions of the same binary. An old World can launch a new cell only when actual control-protocol compatibility exists.

## 5. Space Creation, Capacity, and Destruction

| Phase | Action and acknowledgement |
|---|---|
| Request | Base validates party, eligibility, version, and gameplay rules and supplies a stable request ID |
| Reserve | World filters ready/compatible/admitted executors, reserves capacity, and records intent; no capacity produces explicit rejection |
| Prepare | cell idempotently loads resources and creates the complete Cell/local Scene and optional Instance, without inputs or activity ticks |
| Commit | World durably records placement and requests activation; publish usable route/admission only after activation acknowledgement |
| Enter | Bind original Base, read-only gameplay snapshot, session, and expiring admission; create one authoritative Avatar |
| Destroy | Stop admission, complete activity and durable settlement handling, release simulation resources, record terminal state, reclaim capacity |

On timeout, query the same operation. Cancel prepared resources only after confirming no commit. Lost replies must not duplicate creation; committed activation failure requires recovery or explicit failed cleanup. Destruction checks placement epoch so stale requests cannot destroy a new allocation.

Budget tick time, entities, memory, input backlog, and reserved resources rather than just players. World reserves capacity atomically against concurrent requests; cell enforces a local hard limit. Overload rejects new activities while preserving active budgets. Initially, multiple Spaces use one sequential simulation thread, with its shared failure/stall scope; parallelism is separate work.

Retain recoverable settlement records until durable confirmation. Large map resources may unload, but the only settlement evidence cannot be deleted. World cannot infer crash compensation from an in-memory projection; see [cross-server design](./cross-server-scenes.md).

## 6. Management Persistence and World Restart

Persist allocation/version records, operation decisions, capacity reservations, Instance ownership and settlement-recovery record locations, and draining. Heartbeat loads and address caches are rebuildable; commit decisions are not disposable cache or ordinary log lines.

Use a World-owned persistent management journal/checkpoint initially, separate from player database and without another business process. Successful commit replies wait for the declared durability level. Require idempotent replay, integrity checks, incomplete-tail handling, and single-writer locking. Existing player PersistJournal is a reference, not proof of management transactions or power-loss safety.

Initially support one active manager restarting with the same persistent volume, without automatic cross-host standby. Verify the old manager has stopped, restore records, increase management epoch, and reconcile executor manifests. Executor self-reports cannot replace commit history. Conflicting/missing records or uncertain source isolation keep activities frozen; unavailable records block new allocation instead of silently starting empty.

Local supervision also needs surviving-process reassociation and supervision continuity. Parent-child spawning alone cannot deliver continuous World upgrades. Without that capability, explicitly use a stopped upgrade rather than blindly killing or duplicating existing executors.

For planned updates, stop new creation/migration, complete or record in-flight operations, hand off the journal, stop the old manager, and reassociate executors. Existing activities continue only within validated grace/authorization bounds; otherwise freeze. Simulation pause and client TCP loss are separate outcomes.

The reconciliation record formats (journal v2), protocol extension (963 cell_space_report), and per-item adjudication are detailed in the [World Restart Reconciliation](./world-reconciliation.md) design draft (under review).

## 7. Whole-Space/Instance Migration

New placements and natural draining can ship first, but do not establish continuous hot updates.

1. Reserve target capacity and validate protocol, assets, and snapshot conversion; record migration ID and expected source placement.
2. Freeze the migration unit at a source tick boundary and record the last executed input sequence. Complete or record Base requests and pending settlements, producing no new side effects.
3. Snapshot entities, monsters, Instance phase/goals, clocks, randomness, timers, serializable script state, input watermarks, and pending work. Reject unsupported activities rather than copy Lua memory pointers.
4. Target validates version/digest and restores inactive state, without ticks, rewards, or authoritative output.
5. After confirming source freeze, World durably commits target authority and new epochs. Target activates from this decision; directory/gateway update versioned routes. Source stays frozen. Stale routes return migration/stale status rather than unbounded forwarding.
6. Target resumes with input deduplication and acknowledges. Source cleans up, capacity is released, and World records completion. Precommit cancellation first verifies no commit; postcommit timeout cannot unilaterally unfreeze the source.

Initially migrate the complete Space and associated Instance. Shared activity dependencies across Spaces must move together or make that activity unsupported. Ordinary cross-server entry moves a player to another Space without moving the entire original activity.

Input buffering is bounded by size and time; reject overflow. Replay depends on request sequences and execution watermarks. Legitimate settlements executed before freezing may arrive late: Base validates authorization history, stable settlement IDs, and allowed operation scope. Neither blindly rejecting every old epoch nor permitting new effects from an obsolete source is correct.

## 8. Cross-server Play and Both Update Modes

The destination World allocates its own cell while players retain their original Base. Resolve Base service identities, not home-database endpoints or credentials. Base replacement changes incarnation, not home affiliation or storage ownership.

For cross-World player migration, the source World owns the durable decision; destination World reserves/prepares and activates according to it. If the coordinator is unavailable, query the same migration ID and do not unilaterally cancel or resume both ends. Retain decision evidence and completion records. Live transfer of an entire Space's management ownership between Worlds is deferred pending management-record and in-flight-operation handoff design.

| Update mode | Required capabilities | Limits |
|---|---|---|
| Protocol-changing process replacement | Coexisting releases, actual old/new decoders/adapters, snapshot conversion, freeze/commit/route handoff | Validate internal and client protocols separately; unchanged clients still need actual compatibility, not just a schema allowlist |
| Code-only, unchanged wire | Lua updates at safe points with private-state/coroutine handling; C++ replacement uses the same migration flow | In-place Lua updates do not prove arbitrary state migration; C++ replacement still requires snapshots and authority transfer |

Base transfers persistent state/write ownership, World transfers management records, and cell transfers simulation. An unchanged gateway may preserve TCP while simulation pauses. Replacing gateway still needs independent connection handoff or reconnection.

## 9. Seamless-map Compatibility

Reserve stable Space/entity IDs, common coordinates, explicit Cell bounds/adjacency, topology, and entity authority epochs. Never identify Space by executor ID. Keep Scene as the local simulation container and later separate local bounds from logical Space identity, without another synonymous Scene→Space container chain.

Multiple Cells per Space will additionally require Real/Ghost replication, continuous AOI/Witness, entity-level handoff, cross-Cell combat adjudication, and one owner of shared Instance state. Initially migration may advance authority for the whole Space, but contracts must allow entity authority to diverge later.

Current SceneTransfer recreates Avatar on explicit space change. It cannot implement same-Space handoff preserving entity identity, health, cooldowns, timers, and observation continuity. Reserved interfaces are not evidence that seamless maps work.

## 10. Risks and Review Findings

| Risk | Incorrect shortcut | Recommendation |
|---|---|---|
| Excess complexity for lightweight games | Begin with partition balancing, Ghosts, and automatic failover | Shared binary/roles, local supervision, single-Cell Spaces, natural draining first |
| Manager bottleneck | Proxy movement/combat/visibility through World | Direct executor traffic, bounded control queues, summarized heartbeats, throttled migration |
| Split simulation | Activate a copy on heartbeat timeout | Confirm source freeze/isolation; uncertainty blocks activation |
| Manager failure | Trust executor self-reports after losing memory topology | Exclusive management, durable decisions, reconciliation; no initial automatic standby |
| Incomplete migration | Move only Avatar and recreate other runtime state | Complete dependency snapshots; reject unsupported activities |
| Cross-server writes/duplicate rewards | Direct database writes or validate only the current epoch | Original Base, stable settlement IDs, authorization history, durable deduplication |
| Incorrect durability claim | Treat file flush as power-loss safety | Declare durability level, test crash points, refuse unsafe recovery |

Parameters pending review/measurement: journal durability, heartbeat/grace intervals, capacity budgets, snapshot size/pause budgets, and each gameplay script's migratable-state inventory. Automatic failover, live management-scope transfer, and multi-Cell Spaces are separately scoped work.

### 10.1. Is interfaces Needed?

The [KBEngine configuration reference](https://kbengine.github.io/docs/installation.html) describes interfaces as an external-account, login, and billing integration service. Its [source declarations](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/tools/interfaces/interfaces.h) include account creation/login and charge requests/callbacks. It is separate from client gateway and World simulation management.

Reserve asynchronous integration interfaces without requiring another default process. Simple platform-login checks can initially run through a login adapter with bounded timeout, concurrency, and queues. External latency must not block authentication processing or simulation ticks. Multiple channels, payment callbacks, separate credentials, or deployment isolation may later justify optional `apps/interfaces` / `interfaces` without inventing another name.

A separate service verifies signatures, maps external identities, transforms formats, and forwards bounded requests. login decides game authentication/session outcomes, base decides rewards and consumption, and database persists them. Define order/event IDs, retries, and durable deduplication end to end. Acknowledging external acceptance requires recoverable delivery records rather than an in-memory queue. interfaces neither mutates player profiles directly nor simulates gameplay or acquires home-database credentials. Enabling this optional service and designing payments require separate review.

## 11. Current Code and Implementation Order

| Current code | Target boundary/gap |
|---|---|
| WorldApp gameLoop, acceptanceQueue_, WorldHost, World/Scene | Move simulation and bounded intent consumption into cell; preserve single-writer and shutdown draining |
| WorldApp PlayerStore/Anchor and direct settlement calls | Return long-term ownership to Base; replace with read-only snapshots and remote business requests |
| SquadInstanceService and Instance | Runtime belongs to cell; World reserves/commits placement; local pointers cannot span processes |
| WorldAssignment | Missing complete executor/Cell and management/simulation epochs |
| SceneTransfer and old profile migrations | Explicit space change is separate; moving full profiles does not implement fixed-Base simulation migration |
| M1, directory, machined | Reuse transport/supervision primitives; add authenticated registration, wire, idempotency, authorization |

Complete Base/simulation data separation, then shared binary roles, local supervision, registration, whole-Space creation/destruction, and failure freezing. Add multiple executors and bounded autoscaling, then management recovery, continuous activity migration, and both update drills. Until migration works, upgrading by natural draining is not continuous hot-update completion.

No executables, C++ types, or protocol fields change in this draft, and no new decision is automatically accepted. Root todo.md remains the only backlog; update P2/P3/P4/P5 acceptance conditions after review.

## 12. Acceptance

- Both roles run as real processes from the shared binary; no recursive cell spawning, bounded startup retries, no restart after intentional shrinking.
- No executor means explicit creation failure; World neither ticks simulation nor loads complete profiles.
- Validate thresholds, cooldown, starting counts, limits, natural draining, persistent Spaces blocking shrink, and no duplicate spawning after manager restart.
- Test concurrent placement, Space isolation, reservations/rejection, duplicate requests, lost replies, and delayed destruction.
- Reject forged/stale registration, stale input/output, and paused processes resuming after disconnection; never produce two authorities.
- Crash before/after each durable creation/migration point; recover one decision, and never silently activate replacement state after losing management records.
- Continuous migration preserves members, monsters, goals, health, skills, clocks, randomness, and required scripts; cover unknown decisions, target failures, and source-cleanup failures.
- Two home Bases retain ownership; lost settlement ACKs, duplicates, late results, unavailable Base, and restart cannot duplicate rewards or erase unconfirmed evidence.
- Drill real protocol/snapshot evolution and unchanged-wire code updates separately; record activity pause, login continuity, and TCP continuity independently.
