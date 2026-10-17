# Current Architecture Decisions

Only current rules are kept. See [concepts and names](../design/concept-glossary.md) and the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md) for implementation work.

## ADR-001：Persistent player data and match authority

Existing PlayerAnchor and persistence paths own persistent player data; settlement uses IRewardSink / AnchorRewardSink. Scene advances the match entity. Only one entity per player may accept input and authoritative simulation writes at a time. Persistent data is not another executable match object.

## ADR-002：Scene as an internal simulation boundary

Scene owns entities, AOI, entry/exit, and ticks inside World; it is not a process name. World manages multiple Scenes and Instances. Seamless-world partitions, Ghost mirrors, and continuous cross-process roaming are outside the architecture.

## ADR-003：Instance as the temporary gameplay lifecycle

A group requests an activity; World creates an Instance and associated Scene. Players enter, clear objectives, settle, leave, and release the instance, temporary scene, and resources. Reuse Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed. The state machine exists; group-triggered creation and completion cleanup still need application integration.

## ADR-004：Explicit scene and cross-World transfer

Reuse SceneTransfer admission, detach, attach, and rollback for in-process transfers. Cross-World transfer separately requires source freeze, state handoff, target confirmation, routing, cleanup, and idempotency. An in-process transfer does not establish cross-process takeover.

## ADR-005：Scene owns AOI

Scene owns and advances SceneAoi on its authoritative thread. Visibility is isolated per simulation unit; AOI does not require another business process.

## ADR-006：Battle runtime and settlement

BattleRuntime is the deterministic combat module. Define randomness, input order, phases, and settlement boundaries. Instance owns its gameplay workload; rewards flow through the existing one-way sink into persistent player data. Retries must not duplicate rewards.

## ADR-007：Persistence status and boundary

Current profiles use local JSON files, SaveQueue, and PersistJournal. Real MySQL/Redis business-runtime integration remains deferred until concrete deployment, capacity, or user requirements arise. A declared dependency is not a completed integration.

## ADR-008：Recovery scope

Profile/journal replay and process restart are existing primitives, not recovery of active instance simulation. Reconfirm authority, routing, and resumable gameplay phases after recovery. Without a validated instance recovery path, define affected matches as failed.

## ADR-009：Simulation ownership and threads

The owning Scene logical executor modifies simulation state sequentially. Background work, networking, and debug requests use queue handoff; IO threads must not directly modify entities, AOI, or gameplay state. The current application pumps and advances World in one gameLoop.

## ADR-010：Restart supervision first; standby deferred

machined supplies process supervision, death detection, and restart. Cross-machine standby, arbitration, and automatic takeover are outside current delivery. Reopen them only for explicit deployment or recovery requirements; restart is not standby availability.

## ADR-011：Gateway links and update acceptance

Gateway owns connections, sessions, and routing. Its M1 egress reaches World; complete gameplay delivery, multiple-World selection, and cross-Gateway session recovery still require validation. Validate protocol and code-only updates separately. Availability, no re-login, uninterrupted TCP, and uninterrupted instances are different outcomes. Replicas do not transfer sockets automatically.

## ADR-012：On-demand instances and optional isolation

Create Instances for groups inside World. Existing offload modules and in-memory transport stubs can support optional isolation work. If one process per instance becomes necessary, retain the World role and implement spawn, readiness, admission, exit, and crash-failure protocols. Production auto-spawn is not complete.

## ADR-013：Logging and deferred capabilities

logger supports local structured-log queries and M1 collection; applications can configure log push. Collection failure must not block simulation. Combat verification waits for concrete gameplay work; Kafka/Grafana infrastructure remains deferred. These are open future capabilities, not delivered features.

## ADR-014：Process naming and gameplay objects

World is the sole game-logic process role; its executable is world-app, entry type WorldApp, and namespace world_app. Login and Gateway retain their responsibilities. Instance is a temporary gameplay instance; Scene is the internal simulation unit. Migrate existing base-app/baseappmgr capabilities by authority and routing responsibilities; renaming alone does not complete consolidation. External references retain actual upstream identifiers and source paths.

## ADR-015：Asynchronous intent admission

Network callbacks submit intents to the bounded queue. WorldApp::gameLoop consumes them at fixed execution boundaries and advances World. Enqueue success is not execution completion; do not block IO workers waiting for a tick.

## ADR-016：Watermarks, overload, and shutdown

Retain existing capacity, warning thresholds, tick budget, and explicit rejection. Never silently discard accepted work. Close admission, sweep queued work, and stop simulation and links. Production drain also needs settlement, resource cleanup, deadlines, and forced-stop results.

## ADR-017：Reply semantics

Return correlated M1 intent replies after simulation-thread consumption; admission failure may reply immediately. Associate replies with request_id and distinguish delivery, admission, and execution. Consumption acknowledgement does not replace gameplay validation, idempotent settlement, or persistence confirmation.
