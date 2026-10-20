# BigWorld to Apollo Concept Mapping

This page maps BigWorld 14.4.1 server terms to Apollo processes, objects, and code, distinguishing partial responsibility matches, project gameplay, and missing implementation. Keep gateway, loginServer, baseServer, worldServer, database, and machined. The discussed `cell` execution process corresponds to CellApp; uppercase `Cell` means a spatial partition.

## 1. Process responsibilities

| BigWorld | Apollo counterpart | Responsibility and difference | Implementation boundary |
|---|---|---|---|
| LoginApp | Authentication in loginServer | Login ingress/authentication; Apollo also hosts service/online directories here. | Existing login primitives; final Base/cell wiring incomplete. |
| BaseApp | baseServer | Persistent non-spatial logic; `base::Avatar` implements the player profile side. Base is not the database process. | apps/base provides the standalone base entry with Avatar activation through remote database. Base gameplay and settlement admission are wired; directory ownership remains pending. |
| Relevant BaseAppMgr duties | Base registration, placement, and directories in loginServer | Reuse responsibility boundaries without claiming a full BaseAppMgr implementation. | Base registration/ownership/recovery pending. |
| Relevant CellAppMgr duties | worldServer | Manage cell processes, Space/Cell assignment, capacity, topology, drain, and transfer; no per-entity simulation loop. | Execution kernel extracted from WorldApp (CellApp owns the simulation loop, Scene, AOI, combat and Lua; WorldApp keeps the management admission face and embeds the kernel); the cell registration admission face is landed (contract 960/961 raw frames with an epoch-adjudicating registry, see the L101 first slice in the root todo); process management, Space/Cell assignment, capacity declaration and topology versioning remain pending. |
| CellApp | cell execution process | Host local Cells of one or more Spaces; advance entities, AOI, combat, scripts, and ticks. | The kernel runs from the `--role=cell` entry (intent admission only); both registration faces and heartbeat expiry are landed — World-side admission (960's nine self-declared fields recorded, 961 echoing the management epoch with an adjudication code; raw-frame admission as with 944/952/956, auth credentials tracked in the world-cell-management gap table) and cell-side active dialing (`--register-host/--register-port/--cell-id`, registering on link-up transitions, idempotent refresh on re-dial); periodic heartbeats keep the entry alive (default 5s 960 re-send) and the World-side expiry window drops stale entries (`--cell-expiry-ms`, default 30s — dropped entirely when the window lapses, re-registration admitted as a fresh registration); the multi-Space declaration face is landed (`--add-space <id>:<name>` wired at the startup window, one scene and route per Space, instance-less); the World allocation face is landed (contract 962 `cell_space_assign` — the `--assign-spaces` pool is claimed exclusively by the first ready cell; every accepted registration pushes the full-pool authoritative ownership view, with per-version dedup on the cell side applied into the kernel routing table; expiry release zeroes the owner and re-claiming advances the version as an ABA guard, while an offline former owner converges through ready=0 revocation markers on re-registration — default Spaces refuse revocation, and occupied Spaces enter a bounded pending-revoke table retried every tick until vacated (a World re-claim cancels the stale revoke; the latest view wins), with migration/drain orchestration deferred); control-link grace freeze and old-executor isolation are landed (a control link down beyond the `--link-grace-ms` window freezes the kernel — simulation paused and the admission gate closed; unfreezing only follows a verified 961 re-admission, and a stale-epoch rejection (code 3) freezes the executor on its own; the World-side epoch watermark blocks stale-epoch resurrections after expiry removal); capacity declaration adjudication and topology versioning/rebalancing strategy remain unimplemented, and disconnect suspect-marking with cross-restart persistent tombstones follow the management-log batch. |
| DBApp/DBAppMgr storage duties | database | Load/save, storage routing, durable outcomes; no reward-rule decisions. | apps/database provides M1 load/save and a file journal. Write generations, storage partitioning, and complete queues remain pending. |
| Local bwmachined supervision | machined | Start, stop, supervise, restart local processes; no player Base or Space placement decisions. | Reuse supervision; validate readiness and business recovery separately. |
| Proxy connection/session duties | gateway plus Base/session association | BigWorld Proxy belongs to Base-side entities. Apollo has a separate gateway socket owner; gateway alone is not a complete Proxy implementation. | Connections/partial sessions exist; Base/cell routing incomplete. |

These are responsibility mappings, not process renames. cell registers with its owning worldServer, which owns topology placement. loginServer holds World endpoints and player ownership/location projections; directory projections and gateway caches are not second topology writers.

Source boundaries: [CellAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellappmgr/cellappmgr.hpp), [BaseAppMgr](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseappmgr/baseappmgr.hpp), [Base](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/base.hpp), and [Proxy](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/baseapp/proxy.hpp).

## 2. Space and gameplay

| Concept | BigWorld meaning | Apollo counterpart/boundary |
|---|---|---|
| Map resources | Static geometry/resources loaded by runtime spaces | SceneDescriptor references assets, not an activity or process. |
| Space | An identified runtime coordinate space spanning one or more Cells | Corresponds to the spatial meaning of Apollo's existing Scene. Scene is the current code type name; the execution kernel now has an explicit space→scene routing table (CellApp add_space / scene_id_of_space), so one process can host multiple Spaces, and the mapping is a table rather than an identity alias. Extend logical-space/partition boundaries without stacking synonymous Scene→Space containers. |
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
| Base entity | `base::Avatar` and other persistent objects in baseServer | Base validates persistent gameplay and saves through database; simulation transfers retain storage affiliation, while Base hosting can migrate independently. base::Avatar is not a complete generic Base-entity implementation. |
| Cell/Real entity | Avatar and simulated Entity in the owning cell partition | One effective simulation writer per entity. Avatar is a player gameplay type, not another process. Preserve entity IDs through transfer. |
| Ghost entity | Reserved non-authoritative adjacent-Cell projection | Supports visibility/queries, not independent damage, rewards, or profile writes. Replication/deduplication/expiry are incomplete; a transfer snapshot is not a persistent Ghost. |
| Witness | Partial observer-view duties in SceneAoi, ViewerState, and attribute sync | View management differs from cross-partition projection. Local AOI is not complete Witness or cross-Cell visibility. |
| Mailbox/EntityCall | RemoteEntityRef, method metadata, RouteResolver, message contracts | Validate real addressing/authority/reply wiring; local primitives do not establish distributed calls or old-version decoding. |
| Proxy/client association | gateway connections, Session, and Base association | Connection, profile, and spatial entity have separate owners; changing simulation hosts need not replace client sockets. |

See [Cell Entity](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/entity.hpp) for Real/Ghost ownership and [Witness](https://github.com/v2v3v4/BigWorld-Engine-14.4.1/blob/main/programming/bigworld/server/cellapp/witness.hpp) for observer views. These support comparisons, not claims that Apollo implements them.

### Scope of the Avatar name

Avatar describes a character, not a Cell execution location. BigWorld's 2.1 glossary also includes NPCs, and its tutorial and KBEngine demos show Base/Cell parts. Apollo maps `base::Avatar` to the player's Base part and `cell::Avatar` to the Cell part. This implementation mapping neither makes upstream Avatar Cell-only nor establishes either name as common industry terminology. See [player object model](./player-object-model.md) for sources, other engines, and naming assessment.

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
| Scene, SceneDescriptor | Local spatial container and static resource reference | Explicit space→scene routing table landed (space ID and scene ID are separate domains); unified global coordinates and separating local partition state from logical-space identity remain pending. |
| Instance | Temporary activity lifecycle | Keep eight states; add durable remote settlement and exclusive shared-state ownership. |
| `base::Avatar`, `cell::Avatar` | Persistent Base object / player Cell entity | Separate hosts, snapshots, trusted associations; remove cross-boundary pointer access. |
| SceneAoi, ViewerState | Local partition visibility | Add adjacency replication, merged spatial visibility, deduplication, continuity. |
| WorldAssignment, RemoteEntityRef | Routing/reference primitives | Separate Base, World, Space, Cell, and process identities with authority/version checks. |
| SceneTransfer | Explicit scene departure/recreation | Same-Space Cell handoff preserves health, skills, clocks, and necessary script state without scene-exit reset. |

This documentation does not rename C++ types, executables, or wire fields, and does not claim implementation is complete. Root todo.md tracks migration under this single semantic model.

See [World and cell management](./world-cell-management.md), a review draft covering the proposed short binary name `world`, default manager role, `--role=cell` executors, local automatic growth, and natural shrinking. Execution kernel extraction and the `--role=cell` entry are now landed (see the L98 first slice in the root todo); the explicit Space↔Scene mapping is landed too (L99 first slice — space→scene routing table hosting multiple Spaces per process); both cell registration faces are landed (L101: 960/961 raw frames with an epoch-adjudicating registry, plus `--register-host/--cell-id` cell-side dialing and re-dial); heartbeat expiry and stale-entry removal are landed (periodic 960 keep-alive plus the `--cell-expiry-ms` window, re-registration through the fresh-admission path); the multi-Space declaration wiring is landed (carrying on from the L99 slice — `--add-space <id>:<name>` wires config-declared Spaces at the startup window, idempotent on duplicates, entry rejects invalid or default-colliding ids); the first World allocation slice is landed (962 cell_space_assign — the `--assign-spaces` pool is claimed exclusively by the first ready cell, pushed on every accepted registration, cell-side per-version dedup applies it; the L108 recycle slice refines 962 into a full-pool ownership view — expiry release zeroes the owner without touching the version, re-claiming advances it as an ABA guard, and offline former owners converge through ready=0 revocation markers on re-registration; rebalancing strategy/topology versioning follow the L108 later batches); the first control-link grace freeze and old-executor isolation faces are landed (cell-side `--link-grace-ms` grace freeze plus unfreeze only through a verified 961 re-admission; the World-side epoch watermark blocks stale-epoch resurrections — persistent tombstones and disconnect suspect reconciliation follow the management-log batch). Local automatic growth, natural shrinking, and automatic failover remain pending.
