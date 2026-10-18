# Apollo Concepts and Names

Keep gateway, loginServer, baseServer, worldServer, database, machined, and the managed `cell` execution process. Reuse BigWorld spatial/entity semantics. See [BigWorld to Apollo mapping](./bigworld-apollo-mapping.md) for source meanings, Apollo responsibilities, code, and gaps.

## Process and execution boundaries

| Apollo name | Responsibility reference | Ownership |
|---|---|---|
| gateway | Separate connection layer, partial Proxy/session duties | Client sockets/delivery and cached Base/simulation routes. |
| loginServer | LoginApp plus Apollo service/online directories | Authentication, Base/World lookup, ownership/location projections. |
| baseServer | Persistent non-spatial BaseApp behavior | PlayerAnchor/base gameplay/settlement validation. |
| worldServer | CellAppMgr spatial/process management duties | cell registration/readiness/load, Space/Cell placement/topology, transfer/drain. |
| cell execution process | CellApp | Local Cell entities, AOI, combat, scripts, ticks, and results to owning Bases. |
| database | DBApp storage access duties | Load/save, storage routing, durable outcomes, not reward rules. |
| machined | Local supervision | Process lifecycle/restart, not player or cross-server placement. |

cell registers with its owning worldServer without requiring local machined. worldServer owns topology; loginServer projections and gateway caches refresh by version rather than decide partition ownership independently.

## Spatial and gameplay boundaries

| Concept | Apollo meaning |
|---|---|
| Map resources | Static geometry/configuration referenced by SceneDescriptor, not a running activity. |
| Space | Identified runtime coordinate space corresponding to the spatial semantics of existing Scene. Do not stack synonymous containers; Scene currently implements a local container. |
| Cell | Spatial partition with bounds/authoritative entities assigned to a process; not the cell process itself. |
| Instance | Apollo temporary group/objective/clock/settlement/cleanup lifecycle associated with Space, not a claimed native BigWorld eight-state type. |
| Base entity | Persistent non-spatial object in baseServer; player-side code is PlayerAnchor. |
| Cell/Real entity | Entity with simulation write authority inside cell; reuse Avatar for players. |
| Ghost | Reserved non-authoritative adjacent projection without independent business writes. |
| Witness | Client-visible world context; ViewerState, SceneAoi, and attribute sync cover part of the duties. |

Default to one cell execution process and one Cell per Space. A process can host Cells from multiple Spaces. Scale whole Spaces across processes first; allow multiple Cells within one Space later. Space identity/coordinates survive process/partition changes.

## Character entities and Avatar

BigWorld's Avatar terminology includes player characters and NPCs. BigWorld/KBEngine character entities may have Base, Cell, and client parts as needed. Apollo's current PlayerAnchor maps to the player's Base part; Avatar implements only the Cell simulation part. PlayerAnchor is a project class name, not common upstream terminology or a fixed-process guarantee. Describe responsibilities with Base/Cell parts and retain actual implementation names. See [player object model](./player-object-model.md) for sources and naming assessment.

## Data ownership and lifecycle

The owning Base validates persistent changes through database; one authoritative Cell advances each entity. Simulation movement across servers, Spaces, and Cells retains persistent-data/storage affiliation; Base hosting can migrate independently for load or updates, with directory ownership and write-authority generations transferred together. Destroying an Instance releases temporary gameplay/spatial resources, not profiles.

Group request → Base validates → worldServer allocates → cell creates associated Space/Cell/gameplay → members run → owning Bases/database durably confirm settlement → depart/clean up. Retain `Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed`.

## Transfer, updates, and seamless reservation

Distinguish cross-Space entry/departure from same-Space Cell handoff. The latter preserves entity ID, position, health, skills, and necessary script state without full departure reset. Shared gameplay needs one logical owner without binding a multi-Cell space to one mutable object/thread.

Protocol upgrades separately replace Base objects, worldServer management state, and cell simulation. Code-only updates preserve wire format and may use Lua rollout. Seamless execution is disabled initially; reserve partition, adjacent visibility, Real/Ghost, authority transfer, and cross-Cell interaction boundaries. Track implementation only in root todo.md.
