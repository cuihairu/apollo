# Current Architecture Decisions

Keep only current rules. See the [mapping](../design/bigworld-apollo-mapping.md) for terms and implementation gaps, and root todo.md for tasks.

## ADR-001：Persistent and simulation authority

Base owns `base::Avatar` and persistent gameplay; cell owns authoritative `cell::Avatar`/Entity simulation; database accesses storage. Each domain has one writer. Cross-server play and Instance destruction do not move profiles.

## ADR-002：Space, Cell, and existing Scene

Space is an independent runtime space corresponding to Scene semantics, without a synonymous container chain. Cell is a partition; cell is an executor. Initially one process and one Cell per Space; reserve multiple Cells per Space. Current Scene remains local.

## ADR-003：Instance lifecycle

Base validates groups, World places space/executors, and cell runs Instance and submits settlement to owning Bases for durable database confirmation. Reuse eight states. Instance is neither space nor process; shared state has one logical owner.

## ADR-004：Space changes and Cell handoff

Explicit Space changes differ from same-Space Cell handoff. Current SceneTransfer recreates Avatar, without continuous partition transfer. Freeze, prepare, conditionally commit, activate, and clean while retaining Base. Reconcile ambiguity first.

## ADR-005：Local AOI and observer views

SceneAoi/ViewerState are local visibility primitives executed by cell. Neighbor Ghosts, merged visibility, and complete Witness semantics are future work, without an AOI business process.

## ADR-006：Combat and settlement

cell runs BattleRuntime with explicit randomness, ordering, and clocks. Owning Bases validate stable settlement IDs; database commits profiles and deduplication credentials durably. Delivery, admission, and in-memory rewards are not persistence.

## ADR-007：Independent storage

database owns profile operations, routing, and durable results; Base owns business rules. Extract JSON, SaveQueue, and PersistJournal. Executors receive neither full profiles nor home database credentials. External MySQL/Redis remain requirement-driven.

## ADR-008：Recovery scope

Journal replay/restart does not restore active simulation. Recover Base profiles, World topology, and cell simulation separately; reestablish authority/routes and define failure for unvalidated activity recovery.

## ADR-009：Simulation threads

Each entity has one authoritative Cell writer. Logical executors modify sequentially; IO/background workers submit bounded intents. Shared multi-Cell Instance state has one logical owner without shared memory or global lockstep.

## ADR-010：Registration and supervision

Base/World register with loginServer; executors register with their World. World decides topology; directories project location. machined only supervises locally and is optional beside World/cell. Cross-machine standby/arbitration remain deferred.

## ADR-011：Gateway and two update schemes

gateway retains sockets and separate Base/cell routes. Protocol replacement converts state; Base, cell, and World management hand off their own state. Code-only changes preserve wire and may update Lua. Validate TCP, sessions, and complete activity continuity separately.

## ADR-012：Default deployment and scaling

Initially one executor hosts full Cells for multiple Spaces, without one process per instance. More executors register with World; place whole Space/Cells first, enable same-Space partitions later. Spawn/readiness/failure protocols remain work.

## ADR-013：Observability status

Reuse custom logging, MetricRegistry, span/trace, and M1 collection without blocking simulation. Dedicated Profiler and OpenTelemetry SDK/OTLP exporter are not integrated. External observability deployment is requirement-driven.

## ADR-014：Names and correspondence

Executables are gateway, login, base, world, database, and machined; loginServer/baseServer/worldServer in the design docs name the roles of those binaries. Add the discussed cell execution role (world binary with `--role=cell`). World management overlaps CellAppMgr, cell overlaps CellApp, Scene corresponds to Space, and Instance is Apollo gameplay. Preserve external names.

## ADR-015：Asynchronous admission

Current WorldApp::gameLoop pumps/consumes acceptanceQueue_ and advances World. Move that simulation loop into cell, leaving management in World. Admission is not execution; never block IO waiting for ticks.

## ADR-016：Overload and shutdown

Define capacity, thresholds, budgets, and rejection; never silently discard accepted work. Close admission, sweep, settle, release, then stop. Management placement drain differs from executor activity drain.

## ADR-017：Reply semantics

Correlate request_id and distinguish delivery, admission, execution, and persistence. Consumption acknowledgement does not replace validation, idempotent settlement, or durable save confirmation.
