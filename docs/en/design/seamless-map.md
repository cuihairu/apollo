# Seamless Map and Multi-Cell Space Design

> Status: target design, implemented in stages. The current runtime still uses a local Scene and a single simulation authority per space; this document defines target behavior and reviewable implementation boundaries.

## 1. Goal and terms

A seamless map is one stable Space hosted by multiple Cell partitions, where clients observe continuous entities as they cross boundaries and each entity has exactly one writable simulation authority at a time. Space is the logical world, Cell is a spatial partition, `cell` is an execution role/process, and World manages topology and placement. Instance remains a temporary gameplay lifecycle. Scene is the current local simulation container; no duplicate space container is introduced.

Apollo adopts BigWorld's Real/Ghost and neighboring Cell visibility model and the relevant KBEngine practices for Cell/Space/entity transfer. A Ghost is a read-only projection tagged with source generation, never a second writable Entity. Stable IDs, placement versions, authority epochs, and observer watermarks solve identity, topology, write ownership, and visibility ordering separately.

## 2. Partitioning and load

Space coordinates remain global and continuous. A partition records stable CellID, boundary geometry, adjacency, hosting cell instance/start generation, and topology version. The first multi-Cell version uses axis-aligned rectangles with half-open `[min,max)` edges; the outer map edge is handled explicitly so an entity has one owner. Cross-map travel remains an explicit Space switch.

World owns placement and topology; cell is the writer for its partition. Load samples include tick CPU, entity count, AOI/message volume, and queue watermarks with a measurement window and thresholds. Initially, partitions are static and migration is operator initiated. Automatic split/rebalance waits for migration transactions, load telemetry, capacity reservation, and rollback evidence. Copying write authority is never a load-balancing strategy.

Topology changes publish atomically under `topology_version`. Prepare does not change routing. After commit, directory and Gateway accept only the new version; stale requests receive an explicit stale/migrating result. Adjacency must be symmetric, with neither gaps nor overlap; invalid topology is rejected.

## 3. Ghost lifecycle and consistency

An entity has exactly one Real in one Cell. When it enters a neighboring Cell's ghost-distance band, the source emits `GhostCreate(entity_id, source_cell, authority_epoch, topology_version, position, visible_state, sequence)`; it emits a matching removal on exit or after topology commit. The target applies idempotently by `(entity_id, source_cell, authority_epoch)`, with monotonic sequence. Old epochs, topologies, and duplicate messages cannot overwrite newer state. Ghosts contain only the minimum read-only fields needed for AOI and combat checks, never Base-private data, storage credentials, or arbitrary Lua pointers.

Lifecycle is `Absent → Preparing → Active → Removing → Absent`. The target prepares and acknowledges while the source remains the sole writer. After transfer commit, the target activates as Real under an incremented authority epoch and the source demotes/deletes its Real; neighbors refresh their Ghosts. Before commit, failure lets the source continue and preparation is cancelled. If commit outcome is unknown, query World's durable operation record; never guess or run both writers. Ghost updates may be coalesced to the latest contiguous watermark. A gap triggers a full snapshot request; timeout marks the Ghost unusable for authoritative decisions and raises an alert.

Ghost updates use bounded intent queues alongside simulation. World management is not a per-frame relay. `topology_version`, `authority_epoch`, entity update sequence, and operation ID remain explicit fields. Receivers validate the source Cell's current assignment and generation so a disconnected old process cannot revive a stale projection.

## 4. Continuous visibility and AOI

An AOI query first gets candidates from the local spatial index, merges Real entities from the local Cell with Ghost candidates from adjacent Cells, deduplicates by entity ID, then applies exact distance and visibility rules. Ghost range must cover AOI radius plus maximum movement during synchronization delay; configuration must validate the bound from speed, tick, and network latency. As an observer crosses a boundary, its Witness/visibility watermark is retained: prepare visibility in both Cells, compute ENTER/SYNC/LEAVE differences over one entity-ID set, and switch the source only at commit. This avoids duplicate ENTER or a transient LEAVE/ENTER flicker.

Each observer tracks known entities and the last applied sequence per entity. Entering visibility sends a full visible snapshot before deltas; leaving sends one LEAVE. A source switch for the same entity and continuing ID/epoch emits SYNC, not LEAVE followed by ENTER. Missing snapshots, sequence gaps, and expired Ghosts trigger snapshot repair, resynchronization, or temporary hiding respectively; the client never becomes authoritative.

