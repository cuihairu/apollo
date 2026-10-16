---
title: Base Cell Proxy Object Model
icon: project-diagram
order: 19
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - BaseApp
  - CellApp
  - Proxy
---

[English](base-cell-proxy-model.en.md) | [中文](base-cell-proxy-model.md)

# Base Cell Proxy Object Model

This document addresses one of the most easily misunderstood yet most critical object-model questions in the KBE lineage:

`Why can't a player be represented by just one object — why must it be split into the three tiers Proxy / Base / Cell?`

If this layer is not split apart, Apollo — whether it later goes the ordinary MMO route or the BigWorld route — will keep hitting the same class of problems:

- session coupled to the entity
- logical state hard to preserve after a player disconnects
- muddled object boundaries when switching worlds
- authority in the big-world mode simply cannot be split

## 1. The Conclusion First

For Apollo to learn from KBE, the single most important object-model principle is this:

`Session/Proxy`, `PlayerAnchor/Base`, and `AvatarEntity/Cell` must be kept in separate tiers.

More precisely:

- `Proxy` handles the connection and client carriage
- `Base` handles the player's long-term logical ownership
- `Cell` handles real-time authoritative presentation inside a space
- `DBMgr / PersistenceService` handle persistence execution and are not part of this three-tier object model

These three tiers are in a cooperative relationship, not an inheritance-or-substitution one.

## 2. Direct Evidence in KBE

From the local source tree:

- [proxy.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/proxy.h)
  - `sendToClient`
  - `hasClient`
  - `giveClientTo`
  - `onGetWitness`
  - `kick`
- [entity.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/baseapp/entity.h)
  - `cellEntityCall`
  - `clientEntityCall`
  - `onTeleport*`
  - `onMigrationCellapp*`
  - `reqBackupCellData`
- [entity.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/server/cellapp/entity.h)
  - `realCell`
  - `ghostCell`
  - `baseEntityCall`
  - `clientEntityCall`
  - `controlledBy`
  - `teleport`
  - `changeToGhost`
  - `changeToReal`
  - `onGetWitness`

This shows that in KBE:

- `Proxy` is not a `CellEntity`
- `BaseEntity` is not a `CellEntity` either
- the three sit on different semantic tiers

## 3. Why Not Just One "Player" Object

Many projects start out writing it this way:

- one `Player`
- holding both socket/session
- and inventory/quests/mail
- plus position, movement, AOI, combat

Simple early on; harder and harder to change later.

### Problem 1: The connection and long-term state have different lifecycles

When a player disconnects:

- the connection is gone immediately
- but the player's long-term logical ownership does not necessarily have to vanish at once

### Problem 2: Long-term state and spatial presentation have different lifecycles

When a player changes maps:

- the presentation entity in the world may be destroyed and rebuilt
- but the player's long-term logical object should stay continuous

### Problem 3: Spatial authority migrates in the big-world mode

Under the BigWorld mode:

- the real-time authority inside a cell may change
- but the player's long-term logical object must not drift along with it at will

So the tiers must be split.

## 4. The Recommended Three-Tier Model

Apollo should explicitly adopt this set of objects:

```text
ClientConnection / GatewaySession
    -> Proxy
    -> PlayerAnchor
    -> AvatarEntity
```

### 1. `Proxy`

Semantics:

- the client-side proxy
- the network attachment point
- the session carriage point

### 2. `PlayerAnchor`

Semantics:

- the player's long-term logical ownership
- the main online state after login
- the center for world assignment and reconnect recovery

### 3. `AvatarEntity`

Semantics:

- the player's spatial presentation in the world/cell
- real-time state such as position, movement, AOI, and combat

## 5. What `Proxy` Should Own

The recommended `Proxy` in Apollo corresponds to the semantics of the KBE `Proxy` and owns at least:

- binding the current client connection
- sending messages to the client
- recording the client attachment state
- supporting disconnect, kick, and connection rebinding
- interfacing with the `Witness`

### What `Proxy` Should Not Own

- it should not directly hold full long-term state such as inventory/quests
- it should not become an authoritative entity inside the world
- it should not, on its own, decide which world a player belongs to

## 6. What `PlayerAnchor` Should Own

In Apollo this tier has already been defined fairly clearly in earlier documents.

It owns:

- the player's long-term online state
- gateway/session binding
- world assignment
- dirty-data convergence
- reconnect recovery

In KBE semantics, the closest thing to this tier is:

- `BaseEntity`

and it is strongly tied to `Proxy`.

## 7. What `AvatarEntity` Should Own

This tier corresponds to the real-time entity in the world/cell.

