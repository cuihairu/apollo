---
title: Current Architecture Decisions
icon: gavel
order: 2
category:
  - Architecture
tag:
  - Decisions
---

# Current Architecture Decisions

This document records only rules that remain in effect. It does not retain withdrawn decisions, obsolete naming discussions, or batch history. Target processes and implementation status are defined by the [architecture status](../analysis/architecture-review.md) and the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).

## ADR-001: Player Authority Within a Room

A player has one authoritative gameplay runtime object in a match, owned by the RoomServer hosting that Room. Moving between RoomServers requires a confirmed authority handoff. Persistent player profile data is data, not a second runnable match object.

## ADR-003: Room Is the Match Lifecycle Boundary

A Room represents one gameplay instance and its temporary state, with creation, preparation, runtime, settlement, and destruction phases. A RoomServer is a process and may host multiple Rooms. Existing `Scene` and `Instance` components may support implementation but are not automatically a complete Room.

## ADR-005: AOI Belongs to Room Simulation

AOI determines which entities are visible within a Room and runs with that Room's authoritative simulation state. AOI is not a separate process and does not require cross-process spatial partitioning.

## ADR-006: Battle Simulation Is Authoritative and Verifiable

The server advances battle state. If deterministic replay or verification is used, input ordering, random numbers, and settlement boundaries must be explicit. The current `BattleRuntime` is a module capability, not evidence that full gameplay content has shipped.

## ADR-007: Persistence Is a Capability Boundary

Cross-match data is stored by persistence capabilities; a separate business process is not required. The current player profile path uses JSON files and `PersistJournal`; external databases and Redis are not connected to the service runtime path.

## ADR-010: Validate Update Promises Separately

Multiple instances and draining can keep new connections available. They do not preserve existing TCP connections across Gateway processes or keep active Rooms running across versions. Those outcomes require connection/session handoff and Room-state migration respectively, and must be verified separately. Service availability, avoiding a new login, uninterrupted connections, and uninterrupted matches are distinct promises.

## ADR-012: Room Failures Have an Explicit Boundary

If a RoomServer process crashes, affected Rooms can recover only through an explicit recovery mechanism; their in-memory state does not implicitly exist in another process. Without a verified migration or recovery path, the match must be treated as failed and settled or restarted according to game rules.

## ADR-013: Log Collection Does Not Own Game Authority

The log collector receives and archives observability data; it does not make authoritative decisions about players, sessions, Rooms, or routes. If collection is unavailable, business services should degrade according to configuration instead of blocking simulation threads.

## ADR-015: Simulation State Has a Single Writer

Network and background threads submit intents to simulation workers. One logical executor applies mutations to each Room in order. IO callbacks must not concurrently mutate entities, scenes, or AOI state.

## ADR-016: Overload Is Explicitly Rejected

Acceptance queues are bounded. When a queue is full, the service explicitly rejects new work and exposes metrics and an error result; it does not silently discard previously accepted work.

## ADR-017: Acknowledgement and Execution Semantics Are Separate

Acceptance means only that a request entered the processing flow. If the protocol promises an execution result, the response must be sent after business processing completes. Each message family must distinguish transport acknowledgement, acceptance, and business result.

## Target Process Names

Apollo's target architecture consistently uses `Login`, `Gateway`, `RoomServer`, and `Room`. BigWorld/KBEngine terms are reserved for describing those external engines. See [server applications](../apps/index.md) for current executable names and their mapping.
