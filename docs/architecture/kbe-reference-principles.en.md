---
title: KBE Reference Principles and ECS Compatibility Trade-offs
icon: scale-balanced
order: 15
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - KBE
  - ECS
  - Design Principles
---

[English](kbe-reference-principles.en.md) | [中文](kbe-reference-principles.md)

# KBE Reference Principles and ECS Compatibility Trade-offs

This document is not about one specific module; it sets a general principle for all subsequent Apollo architecture design:

`Apollo can use KBE as a reference, but must not copy it wholesale.`

This sentence is not a platitude; it is an engineering constraint that must land on every design point.

One point must be made especially clear:

- Many core concepts of `KBE/BigWorld`
- and modern `ECS / data-oriented / cache-friendly batch processing`

are not naturally aligned.

If this premise is not stated up front, later designs easily fall into two kinds of trouble:

- Mechanically copying KBE and ending up in conflict with the modern modular architecture Apollo wants
- Talking only about ECS while refusing to acknowledge the real constraints of an MMO distributed world and an online object model

## 1. Conclusion First

Apollo's correct attitude toward KBE should be:

- `Reference the way it solves problems`
- `Do not copy its historical implementation forms`
- `Preserve room that stays friendly to ECS/data-oriented design`
- `Every design point must state its rationale, advantages, costs, and rejected alternatives`

More precisely:

`Apollo should absorb KBE's distributed-world experience, not replicate all of KBE's object and runtime style.`

## 2. Why KBE Must Not Be Copied Wholesale

### 1. KBE solved problems of a specific era

The core strengths of KBE/BigWorld lie in:

- Large-world distributed spaces
- Proxy / Base / Cell object layering
- Witness / Ghost
- Remote entity calls
- Script-driven business logic

These capabilities remain very valuable today.

But many concrete KBE implementations also carry clear marks of their era:

- Deep Python binding
- A very heavy object-oriented entity model
- Distributed-object semantics taking priority over data-oriented semantics

Apollo has no need to reproduce these implementation forms as-is.

### 2. Apollo's goals are broader than KBE's

Apollo does not serve only one form:

- BigWorld MMO

It must support:

- Light online
- Room-based games
- Ordinary MMO
- BigWorld enhanced mode

This means Apollo must preserve:

- Stronger modularity
- Cleaner layering
- More flexible business-domain assembly

### 3. Modern engineering goals have changed

What Apollo pursues now is not just:

- Getting something to run

it also includes:

- Modular assembly
- Layered extension
- Replaceable script backends
- Operability and governance
- Reusable platform capabilities

This requires designs that state their trade-offs far more explicitly than KBE ever did.

## 3. The Main Conflict Points Between KBE and ECS

This must be written out explicitly, otherwise people later tend to assume the two are naturally compatible.

## 4. Conflict Point 1: Object-Centric vs Data-Centric

### KBE leans object-centric

KBE's core view:

- The Entity is the center
- BaseEntity / CellEntity / Proxy / Ghost are all objects
- Remote calls revolve around object references

### ECS leans data-centric

ECS's core view:

- Data components are the center
- Systems traverse components in batches
- Object identity is a secondary concept

### Design conclusion

Apollo cannot turn the entire runtime into pure ECS, nor can it make everything a heavy object model.

The more reasonable approach:

- Use object semantics for online identity, routing, and distributed-world boundaries
- Preserve ECS/data-oriented space for combat, attributes, local simulation, and batch-processing hot paths

## 5. Conflict Point 2: Remote Entity Calls vs Batch System Updates

### KBE emphasizes `EntityCall`

That is:

- Call a certain entity
- Call a certain method
- Route to a certain host

### ECS emphasizes system-level batch processing

That is:

- A batch of Position + Velocity
- One system update

### Design conclusion

Apollo should keep both paths:

- `RemoteEntityCall`
  - Solves cross-host, cross-process, cross-space object semantics
- `System Batch Update`
  - Solves local high-frequency computation

Neither path may forcibly replace the other.

## 6. Conflict Point 3: Heavy Entity Lifecycle vs Lightweight Component Hot Paths

### KBE's entity lifecycle is heavier

For example:

- enter world
- leave world
- get witness
- migration
- backup

### ECS hot paths want to stay lightweight

For example:

- Position updates
- Attribute evaluation
- Combat system iteration

### Design conclusion

Apollo should handle this in layers:

- `PlayerAnchor / AvatarEntity / GhostReplica`
  - Maintain the object lifecycle
- `Movement / Combat / Attribute`
  - Allowed to adopt ECS or data-oriented structures internally

That is:

- Object model on the outside
- Data-oriented systems on the inside

## 7. Conflict Point 4: Script Object Semantics vs a Data-Oriented Kernel

KBE strongly emphasizes:

- Script entities
- Script methods
- Script properties