Stage 0 improves candidate coverage in one local Scene only. It creates neither inter-process Ghosts nor a claim of boundary continuity.

## 5. Cross-boundary combat and transfer

The Real Cell for an entity owns its movement, state, and personal combat result. If attacker and target occupy different Cells, the request is sent to the target's current authority. That authority validates attacker identity, input sequence, trusted position/occlusion data as needed, cooldown, and damage rules. Required remote state comes from a generation-tagged, read-only Ghost; stale state beyond the allowed age is rejected or delayed rather than independently settled by both Cells.

Each combat event has a stable `combat_event_id` and target `authority_epoch` for deduplication. Shared objectives, team effects, and Instance outcomes have one logical owner (a World-assigned owner Cell or a dedicated gameplay authority); partitioned Cells do not each submit final rewards. The original Base validates results and database persists them. During entity transfer, the source freezes new combat side effects and drains or records issued events. After World durably commits the target and new epoch, the target resumes input. Late events are checked against epoch and stable event ID; the old writer cannot continue.

## 6. Failure takeover and planned rollback

On Cell failure, World confirms the old writer is fenced by lease/start generation; heartbeat timeout alone cannot prove that one side of a network partition stopped writing. Automatic takeover stays disabled without fencing or an equivalent write barrier. World persists partition placement, topology version, operation phase, and the unique commit decision. Cells periodically publish recoverable checkpoints; combat/settlement side effects have a separate idempotency ledger where required.

Takeover order: isolate the old generation → reserve a candidate Cell → restore the last complete checkpoint → validate entity/Instance/timer/random state and pending events → durably advance authority epoch → publish routes → accept new input. If checkpoint completeness or pending settlement ownership cannot be proven, freeze the affected Cell and use an explicit recovery/compensation path; never reconstruct full simulation state from neighbor Ghosts.

Planned migration uses freeze, snapshot, target prepare, durable commit, target activation, route update, and source cleanup. Before commit it can be cancelled and the source resumes. After commit it only moves forward; rollback is a new reverse migration, never deletion of the original decision or simultaneous source write access. World restart/rollback restores its management log and generation, reattaches Cells, and reconciles each assignment. Without fencing the old manager, only stopped takeover is allowed. Cross-host arbitration and automatic hot standby are outside this design.

## 7. Integration with the current architecture

Line numbers refer to repository `main` at the time this document was written.

| Existing boundary | Current source | Integration |
|---|---|---|
| 30-Instance GameServer | `docs/30-Instance_GameServer_Design.md:7` defines Space/Cell/Instance; `:24-26` distinguishes Space switch, Base ownership, and updates | Preserve these semantics; same-Space partitioning does not reuse Instance's eight-state lifecycle or migrate Base |
| World/Cell management | `docs/design/world-cell-management.md:34-45` assigns topology to World, simulation to cell, and direct Gateway routing; `:49-63` defines identity/version roles | Add stable Cell geometry, adjacency, and topology version; retain operation ID, start generation, and separate authority epoch |
| Active ADRs | `docs/architecture/adr.md:9-23` (ADR-002/004/005) defines Scene, migration, and local AOI boundaries; `:37-51` (ADR-009/010/012) defines single writer, directory, and placement | Extend these decisions; new writable cross-Cell behavior requires its own ADR/protocol decision |
| Entity/Cell lifecycle | `modules/game/world/include/apollo/game/world/scene.hpp:20-63` makes Scene own Avatar/Entity/AOI; `apps/world/src/world_app.cpp:716-800` handles create, destroy, move, and cross-border intents | Keep Scene local initially; map stable IDs/epochs across transfer and replace hosting only after commit, never copy Entity pointers |
| Local AOI | `modules/game/world/include/apollo/game/world/scene_aoi.hpp:13-25,57-81` defines SceneAoi/events/index; `:175-200` is candidate lookup | Extend candidate lookup across adjacent partitions, then connect Ghosts; reuse ViewerState watermarks with source-generation extension |
| Gateway egress | `apps/world/src/world_app.cpp:1533-1564` queries viewers in the entity's Scene and sends; `apps/gateway/src/m1_egress.cpp:330-380` handles Cell create/destroy/move/cross-border frames | Keep Gateway sockets and egress; add topology/entity generations and source-switch continuity, while Gateway routes only committed placements |
| Instance cleanup | `apps/common/src/squad_instance.cpp:150-175` clears entities/AOI/temporary Scene on close | Multi-Cell Instance shutdown waits for each partition to drain; persistent Spaces remain independent of temporary Instance cleanup |

