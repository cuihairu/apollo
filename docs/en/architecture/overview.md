# Architecture Overview

Keep gateway, loginServer, baseServer, worldServer, database, and machined. Use the discussed cell executor: worldServer overlaps CellAppMgr management and cell overlaps CellApp execution. Base owns persistent objects and saves through database.

Space corresponds to existing Scene spatial semantics, Cell is a partition, and Instance is a temporary gameplay lifecycle. Initially one executor and one Cell per Space; reserve multiple Cells per Space. Seamless splitting, Ghosts, and cross-boundary combat remain unimplemented.

Executors register with World, which manages topology/migration. loginServer hosts service/online location projections; gateway forwards directly to Base/cell. machined supervises locally and is optional beside independently deployed World/cell. Cross-server play retains home Base/storage affiliation without moving complete profiles.

Protocol replacement separately transfers Base objects, cell simulation, and World management state before switching authority/routes. Code-only updates preserve wire and may update Lua; C++ uses replacement processes. Validate TCP, sessions, and complete activity continuity separately.

## Temporary instance lifecycle

Group request → Base validation → World Space/Cell placement → cell creates/runs Instance → original Base/database durable settlement → departure/cleanup. Reuse eight states; persistent Spaces need no temporary Instance.

## Reading

- [BigWorld–Apollo mapping](../design/bigworld-apollo-mapping.md): original terms, roles, code types, and gaps.
- [Architecture](../guide/architecture.md), [player objects](../design/player-object-model.md), [cross-server play](../design/cross-server-scenes.md), and [updates](../design/gateway-topology-survey.md).
- [Applications](../apps/index.md): current WorldApp combines management/execution; Base/database prototypes exist; cell entry and full wiring remain pending. Track implementation in root todo.md.

Preserve external-engine source names without treating them as another Apollo topology.
