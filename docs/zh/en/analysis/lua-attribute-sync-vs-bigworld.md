# Lua Attribute Change Detection and Sync: Apollo vs BigWorld

> Scope: how attribute **changes are detected** (write path/declarations/hooks) and **propagated**
> (visibility domains/deltas/recovery/budget), compared with BigWorld's Entity attributes.
> This is a mechanism analysis, not an acceptance claim — "exists" means a primitive is
> present; end-to-end client delivery is a separate gap (§7).
> Design baselines: [Attribute System and Synchronization](../design/attribute-sync.md);
> [Lua Scripting](../design/scripting-lua.md);
> [BigWorld–Apollo Mapping](../design/bigworld-apollo-mapping.md);
> BigWorld source baseline: `27446bab` (14.4.1).

## 0. Summary

Apollo narrows **detection** to a single write path plus a hook window and a sequenced change
history (C++ direct writes and Lua value revisions both land on `AttrSyncManager::set_attribute`),
and narrows **sync** to AOI-clipped per-viewer deltas with seq/ACK watermarks, domain projection,
and snapshot degradation. BigWorld's equivalent is built on `.def` declarations, generated
accessors, client visibility flags, detail levels, and volatile properties. Apollo's missing
piece is not the sync algorithm but three things: attribute metadata declarations (domain/
visibility/persistence), a client wire contract, and a host consumer for Scene output frames.
The Lua detection face has a controlled boundary (allowlist + frame budget + keep-original on
error), but no real gameplay property writes flow through it yet — closing that is the top gap.

## 1. Detection path (current)

### 1.1 Single writer and canonical write face

The only entry point is `AttrSyncManager::set_attribute`. It updates values, appends a
`ChangeHistory` entry, marks dirty, and fires `ChangeSink` (persistence handoff). All observers
share this face; no shadow table exists. BigWorld generated accessors converge on the same
goal through code generation; Apollo uses explicit runtime calls.

### 1.2 Two detection entries

- **C++ direct write**: simulation calls `set_attribute` directly.
- **Lua hook window** (scripting-lua §5.1): the six-phase Scene tick runs `apply_attr_hook`
  after dirty fixation and before collection. `install_attr_hook_bridge` translates attrsync's
  `HookChange` window into `ScriptRuntime::AttrChange` snapshots and calls
  `run_attr_hook_window` (allowlist from `contract_write_whitelist`, per-frame budget from
  `attr_hook_frame_budget_`). Revised values are written back and committed through the
  canonical write path. Off-allowlist writes are skipped, exhausted budgets skip the remainder,
  and hook errors keep the original value with a recorded fault. This is a **post-hoc revision
  hook**, not a BigWorld-style write-time subscription.

### 1.3 Detection output: history is the projection

Each change appends `(seq, attr_id, value)` to a ring (default capacity 1024). Deltas are
`entries_since(acked_seq)`; snapshots are the current value set. No polling diff is needed;
ring overwrite forces snapshot degradation for stalled viewers (§2.2).

## 2. Sync path (current)

### 2.1 Trigger and clipping

AOI Enter/Sync creates a `ViewerAttrState`, Leave reclaims it. AOI decides whether an **entity**
is visible; `AttrDomain` decides whether an **attribute** is visible: Public/World always,
Self only for the owner, Team/Guild via an injected group gate (hidden without one). Snapshots
and deltas use the same clipping.

### 2.2 Deltas and recovery

`DeltaBuilder` takes `(acked_seq, world_seq]` entries, emits in attr_id order (per-slot
coalescing and length-before-slot ordering come free from composite IDs). Snapshot degradation
triggers on: first sight, history overwritten, too many delta attributes, or ACK timeout
(default 3 ticks). Recovery is a full snapshot rebuild rather than infinite replay.

### 2.3 Budget and throttling

`AttrSyncScheduler` uses per-viewer token buckets; `peek` computes a candidate load before
`collect` commits side effects. `min_interval` throttles low-frequency attributes. Insufficient
budget retains recoverable state. Importance/starvation semantics are defined; real gameplay
importance and distance data sources remain unwired (design §5.2).

### 2.4 Wire encoding

`AttrDelta`/`FrameCodec` provide varint/zigzag encoding with boundary checks. **No contract
message carries these frames yet** (§4).

## 3. Comparison with BigWorld

