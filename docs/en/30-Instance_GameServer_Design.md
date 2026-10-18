# World and Instance Design

worldServer manages Space/Cell and executors; cell runs simulation. Base owns persistent objects and database owns storage. Follow the [BigWorld–Apollo mapping](design/bigworld-apollo-mapping.md) without synonymous containers or code renames.

## Processes and objects

Space corresponds to existing Scene semantics. Cell is a partition; its executor corresponds to CellApp. Initially one executor hosts full Cells from several Spaces. Scale whole-Space placement first and reserve same-Space partitioning. Instance is Apollo's temporary lifecycle for members, objectives, clocks, settlement, and cleanup; persistent Spaces need no temporary Instance.

<ArchitectureDiagram kind="space" />

## Temporary activity flow

1. Base validates members, eligibility, configuration, and compatibility, then requests World capacity.
2. World places Space/Cell; cell creates Instance/space and validates Base associations/admission.
3. cell advances combat, enemies, objectives, and clocks using necessary snapshots/runtime state only.
4. Submit completion/failure results to each original Base using stable settlement IDs; database confirms durability.
5. Define pending, partial success, compensation, and cancellation without deleting sole recovery evidence. Cancellation grants no success reward.
6. Depart and reclaim entities, AOI, scripts, Instance, and temporary Space; Base objects survive.

Reuse Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed. Cover no entry, all disconnected, disbanding, and timeout. A state machine does not establish distributed persistence.

## Cross-server play and updates

Transfer simulation authority without moving Base/storage affiliation. Executors submit outcomes to original Bases, without home storage credentials. Explicit Space changes differ from future same-Space Cell handoff; current SceneTransfer recreates Avatar and cannot provide seamless continuity.

Protocol updates separately transfer Base objects, cell simulation, and World topology/outstanding decisions. Full activity continuity includes members, entities, enemies, objectives, clocks, randomness, and script state. Code-only updates preserve wire; Lua changes commit at execution boundaries and C++ uses replacement. Validate gateway survival/replacement separately.

## Implementation status

World, Scene, Instance, SceneTransfer, BattleRuntime, and rollout/drain primitives remain reusable. Current WorldApp combines management/execution; Base/database prototypes exist, while the cell entry, trusted associations, real routes, and complete migration remain pending. Future seamless maps reserve identity, partition, topology, Ghost, and visibility boundaries but are disabled today. See [cross-server play](design/cross-server-scenes.md), [updates](design/gateway-topology-survey.md), and root todo.md.
