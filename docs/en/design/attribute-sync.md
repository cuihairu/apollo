
# Apollo Attribute Synchronization System Design (attribute-sync)

> Status: design draft (under review). Goal: using BigWorld's complete model as the baseline, design apollo's authoritative synchronization system for MMO attributes/state; the comparison set includes apollo's existing drafts (docs/03, `include/apollo/game/attributes`), SSEngine (the old engine, no such layer), KBEngine (.def contract + multi-end SDKs), Aeron (transport-layer ideas only, see net-abstraction.md).
> Related: `docs/design/sdk-contract.md` (contract and multi-end SDKs), `docs/design/scripting-lua.md` (attribute hooks and formula hot-update), `docs/design/net-abstraction.md` (QoS channels and backpressure), `docs/analysis/architecture-review.md`.

---

## Executive Summary

1. **Characterizing the existing approach: half-finished** (against BigWorld). The docs/03 draft and the `game/attributes` prototype only got as far as "flag bits + a 50ms batching queue": sync decisions dispatch immediately per change (`SyncTask` enters the thread queue item by item, docs/03 §4.1), with no per-viewer state, no change history, no ACK/retransmission semantics, no bandwidth budget — the three inevitable problems of packet loss, reordering, and slow clients all go unanswered. BigWorld closes the gaps with three mechanisms: **attribute ownership/visibility-domain declaration, change-history-driven per-client deltas, per-client bandwidth priority**. This design makes those three first-class citizens.
2. **Only two core data structures**: the per-entity `ChangeHistory` (a sequenced ring log of changes, the source of truth) and the per-viewer `viewer progress` (how far the other side's knowledge extends). delta = the difference of the two; a full snapshot = a history reset. Everything else (AOI enter/leave, throttle merging, priority drops) is policy layered on those two structures.
3. **Four attribute layers + dual-axis markers**: static config / persistent state / volatile state / derived computation, four layers under separate governance; every attribute carries an **ownership axis** (base authority / cell spatial authority — in the current single-process implementation the same authority, the marker is retained) × a **visibility-domain axis** (SELF/TEAM/GUILD/AOI/WORLD).
4. **The protocol is contract-generated, not hand-written**: the field semantics of attribute delta packets go into the single contract of `docs/design/sdk-contract.md` (attribute table + domain markers), and the server C++ and the Unity/Cocos/Laya SDKs are generated from the same contract — KBEngine's essence, with its storage weakness corrected.
5. **Persistence is independent of the sync pipeline**: the analysis of KBEngine's storage weakness is in §8 (entity tables + blob-ified complex types + non-dirty-driven periodic archiving + no write-behind journal; reworded after the C-51 correction); the countermeasure is "snapshot + write-behind journal + columnar promotion of queryable fields + BI stream split", reusing apollo `modules/data`'s async DB model (see ssengine-reference.md §4.1).
6. **Three-phase landing**: P1 single process + AOI + history/delta/ACK (shippable at this stage); P2 composite-type slices + bandwidth budget + compression; P3 cross-process cell mirroring (the protocol bits are already reserved in P1).
7. **Seventh-round deepening (2026-09-28, source evidence in architecture-review §16)**: §5.2 gains the BigWorld witness baseline evidence (per-viewer dual sequence numbers + a priority symbiosis structure); §7.3 gains the relationship of entity inheritance to the attribute contract (flat expansion at generation time); §8.2 settles the DB-flush cadence on KBEngine's Archiver smoothed writes as the same-form reference (dirty-driven + even dequeue); §10.1 adds the tick-vs-message-driven argument (G-7), §10.2 adds the graceful shutdown sequence (G-3).

---

## 0. Characterizing the Status Quo: Why the Existing Approach Is Half-Finished Next to BigWorld

### 0.1 Three-Way Comparison

| Capability | BigWorld (baseline) | KBEngine | apollo current state (docs/03 + game/attributes) |
|---|---|---|---|
| Attribute declaration | .def files: type + ownership flags + persistence + catalog | .def simplified from the same source + code generation for multi-end SDKs | The `AttributeDef` struct/config table (docs/03 §2) is a prototype, with **no ownership axis and no contract generation** |
| Sync decisions | attribute flags decide destination (OWN_CLIENT/OTHER_CLIENT/CELL_PUBLIC…), per-client lastSeq maintained, **change-history-driven deltas** | the same idea simplified, attribute ID + flags, clients receive deltas per entity | every change immediately produces a SyncTask dispatched to a background thread; **no per-viewer progress, no history, no ACK** |
| Visibility | AOI/interest + ghost (cross-cell mirrors) | AOI simplified + entity create/destroy messages | the `SceneAoi` event surface is delivered (P1-3: Enter/Sync/Leave + `ViewerState` per-observer watermark), **already wired into the attribute system** (batch 5: Enter/Sync first sight → snapshot frame, Leave → viewer state reclaimed) |
| Bandwidth control | per-client budget + entity priority (distance/importance); when the budget runs short, low-priority entities drop in frequency | has per-client options, coarse granularity | **none**. Fixed 50ms batching; fast and slow clients treated alike |
| Complex types | FIXED_DICT/ARRAY tracked dirty per slice | supports nesting, field-level granularity | blob attributes like `BAG_DATA` can only be resent whole |
| Persistence | base authority + backup + snapshot migration | MySQL entity tables, complex types as blobs (**weakness**, §8) | file archives + `PersistJournal` write-behind (P1-4/P1-5: atomic writes, quota'd drain, crash replay; MySQL not wired) |
| Client prediction | movement prediction + server reconcile (cell authority) | same | no design |

### 0.2 Concrete Defects of the Existing Prototype (code-reading findings)

- `AttributeContainer` (`include/apollo/game/attributes/attribute.hpp:52-97`): a `std::mutex` wrapping the whole map + a **single** `listener_` (an entity can attach only one callback — cannot simultaneously support "sync pipeline + script hooks + BI"); `AttributeValue = std::variant<...>` (a six-type variant) costs branches and size on the hot path, and **defines a second type system** duplicating `attribute_id.h`'s `AttrValueType` (a 12-type enum).
- `AttributeManager` (`:100-137`): a global singleton + global lock + `shared_ptr<AttributeContainer>` looked up by objectId — exactly the "global-lock lookup every tick" pattern that architecture-review.md P0-1 criticizes.
- `AttributeDef::syncToClient` (`:40`) is a bool — it answers only "sync or not", never "sync to whom, at what frequency, who may write".
- attribute_id.h's ~300 IDs are a flat transplant of the old project's attribute table (ID segments 1-12/101-500/301-700/3001+…), **with overlap conflicts between segments** (PLAYER_START=301 falls inside the CREATURE segment 101-500; CURRENT_STAGE=301 coexists with POISON_PERCENT=155 etc. inside the CREATURE range) — the first act of contractualization is rebuilding the ID space.
- docs/03 §4.1's `AttributeSyncManager`: a dedicated sync thread + `ThreadSafeQueue<SyncTask>` — putting "broadcast" outside the logic thread means attribute order for the same entity cannot be guaranteed (no barrier between the writing thread and the sync thread), the `excludeList` hangs off every task (an O(viewers) copy); `sequence` is a global counter rather than per-viewer, so the client cannot tell from it what it is missing.

**Conclusion**: do not "add features" onto the existing prototype — rebuild the skeleton per this design; what is worth keeping from the prototype is only the semantic asset of the attribute ID table (business meaning) and the AOI grid implementation.

## 1. Design Principles (the Essence Distilled from BigWorld/KBEngine)

1. **Declaration is policy**: an attribute's sync behavior (who may write, who it is sent to, how often, whether persisted) is declared entirely in the attribute definition (the contract), and the runtime executes it with zero if-branches-by-table-lookup — no scattered code where "every system picks its own message type to sync".
2. **One sequenced source of truth**: the server attribute store is the only truth; every change appends into a sequenced history; everything sent down (delta/snapshot/retransmit) is a projection of the history. Client state = a slice of the history at some sequence number, so "resend / reconnect / slow client" are all different entrances to the same mechanism.
3. **Think per viewer, not per change**: the unit of synchronization is "what a given client knows about this entity", not "one attribute change". Bandwidth, priority, and snapshot switching all hang off the (viewer, entity) pair.
4. **Bandwidth is a first-class constraint**: dispatch under a budget — rather delay low-priority entities than create burst congestion; deferred data is never lost (the truth lives in the history) and is naturally caught up in the next budget period.
5. **Layered authority, protocol first**: single-process today, but the base/cell ownership and the "remote mirror is read-only" protocol bits are fixed now; when BigWorld-izing, the attribute pipeline changes zero — only the broadcast backend swaps.
6. **Upstream carries only intents; downstream carries state**: the client never sends "SetAttribute", only intents (Move/UseSkill/Buy); attributes synchronize one-way from authority. Both anti-cheat and consistency follow from this one rule.

## 2. Attribute Model

### 2.1 Four-Layer Classification (Time Dimension)

| Layer | Examples | Storage | Sync | Persistence |
|---|---|---|---|---|
| L1 static config | monster attack power, level-up curves | config tables (loaded at startup, Lua-hot-updatable per version) | no incremental sync; a versioned reference sent on entering view | follows the version |
| L2 persistent state | gold, level, quest progress | attribute store + persistence layer | delta, mostly the SELF/GUILD domains | yes (write-behind) |
| L3 volatile state | HP, position, buff remaining | attribute store (memory authority) | high-frequency delta / a dedicated movement channel | no (or second-level sampling into BI) |
| L4 derived computation | all FINAL_* (the TOTAL_/FINAL_ segments in docs/03 and attribute_id.h) | **not stored**; recomputed cache | marked dirty after recompute, sent along with L2/L3 | no |

### 2.2 Type System (Converged to One)

- **Numeric attributes**: carried uniformly on `int64` (basis-point scaling — 1/10,000 units; docs/03 already follows this convention) — cache-friendly, efficient for delta/zigzag encoding, protocol-stable. Floating point is allowed only as server-side intermediate computation; anything that lands in an attribute is a scaled integer.
- **String attributes**: low-frequency attributes like DISPLAY_NAME/APPEARANCE, stored in a separate bucket (separated from numerics, avoiding variant).
- **Composite attributes** (BAG_DATA/REFINE_*/RUNE_INFO etc.): no longer synchronized as a whole blob. Defined as **slice containers**: the contract declares `slice_count` and `slice_type`, and every slot is an independent sync unit (dirty flags, sequence numbers, and sends all at slot granularity) — this is BigWorld's slice idea, solving "editing one bag slot resends the whole bag".
- **Event-like changes** (the "actions" of gaining/losing items): they do not belong to the attribute stream; they go on the reliable event channel; the attribute stream expresses only final state. The two converge on the client (attributes are the state machine; events are the stimuli).
- Delete `AttributeContainer`'s variant and `AttrValueType` dual track; in the contract every attribute has exactly one determined type (`sdk-contract.md` generates strongly-typed accessors for every end: `attr.get(AttributeId::CUR_HP)`).

### 2.3 Dual-Axis Markers (Contract Core)

Ownership axis (who has the right to write / where authority lives, the BigWorld base/cell semantics):

| Marker | Semantics | Single-process-phase behavior |
|---|---|---|
| `BASE` | persistent authority (login/archive side) | same-process write path; the persistence pipeline's integration point |
| `CELL` | spatial authority (combat/movement side) | same-process write path; in P3 this marker decides the attribute broadcasts with the cell |
| `RO_MIRROR` | remote read-only mirror | introduced in P3: the non-authoritative process receives the broadcast, writes the local mirror, marks dirty |

Visibility-domain axis (who it is sent to):

| Marker | Domain | Typical |
|---|---|---|
| `SELF` | the owner's client only | CASH, quest progress |
| `TEAM` | owner + teammates (AOI intersection) | TEAM_STATE, buffs |
| `GUILD` | guild members (may cross scenes, via global routing) | GUILD_ID changes |
| `AOI` | all observers within the interest range | appearance, LEVEL, ACTION, a visible combat-attribute subset |
| `WORLD` | whole-server broadcast (rate-limited whitelist) | world-boss phases, announcement-grade state |

- **The domain decides "field visibility"**: the same entity projects differently to different observers (a monster seen by an observer has no CASH field) — both snapshots and deltas are trimmed by domain; this is the essence of BigWorld's CELL_PUBLIC/CELL_PRIVATE.
- docs/03's PROPERTY_APPR/PRO/DB/SELF/TEAM/GUILD/WORLD bit flags: **semantics retained, bit definitions discarded** — the APPR/PRO distinction (immediate/batched) is not a "domain"; it moves into the frequency tiers of §5; DB moves into the persistence declaration. The three questions (who to send / how fast / whether to persist) were mixed into one uint by the old bit flags — split them apart.

### 2.4 Computation Pipeline (L4 Derived Attributes)

- Dependency graph: the contract declares a derived attribute's inputs (base attributes/equipment slots/buff categories), forming a DAG; cycles forbidden (validated at build time).
- Triggering: an input attribute changes → the owning derived attributes enter the recalc set → the tick's recalc stage recomputes them uniformly → results marked dirty (entering the §3 pipeline).
- Formula carrier: **Lua functions** (hot-update friendly, see scripting-lua.md §formulas); the C++ side keeps only dependency-graph scheduling and scaling. docs/03's `Formula` string-compilation scheme is discarded (building an in-house expression compiler is costly and not hot-updatable).

## 3. Dirty Flags and Delta Synchronization

### 3.1 Core Structures

```
EntityAttrs (per entity, owning thread 独占)
├── values[]            // 数值桶 int64 / 字符串桶 / slice 桶
├── dirty   : bitset    // 本 tick 待重算/待收集的属性位
├── history : ring[(seq, attr_id, new_value|slice_ref)]  // 变更环形日志（容量 N，默认 1024）
└── world_seq : u64     // 该实体最新 seq

ViewerState (per (client, entity))
└── acked_seq : u64     // 客户端已确认收到的实体 seq
```

- Write path: `set(attr, v)` → validate write permission (ownership axis + calling domain) → update values → append to history (carrying the changer's context: skill/script/GM) → set dirty → derived dependencies enter the recalc set. **Lock-free throughout**: attribute writes happen only on the owning thread (§10).
- `history` is the only truth projection for synchronization: its capacity is set by "slowest legitimate observer × highest change rate" (overflow policy in §3.3).

### 3.2 Collection and Dispatch (per broadcast tick, e.g. 10Hz)

1. For every dirty entity, find its observer set (the §4 viewer set cache);
2. For every viewer: `delta = history[(acked_seq, world_seq]]` trimmed by that viewer's visibility domains → generate the per-viewer delta packet;
3. The delta enters that client's **send priority queue** (§5), sent as budget allows;
4. Sending succeeded does not mean acknowledged: the client returns `AttrAck{entity_id, seq}` (which may piggyback on any upstream packet), advancing `acked_seq`.

### 3.3 Full Snapshot vs Delta Selection (Switch Timing)

| Timing | Action |
|---|---|
| entering view / login / reconnect | full snapshot (the current value set trimmed by domain) + record `acked_seq = world_seq`; delta thereafter |
| ACK timeout (e.g. no acks at all for 3 broadcast ticks) | that viewer degrades: resend the recent delta; timing out again → snapshot reset |
| `world_seq - acked_seq > history capacity` | snapshot reset (history can no longer cover the other side's progress) |
| delta attribute count > threshold (e.g. >128, or larger when encoded than a snapshot estimate) | snapshot directly (cheaper) |
| entity respawn / transparency switch causing a domain change | the changed part of the domain projection is re-sent as a snapshot subset |

Snapshots and deltas share one sequence space: a snapshot is essentially "pushing the other side's progress straight to the current seq", so both sides can use seq to detect and discard reorderings/duplicates.

### 3.4 Merge Semantics

- Multiple changes to the same attribute within one broadcast tick: **last-write-wins + change count** (the count is for BI/debugging; it does not participate in client state).
- Cross-tick backlog (low priority deferred): sending the final value suffices — the attribute stream is state, not events; intermediate values are meaningless.
- The unmergeable exception: an "array length change" of a slice structure must arrive in order (grow first, then write the slot); slice operations are sent as an order-preserving subsequence.

## 4. Visibility and Broadcast Domains

### 4.1 Viewer Set: Wiring to AOI

- The `SceneAoi` event surface is delivered (after P1-3 convergence, the repo's only implementation): the three events Enter/Sync/Leave + the `viewers_of`/`viewers_at` queries, with the diff baseline being `ViewerState` (per-observer watermark) — **the attribute system is its first official consumer (wired, batch 5)**:
  - Enter (entity A enters B's AOI): create `ViewerState` for (B, A), and A sends create+snapshot to B; simultaneously in reverse (AOI is a symmetric relation — naturally symmetric by grid adjacency, but events remain authoritative).
  - Leave: send the destroy message and reclaim the ViewerState.
  - Sync (cross-grid movement): only updates the viewer set's cache ownership, triggering no protocol.
- Observer computation is maintained incrementally per tick (the incremental nature of AOI events); the attribute collection stage does **no spatial queries at all** — spatial computation and sync collection are decoupled, each O(amount of change).
- The `SELF` domain does not depend on AOI: the owner's ViewerState for itself is established at login and destroyed only at logout.

### 4.2 Domain Composition and Projection

- The set of attributes visible to viewer V = the intersection of the attribute contract's visibility-domain markers with (the set of relations between V and the owner: SELF/TEAM/GUILD/AOI distance).
- Team/guild membership is provided by the social domain as a relation bitmap (every entity maintains `team_mask/guild_id`); the collection stage decides in O(1).
- The same entity has different deltas for different viewers (domain trimming happens independently on each ViewerState) — this is the price of the per-viewer design (a single packet cannot be globally multicast), buying correctness: snapshot/delta/ACK/bandwidth all close the loop. Packet-count optimization in AOI-dense scenes comes from §5's priority and merging, not from sacrificing per-viewer correctness.

### 4.3 Special Visibility Rules (Expressible in the Contract)

- Stealth/death: AOI flags + attribute-domain linkage — while stealthed, the viewer set for that entity shrinks; the attribute system does no special-casing (it is just the viewer set changing). **Not implemented**: the flags bits were deleted with the old `aoi.hpp` (P1-3); `SceneAoi` has no per-entity flags yet, to be added with the wiring batch.
- Fog of war / segmented visibility (instances): scene isolation is guaranteed by AOI — already implemented as structural isolation (each `Scene` has its own `SceneAoi` instance, P1-3); no attribute-system involvement needed.

### 4.4 Cross-Process Reservations (P3, BigWorld-ization)

- `CELL`-marked attributes are forwarded between cell processes by the spatial manager (ghost: mirrors of entities in adjacent cells); mirror entities are `RO_MIRROR` in the local process — the attribute pipeline is oblivious (it writes the mirror + marks dirty + serves local observers as usual); only the write source becomes "a remote broadcast".
- Protocol bits reserved: the delta packet carries `authority_epoch` (the authority epoch), which the mirror side uses to reject write-backs; migration (an entity crossing a boundary) = the new cell builds a snapshot + the old cell sends a destroy, reusing §6's enter/leave mechanism.
- The method-call dimension (cross-process RPC / RemoteEntityCall) is out of scope for this design — the P3 prerequisite wiring is in net-abstraction §7 "P3 Prerequisite Design", the entity remote call block; this section governs only attribute projection and mirroring.

## 5. Priority and Throttling

### 5.1 Client Bandwidth Budget

- Token bucket per client (e.g. 32KB/s by default; login-configured + dynamically lowered by congestion signals; upstream unlimited).
- Every broadcast tick draws bytes from the budget and packs by priority queue; budget exhausted → the queue is kept (the truth is not lost) and transmission continues next tick.
- Burst protection: a per-tick per-entity delta cap (to keep one recalc storm from blowing the budget).

### 5.2 Entity Priority (per viewer)

```
priority(entity, viewer) = w1·importance(class)      // BOSS > 精英 > 玩家 > 怪 > 掉落物
                         + w2·f(distance)            // 近者优先
                         + w3·recency(changes)       // 刚变过的优先
                         + w4·visible(遮挡/战斗参与)  // 可选
```

- docs/03's intuition of "appearance syncs immediately, the rest batched" is right, but it was implemented as a per-task branch; this design unifies it into priority parameters (importance is a contract field), and the mechanism is a single queue.
- Starvation protection: a maximum deferred-tick count per entity; over the limit, forced send (even slightly over budget).
- **Baseline evidence** (16.5-①, witness.cpp:2470-2514, the `dumpAoI` introspection format): BigWorld records **dual sequence numbers** per (viewer, entity) — `volatile 129/130. event 45/45` (separate lastSeq/received for volatile updates and event updates), plus the per-frame-recomputed `priority !0.000000` flag; isomorphic to the §5.4 channel split (the movement channel with its own seq, the attribute/event channels each with their own seq), and it proves that "per-viewer progress + priority" was a symbiotic pair of structures in the original — this design's §3.1 ViewerState and this section's formula follow the same skeleton.

### 5.3 Throttle Tiers (contract field `min_interval`)

| Tier | min_interval | Examples |
|---|---|---|
| INSTANT | 0 (sent on the current tick) | death, stealth, name |
| HIGH | 1 broadcast tick | big HP swings, LEVEL |
| NORM | 4 ticks | BUFF-type, non-critical combat values |
| LOW | 20 ticks + client-side interpolation | natural HP-regen ticks, minor coordinate adjustments (position itself goes on the movement channel) |

High-frequency jitter converges naturally inside the merge window (last-write-wins); no separate "downsampler" is needed.

### 5.4 QoS Channel Split (interfacing with net-abstraction.md)

| Channel | QoS | Content |
|---|---|---|
| movement channel | unreliable, latest-value overwrite | position/orientation/velocity (20Hz, drop old not new) |
| attributes channel | reliable ordered | attribute deltas/snapshots/ACKs (10Hz batches) |
| events channel | reliable ordered | damage numbers/item acquisition/system events |
| control channel | reliable ordered | login/reconnect/heartbeat/ACK piggyback |

Ordering on the reliable channels is guaranteed by the session layer (net-abstraction.md §session layer); the attribute seq serves ACK and snapshot-switch decisions and carries no transport-layer duty — the two sequence-number layers have distinct responsibilities.

## 6. Entity Lifecycle Synchronization

| Phase | Server action | Client receives |
|---|---|---|
| Spawn (enters view) | create ViewerState → `CreateEntity{template_id, position, flags}` + domain snapshot (attributes channel; same tick as Create, guaranteeing create-before-modify) | create the entity → apply the snapshot |
| resident | delta batches (§3.2) | apply deltas |
| Leave (exits view/destroyed) | `DestroyEntity{reason}` + reclaim the ViewerState (reclaimed one tick late to prevent ping-pong: briefly leaving view does not immediately clear acked_seq; on re-entering view, if the progress is still continuous, only deltas are backfilled — the usual optimization for AOI-boundary jitter) | destroy the entity |
| reconnect | session resume (an net-layer token) → snapshot reset for all SELF-domain entities; AOI entities go through Spawn again | full-scene rebuild |
| death | STATUS attribute (INSTANT, AOI domain) + flags linkage; the corpse timer belongs to the scene | status presentation |
| migration/connection change (P3) | Destroy at the old place + Create at the new (from the observer's perspective); from the owner's perspective, a scene-rebuild snapshot | rebuild |

## 7. Protocol Shape

### 7.1 Message Definitions (protobuf notation = encoding layout reference; the contract source is XML+XSD, architecture-review §15.3)

> 2026-09-28 sync annotation: this section's protobuf describes the **wire encoding layout** (varint/zigzag/packing strategy), not the contract source — the single source of truth for field semantics is the contract (attrs/messages.xml, sdk-contract §2.3); the server and every end's encoders are produced by the generator (xml-generation.md §4). The notation is kept because protobuf expresses compact layouts most directly.

```protobuf
message AttrDelta {
  uint32 attr_id   = 1;              // varint；slice 属性高 16 位为 slot
  sint64 value     = 2;              // zigzag（数值/字符串桶则走 bytes）
  uint32 change_n  = 3;              // 合并次数（诊断用）
}
message AttrBatch {
  uint64 entity_id = 1;
  uint64 base_seq  = 2;              // 本批起点（history seq）
  repeated AttrDelta deltas = 3;
  bytes  snapshot  = 4;              // 快照模式：域裁剪后的全量（packed attr k/v）
  uint32 epoch     = 5;              // 权威纪元（P3 镜像防回写）
}
message AttrSyncFrame {               // 每客户端每广播 tick 至多一帧
  repeated AttrBatch batches = 1;
  uint32 frame_ms   = 2;
  bool   zstd       = 3;             // payload 压缩标记
}
```

- Variable-length first: attr_id varint, numerics zigzag; batches packed per entity and **zstd only over 256B** (compression is a net loss on small packets).
- **Snapshots/resets go through the same message too** (the snapshot field set) — the client handles only one kind of sync message.

### 7.2 Sequence Numbers, Reordering, and Loss

- The reliable ordered channel guarantees arrival order ⇒ base_seq is monotonic; the client detects a seq jump (theoretically it should never happen) → requests a snapshot reset (the self-healing path is always available).
- The movement channel tolerates reordering and loss: packets carry timestamps; anything older than the local newest is discarded (the standard dead-reckoning receive side).
- **Attribute table version**: the login handshake exchanges `attr_schema_hash` (generated from the contract, see sdk-contract.md); a mismatch → full SELF-domain send + an alarm (preventing silent semantic drift between the two ends from corroding state).

### 7.3 Relationship to the SDK Contract

Attribute IDs, domain markers, min_interval, slice definitions, derived dependencies — **all written exactly once, in the contract** (`docs/design/sdk-contract.md`); the server generates C++ accessors, and the Unity/Cocos/Laya SDKs generate strongly-typed AttrId constants and container classes. The server's `attribute_id.h` current state (hand-written enum + segment conflicts) migrates to "contract-generated + semantic review".

**Relationship to entity inheritance** (sdk-contract.md §2.3 entities.xml / architecture-review §16.7.2): attrs.xml is a **per-entity flat** attribute table (the generator expands it at build time per entities.xml's `parent` single inheritance: parents enter first, children override, same-name type changes forbidden, with provenance). The attribute contract describes only the expanded result; inheritance syntax never enters attribute projection — the three consumers (protocol/SDK/server) all see an already-flat attribute set; `attr_schema_hash` is computed over the source files + the generator version, and a parent entity's changes propagate automatically to every child entity's artifacts.

## 8. Persistence (Including the KBEngine Storage Weakness Analysis)

### 8.1 Why KBEngine Is "Too Weak" (Point by Point)

1. **Entity tables + blob-ified complex types**: FIXED_DICT/ARRAY land in the database as strings/blobs — MySQL can store them but not query them (no `WHERE bag->slot=...`); ops/operations queries all require full loads. (C-51 recheck upholds this)
2. **Base-only persistence + non-dirty-driven periodic archiving** (C-51 correction; the original claim "snapshot-style writes/uncontrollable window" was inaccurate): an Archiver archives periodically — 300s by default (kbengine_defaults.xml:621), with the entity queue in a random sequence smoothing the writes (archiver.cpp:26-63, avoiding whole batches flushing to the DB simultaneously), gated per entity by `shouldAutoArchive`; **the real weakness is being dirty-blind and rewriting the entity whole** — the default archive window is coarse (5 minutes), the crash-loss granularity = the archive interval, and dirty-driven fine-grained persistence remains apollo's differentiator (the write-behind journal). 
3. **No write-behind journal**: no "replay the changes" capability; recovery = back to the last snapshot, and the changes in between are permanently lost. (C-51 recheck upholds this)
4. **Query capability = point lookups mostly** (C-51 correction; the original claim "no secondary indexes" was inaccurate): `<Indexed>` attributes get **real MySQL single-column indexes** (entity_table_mysql.cpp:236-300 index sync + :113 ALTER ADD INDEX) and point lookups by DBID; but no aggregation/composite queries/query surface — leaderboards, complex GM queries, and BI must still fend for themselves.
5. **Weak sharding/partitioning strategy**: hashing by account exists, but entity hot/cold tiers, archiving, and cross-server migration have no complete story.

### 8.2 apollo Countermeasures

> Scope statement (added 2026-09-29, architecture-review G-2): this section answers only **data is not lost** (journal/snapshot/replay), not **the process never falls** — process-level high availability (hot backup/takeover/migration) is P3 prerequisite design, see net-abstraction.md §7 "P3 Prerequisite Design"; the single-process phase explicitly has no high availability; crash safety = this section's mechanisms + restart-and-launch.

- **Write model: snapshot + write-behind change journal**.
  - L2 attribute changes append into the per-entity `PersistJournal` (separate from §3.1's sync history: the sync history faces observers and may be evicted by capacity; the persistent journal faces the DB and must not be lost);
  - the DB worker thread flushes to the DB in budgeted batches (reusing the §async DB model, see ssengine-reference.md §4.1); a successful flush advances `persisted_seq`; crash recovery = latest snapshot + journal replay (replay is idempotent: attribute-set semantics is naturally idempotent);
  - **the flush cadence = even smoothed writes, not "hoard then dump"** (the C-51 same-form reference): KBEngine's Archiver processes, per tick, the segment "entity count × idx / period" (archiver.cpp:26-63 — smoothed evenly across the whole period, preventing whole batches of entities hitting the DB on the same tick); apollo's write-behind queue likewise dequeues a fixed quota per tick (each tick takes the head segment of `dirty entity count / target flush tick count`), avoiding "backlog accumulating in low-traffic periods → DB spikes in high-traffic periods"; the difference is the trigger source — the Archiver blind-scans on a period (dirty-blind), while apollo dequeues only entities whose `PersistJournal` is non-empty (dirty-driven + even smoothed writes, taking both);
  - L3 does not enter the journal (volatile state is discarded anyway); critical L3 (e.g. HP just before battle settlement) is snapshotted explicitly by the business.
- **Flush form: columnar promotion of queryable fields**. The contract gives attributes a `column: true` marker (gold/level/class and other operations-query hot spots), generating standalone columns; the rest go into a compressed snapshot column. Both "fast whole-entity read/write" and "SQL queryable".
- **Read path**: login = snapshot + replay of the un-flushed journal tail; cross-server/migration = snapshot + journal following.
- **BI stream split**: docs/08's BI/TLog goes through an independent collection channel (attribute-change events are sampled and exported in a bypass during the collection stage, outside the §5 budget); **BI requirements must not pollute the attribute table structure in return** (KBEngine's blob-ification was induced precisely by "wanting to store and query everything").
- **The Redis layer**: only cross-process shared hot data (guild/leaderboards/global state); **never an entity-attribute cache** (two-write consistency costs exceed the benefit; attribute authority lives in process memory + the DB).

### 8.3 DDL Lifecycle: Versioned Migrations

storage.xml fixes the **runtime statement surface** (§15.5's three statements: snapshot upsert / journal append / replay range, plus promoted-column reads/writes); this section adds the **structural evolution surface** — how tables are created, how columns are added, how migrations are governed. The two share a domain: server-private, never entering the contract directory, never entering schema_hash (§15.5's two-stage discipline unchanged — pure storage evolution causes zero protocol-version events).

- **Versioned migration directory + an offsets table**: the server-private `db/migrations/`, one file per migration, named `NNN_description.sql` (NNN a monotonic index, starting at 001); **the offsets table `schema_migrations`(version, checksum, applied_at) is the single source of truth** — not file markers (they drift in multi-instance/container environments; a table cannot).
- **First table creation = migration 001**: the full table-creation DDL. The initial form may be produced by the generator from the contract's `persist/column` hints + storage.xml as a **suggested draft**, but the DDL always goes into the repo only after human review — the data-safety line: the generator never touches the schema directly.
- **Forward-only, no down-migrations**: rollback = roll-forward (a new migration corrects an old one). Rationale: MMO data is unrebuildable player assets, and down-migrations invite the illusion of "rolling back the schema, losing the data"; Flyway's same trade-off. **Destructive statements forbidden** (migration lint built in; a hit refuses to load): DROP COLUMN / DROP TABLE / TRUNCATE / semantics-changing RENAME — column deprecation = marked deprecated + writes stopped (data retained); actually dropping a column = a manual DBA operation in a maintenance window, never through the migration line. Added columns must be NULL-able or carry a default: a post-migration database must be able to replay a pre-migration journal (§8.2 recovery-path compatibility).
- **Startup runner**: at process startup (before module assembly) it compares the offsets table with the directory → unapplied migrations execute **in order**; any failure = startup failure (fail-fast, on the same line as "errors reportable at startup never wait for runtime"); under MySQL DDL's implicit-commit limitation, at minimum abort item-by-item + offsets not advanced (a restart resumes from the interruption point; applied ones never re-run). **Locked during execution**: in the single-process phase, a local file lock; P3 multi-process same-DB = a fleet lock (G-1 mgr arbitration) — two processes migrating the same database simultaneously is an incident, not an edge case.
- **Large-table ALTER discipline**: index/column adds explicitly declare `ALGORITHM=INPLACE, LOCK=NONE` (MySQL 8); estimated row counts and durations go into the migration file's header comment; shadow-database rehearsal (running `--dry-run` against the shadow DB first) belongs to the apps/-side db tools — the standalone-tool-family form references BW server/tools/sync_db.
- **Precedents**: KBE entity_table_mysql.cpp:113 startup-time `ALTER TABLE ADD INDEX` — unversioned, potentially self-modifying the table on every startup, no offset record (the **negative reference**: unauditable and unreplayable, exactly the form this section eliminates); BW sync_db as a standalone tool (form reference); the Flyway/Liquibase offsets-table model (borrowing the form without the dependency — the MySQL-side runner is ~100 lines; unlike §15.5's "validator ≤ 0 lines" trade-off but the same reasoning: no ready-made zero-cost part exists here, worth writing ourselves).
- **Linkage with contract column promotion**: an attrs.xml `column:true` change → the generator's CI emits a **diff reminder** ("the contract declares column promotion but no corresponding column is seen in the schema"); it never auto-produces a migration — every line of DDL passes human eyes.

### 8.4 Redis Deployment and Keyspace Discipline

The last line of §8.2 already fixes the **positioning** (cross-process shared hot data; no entity-attribute cache, no second-level of cache); this section adds the deployment surface that makes the positioning executable.

- **Topology**: single instance to start (the P3 single-machine fleet period); Sentinel introduced with multi-machine fleets (primary-replica + failover); **Cluster mode is off the roadmap** — the keyspace scale (guild/leaderboard/global state) sits far below a single instance's memory, and Cluster's multi-key operation limits and hash-tag complexity buy no matching benefit (the same trade-off family as net-abstraction §5.3's "no external coordination service").
- **Connections and threads** (target state, **not implemented** — P1-4 deleted the whole redis family; the Redis client and connection pool are to be reintroduced with the cross-process batch): one unified modules/data client + connection pool; **the synchronous face bars direct calls from scene threads** — a Redis network round trip is the same as SQL, always via scripting-lua §8's async handoff (an L0 execution-layer IO thread pool + request_id back to the scene thread); command timeouts + PING health checks + a per-process connection-limit constant.
- **Keyspace**: the `apollo:{domain}:{world}:{key}` prefix hierarchy (`guild:{wid}:{gid}` / `rank:{wid}:{board}` / `global:{key}`); **every key must have a TTL or an explicit permanent-residency justification** — an unbounded key with no TTL = using Redis as a second schema-less DB; forbidden (the data-plane edition of the §15.2 "forbid a second set" discipline).
- **Consistency boundary**: Redis is **shared working memory, not a source of truth** — the source of truth = the owner process's memory + the DB journal (§8.2); losing the whole Redis store = rebuild from the DB + brief degradation (leaderboard rebuild/guild read-only), **not a data-loss incident** (this is also a corollary of "no entity-attribute cache"). **Cross-process mutual exclusion must not depend on Redis locks** — rights adjudication belongs to the G-1 mgr's single-point ordering (fleet events); Redis atomicity serves only the data plane (INCR/ZADD/LUA scripts), never ownership and arbitration.
- **Legacy convergence**: C-45's four coexisting redis clients were an audit issue, and the convergence discipline is set (architecture-review §15.2); **executed** (P1-4: the five legacy sets were deleted together with the entire `modules/data/redis` tree, 42 files); this section remains target-state design.

## 9. Client Prediction and Server Validation

| Category | Attribute/behavior | Rule |
|---|---|---|
| predictable | own movement (position/orientation) | client leads + server-authoritative reconcile: upstream input (direction + duration); the server simulates by the same rules and, on deviation past a threshold, sends an authoritative position correction (movement channel) |
| predictable (presentation) | own skill windup/anticipation animations | the client plays the animation first; the server validates CD/resources/distance; if invalid → authoritative state overrides (a cast-cancel event) |
| presentation-only prediction | enemy HP-drop display | the client may interpolate from damage events, but **state defers to the attribute stream**; short-term presentation/state divergence is allowed (that is the definition of presentation-layer prediction) |
| not predictable | others' attributes, gold, inventory, quests | purely server-authoritative, zero prediction |

- **Validation principle** (server side): everything upstream is an "intent"; speed/CD/resources/distance validated against contract constants; validation failure neither crashes nor kicks — **authoritative correction + a counter** (past a threshold, into risk control).
- Prediction introduces no new sync mechanism: correction is just "the authoritative value of the attribute stream/movement channel naturally overwriting the client's local value"; the client SDK's duty is maintaining the blend between predicted and authoritative values (the client container interface of `sdk-contract.md` is designed for this).

## 10. Threading Model and Tick Integration

```
主循环（每 tick，owning thread 串行）:
  1. simulate       — 业务/脚本写属性（只允许本阶段；set() 无锁，因为单写者）
  2. recalc         — 派生属性重算（依赖图拓扑序）
  3. aoidecay       — AOI 增量事件 → viewer set 增删（SceneAoi 事件流，P1-3 已交付）
  4. collect        — 按 (viewer, entity) 组装 delta/快照（读 history，无锁）
  5. budget/flush   — 预算装包 → net 抽象层发送（net-abstraction.md）
  6. persist-batch  — 持久日志批量交给 DB 工作线程（异步，回调排回下一 tick 阶段 1 前执行）
  7. shutdown*      — 仅停机路径进入（§10.2）：flush journal → 停收新连接 → drain → 落库 → 逆序停模块
```

### 10.1 Why Fixed-Step Ticks, Not Message-Driven (deepened 2026-09-28, architecture-review G-7)

The three-way paradigm comparison (architecture-review §16.2): BigWorld/KBEngine use fixed-step ticks (BigWorld cellapp.cpp:806-808; KBEngine gameUpdateHertz=10, kbengine_defaults.xml:5 + cellapp.cpp:252-261); skynet is purely message-driven with no tick (skynet_server.c:293-315 — one dispatch per message, cond_wait on empty queue). This design **chooses ticks**, and the argument is written down to prevent re-litigation:

1. **The design's three core mechanisms all presuppose deterministic cadence boundaries** — the `acked_seq`/`world_seq` sequence advancement and timeout decisions (§3.2/§3.3 "3 broadcast ticks"), the derived DAG's topological batching (the recalc stage converges the whole graph at once, §2.4), and the token bucket's per-tick budget draw and burst cap (§5.1). Under message-driven, each of the three must invent a private cadence — which amounts to scattering the tick into every mechanism.
2. **The skynet paradigm mismatches the authoritative entity model**: skynet suits **stateless service orchestration** (one message triggers one side-effect-free service), and its single-writer boundary is the "service", not the "entity"; apollo's single-writer boundary is **the scene thread owning the entity**, and the write semantics (the precondition of §3.1's lock-free whole path) demand "a batch of changes visible, ordered, and sequenceable inside a fixed boundary" — that is the definition of a tick, not of a message.
3. **Apply §0's "ideas vs forms" split to the scheduling paradigm**: skynet's ideas (lightweight services, message-driven, visible overload) are each absorbed (services ≈ scenes, queues ≈ the MPSC ring, MQ_OVERLOAD ≈ the four-level watermark); ticks vs message-driven is a **form** choice, decided by the nature of authoritative attribute synchronization, not by whichever is more modern. Both MMO precedents, BigWorld/KBEngine, chose ticks (16.5-④); the selection is unremarkable.

### 10.2 Graceful Shutdown Sequence (deepened 2026-09-28, architecture-review G-3)

Stage 7 expanded (the normal shutdown path; the crash path never passes through here):

```
停机触发 → ① 停收新连接与新 intent（control 通道回"维护中"）
         → ② 本 tick 跑完阶段 1-6 的最后一轮（在途变更全部进 history/journal）
         → ③ 阻塞等待 DB 工作线程把 PersistJournal 全量刷完（persisted_seq 追平 world_seq；
             复用 §8.2 三条语句，KBEngine 停机同型零件：onDestroyEntity → writeToDB，
             baseapp/entity.cpp:698-731）——带超时上限，超时告警并继续（日志完整性优先于停机速度）
         → ④ net 层 drain 在途帧 → 断开（客户端走重连，命中另一台/重启后的本机）
         → ⑤ 模块按依赖逆序停止（architecture-review §0 生命周期纪律的停机镜像）
```

- **The single-writer discipline replaces locks**: an entity belongs to its scene/spatial thread (in the future, multiple cells = static thread ownership per entity; cross-thread access always delivers intents/messages). `AttributeContainer`'s existing mutex (`attribute.hpp:96`) and `AttributeManager`'s global lock are abolished with the rebuild.
- docs/03's "dedicated sync thread" scheme is discarded: broadcasting completes in stages inside the logic thread (collect/flush are pure memory operations + non-blocking sends, with controllable cost); cross-thread happens only at net sending and DB flushing, and both go through lock-free queues — ordering is guaranteed naturally by the single writer.
- Timeout backstop: stages 4/5 exceeding budget (e.g. >1.5ms) truncate immediately, the remainder deferred to the next tick — the broadcast latency ceiling stays controllable.

> **2026-10-09 delivered on the code face (G-3, apps/base-app)**: `BaseServer::stop()` rewritten as five stages — (1) the `shuttingDown_` atomic bit goes up first (the acceptance face `dispatchRequest` uniformly answers "Server is shutting down" maintenance-mode responses, session numbers preserved), the listener closes and in-flight handlers are joined (RepSocket semantics; doubles as stage (4)'s same-tick disconnect); (2) autoSaveThread_ is joined before the full `flushDirtyAnchors` (the producer stops first — fixing the earlier race where the join came after the queue drain and re-enqueueing was still possible post-drain); (3) `SaveQueue::stop` drains fully, then the journal loop drains to `pending()==0`; with zero progress within the timeout cap `shutdownFlushTimeoutMs` (default 5s) it alarms and lets it pass — leftover entries await restart replay (idempotent); log integrity outranks shutdown speed; (5) the innermost storage closes last. Idempotent gate: CAS guarantees the signal handler + destructor double call runs exactly once (previously a repeated flush/double shutdown). Accompanying: autoSaveLoop's sleep becomes 50ms slices (a shutdown join no longer eats an entire save cycle — SIGTERM shutdown tightened from up to 60s to ~1s); `base_anchor_tests` gains two groups, the maintenance gate + journal-caught-up/archive-re-readable (the acceptance face exposed as the `dispatchRequest` direct-test entry — the stub-transport tree has no real REP face). Real-process smoke: SIGTERM → maintenance bit → five stages → exit 0 (journal pending 0).

## 11. Metrics and Acceptance

| Metric | Target |
|---|---|
| attribute change to client p99 latency | ≤ 150ms (NORM tier), INSTANT ≤ 1 tick |
| single-client steady-state bandwidth | ≤ 80% of budget (100 entities on screen in a combat stress test) |
| delta compression ratio | batches over 256B at ≤ 50% after zstd |
| snapshot switch rate | steady state < 0.1%/viewer·min (should be produced only by view enter/leave and reconnects) |
| reconnect recovery | ≤ 1 RTT + 1 snapshot packet (≤ 50KB) |
| crash rollback window | journal replay within the write-behind flush interval (2s default), zero loss |
| contract consistency | CI: attr_schema_hash three-way (server/unity/ts) comparison passes |

## 12. Implementation Phasing

- **P1 (single-process usable)**: attribute store rebuild (four layers + dual-axis markers) → ChangeHistory/ViewerState/delta/ACK → AOI wiring (spawn/leave) → three protocol messages + SDK contract generation (unity/ts minimal ends) → movement channel separation. Acceptance: AOI 100-entities-on-screen stress test.
- **P2 (experience and bandwidth)**: slice composite types → bandwidth budget + entity priority → throttle tiers + zstd → write-behind persistence + columnar promotion → client prediction (movement).
- **P3 (BigWorld-ization)**: CELL/RO_MIRROR mirroring + authority_epoch → entity migration → ghost broadcast backend (enabled when net-abstraction.md's inter-process channel lands).

## 12.1 Landed Implementation

The `modules/game/attrsync/` submodule has delivered the per-viewer delta pipeline core layer + integration layer + frame codec + ACK-timeout degradation + SceneAoi wiring + §5 priority and throttling + the §8.2 write-behind journal entry, seven batches in all:

### Batch 1 — Core Layer (`c7b6c3dc`)

| Item | Files | Status |
|---|---|---|
| `ChangeHistory` | `include/apollo/game/attrsync/change_history.hpp` + `src/change_history.cpp` | ✅ landed |
| `ViewerAttrState` | `include/apollo/game/attrsync/viewer_attr_state.hpp` + `src/viewer_attr_state.cpp` | ✅ landed |
| `AttrDelta` | `include/apollo/game/attrsync/attr_delta.hpp` + `src/attr_delta.cpp` | ✅ landed |
| `DeltaBuilder` | `include/apollo/game/attrsync/delta_builder.hpp` + `src/delta_builder.cpp` | ✅ landed |

**Design sections mapped**: §3.1 (the ChangeHistory/ViewerState skeleton), §3.2 (delta collection), §3.3 (snapshot switching), §7.1 (AttrDelta encoding).

**Encoding primitives**: `AttrDelta::encode()/decode()` uses hand-written varint/zigzag (zero dependencies): attr_id varint + value zigzag varint + change_n varint.

### Batch 2 — Integration Layer + AOI Wiring (`b4449d9d`)

| Item | Files | Status |
|---|---|---|
| `EntityAttrs` | `include/apollo/game/attrsync/entity_attrs.hpp` + `src/entity_attrs.cpp` | ✅ landed |
| `AttrSyncManager` | `include/apollo/game/attrsync/attr_sync_manager.hpp` + `src/attr_sync_manager.cpp` | ✅ landed |

**Duties**: `EntityAttrs` = per-entity attribute store + dirty bits + ChangeHistory; `AttrSyncManager` = entity registration, the attribute write path, AOI Enter/Leave event wiring, broadcast collect, ACK advancement.

**Design sections mapped**: §3.1 (EntityAttrs), §3.2 (broadcast tick), §4.1 (AOI wiring).

**Tests**: `tests/test_attr_sync_manager.cpp` — 20 cases (EntityAttrs 7 + AttrSyncManager 13).

### Batch 3 — Frame Codec (`0d0b6def`)

| Item | Files | Status |
|---|---|---|
| `FrameCodec` | `include/apollo/game/attrsync/frame_codec.hpp` + `src/frame_codec.cpp` | ✅ landed |

**Duties**: `AttrSyncFrame ↔ wire bytes`; encoding layout entity_id varint + base_seq varint + flags varint (bit0=is_snapshot) + count varint + count × AttrDelta; batch encoding count + count × frame. Decoding validates truncated / trailing garbage and returns `nullopt`.

**Contract fix**: `AttrDelta::decode_into`'s return value changed from absolute offset to bytes consumed, eliminating a double accumulation in frame_codec.

**Tests**: `tests/test_attr_frame_codec.cpp` — 10 cases.

### Batch 4 — ACK Timeout Degradation (`2fcb80c2`)

| Item | Files | Status |
|---|---|---|
| `AttrSyncManager` ACK timeout degradation | `src/attr_sync_manager.cpp` (increment) | ✅ landed |

**Semantics**: a viewer silent ≥ the threshold ticks (default 3) → forced snapshot reset every tick (`is_snapshot=true` inside `collect` + advancing `acked_seq` + `clear_dirty`); the silence counter is retained (no erase) and cleared only when an ACK arrives, ending the degradation. Corresponds to §3.3's "self-healing path".

**Tests**: `tests/test_attr_ack_degrade.cpp` — 7 cases (including ConfigurableThreshold / RecoverAfterAck / MultipleViewersIndependent).

### Batch 5 — SceneAoi Wiring

| Item | Files | Status |
|---|---|---|
| `Scene::attach_attrsync` / `detach_attrsync` / `attrsync_attached` / `drain_attrsync_frames` | `modules/game/world/include/apollo/game/world/scene.hpp` + `src/scene.cpp` | ✅ landed |
| AOI event consumption `Scene::on_aoi_event` | `src/scene.cpp` | ✅ landed |
| spawn/enter registration hooks | `src/scene.cpp` (inline registration inside `spawn_entity` / `enter`) | ✅ landed |

**Duties**: the Scene holds an `AttrSyncManager*` (valid while attached) and takes over `SceneAoi::EventSink` — Enter/Sync first sight (the manager lacks that (observer, subject) viewer state) → create `ViewerState` + produce a snapshot frame; cross-grid Sync inside view triggers no protocol; Leave → reclaim the `ViewerState`. In `enter()`, registration precedes `aoi_.enter()` (the new arrival's own snapshot frame requires the subject already registered). Snapshot frames are handed to the host via `drain_attrsync_frames()`. `apollo_game_world` links `apollo::game_attrsync`.

**Design sections mapped**: §4.1 (viewer set and AOI wiring).

**Tests**: `tests/test_scene_attrsync_wiring.cpp` — 9 cases (AttachRegistersExistingEntities / SpawnAfterAttachRegisters / AvatarEnterBuildsMutualViewerStates / FirstSightFramesCarrySubjectIds / LeaveSceneRecyclesViewerStates / MoveWithinViewDoesNotResnapshot / MoveOutOfViewThenBackResnapshots / DetachStopsWiring / DeltaFlowsAfterFirstSight), build tree + sanitizer tree all green.

### Batch 6 — §5 Priority and Throttling

| Item | Files | Status |
|---|---|---|
| `AttrSyncScheduler` | `include/apollo/game/attrsync/attr_sync_scheduler.hpp` + `src/attr_sync_scheduler.cpp` | ✅ landed |
| `AttrSyncManager::peek` | `src/attr_sync_manager.cpp` | ✅ landed |

**Duties**: §5 priority and throttle scheduling — token bucket per viewer (§5.1), entity priority ordering (§5.2, importance × 1000 + dirty_count; distance/visible await the scene position surface), min_interval throttle tiers (§5.3), starvation protection (§5.2). `peek` is read-only collection (no acked advancement, no dirty clearing) so the scheduler can pre-check the encoded volume before packing — `collect`'s advancing side effects happen only on confirmed sends.

**Design sections mapped**: §5.1 (bandwidth budget), §5.2 (entity priority + starvation protection), §5.3 (min_interval).

**Tests**: `tests/test_attr_sync_scheduler.cpp` — 11 cases (TokenBucketRefillsPerTick / NoBudgetNoSend / NoChangesNoFrame / PriorityOrdersImportanceFirst / HigherDirtyCountBreaksTie / MinIntervalThrottlesLowFrequency / InstantBypassesMinInterval / StarvationForcesSend / LastWriteWinsMergesJitter / EmittedFrameCarriesDeltas / ScheduleTracksPerViewer), build tree + sanitizer tree all green.

### Batch 7 — §8.2 Write-Behind Journal Entry

| Item | Files | Status |
|---|---|---|
| `AttrSyncManager::ChangeSink` / `set_change_sink` / `clear_change_sink` | `include/apollo/game/attrsync/attr_sync_manager.hpp` + `src/attr_sync_manager.cpp` | ✅ landed |

**Duties**: the attrsync-side entry of the §8.2 write-behind pipeline — after every successful `set_attribute`, the `ChangeSink(entity_id, attr_id, value)` callback fires, for the host to wire into the `PersistJournal` (`modules/data/orm`). attrsync never links the data layer directly (dependency direction: the host assembles the journal sink).

**Design sections mapped**: §8.2 (write model: snapshot + write-behind change journal).

**Tests**: `tests/test_attr_sync_change_sink.cpp` — 6 cases (FiresOnSet / FiresPerEntity / FiresPerAttribute / NoSinkNoCrash / ClearStopsReporting / UnregisteredEntityNoFire), build tree + sanitizer tree all green.

### Batch 8 — §3.3 Snapshot Payload Assembly

| Item | Files | Status |
|---|---|---|
| `DeltaBuilder::build` signature changed to `const EntityAttrs&` + `fill_snapshot` | `include/apollo/game/attrsync/delta_builder.hpp` + `src/delta_builder.cpp` | ✅ landed |
| `EntityAttrs::values()` full current-value accessor | `include/apollo/game/attrsync/entity_attrs.hpp` | ✅ landed |

**Duties**: snapshot frames no longer carry only a marker — the deltas are filled with full current values (`EntityAttrs::values()`, attr_id ascending, change_n=1), and `base_seq` is pushed to world_seq (a snapshot is "pushing the other side's progress straight to the current"). The three snapshot paths (first sight acked=0 / history cannot cover / delta over threshold) uniformly go through `fill_snapshot`. The delta path in the same batch switched to attr_id-ascending output (determinism). The domain-trimming mechanism layer is in batch 9.

**Design sections mapped**: §3.3 (snapshot switching).

**Tests**: `tests/test_attr_sync.cpp` gains 4 cases (SnapshotCarriesAllValues / SnapshotAfterHistoryExhaustionCarriesValues / SnapshotLatestValueWins / SnapshotThenDelta); the existing 4 DeltaBuilder cases migrated to the EntityAttrs parameter; `tests/test_scene_attrsync_wiring.cpp` FirstSightFramesCarrySubjectIds asserts snapshot frames carry full values. Build tree + sanitizer tree + OFF tree all green.

### Batch 9 — §2.2 Domain-Projection Mechanism Layer

| Item | Files | Status |
|---|---|---|
| `AttrDomain` enum (kPublic/kSelf/kTeam/kGuild/kWorld) | `include/apollo/game/attrsync/attr_domain.hpp` | ✅ landed |
| `EntityAttrs::set_domain` / `domain` (domain bit table; undeclared = kPublic) | `include/apollo/game/attrsync/entity_attrs.hpp` | ✅ landed |
| `DeltaBuilder::build` gains viewer_id + `visible` domain trimming + `set_group_gate` | `include/apollo/game/attrsync/delta_builder.hpp` + `src/delta_builder.cpp` | ✅ landed |
| `AttrSyncManager::set_attr_domain` / `set_group_gate` | `include/apollo/game/attrsync/attr_sync_manager.hpp` + `src/attr_sync_manager.cpp` | ✅ landed |
| Degradation-path fix: watermark-to-zero goes through a true snapshot (full values + domain trimming), no longer forcing a delta-flagged frame | `src/attr_sync_manager.cpp` | ✅ landed |

**Duties**: the domain decides "field visibility" — the same entity projects differently to different observers, and both snapshots and deltas are trimmed by domain. Mechanism-layer rules: kPublic/kWorld always visible; kSelf owner-only (viewer_id == entity_id); kTeam/kGuild decided by a host-injected group gate (unset = invisible). Trimming happens after delta dedup and before threshold judgment (the threshold counts post-trim entries). This batch delivers only the mechanism-layer trimming; contract-declaration generation of domain markers landed with batch 11 (mapping primitives + table-application helper).

**Design sections mapped**: §2.2 (domains and visibility), §3.3 (snapshot switching).

**Tests**: `tests/test_attr_sync.cpp` gains 8 cases (SelfDomainHiddenFromOtherViewer / SelfDomainVisibleToOwner / PublicDomainVisibleByDefault / WorldDomainVisibleLikePublic / TeamDomainHiddenWithoutGate / TeamDomainVisibleThroughGate / DeltaPathRespectsDomain / ExhaustedHistorySnapshotAlsoFiltered); `tests/test_attr_sync_manager.cpp` gains 2 cases (CollectHidesSelfDomainFromOtherViewer / GroupGateControlsTeamVisibility); `tests/test_scene_attrsync_wiring.cpp` gains 1 case (FirstSightHidesSelfDomainFromOther); `tests/test_attr_ack_degrade.cpp` gains 1 case (DegradeSnapshotCarriesFullValues). Build tree + sanitizer tree + OFF tree all green.

### Batch 10 — §2.2 Slice Composite-Type Mechanism Layer

| Item | Files | Status |
|---|---|---|
| Composite-id discipline: `(slot << 16) \| base`; slot 0 = the container length cell (= the base itself), data slots 1..max_slots | `include/apollo/game/attrsync/entity_attrs.hpp` | ✅ landed |
| `declare_slice` / `is_slice` / `slice_max_slots` (mechanism-layer registration; produces no history entries) | `entity_attrs.hpp` | ✅ landed |
| `set_slice_len` (a sync op; shrink is final — out-of-range slot values are erased along with their dirty flags); direct base writes rejected (the length gate is the sole entry) | `entity_attrs.cpp` | ✅ landed |
| `set_slot` / `get_slot` (validation: declared, slot ≥ 1 and ≤ current length) | `entity_attrs.cpp` | ✅ landed |
| `slice_coord` composite-id decomposition + `domain()` composite id inherits the base domain | `entity_attrs.cpp` | ✅ landed |
| `DeltaBuilder` delta-path dead-slot filtering: entries for slots overtaken by a new length within the same batch are dropped (the §3.4 ordering exception) | `src/delta_builder.cpp` | ✅ landed |

**Duties**: a composite attribute = a slice container; every slot is an independent sync unit (dirty flags, sequence numbers, sends, ACK watermark, thresholds, and snapshot assembly all at slot granularity). Composite ids ride the existing values/dirty/history surface directly — per-slot last-write-wins merging, per-viewer ACK, and snapshot payloads all come for free. attr_id-ascending output naturally satisfies "grow first, then write the slot" (the length cell = the base sorts first); the shrink direction gets final-state consistency from both halves — DeltaBuilder's dead-slot filtering + EntityAttrs' shrink erasure. The declaration surface (slice_count/slice_type contract-declaration generation) belongs to the contract batch; this batch delivers only the mechanism (`declare_slice` is mechanism-layer registration).

**Design sections mapped**: §2.2 (slice composite types), §3.4 (shrink-overtake = the dead-slot ordering exception), §7.1 (wire: attr_id's high 16 bits are the slot).

**Tests**: `tests/test_attr_sync.cpp` gains 11 cases (LenWriteReadAndHistoryEntry / LenAboveMaxRejected / DirectBaseWriteRejected / SlotGranularWriteAndDirty / SlotWriteValidation / ShrinkErasesDeadSlotValues / CoordDecomposeAndDomainFallback / SliceDeltaLenBeforeSlots / SliceDeltaShrinkDropsDeadSlotEntries / SliceDeltaPerSlotLastWriteWins / SliceDomainHiddenFromOtherViewer). Build tree + sanitizer tree + OFF tree + examples tree all green.

### Batch 11 — §2.2 Domain-Marker Contract-Declaration Generation (mapping primitives + table application, 2026-10-16)

| Item | Files | Status |
|---|---|---|
| Local mirror of the contract sync-mask bits (kSyncMaskAppr..kSyncMaskImmediate) | `include/apollo/game/attrsync/attr_domain_mapping.hpp` | ✅ landed |
| `domain_from_sync_mask` (widest visibility wins: WORLD > APPR/PROP > GUILD > TEAM > SELF) | `src/attr_domain_mapping.cpp` | ✅ landed |
| `is_group_scoped` (kTeam/kGuild via the group gate; kSelf goes through owner determination, not counted as a group domain) | `attr_domain_mapping.hpp` | ✅ landed |
| `ContractAttrEntry` + `apply_contract_domains` (contract table → EntityAttrs domain markers; declaring is not changing, produces no history) | `attr_domain_mapping.{hpp,cpp}` | ✅ landed |

**Decided by proxy (FW1016D authorization, overturnable after the fact)**:

1. **The mapping rule = widest visibility wins**, total order WORLD > APPR/PROP > GUILD > TEAM > SELF; empty mask → kPublic (consistent with `domain()`'s undeclared default); IMMEDIATE is a timing bit and does not participate in domain mapping; the DB bit (0x04) is contract-forbidden and, even if it appears, contributes nothing; a TEAM|GUILD tie → kGuild (a guild is wider than a team). Rationale: the contract mask is a union declaration of visibility range, while batch 9's trimming semantics judges binarily per domain (visible/invisible) — taking the widest of the union is the only monotone choice that never narrows (taking the narrow = data loss; under a non-widest rule, recovery from a single entity is impossible). kWorld is mechanically identical to kPublic; mapping to kWorld preserves the upstream whole-server routing intent (`attr_domain.hpp` comment: routing differences belong upstream).
2. **Consumption form = table-driven (contract = data / attrsync = mechanism / host = wiring)**: the mapping helpers and `apply_contract_domains` live in attrsync (the result type is this module's `AttrDomain`); the mask bits are mirrored locally rather than adding a game_attrsync→contract dependency edge (the contract-face data is extracted by the host from the generated artifact `kAttrs`); drift between the mirror and the generated artifact is pinned at compile time by 7 `static_assert`s in attrsync_tests. Rationale: the mechanism layer stays zero-contract-coupled (the division of labor of batches 9/10); the generated artifact is a constant table already checked into this repo, not a runtime dependency edge.

**Duties**: translate the contract-declared sync mask into mechanism-layer domain markers; at host startup, apply the contract table to every entity's EntityAttrs (`apply_contract_domains`); domain trimming itself remains on batch 9's DeltaBuilder path. A slice base's domain is inherited from the slot composite id (`entity_attrs.hpp` `domain()`), so the table needs it declared once for the base only. Declaring is not changing: no history entries, no dirty bits; repeated application takes the last.

**Design sections mapped**: §2.2 (domains and visibility, slice composite types); `docs/design/sdk-contract.md` §10.2.4 (the mask produces no proto elements; worked examples hp=PROP / level=PROP TEAM WORLD / gold=SELF).

**Tests**: `tests/test_attr_domain_mapping.cpp` adds 11 cases (SingleFlagMaps / EmptyMaskDefaultsPublic / TimingAndStorageBitsIgnored / WidestVisibilityWins / DomainIsDeterminedByHighestPriorityBit (a 256-mask exhaustive sweep: a multi-bit mask's domain == the domain of its highest-priority bit; the rule degenerating to bit-counting or last-bit-wins is an immediate red) / GroupScopedPredicate / ContractTableMatchesDocWorkedExample (reads the generated artifact `kAttrs`, measures the actual sync values, and asserts the forbidden DB bit never appears in the table) / ApplyContractDomains / ApplyContractDomainsSliceInheritsBase / ApplyContractDomainsLastWins / ApplyContractDomainsEmptyTableIsNoop) + 7 `static_assert`s pinning the mirror constants == the generated artifact canonical. Build tree 77/77 + sanitizer tree 64/64 + examples tree 75/75 + GAME_MODULE-toggle tree 75/75 all green.

**Not wired (later batches)**: the host-startup `apply_contract_domains` registration point — currently the repo has no host consumer of `EntityAttrs` / `set_domain` (zero touches outside the mechanism layer and tests); it lands together with the social/gameplay batch that introduces membership-data injection and the entity-registration surface.

### Module Build and Test Summary

**Module build**: `modules/game/attrsync/CMakeLists.txt` — a STATIC library + the ALIAS `apollo::game_attrsync`, linking `apollo::game_core`; 9 source files (change_history / viewer_attr_state / attr_delta / delta_builder / entity_attrs / attr_sync_manager / attr_sync_scheduler / frame_codec / attr_domain_mapping).

**Tests**: `tests/test_attr_sync.cpp` (40) + `tests/test_attr_sync_manager.cpp` (22) + `tests/test_attr_frame_codec.cpp` (10) + `tests/test_attr_ack_degrade.cpp` (8) + `tests/test_scene_attrsync_wiring.cpp` (10) + `tests/test_attr_sync_scheduler.cpp` (11) + `tests/test_attr_sync_change_sink.cpp` (6) + `tests/test_attr_sync_attr_hook.cpp` (9) + `tests/test_attr_domain_mapping.cpp` (11) = 127 cases, build tree + sanitizer tree + OFF tree + examples tree all green.

### Boundaries (Not Implemented, Later Batches)

- Team/guild membership-data injection (domain-marker mapping landed with batch 11; the group gate's membership data belongs to the host/social batch) + the host-startup `apply_contract_domains` registration-point wiring (currently `EntityAttrs` has no host consumer)
- The slice declaration surface (slice_count/slice_type contract-declaration generation — **schema + generator landed (2026-10-15)**: declarable in attrs.xml/XSD, parse-validated (paired + element type parseable), carried by apollo_contract.h AttrMeta and apollo_contract.json, contract.lua carried on demand; remaining consumption wiring — the host/gameplay batch calling `declare_slice` from the contract table, with the mechanism layer already landed in batch 10)
- ~~the client send path (the GameConnection Facade was not built)~~ (delivered with the Facade L2/L3 batch: `tests/test_game_connection.cpp` 20 cases)
- ~~slice composite types (§2.2)~~ (the mechanism layer was delivered with batch 10)

## Appendix: Three-Way Mechanism Quick Reference

| Mechanism | BigWorld | KBEngine | This design |
|---|---|---|---|
| Attribute declaration | .def + flags | .def + code generation | contract (XML+XSD, see sdk-contract.md §2.3) + three-end generation |
| Delta basis | per-client lastSeq + history | per-entity versions | per-viewer acked_seq + ChangeHistory |
| Visibility | interest areas + ghost | AOI simplified | AOI viewer set + domain projection |
| Bandwidth | per-client priority budget | coarse-grained configuration | token bucket + entity priority + min_interval |
| Persistence | base + backup + column promotion (row storage isomorphic to KBEngine: columns + blobs — C-52) | MySQL entity tables (row storage isomorphic across both; **weak in backup/disaster recovery** — no hot-backup/takeover chain, archiving not dirty-driven: C-51) | snapshot + write-behind journal + columnar promotion (the differentiators are the journal and dirty-driving, not the row-storage form) |
| Prediction | movement prediction + reconcile | client-optional | movement prediction + presentation-layer prediction + intent upstream |

---
