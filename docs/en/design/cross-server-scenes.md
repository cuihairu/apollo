# Cross-Server Scene Reservation

Reuse worldServer management and cell execution without another process role. Space corresponds to existing Scene semantics; temporary activities use Instance. See the [mapping](./bigworld-apollo-mapping.md). Preserve home Base, server, and database affiliation.

## Registration and independent deployment

Base/World publish identities, endpoints, generations, compatibility, and readiness to loginServer directories. Executors register load/capabilities with their worldServer. World decides Space/Cell placement and publishes location projections; directories do not independently assign partitions.

machined supervises local processes only. Independent World/cell deployment requires neither local machined nor home database credentials. Cross-server pools configure admission scope while retaining the same roles.

## Admission contract

| Data | Purpose |
|---|---|
| Player, home server, Base, ownership version | Retain profile authority with server-validated origin. |
| Source/target World, Space, Cell, executor, optional Instance | Separate management domain, space, partition, and hosting process. |
| Process/simulation generations and topology version | Fence stale executors/placements independently of Base writes. |
| gateway/session and session/route versions | Preserve connection and update simulation forwarding. |
| Transfer ID, expiring admission, compatible versions | Bind destination, members, capacity, and handoff; reject replay. |
| Gameplay snapshot, runtime state, state version | Supply needed gameplay data without a complete profile. |
| Stable settlement ID and return placement | Deduplicate outcomes and return safely to the home server. |

These are target requirements, not completed WorldAssignment/wire fields. Resolve Base endpoints from cached directory entries; neither IP nor storage location defines player affiliation. home_zone_id is not a complete home-server identity.

## Authority handoff

1. Original Base validates admission; World selects the executor and records return placement.
2. Source cell freezes at a simulation boundary, reconciles or records in-flight work, and exports runtime state. Original Base keeps profiles.
3. Target cell validates admission, Base, state, and generations, then restores without activation.
4. World conditionally commits against expected source/authority. Cross-World transfer requires coordination with one recoverable commit decision. Target activates from commit evidence, directories project the result, and gateway refreshes.
5. Source deletes old entities/resources and confirms completion. Fence late source inputs, settlement, and writes by generation.

Preparation is not activation. Confirm no commit before canceling; reconcile ambiguous outcomes from durable records, never reactivate both ends. Complete Instance continuity includes members, enemies, objectives, clocks, randomness, and necessary script state.

## Relationship to seamless handoff

Cross-server entry into another Space changes spaces explicitly. Future movement between Cells in the same Space changes simulation ownership while preserving Space/entity IDs, coordinates, health, cooldowns, clocks, script state, and continuous visibility. Current SceneTransfer recreates Avatar after departure and cannot implement this path.

Initially use one executor and one Cell per Space. Spatial splitting, Ghost replication, and cross-boundary combat remain deferred. Reserve multiple Cells per Space, adjacency, topology versions, and authority handoff within the same architecture.

## Settlement and failure

cell submits rewards, consumption, and quest outcomes to each original Base using stable activity/settlement IDs and simulation generations. Base validates; database durably commits changes with deduplication credentials. Transport ACK is not persistence. Query or retry the same transaction after ambiguity; never create a new ID to repeat it.

Bases confirm independently, without a cross-Base atomicity claim. Define partial success, compensation, unavailability, and durable pending evidence; never delete the sole recovery record early. Return to the home server or a safe Space if the original is unavailable. Base upgrades change routing/generations without changing home/storage affiliation.

## Activation acceptance

- Two home servers/Bases share a Space with unchanged ownership; persistent and temporary Spaces are addressable.
- Executors register with World; standalone deployment works without machined; stale generations fail after restart.
- Cover admission, versions, capacity, freeze, prepare, ambiguous commit, restart, and cleanup over real links.
- Durable settlement is idempotent per Base; partial success and safe return recover.
- Validate backend route switches, sessions, and client TCP separately; implement seamless features later.

Track work in root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
