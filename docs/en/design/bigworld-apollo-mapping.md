# BigWorld to Apollo Concept Mapping

This page maps BigWorld 14.4.1 server terms to Apollo processes, objects, and code, distinguishing partial responsibility matches, project gameplay, and missing implementation. Keep gateway, loginServer, baseServer, worldServer, database, and machined. The discussed `cell` execution process corresponds to CellApp; uppercase `Cell` means a spatial partition.

## 1. Process responsibilities

| BigWorld | Apollo counterpart | Responsibility and difference | Implementation boundary |
|---|---|---|---|
| LoginApp | Authentication in loginServer | Login ingress/authentication; Apollo also hosts service/online directories here. | Existing login-app primitives; final Base/cell wiring incomplete. |
| BaseApp | baseServer | Persistent non-spatial logic; PlayerAnchor implements the player profile side. Base is not the database process. | apps/base-server provides baseServer with PlayerAnchor activation through remote database. Directory ownership, business migration, and settlement wiring remain pending. |
| Relevant BaseAppMgr duties | Base registration, placement, and directories in loginServer | Reuse responsibility boundaries without claiming a full BaseAppMgr implementation. | Base registration/ownership/recovery pending. |
| Relevant CellAppMgr duties | worldServer | Manage cell processes, Space/Cell assignment, capacity, topology, drain, and transfer; no per-entity simulation loop. | WorldApp still directly ticks Scene; management/execution separation pending. |
| CellApp | cell execution process | Host local Cells of one or more Spaces; advance entities, AOI, combat, scripts, and ticks. | Independent entry, registration, readiness, and remote dispatch pending. |
| DBApp/DBAppMgr storage duties | database | Load/save, storage routing, durable outcomes; no reward-rule decisions. | apps/database provides M1 load/save and a file journal. Write generations, storage partitioning, and complete queues remain pending. |
| Local bwmachined supervision | machined | Start, stop, supervise, restart local processes; no player Base or Space placement decisions. | Reuse supervision; validate readiness and business recovery separately. |
| Proxy connection/session duties | gateway plus Base/session association | BigWorld Proxy belongs to Base-side entities. Apollo has a separate gateway socket owner; gateway alone is not a complete Proxy implementation. | Connections/partial sessions exist; Base/cell routing incomplete. |

These are responsibility mappings, not process renames. cell registers with its owning worldServer, which owns topology placement. loginServer holds World endpoints and player ownership/location projections; directory projections and gateway caches are not second topology writers.

Source boundaries: [CellAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp), [BaseAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseappmgr/baseappmgr.hpp), [Base](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/base.hpp), and [Proxy](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/proxy.hpp).

## 2. Space and gameplay

| Concept | BigWorld meaning | Apollo counterpart/boundary |
|---|---|---|
| Map resources | Static geometry/resources loaded by runtime spaces | SceneDescriptor references assets, not an activity or process. |
| Space | An identified runtime coordinate space spanning one or more Cells | Corresponds to the spatial meaning of Apollo's existing Scene. Scene currently implements a local container. Extend logical-space/partition boundaries without stacking synonymous Scene→Space containers. |
| Cell | A spatial partition owning authoritative entities within its range | Reuse Space association, partition ID, bounds, host process, and topology version. Not an execution-process name. |
| CellApp | Process hosting Cells | Apollo cell execution process, potentially hosting Cells from multiple Spaces. |
| Instance | No one-to-one built-in counterpart to Apollo's eight-state gameplay lifecycle was established in the inspected spatial core | Retain existing group, objective, clock, phase, settlement, and cleanup management. It associates with runtime space and is not another Space/Cell name or a claimed native BigWorld type. |

Default deployment has one cell execution process and one full-range Cell per Space. Multiple activity Spaces/Cells can share that process. Scale whole Spaces/Cells to additional processes first; future seamless simulation may partition one Space across multiple Cells.

<ArchitectureDiagram kind="space" />

Instance is gameplay rather than a spatial layer. Temporary tower-defense activities manage their objectives, settlement, and associated Space. Persistent Spaces need no temporary Instance. Two groups can load the same assets into separate runtime Spaces. The existing one-Instance/one-Scene rule is Apollo gameplay scope, not a universal BigWorld constraint.