## 8. Staged implementation and acceptance

Each stage produces retained evidence. Later stages remain disabled until their prerequisites pass.

| Stage | Scope and prerequisites | Acceptance evidence |
|---|---|---|
| 0: local spatial query baseline | Correct candidate coverage for any configured view radius in one Scene; no cross-process transfer | New multi-grid/boundary unit cases, all AOI/Scene cases passing, results equal a brute-force distance oracle |
| 1: topology and partition identity | Space/Cell geometry, adjacency validation, topology version, static configuration; needs World management records and stable IDs | Tests for valid/gapped/overlapping/asymmetric topology; restart preserves topology digest; invalid config is rejected |
| 2: inter-process Ghosts and boundary AOI | Message identity, lifecycle, sequence/snapshot repair, continuous Witness differences; needs Stage 1, generated contracts, bounded Cell transport, and fencing | Deterministic multi-process edge round trips under duplicate/reorder/disconnect; no dual Real or duplicate ENTER/LEAVE; load/queue metrics |
| 3: entity and combat transfer | Per-entity freeze, snapshot, target prepare, commit, cleanup; combat owner/event dedupe/Base settlement boundary | Fault injection at every transfer phase; one authority before/after commit; replay causes no duplicate damage/reward and client view stays continuous |
| 4: failure takeover and controlled migration | Management log, checkpoints, fencing, reconciliation, forward completion and reverse transfer; needs Stages 1–3 | Kill/process/network-isolation/restart matrix; state and settlement ledger reconciliation; old-generation writes rejected; recorded rolling-upgrade drill |
| 5: dynamic load and operations | Hotspot detection, capacity reservation, automatic re-placement and rollback policy; needs stable transfer and telemetry baseline | Reproducible report for tick latency, queues, transfer cost/abort rate; policy simulation without oscillation; feature switch and rollback drill |

Stage 0 is independently verifiable and improves one local Scene's spatial query. It assumes no Cell topology, Ghost, or remote entity container.

## 9. Risks and deferred scope

- **Defer same-Space multi-Cell production:** Scene and WorldApp still combine local simulation/management paths; durable topology commit, fencing, remote authority handoff, and cross-Cell Witness are absent.
- **Defer automatic split/rebalance:** without migration consistency and measurements, automatic movement expands the failure surface. Start with static partitions and operator actions.
- **Do not put full entity state in Ghosts:** Ghosts stay minimal and read-only; Base-private data, mutable Lua state, and arbitrary shared pointers are not mirrored.
- **No dual writes or independent combat decisions:** these create duplicate damage/rewards and divergent state. Every target and shared gameplay result has one authority.
- **Defer automatic recovery without a log and cross-host hot standby:** unknown commit outcomes or an unfenced old writer can create split brain. Freeze and reconcile first.
- **Do not make Gateway or AOI a business process:** Gateway keeps sockets and forwarding; AOI is simulation spatial data, while management publishes topology and routing decisions.
- **Do not change persistent ownership, expand Instance semantics, or claim Stage 0 is seamless:** that would violate existing Base, database, Scene, and Instance boundaries.

## 10. External references

External material validates mature-engine mechanisms and does not prove Apollo support. Retrieved 2026-10-19.

- BigWorld Server Overview, “CellApp,” Real/Ghost and neighboring Cell behavior: [manual mirror](https://howarduong.github.io/github.io/doc/html/server_overview/ch05.html).
- BigWorld Server Programming Guide, “Implementing Entity Extras”: [`BigWorld-Engine-1.9.1` manual source](https://github.com/v2v3v4/BigWorld-Engine-1.9.1/blob/master/bigworld/doc/generated_html/server_programming_guide/ch23.html), for Ghost-domain controllers and visibility queries.
- KBEngine official configuration documentation: [ghostDistance](https://kbengine.github.io/docs/configuration/kbengine_defaults.html). This supports the configurable replication-distance concept; behavior must be checked against the selected KBEngine source version.
- In-repository comparisons: [BigWorld–Apollo mapping](./bigworld-apollo-mapping.md), [KBEngine analysis](../architecture/mmo-frameworks/kbengine-analysis.md).
