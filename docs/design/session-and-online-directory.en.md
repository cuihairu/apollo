[English](session-and-online-directory.en.md) | [中文](session-and-online-directory.md)

# Apollo Sessions and Online Directory (session-and-online-directory)

> Status: design draft (under review). Positioning: **the authoritative registration and query surface for player online state** — who is online, on which line/baseapp/instance, duplicate-login and displacement rulings, the disconnect keep-alive window, and the data source for cross-process player addressing (the data supplier of RouteResolver's "host location" duty). Gap registered: design-gap-inventory #12 (initiated and set down the same day, 2026-09-30). Cross-references: net-abstraction §7 (G-1 fleet/baseappmgr domain/shared-model four-class channel table), §3 (L2 resume), §5.7 (InterServerLink/authority_epoch), §5.9 (login_token), attribute-sync §8.2 (Redis boundary), sdk-contract §11 (internal domain), scripting-lua §7.5 (GM query surface), clock-and-time (window timing), capacity-and-benchmark (capacity and reconciliation budget); the login chain overview (#13), inbound interfacing (#14), and Bots stress testing (#15) are downstream consumers.

---

## Executive Summary

1. **The online directory = authoritative in-process state of the baseappmgr domain; it does not go into Redis, does not land in DB, has no journal** — the direct landing point of the "global arbitration state: centralized, not shared" channel family (last row of the net-abstraction §7 four-class channel table): displacement and admission rulings must be single-point serial (two processes each claiming the same account = dual authority, violating the inter-process corollary of the single writer); online state is volatile state (invalid on process restart), and the only persistence is the DB account slot (last-online time, a storage.xml archive column). Capacity is trivial (5000 CCU × ~200B ≈ 1MB, per capacity §3 semantics).
2. **Write path = event reporting + periodic reconciliation**: the registration anchor = when admission passes and the baseapp accepts the session entity (account authentication/token/server-selection queueing before admission belongs to #13 — this design fixes only the directory's data plane and registration anchor); three incremental event sources — baseapp session add/remove, gateway connection life/death (L2 heartbeat timeout / resume TTL expiry), baseappmgr's own rulings (displacement/window timeout). Anti-drift = periodic reconciliation (directory ↔ per-baseapp session-set hash comparison); on mismatch, snapshot reset — the owner-subscription model's standard recovery path, not item-by-item patching.
3. **Displacement = the new displaces the old; a framework semantic; the ruling authority is baseappmgr** (the directory owner's single point): a new login hits an existing Online/Suspended entry → within the same ruling critical section the old entry is terminated and the new entry written → asynchronous kick delivery (old baseapp destroys/transfers custody of the session entity + old gateway close(kicked)). No conflict with resume: resume is **connection-level recovery of the same session** (L2 resume token, net-abstraction §3), not a new login; being displaced inside the window holds equally (Suspended is no death-exemption token). KBE leaves the ruling to the script layer (nothing built into the engine) — apollo promotes it to a framework semantic, on the rationale that displacement is tightly coupled with the directory/admission; putting it in the framework prevents every business from rewriting its own ruling state machine.
4. **The disconnect keep-alive window (Suspended) and the resume token TTL share one and the same value** — no two window systems, no two timer families: within the window the entity stays on the baseapp (disconnect state visible to the business, the target semantics of PlayerAnchor `Disconnected`); window full → directory deletes the entry + baseapp-side termination (archiving goes through attribute-sync §8 write-behind, no special handling). Timing = a baseappmgr-side timer (deadline = tick number, per clock-and-time semantics) — depends on the timer-wheel component (architecture-review §23-④ not yet started, C-80 same family; registered with a code batch).
5. **Query surface, three consumers**: ① RouteResolver host location = an **in-process read-only mirror** of the directory delta (each process subscribes via InterServerLink, seq resumption) + ServerID-segment a-priori routing (type/instance segments embedded in entity_id, No. 36 #15) + route_version/authority_epoch verification as backstop (§5.7 — a stale mirror does not cause wrong routing; a failed call goes re-resolve); ② GM/observability = control-channel admin querying baseappmgr (the central truth); ③ whole-server broadcast = the local mirror distributes by baseapp. High-frequency friend-presence goes through the mirror, not baseappmgr. Mirror consistency declaration: **the projection is eventually consistent (seconds-level); rulings and authoritative reads live only in baseappmgr** — the worst consequence of reading a stale mirror = one failed call + re-resolve, which does not break the single writer.
6. **Crash recovery = full re-report rebuild**: baseappmgr dies → machined relaunch (G-1) → the recovery phase takes over exclusively (baseappmgr domain, refusing new entries until convergence) → every baseapp/gateway fully re-reports its live sessions → converged, reopen. The directory is an index, not the authoritative data holder (connections live on gateway, entities on baseapp); a rebuild loses no service; no journal — 5000 entries fully re-reported in < 1s, cheaper to rebuild than to maintain a journal.
7. **Message surface = the internal-domain event family** (sdk-contract §11 id 900+ segment; **zero client-domain additions** — domain discipline stands): the `SessionUp/SessionDown/SessionMoved/SessionKicked` event family + the `DirectorySnapshotRequest/Reply` reconciliation family; envelope = InternalMessageEnvelope (net-abstraction §7); invoke_mode: events OneWay, reconciliation snapshot RequestReply (low-frequency control plane, compliant). The kicked reason code for the displaced client goes into errors.xml (with a code batch; the contract files stay frozen this round).
8. **Interfacing with the status quo (the legacy is not overthrown)**: `modules/game/session` (six files, 302 lines: PlayerAnchor six states + SessionBinding + WorldAssignment + AnchorManager + SessionLocator double hash) = **the in-process anchor domain already exists**, sole consumer base-app (base_server.cpp:131-167 bindSession/unbindSession/assignWorld) + direct test tests/test_base_anchor.cpp. Positioning: the directory entry fields = the inter-process extension of `SessionBinding + WorldAssignment`; SessionLocator::bind (session_locator.cpp:9-11 displaces the old index) is the in-process prototype of displacement (no kick-notification surface); the domain currently hangs on base-app (a BW-form legacy app; the current topology has no base side — docs/index) — the target placement = the base-app session face + the baseappmgr directory face; migration belongs to a code batch (registered; source frozen untouched).

## 0. Why This Is a Gap (Negative Space and Precedents)

- **Negative space**: grepping "顶号|重复登录|在线状态|online" across the nine design/ files yields zero hits (gap #12 evidence, tested 2026-09-30); the existing pieces cover only "is the connection alive" (L2 heartbeat/resume) and "which owner does the entity belong to" (ServerID/sceneId) — **"which process is the player on" has no global registration**.
- **KBE**: the engine has no online directory — online = baseapp entities in memory; assignment belongs to baseappmgr; **duplicate-login ruling sits in the assets script layer** (no built-in engine semantics; the assets repo is not on this machine, the sdk_templates placeholder verified; gap #12 evidence). "Lines" are not a first-class engine concept either (zero hits for `line` in kbe engine source/config — one map with multiple Space instances is multiple lines; Spaces management lives in the script layer).
- **BW**: the online directory **is scattered across the mgr** — baseappmgr holds the base allocation table + `(addr, load)` pairs reported to loginapp for login routing (baseappmgr.cpp:588-599/:1117); displacement = baseapp destroying the old base entity family (handled by the session side itself).
- **Why the directory cannot be self-managed per baseapp**: addressing requires cross-baseapp lookup (no global index = O(N) broadcast or guessing); displacement requires global serialization (two baseapps each claiming the same account = dual authority). Both dilemmas are solved at once by "centralized, not shared" — this is the argument for this design belonging to the baseappmgr domain.
- **Existing pieces vs. the gap** (against duplicate construction):

| Existing piece | Covers | Missing (this design adds) |
|---|---|---|
| baseappmgr lightest allocation/admission gate (net-abstraction §7) | Placement choice, overload login refusal | **Continuous tracking** after registration and the query surface |
| L2 session resume (net-abstraction §3) | Same-connection disconnect recovery | **Cross-process visibility** (other processes do not know about this session) |
| login_token (net-abstraction §5.9) | One-time entry credential | The **fact of being online** after entry |
| ServerID segmentation (No. 36 #15)/sceneId (attribute-sync §4.3) | id carries its origin; scene isolation | The mapping from id to the **current host process** |
| modules/game/session anchor domain (this section, Status Quo) | In-process player↔session↔world | Inter-process aggregation and ruling |

## 1. Data Model

**Directory entry** (owner = baseappmgr domain, in-process memory):

| Field | Type | Notes |
|---|---|---|
| `account_id` | key | Account-domain id (the subject of the token issued by login-app) |
| `player_entity_id` | u64 | ServerID segmentation (No. 36 #15) — for RouteResolver a-priori routing |
| `session_id` | u64 | Connection-domain id (same source as §5.9 HKDF info) |
| `gateway_id` | u32 | Access gateway — for reverse-lookup handling on gateway death |
| `zone_id` | u32 | Host baseapp process — for reverse-lookup handling on baseapp death |
| `world_assignment` | struct | **Reuses modules/game/session WorldAssignment** (world_id/map_id/instance_id/space_id/route_version) — the four-piece locator for the current line/instance |
| `state` | enum | `Online / Suspended / Leaving` (see state machine) |
| `anchor_epoch` | u64 | The Nth session of this account — **idempotency key + race-adjudication key** (§3/§4) |
| `deadline_tick` | u64 | Suspended window deadline (clock-and-time tick number) |

**Reverse-lookup indexes**: by_entity (addressing), by_zone / by_gateway (batch handling on process death). All in-memory hashes, nothing on disk.

**Entry state machine**:

```
        bindSession (admission passed + baseapp accepts)         connection dead (heartbeat timeout / gateway death)
 (none) ─────────────────────────► Online ─────────────────────────► Suspended
                                    │  ▲                                │
                                    │  └────── resume success ─────────┘
                                    │ (same session_id + anchor_epoch)
              explicit logout/migration       │              window full (deadline_tick)
        Online ──────────► Leaving ─┴─► Removed ◄───────────────────────┘
        any state + new login hit ──────────► Removed (old, kicked) + new entry (§3 displacement)
```

- `Leaving` = the intermediate state of explicit logout / cross-baseapp handoff (interlocking with §7 and todo batch-3 explicit handoff); `Suspended` = the disconnect keep-alive window (§4).

## 2. Owner and Write Path

**owner = baseappmgr domain** (same process and domain as the admission gate and the recovery phase — the ruling serialization point holds naturally; the current topology is exactly the World/baseappmgr of docs/index).
- **Three event sources**: ① baseapp session add/remove (SessionUp/SessionDown, reported at entity creation/destruction); ② gateway connection life/death (L2 heartbeat timeout, resume TTL expiry, gateway process-death event — a G-1 fleet event); ③ baseappmgr's own rulings (displacement kick, window timeout).
- **Registration anchor (timing)**: admission passes (baseappmgr fixes the placement baseapp, lightest allocation) → directive to baseapp → baseapp creates the session entity → `SessionUp` reported → **the directory is visible from this moment on**. Admission failure / baseapp refusal → no entry (no ghosts left behind). Boundary: the chain before admission (account authentication/token issuance/server selection/queueing) belongs to #13.
- **Reconciliation (anti-drift)**: periodically (default 30s, calibrated in P3) baseappmgr ↔ per-baseapp session-set hash comparison; on mismatch → that baseapp gets a snapshot reset (DirectorySnapshotRequest/Reply full table comparison) — consistent with net-abstraction §7's "three meanings of sync": a lost event is the transport layer's resumption business; a reconciliation mismatch is the state layer's snapshot-reset business; neither backstops the other.
- **Idempotency and reordering**: events carry `(account_id, anchor_epoch)`; late-arriving events with an old epoch are dropped; replay has no side effects (set semantics are naturally idempotent, same argument as attribute-sync §8.2).

## 3. Displacement and Duplicate Login (Framework Semantic)

**Ruling: the new displaces the old.** A new login on the same account (a brand-new ClientHello + new login_token, not a resume) hits an existing directory entry (Online/Suspended/Leaving alike):

1. baseappmgr, in the **same ruling critical section** (serialized tick processing, no lock contention — the payoff of centralized-not-shared): old entry → Removed, new entry written (new anchor_epoch).
2. Asynchronous kick delivery: the old baseapp receives `SessionKicked` (destroy the session entity or transfer custody — a business policy; the framework delivers the event, not the policy); the old gateway receives close(kicked).
3. Race window: the old client's resume/uplink arrives after the kick directive is already in flight — verification fails on the invalidated `anchor_epoch` and is rejected (token and epoch double-anchored; the §5.9 session_id domain gets one more epoch layer).
4. Client contract: the displaced end receives the kicked reason code (registered in errors.xml, with a code batch).

- **Why the new displaces the old**: an active re-login = the player's explicit intent; clean division of labor with resume (resume = same-session recovery, displacement = new session replacing); "old kicks new" or "queue up" push the ruling complexity onto the business while both still need a global serialization point — zero payoff. KBE's script-layer default behaves the same way (destroy the old avatar, build the new); BW's baseapp is the same family.
- **Why in the framework, not in scripts**: the displacement ruling is tightly coupled with the directory/admission/epoch; putting it in the framework (this design) prevents every business from rewriting a ruling state machine — the difference from KBE's "the engine does not manage this" is a deliberate reinforcement (same-form argument as the GM surface, scripting-lua §7.5).

## 4. Disconnect Keep-Alive Window (Suspended)

- **The window and the resume token TTL share one value** (one config item, one timer family) — two window systems inevitably drift, and drift means "token still valid but entity already destroyed" or the reverse. Default recommendation 30s (calibrated in the P3 capacity batch; this design does not fix the resume TTL value — existing semantics in net-abstraction §3).
- Inside the window: the entity stays on the baseapp (disconnect state visible to the business — the target semantics of `AnchorState::Disconnected`), the directory stays Suspended; **resume succeeds → Online (the directory only flips the state column, does not touch the entity)**; window full (`deadline_tick` reached) → directory deletes the entry + baseapp receives SessionDown (termination/transfer custody decided by the business; archiving goes through write-behind with no special handling).
- Timing ownership: a baseappmgr-side timer (deadline = tick number); **depends on the timer-wheel component** (architecture-review §23-④ not yet started, C-80 registered in the same batch) — a code-batch dependency, not a design gap.

## 5. Query Surface (Three Consumers)

| Consumer | Form | Path |
|---|---|---|
| **RouteResolver host location** (one of the remote-entity-call-design four-piece set; its data source is this design) | In-process **read-only mirror** (subscribes to the directory delta, InterServerLink projection + seq resumption) | First ServerID-segment a-priori routing (type/instance segments embedded in the id) → mirror lookup of zone_id → route_version/authority_epoch verification (§5.7) → on mismatch/miss, re-resolve (RequestReply low-frequency control plane) |
| **GM/observability queries** (scripting-lua §7.5 / observability) | Central truth | control-channel admin → baseappmgr: online list / single-player trajectory / per-baseapp statistics |
| **Whole-server broadcast / friend presence** | Mirror projection | The business subscribes to the SessionUp/Down stream (events semantics); broadcasts are distributed by the mirror per baseapp — baseappmgr is not scanned |

- **Consistency declaration**: the mirror = a projection (eventually consistent, seconds-level); **rulings and authoritative reads live only in baseappmgr**. The worst consequence of reading a stale mirror = one failed call + re-resolve — the same philosophy as RO_MIRROR (net-abstraction §7: sharing = subscribing to the owner's projection).

## 6. Crash Recovery (by the Role of the Dead)

| The dead | Handling | Basis |
|---|---|---|
| **baseappmgr** | machined relaunch (G-1) → recovery phase takes over exclusively (refuse new) → every baseapp/gateway **fully re-reports** live sessions → converged, reopen. The directory has no journal (volatile index, rebuild < 1s @ 5000 entries) | baseappmgr-domain recovery phase (net-abstraction §7); "the two recovery layers hold independently" (client resume and directory rebuild: whoever finishes first serves first) |
| **gateway** | Reverse-lookup by that `gateway_id` → all entries switch to Suspended (through the §4 window); client reconnects (any gateway instance) → resume (the token binds the baseappmgr-domain session semantics, not a gateway instance) or a new login | L2 resume (net-abstraction §3) |
| **baseapp** | Reverse-lookup by that `zone_id` → entries handled per the §7 recovery phase (reviver/rebuild); on rebuild completion SessionUp re-reports; sessions not recovered within the window follow disconnect timeout | G-2/recovery phase (net-abstraction §7) |

## 7. Boundary with Redis/DB

- **Not into Redis**: the directory is **arbitration state**, not shared hot data (attribute-sync §8.2 positioning: Redis does only the latter); displacement/admission need single-point serialization, and a Redis shared table splits the ruling into a distributed race — double-write consistency costs exceed the payoff, and it violates "centralized, not shared" (net-abstraction §7 four-class channel table). High-frequency friend-presence reads are served by the mirror (§5); Redis is not needed.
- **DB surface**: online state does not land in the DB; the account slot (last-online time / last baseapp) = archive-column promotion (`column: true`, storage.xml — belongs to #13/batch 2 statement set).
- **journal**: the directory has no journal (§6 rebuild argument).

## 8. Message Surface (internal-domain event family)

| Message | Direction | invoke_mode | Payload |
|---|---|---|---|
| `SessionUp` | baseapp → baseappmgr | OneWay | account_id/player_entity_id/session_id/gateway_id/zone_id/world_assignment/anchor_epoch |
| `SessionDown` | baseapp → baseappmgr | OneWay | account_id/anchor_epoch/reason |
| `SessionMoved` | baseapp → baseappmgr | OneWay | account_id/anchor_epoch/new world_assignment (cross-baseapp handoff/todo batch 3) |
| `SessionKicked` | baseappmgr → baseapp | OneWay | account_id/old anchor_epoch/reason |
| `DirectorySnapshotRequest/Reply` | baseappmgr ↔ baseapp | RequestReply | Full session set on reconciliation mismatch (low-frequency control plane) |

- Envelope = `InternalMessageEnvelope` (net-abstraction §7); ids enter the sdk-contract §11 internal-domain segment (900+) — **into the contract with a code batch** (contract files frozen; design only this round); zero client-domain additions (domain discipline); cross-domain bans stand.
- **Downlink notification (the kick close frame)** goes through the client control channel (net-abstraction §4.1 never-drop semantics).

## 9. Phased Landing

- **P2 (single process/Compact)**: the directory degenerates = a baseappmgr-domain in-process table — an extended consumer of `modules/game/session` SessionLocator (the existing mutex component goes to owning-thread form, with a code batch); displacement closes the loop in-process; the same for the docs/30 Compact form (baseappmgr embedded). **This form adds zero new processes, zero new protocols**.
- **P3 (multi-process full)**: InterServerLink projection/recovery phase/reconciliation/event family into the contract.
- **Dependencies**: G-1 fleet events (gateway/baseapp death detection), #13 login chain (upstream), the timer-wheel component (window timing — architecture-review §23-④/C-80 same batch), #15 Bots (acceptance counterpart: displacement storms/disconnect-reconnect stress tests as mutual acceptance).
- **Acceptance suggestions**: ① concurrent displacement (N ends logging in concurrently on the same account) leaves zero dual-authority residue (the directory's final state has exactly one entry); ② baseappmgr kill -9 → directory converges < 5s with zero mis-routing during the window (the mirror re-resolve backstop is provable); ③ mirror-routing mismatch rate (a new entry in the capacity metric family).

## 10. Intersections with Other Designs

| Related design | Placement |
|---|---|
| net-abstraction | §7 shared-model four-class channel table "session" row and "global arbitration state" row's landing piece; §3 resume = same-source window; §5.7 epoch = race-adjudication backstop; §5.9 login_token = entry credential (this design governs after entry); baseappmgr domain = directory owner |
| attribute-sync | §8.2 Redis boundary (directory stays out); §8 write-behind (archiving when the window fills); §4.3 sceneId (world_assignment positioning granularity) |
| sdk-contract | §11 internal-domain event family + errors.xml kicked code (with a code batch); zero client-domain additions |
| scripting-lua | §7.5 GM online queries consume this directory (admin single entry → control → baseappmgr); friend presence goes through mirror subscription |
| clock-and-time | window deadline = tick number; cross-process comparison uses only epoch/seq (anchor_epoch complies) |
| capacity-and-benchmark | directory capacity (~1MB, trivial); reconciliation/full re-report enter the frame budget (recovery-phase batching); mirror mismatch-rate metric |
| architecture-review | positioning of the legacy modules/game/session (§ Status Quo — migration registered as a code-impact item); remote-entity-call-design's "gap ③ host location has no data source" is closed by this design |
| design-gap-inventory | #12 CLOSED; prerequisite of #13/#14/#15 (login chain/inbound/Bots all consume the directory) |

---

*Baseline: apollo main @ 1e701d51 (documentation state; source frozen untouched). Legacy read (2026-09-30): modules/game/session six files 302 lines (player_anchor.hpp:12-30 AnchorState/SessionBinding, world_assignment.hpp:7-17, session_locator.cpp:5-66 incl. :9-11 displacement prototype) + apps/zone-app/src/base_server.cpp:106-167 (sole consumer bindSession/unbindSession/assignWorld) + tests/test_base_anchor.cpp (100 lines, direct test); KBE/BW precedent evidence = gap #12 registration (baseappmgr.cpp:588-599/:1117; KBE script-layer displacement/assets not on this machine, verified). The eight design rulings are in the executive summary; window defaults and the reconciliation period are initial recommendations, calibrated in the P3 capacity batch.*
