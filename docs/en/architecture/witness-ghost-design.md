---
title: Witness and Ghost Design
icon: eye
order: 16
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - Witness
  - Ghost
  - Replication
---

# Witness and Ghost Design

This document is the most critical runtime topic for Apollo after it enters BigWorld mode.

It addresses two questions that are very easy to conflate:

1. What exactly the client "sees".
2. In a distributed space, in what form entities near a boundary actually "exist".

If these two questions are not pulled apart, the system can easily end up written as:

- AOI query
- broadcast entities directly

That implementation barely works in a single-process world, and falls apart quickly in a distributed space.

## 1. The Conclusion Up Front

After Apollo enters BigWorld mode, these two layers must be explicitly separated:

- `GhostReplica`: the entity projection on a non-authoritative node
- `Witness`: the client-facing visible-world context

In other words:

- `Ghost` solves the near-field existence problem across nodes
- `Witness` solves the client visible-set problem

The two are related, but they are by no means the same concept.

## 2. Evidence of These Two Objects in KBE

From the local source:

- `witness.h`
  - holds `viewRadius`
  - holds `ViewTrigger`
  - holds `viewEntities`
  - provides `onEnterView/onLeaveView`
  - provides `sendToClient`
  - provides `addUpdateToStream`
- `ghost_manager.h`
  - manages `realEntities_`
  - manages `ghost_route_`
  - manages the pending-synchronization `messages_`
  - provides routing and synchronization capability

This already makes it quite clear that:

- `Witness` cares about the client view
- `GhostManager` cares about entity projection and message relay

This document discusses the runtime objects in the BigWorld enhanced state,
not capabilities already fully landed in the current `apps/world-app`.

## 3. Why Plain AOI Is Not Enough

In an ordinary MMO world, AOI can often be understood as:

- query nearby entities
- broadcast entering the view
- broadcast leaving the view

But a distributed space has two extra layers of problems:

### Problem 1: nearby entities may not be on this node

A player is near the boundary of partition A, and another entity is in partition B.

Querying only the local entity set is not enough here.

### Problem 2: the client visible set cannot simply equal "this node's entity table"

The world the client sees should be a continuous, stable view.

This view may simultaneously contain:

- real entities on this node
- ghost replicas synchronized from adjacent nodes

Without `Witness` as the point of convergence, the client view will be extremely unstable.

## 4. The Positioning of `GhostReplica`

`GhostReplica` is the entity projection on a non-authoritative node.

Its purpose is not to replace the real entity, but to:

- keep observation relationships near boundaries from breaking
- let adjacent partitions see near-field objects
- provide temporary relay during the migration window

### What `GhostReplica` should hold

- `entityId`
- `sourceCellId`
- `authorityRole = Ghost`
- position/orientation snapshot
- key synchronized properties
- route version

### What `GhostReplica` should not do

- it should not make final authority rulings
- it should not independently decide combat outcomes
- it should not write long-term entity state

## 5. The Positioning of `Witness`

`Witness` is the client-perspective context.

It is not:

- the entity itself

Nor is it:

- merely a nearby-query algorithm

Its more accurate role is:

- the runtime container of the client's "what can I see right now"

### What `Witness` should hold

- the current view radius
- the current visible entity set
- enter-view and leave-view state
- visible entity alias / compressed ID
- the send buffer
- the current route version

### What `Witness` is responsible for

- deciding enter view / leave view
- deciding which entities to send to the client
- deciding whether to send real data or ghost data
- aggregating the visible set from multiple sources

## 6. The Relationship Between the Two

It is recommended to make the relationship explicit as follows:

```text
RealEntity / GhostReplica
    -> participates in AOI / replication source
Witness
    -> selects and organizes the visible set for the client
Gateway
    -> actually delivers the data to the client
```

In other words:

- `GhostReplica` is part of the view's sources
- `Witness` is the view organizer
- `Gateway` is the network egress

## 7. Recommended Object Relationships

It is recommended that Apollo later form the following set of objects:

```text
ReplicationService
    ├── WitnessContext
    ├── ViewTrigger
    ├── GhostReplica
    ├── GhostRouteTable
    └── AliasAllocator
```

### `WitnessContext`

Responsibilities:

- maintain the client's current visible set
- track enter/leave state
- aggregate outgoing packets

### `ViewTrigger`

Responsibilities:

- handle spatial boundary events
- notify the witness that an object has entered or left the observation range

### `GhostRouteTable`

Responsibilities:

- record an entity's current ghost route
- backstop message forwarding during the migration window

### `AliasAllocator`

Responsibilities:

- assign short aliases to entities in the current view
- reduce the size of synchronization packets