It owns:

- position, orientation
- movement
- AOI
- combat/skill presentation
- real-time authority on the cell side

It should not hold:

- all of the long-term account logic
- all of the persistence flow

## 8. The Recommended Object Relationships

Apollo should later explicitly form the reference relationships below:

```text
Proxy
    -> player_id
    -> session_id
    -> current_witness

PlayerAnchor
    -> player_id
    -> proxy_ref
    -> world_assignment
    -> avatar_ref

AvatarEntity
    -> entity_id
    -> player_id
    -> anchor_ref
    -> proxy_hint
```

### Why refs here instead of stuffing in whole objects

Because these three tiers often live in different hosts:

- `Proxy` leans toward gateway/base cooperation
- `PlayerAnchor` lives in base
- `AvatarEntity` lives in world/cell

Only once the reference boundaries are clear do migration and recovery become easy to do.

## 9. The Relationship Between `Proxy` and `Witness`

These two tiers are easy to conflate.

### `Proxy`

Owns:

- which client I am connected to
- how I deliver messages to the client

### `Witness`

Owns:

- what this client should currently be seeing

So the more sensible relationship is:

- `AvatarEntity` carries a `Witness`
- the `Witness` sends to the client through the `Proxy`

## 10. The Relationship Between `PlayerAnchor` and `AvatarEntity`

These two tiers also must not be merged into one object.

### `PlayerAnchor`

Owns:

- the player's long-term ownership
- world assignment
- the online lifecycle

### `AvatarEntity`

Owns:

- the real-time presentation after entering a world

### The Typical Flow

1. `PlayerAnchor` is activated
2. a world is assigned
3. `WorldHost` creates the `AvatarEntity`
4. `PlayerAnchor` holds an `avatar_ref`
5. after a map change the old `AvatarEntity` can be destroyed and the new `AvatarEntity` rebuilt

But:

- the `PlayerAnchor` itself stays continuous

## 11. Extra Relationships Under the BigWorld Mode

Once the BigWorld mode is entered, things split further into:

- long-term logic on the base side
- real-time authority on the cell side
- projection on the ghost side

That is:

```text
Proxy
PlayerAnchor(Base)
AvatarEntity(Cell Real)
GhostReplica(Cell Ghost)
```

### The Key Point Here

- `GhostReplica` is not a fourth long-term object
- it is only the projection of `AvatarEntity` on a non-authoritative node

## 12. Abstractions Suggested for Apollo

At minimum, add these going forward:

- `modules/game/session/include/apollo/game/session/player_proxy.hpp`
- `modules/game/session/include/apollo/game/session/player_anchor.hpp`
- `modules/game/world/include/apollo/game/world/avatar_entity.hpp`
- `modules/game/world/include/apollo/game/world/avatar_ref.hpp`
- `modules/game/session/include/apollo/game/session/proxy_ref.hpp`

If the BigWorld mode is launched, also add:

- `modules/game/world/include/apollo/game/world/ghost_replica.hpp`

## 13. The Landing Order for Apollo Today

Land in this order:

1. first define `PlayerAnchor`
2. then define `Proxy`
3. then define `AvatarEntity`
4. only then add `GhostReplica`

### Why not `Proxy` first

Because what Apollo actually lacks today is:

- the player's long-term logical ownership

in other words, `PlayerAnchor`.

## 14. The Near-Term Minimal Deliverable

### Minimal Scope

- `GatewaySession` keeps the connection semantics
- `PlayerAnchor` keeps the long-term online semantics
- `WorldSession + AvatarEntity` keeps the world-side semantics
- these three tiers are explicitly split apart

### Not Doing for Now

- a full KBE-style `Proxy` API
- cross-proxy handover details such as `giveClientTo`
- full ghost semantics

### Acceptance Criteria

At minimum, these must be verifiable:

1. when a player disconnects, the `GatewaySession` disappears
2. the `PlayerAnchor` can still be retained briefly
3. the `AvatarEntity` can be destroyed or restored
4. the three tiers' lifecycles never get conflated

## 15. Conclusion

If Apollo wants to truly absorb the KBE object model, the key point is not the names — it is to split apart the three-tier semantics of:

- `Proxy`
- `Base`
- `Cell`

into separate tiers.

Put in terms that better fit Apollo today:

- `Proxy`
- `PlayerAnchor`
- `AvatarEntity`

Once these three tiers are left unsplit, world switching, reconnect recovery, and BigWorld authority will keep fighting each other down the road.

## Related Reading

- [Witness and Ghost Design](./witness-ghost-design.en.md)
