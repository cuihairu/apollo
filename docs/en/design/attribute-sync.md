# Attribute System and Synchronization

The owning Scene logic thread manages values, history, and each observer's progress. World holds current authority. Persistent data and simulation remain distinct responsibilities without another business process topology.

## Executive summary

ChangeHistory, ViewerAttrState, DeltaBuilder, AttrSyncManager, FrameCodec, AttrSyncScheduler, domain projection, slices, and Scene AOI wiring exist. Actual client delivery, membership injection, and persistent settlement need separate integration and acceptance.

## 0. Current boundary

`modules/game/attrsync` provides reusable primitives. Scene can collect output and GameConnection provides channels. Complete World → Gateway → client delivery remains an application requirement. Generated contracts do not establish migration of every handwritten application frame.

## 1. Core rules

One writer owns simulation state. Network and worker results enter through bounded handoff at the owning logic boundary. Visibility, transport reliability, and persistence are separate guarantees. Client delivery does not imply storage.

## 2. Attribute model

### 2.1 Lifetime categories

Separate static definitions, persistent state, transient simulation, and derived values. Definitions describe type, default, and policy; entity containers own current values. Wire contracts do not directly freeze storage structure.

### 2.2 Types and slices

Current primitives use integer values and slice slots. Composite ID is `(slot << 16) | base`; slot 0 stores length, with data slots starting at 1. Declare capacity and set length before access. Shrinking removes invalid values, dirty state, and pending dead-slot output.

### 2.3 Authority and visibility domains

The current World manages profile write authority and Scene simulation, with explicit transfer. Visibility is separate: Public/World, Self, Team, and Guild projections depend on declarations and host-provided membership.

Current Public/World domains are visible; Self compares observer/entity identity; Team/Guild require an injected group gate and remain hidden without one. Hosts must correctly map player and entity identity instead of treating defaults as complete authorization.

### 2.4 Derived values

Define evaluation and dependency order. Lua hooks require a controlled boundary, write allowlists, recursion limits, and budgets. Async completions cannot change a completed tick's decision.

## 3. History and deltas

### 3.1 Core state

ChangeHistory stores ordered changes. ViewerAttrState stores baseline and ACK progress. EntityAttrs manages current values, dirty state, and history.

### 3.2 Collection

DeltaBuilder constructs deterministic attribute-ID ordered output from values, history, and viewer progress. Scheduling can peek at candidates before committing collection effects.

### 3.3 Snapshots

First visibility, exhausted history, large deltas, or ACK fallback require a snapshot containing actual current visible values. Setting is_snapshot without values is insufficient. Apply identical visibility rules to snapshots and deltas.

### 3.4 Coalescing

Retain final values per attribute. Apply slice length before slots; stale slots in the same batch cannot undo shrinking. Business events cannot automatically use attribute last-write-wins semantics.

## 4. Visibility

### 4.1 Scene AOI wiring

Attached Scene integration registers entities, creates observer state and snapshots on first Enter/Sync, and removes viewer state on Leave. Movement within continued visibility must not repeatedly generate first-sight snapshots.

### 4.2 Projection

Each observer receives its own projection. AOI controls entity visibility; domains control field visibility. Both must pass. Membership, stealth, death, and other gameplay rules require host data, not only proximity.

### 4.3 Changing rules

Membership and authorization changes require projection refresh, including removal of previously visible client state. A group-gate callback does not maintain every membership change automatically.

### 4.4 Cross-server play

Reuse Scene and synchronization. Transfer exclusive authority, refresh routes, and rebuild observer baselines. Ghosts and seamless spatial partitioning are not targets. See [cross-server scenes](./cross-server-scenes.md).

## 5. Budgets, priority, and throttling

### 5.1 Budget

AttrSyncScheduler uses per-observer token buckets. Transport watermarks independently decide queue admission; a budget does not establish a writable network.

### 5.2 Priority

Current scheduling considers importance, dirty count, and starvation handling. Distance and gameplay-specific importance need real host integration.

### 5.3 Intervals

Use min_interval and declared immediate behavior. Preserve recoverable final authority when skipping output because of insufficient budget.

### 5.4 Channels

See [network design](./net-abstraction.md) for movement, attributes, events, and control. Droppable delivery does not replace baseline/ACK semantics for attribute state.

## 6. Entity lifecycle

Send a usable baseline before deltas. Remove viewer and client entity state on leave. Instance destruction clears its Scene entities, AOI, and output, rejecting stale input and completions.

## 7. Encoding and contracts

### 7.1 Current encoding

AttrDelta and FrameCodec implement varint/zigzag encoding and bounds checks. XML/XSD generation supplies metadata and message definitions. Existing `.proto` projections do not establish protobuf use on real application paths.

### 7.2 Sequences and recovery

Track sequences, baselines, and ACKs; timeout fallback uses a real snapshot. Link reconnect, session recovery, and authority transfer are distinct events requiring appropriate baseline replay or rebuild.

### 7.3 Inheritance

Generation expands entity inheritance into flat metadata. SDKs must not independently recreate incompatible protocols. Validate compatibility windows through [SDK contracts](./sdk-contract.md).

## 8. Persistence

### 8.1 Ownership

World hosts PlayerStore, profiles, SaveQueue, and PersistJournal. Scene state and long-term data are not two independently writable gameplay authorities.

### 8.2 Handoff

AttrSyncManager::ChangeSink reports changes to its host without directly linking storage. Player saving records a journal before updating profiles by policy. Validate field conversion, failures, ordering, and replay. mark_dirty alone does not establish durable rewards.

### 8.3 Settlement

Reuse IRewardSink / AnchorRewardSink for long-term effects. Complete persistence mapping, settlement-ID deduplication, retries, and recovery. Success delivery follows the defined durable confirmation.

### 8.4 External storage

Current business persistence uses local profiles and journals. SQL, Redis, and analytical stores remain demand-driven. Configuration fields or dependencies do not establish integration.

## 9. Client prediction

Keep predicted and authoritative values separate, predict only allowed fields, and reconcile authoritative responses. Movement prediction, combat decisions, and complete SDK recovery require real gameplay/client acceptance.

## 10. Threads and shutdown

### 10.1 Tick boundary

Consume input, advance simulation, run hooks, and collect output on the owning logic thread. Revalidate async target lifetime, session, and authority at completion; do not alter completed decisions.

### 10.2 Shutdown

PlayerStore closes intake, stops save producers, collects dirty state, drains SaveQueue and journal within its deadline, then closes storage. Retain timed-out journal work for replay. This does not recover active Instances or establish that every long-term field reaches the save payload.

## 11. Acceptance

Cover snapshot/delta equivalence, visibility and membership changes, slice shrinking, ACK timeout, budget exhaustion, slow clients, and Scene destruction. End-to-end checks include encoding, routing, socket delivery, receiver application, and persistent settlement retries.

## 12. Current work

Track primitives separately from business integration. The root todo.md is the only current backlog; this page does not keep another phased completion history.