## 8. Recommended Client View Flow

### Standard view update flow

1. `AOI/Trigger` detects an object approaching the visibility boundary
2. `WitnessContext` computes whether the object should enter the view
3. if the object comes from a real entity on this node, register a real source
4. if the object comes from a ghost on an adjacent node, register a ghost source
5. `WitnessContext` updates the visible set
6. `ReplicationService` assembles the delta packet
7. send it to the client through the `Gateway`

### Key point

What the client sees is:

- the visible world as decided by `WitnessContext`

not:

- a raw dump of the local entity table

## 9. Recommended Boundary Observation Flow

### Scenario

Player A is near the boundary of partition 1.

Player B is in partition 2, very close by.

### Recommended flow

1. the real entity for B lives on partition 2
2. partition 1 builds a `GhostReplica` for B
3. A's `Witness` can see B's ghost directly from partition 1
4. if B truly migrates across the boundary, the ghost then participates in the transition

### Why querying the remote side ad hoc is not enough

Because a real-time, per-frame remote lookup:

- has high latency
- is hard to control
- has unstable ordering

The point of `GhostReplica` is to pull near-boundary, near-field data to the local node ahead of time.

## 10. Recommended Ghost Routing Inside the Migration Window

What most easily goes wrong in the migration window is:

- the old authority has just switched away
- the new authority has just been established
- some messages are still in flight

This calls for an explicit ghost route table.

### Recommended flow

1. the source cell writes a `GhostRoute` for the migrating entity
2. a message that arrives late first consults the route
3. if the real entity is no longer local, forward along the route to the target cell
4. the target cell receives it and hands it to the new real entity
5. when the migration window ends, clean up the route

### Why this layer must exist explicitly

Because migration is not an atomic switch.

Without a route table, you can only bet that messages happen not to be reordered.

## 11. Recommended `WitnessContext` States

It is recommended to maintain at least the following states:

- `Building`
- `Active`
- `Refreshing`
- `Suspended`
- `Closed`

### `Building`

- initializing the client view

### `Active`

- serving view deltas normally

### `Refreshing`

- rebuilding the view after a scene transfer or a route change

### `Suspended`

- briefly suspended after a disconnection

### `Closed`

- fully closed and reclaimed

## 12. Recommended `GhostReplica` States

It is recommended to maintain at least:

- `Shadowing`
- `Bridging`
- `Expiring`
- `Closed`

### `Shadowing`

- doing normal near-boundary projection

### `Bridging`

- in the middle of a migration window, participating in message relay

### `Expiring`

- the authority has moved away; preparing for reclamation

### `Closed`

- reclaimed

## 13. Recommended Interface Directions

### `ReplicationService`

- `update_witness_context`
- `build_visibility_delta`
- `flush_to_gateway`

### `GhostManager`

- `create_ghost_replica`
- `update_ghost_replica`
- `install_ghost_route`
- `resolve_ghost_route`
- `retire_ghost_replica`

### `WitnessContext`

- `enter_view`
- `leave_view`
- `refresh_view`
- `suspend_view`
- `close_view`

## 14. Abstractions Suggested for Addition in Apollo

It is suggested to add, in the future:

- `modules/game/world/include/apollo/game/world/witness_context.hpp`
- `modules/game/world/include/apollo/game/world/view_trigger.hpp`
- `modules/game/world/include/apollo/game/world/ghost_replica.hpp`
- `modules/game/world/include/apollo/game/world/ghost_route_table.hpp`
- `modules/game/world/include/apollo/game/world/replication_service.hpp`
- `modules/game/world/include/apollo/game/world/alias_allocator.hpp`

## 15. Relation to Current Apollo

Apollo has not yet implemented a true distributed space, so this layer should not rush into full-landing the code.

The steadier approach is:

### Current stage

- first get `WorldSession` and `WorldHost` running smoothly
- in the single-process world, first extract the client-view semantics

### Next stage

- add `WitnessContext`
- make the client view no longer depend directly on local AOI broadcast

### The stage after that

- add `GhostReplica`
- add `GhostRouteTable`
- enter the true distributed space

## 16. Conclusion

If Apollo is to truly enter BigWorld mode, `Witness` and `Ghost` must be understood as two separate things:

- `Ghost` answers "how does an entity exist on a non-authoritative node"
- `Witness` answers "what exactly does the client see"

Only with these two layers separated will distributed AOI, boundary observation, unbroken migration, and client synchronization become truly stable.

Otherwise the system easily degenerates into:

- a big-world prototype that can query nearby objects across nodes

but cannot run stably over the long term.

## Related Reading

- [KBEngine Source Analysis](/architecture/kbe-source-analysis)