[Space](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/space.hpp) manages Cells and positional lookup; [Cell](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cell.hpp) associates bounds/entities with a Space; [CellApp](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/cellapp.hpp) hosts execution.

## 3. Players, visibility, and calls

| BigWorld | Apollo counterpart | Authority/implementation boundary |
|---|---|---|
| Base entity | PlayerAnchor and other persistent objects in baseServer | Base validates persistent gameplay across Cell/Space/server transfers and saves through database. PlayerAnchor is not a complete generic Base-entity implementation. |
| Cell/Real entity | Avatar and simulated Entity in the owning cell partition | One effective simulation writer per entity. Avatar is a player gameplay type, not another process. Preserve entity IDs through transfer. |
| Ghost entity | Reserved non-authoritative adjacent-Cell projection | Supports visibility/queries, not independent damage, rewards, or profile writes. Replication/deduplication/expiry are incomplete; a transfer snapshot is not a persistent Ghost. |
| Witness | Partial observer-view duties in SceneAoi, ViewerState, and attribute sync | View management differs from cross-partition projection. Local AOI is not complete Witness or cross-Cell visibility. |
| Mailbox/EntityCall | RemoteEntityRef, method metadata, RouteResolver, message contracts | Validate real addressing/authority/reply wiring; local primitives do not establish distributed calls or old-version decoding. |
| Proxy/client association | gateway connections, Session, and Base association | Connection, profile, and spatial entity have separate owners; changing simulation hosts need not replace client sockets. |

See [Cell Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp) for Real/Ghost ownership and [Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp) for observer views. These support comparisons, not claims that Apollo implements them.

## 4. Apollo request ownership

<ArchitectureDiagram kind="system" />

Base profile authority and Cell simulation authority concern different data. Cross-server simulation receives gameplay snapshots/state and trusted Base association, not full profiles or storage credentials. Base rollout hands off persistent objects, cell rollout hands off simulation, and worldServer rollout hands off management topology and in-flight transfers.

## 5. Seamless-world reservation and current scope

The initial implementation target is one executor and one Cell per Space. Seamless execution is disabled, but the design expresses multiple Cells per Space; deferral is not a permanent prohibition.

Reserve separate Space/Cell/process identities, global coordinates/stable entity IDs, bounds/adjacency/topology versions, per-entity authority generations, Real/Ghost/view boundaries, complete runtime snapshots, and cross-Cell ordering/adjudication/deduplication/reconciliation. Shared Instance objectives/settlement require one logical owner, not a shared mutable object or global lockstep across all partitions.

Implement spatial splitting, adjacent replication, boundary combat, dynamic load balancing, and takeover when required. Default deployment need not run these algorithms; interfaces must not equate a Space with one process forever.

## 6. Existing code and migration

| Apollo code | Correspondence | Missing integration |
|---|---|---|
| WorldApp, World, WorldHost | Currently combine management and simulation | Separate worldServer management from cell execution; existing names identify implementation only. |
| Scene, SceneDescriptor | Local spatial container and static resource reference | Unify Space identity/coordinates and separate local partition state from logical space. |
| Instance | Temporary activity lifecycle | Keep eight states; add durable remote settlement and exclusive shared-state ownership. |
| PlayerAnchor, Avatar | Persistent Base object / player Cell entity | Separate hosts, snapshots, trusted associations; remove cross-boundary pointer access. |
| SceneAoi, ViewerState | Local partition visibility | Add adjacency replication, merged spatial visibility, deduplication, continuity. |
| WorldAssignment, RemoteEntityRef | Routing/reference primitives | Separate Base, World, Space, Cell, and process identities with authority/version checks. |
| SceneTransfer | Explicit scene departure/recreation | Same-Space Cell handoff preserves health, skills, clocks, and necessary script state without scene-exit reset. |

This documentation does not rename C++ types, executables, or wire fields, and does not claim implementation is complete. Root todo.md tracks migration under this single semantic model.

See [World and cell management](./world-cell-management.md), a review draft covering the proposed short binary name `world`, default manager role, `--role=cell` executors, local automatic growth, and natural shrinking. Naming and wiring migration remain pending.