| Dimension | BigWorld (Entity attributes) | Apollo current | Gap type |
|---|---|---|---|
| Detection entry | `.def` declarations → generated Python accessors (assignment subscribes) | Explicit `set_attribute` / Lua hook window | Declarative vs explicit |
| Client visibility | `ALL_CLIENTS/OTHER_CLIENTS/OWN_CLIENT/CELL_PUBLIC/CELL_PRIVATE/BASE_ONLY` + detail levels 0–3 | `AttrDomain` (Public/Self/Team/Guild/World) | Domains not declarative; manually set; undeclared = Public (too broad by default) |
| Change detection | Accessor sets per-client `changed` flags; Witness aggregates at tick end | Sequenced `ChangeHistory` + dirty bits | Equivalent; Apollo avoids polling |
| Visibility set | Witness tracks per-Observer detail lists, fills levels 0–3 progressively | AOI clipping + domain projection + per-viewer seq | No detail-level progressive fill (not needed without Ghosts) |
| Volatile properties | position/yaw/pitch on a separate fixed-rate quantized path | Movement uses explicit `CELL_ENTITY_MOVE` wire frames with per-viewer header rewrite | Same design intent |
| Persistence | `PersistentId`/`writeToDB`, offload flags, CELL_PUBLIC Base mirroring | `ChangeSink` → host (journal → database), no direct data-layer link | Aligned; host field mapping/dedup/retry still open (design §8) |
| Budget | Witness bandwidth/detail-level throttling | Token bucket + min_interval + snapshot degradation | Mechanism present; gameplay importance source unwired |
| Sequence/recovery | Client tracks received level per entity, requests missing levels | seq/acked_seq + ring history + ACK-timeout snapshot | Equivalent semantics; Apollo degrades to snapshot rather than replaying missing levels |

**Key difference**: BigWorld's attribute sync is **declaration-driven** — change flags, visibility
levels, and persistence all live in `.def`; runtime only generates accessors. Apollo is
**mechanism-driven** — primitives are complete, but the declaration layer (which attributes,
which domain, what persistence) and gameplay data are still absent. The detection chain works
with nothing to detect: `contract_write_whitelist` comes from the loaded contract table, while
the shipped `contract_handlers.lua` is a placeholder shell with no real attribute writes.

### 3.1 The Lua difference

BigWorld uses **full Python scripting** (Entity is a Python object; attributes are Python
attributes) — detection is free at the language level, with a coarse sandbox boundary.
Apollo uses **controlled Lua plugins**: hooks run only at a fixed tick boundary with allowlists,
frame budgets, error isolation, and independent module breakers. Detection is an explicit
`onAttrChanged(entity, attr, old, new)` hook and window rather than a language-level property
proxy. The cost is a controlled boundary per write (skipped when budget is exhausted); the gain
is that broken/slow scripts cannot corrupt authoritative values or block the tick. This is a
deliberate boundary, not a defect.

## 4. Gaps and priorities (within the mechanism)

1. **Declaration layer** (highest): generate the `.def` equivalent — attribute table (domain,
   visibility, persistence, min_interval) — from the contract XML, feeding `set_attr_domain`/
   `contract_write_whitelist`/persistence mapping. Without it, domains default to Public, the
   allowlist is empty, and the sink is disconnected.
2. **Client wire contract**: `FrameCodec` frames need a contract message family (attr_sync
   delta/snapshot/ack); messages.xml has none — produced frames have no wire outlet (design §0).
3. **Host consumer for Scene output**: `drain_attrsync_frames` exists but **apps/world has no
   consumer** — frames die inside the cell process. A cell→World relay or 939 outlet is needed.
4. **One real gameplay property**: write a combat attribute through `set_attribute`, project it
   to the owner, persist it through the sink, and pin the full chain with real data. D21 transfer
   tests cover entity runtime state but not the attribute-sync ring.

Out of scope: Ghost/Witness cross-Cell (requires multiple Cells), seamless map, client-side prediction.

## 5. Falsifiable acceptance criteria

- Detection: one property write → exactly one history entry, seq+1; off-allowlist Lua writes do
  not land; exhausted budget skips the remainder; hook error keeps the original value.
- Sync: two in-AOI viewers receive the same seq delta; Team/Guild non-members receive zero frames;
  ACK timeout (3 ticks) triggers snapshot rebuild; ring overwrite degrades to snapshot.
- Persistence: exactly one ChangeSink call per write; journal → database order and replay match
  (design §8.2: "mark_dirty" alone proves nothing).
- Wire: one delta frame round-trips through encode/decode; adding the contract message requires
  synchronized golden counts.

No "exceeds BigWorld" claim is made: BigWorld's generated declarations, detail levels, and
production-grade tolerance are evidenced capabilities. Apollo has the mechanism; declarations
and delivery remain to be wired.