But if all of Apollo were scripted, it would directly conflict with high-frequency system optimization.

### Design conclusion

Apollo should hold to:

- The script layer owns business rules
- Hot-path kernels stay C++ first

This is consistent with the earlier script-layer design.

## 8. What Apollo Should Keep When Referencing KBE

The following are what Apollo should focus on absorbing.

### 1. The `Proxy / Base / Cell` layering idea

Rationale:

- Separates the lifecycles of connection, long-term state, and spatial presence

Advantages:

- Better suited to reconnection, map switching, and distributed authority migration

Costs:

- Object-model complexity rises

Why it is still worth keeping:

- It is a core constraint of the MMO online main path, not historical baggage

### 2. The `Witness / Ghost` idea

Rationale:

- Solves continuity of the client-visible world in a distributed space

Advantages:

- Supports boundary observation, migration windows, and cross-partition views

Costs:

- Replication path and object-routing complexity increase

Why it is still worth keeping:

- It is a core capability of the BigWorld model

### 3. A unified entity-definition layer

Rationale:

- Properties, methods, replication, persistence, and script interfaces must be unified

Advantages:

- High schema consistency

Costs:

- Higher up-front abstraction cost

Why it is still worth keeping:

- Without this layer, things scatter more and more later on

### 4. Explicit host boundaries

Rationale:

- Which logic lives in Base, which in World, which in Gateway must have explicit boundaries

Advantages:

- Stable architecture

Costs:

- More design constraints needed early on

## 9. What Apollo Should Not Copy

The following are not recommended for direct copying.

### 1. Do not copy Python-only runtime binding

Rationale:

- Apollo must support replaceable script backends

Advantages:

- Lowers the risk of being locked into one language binding

Costs:

- A unified Script ABI must be built first

### 2. Do not copy "all logic revolves around heavy entity objects"

Rationale:

- Combat, attributes, and batch-processing hot paths would be dragged down

Advantages:

- Preserves room for ECS/data-oriented design

Costs:

- Must handle object semantics and system semantics coexisting

### 3. Do not copy KBE's full process family as the default starting form

Rationale:

- Not every project needs BigWorld mode

Advantages:

- Ordinary online games and ordinary MMOs land more easily

Costs:

- The enhanced layer and the default layer must be kept clearly apart

### 4. Do not treat historical naming as true semantics

Rationale:

- For example, Apollo's current `base-app` and `cell-app` both carry historical semantic drift

Advantages:

- Correct the semantics first, then decide the directory names

Costs:

- The transition period needs especially clear documentation

## 10. Apollo's Recommended Compromise Route

Apollo's more reasonable route is neither:

- `A pure KBE object model`

nor:

- `A pure ECS server`

but rather:

- Object model for online identity and distributed boundaries
- ECS/data-oriented implementations allowed for high-frequency local systems
- Layering to keep the two apart

### More specifically

#### Carried by the object model

- `Proxy`
- `PlayerAnchor`
- `AvatarEntity`
- `GhostReplica`
- `WitnessContext`
- `RemoteEntityRef`

#### Where ECS/data-oriented implementations are allowed

- movement subsystem
- combat subsystem
- attribute evaluation
- AI local state updates

## 11. The Format All Subsequent Documents Must Adopt

This is the most critical requirement.

All subsequent Apollo design documents should uniformly contain the following 6 sections.

### 1. Design goals

Answers:

- What problem does this design solve

### 2. References

Answers:

- Which KBE concepts were referenced
- Which modern architecture ideas were referenced

### 3. Why design it this way

Answers:

- The direct reason for choosing this approach

### 4. Advantages

Answers:

- What benefits were gained

### 5. Costs and risks

Answers:

- Where do complexity, performance, and maintenance costs increase

### 6. Why not another option

Answers:

- Why not copy KBE
- Why not make it pure ECS
- Why not make it simpler but less stable

## 12. Direct Requirements on Existing Documents

As documents continue to be added, it is recommended to uniformly include these items:

- `Why design it this way`
- `Advantages`
- `Costs`
- `Why other options were rejected`

This upgrades the whole document set from "a collection of conclusions" into "a defensible architecture design draft."

## 13. Conclusion

Apollo's correct path is neither to deny KBE nor to deify it.

The more reasonable approach:

- Reference KBE's experience in solving MMO distributed-world problems
- Preserve modern modularity, script abstraction, platform-capability layering, and ECS/data-oriented room
- For every design point, state clearly the rationale, advantages, costs, and rejected alternatives

Only then will this overall framework avoid becoming:

- A replica of historical design

and also avoid becoming:

- A hollow modern rewrite that has lost the real constraints of MMO

## Related Reading

- [Base Cell Proxy Object Model](./base-cell-proxy-model.en.md)
- [KBEngine Source Analysis](./kbe-source-analysis.md)
