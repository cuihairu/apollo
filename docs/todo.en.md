[English](todo.en.md) | [中文](todo.md)

# TODO

## P0: Unify target terminology and design baseline

- [ ] Review developer-facing design documents and navigation repo-wide. Use `Login`, `Gateway`, and `RoomServer` for target process roles; use `Room` for a match; give each player one authoritative object in a match. BigWorld/KBEngine terms such as Base/Cell/Zone describe those external engines only and are not Apollo target names.
  - Acceptance: README, docs home, architecture overview, glossary, login, routing, player-object, and Gateway docs agree; other design pages no longer describe the old process topology as Apollo's current/target architecture; engine comparison pages distinguish external facts from Apollo design.
- [ ] Check old document paths, navigation, scripts, and references. Remove superseded architecture drafts that have no independent reference value; do not retain conflicting historical naming inventories.
  - Acceptance: docs build passes; internal links resolve; old executable names that remain in docs accurately describe current source implementation only and are not presented as target concepts.
- [ ] Decide and record update acceptance goals: continuous service ingress, no client re-login, uninterrupted existing TCP connections, and active Rooms continuing across versions are separate outcomes.
  - Acceptance: for each goal, specify whether it is required, client behavior, update deadline, and failure rollback conditions.

## P1: Converge applications and implementation names

- [ ] Audit `apps/` against the target model and define Login, Gateway, and RoomServer implementation boundaries and deployment co-location; remove process splits without a target responsibility.
- [ ] Converge the existing `cell-app` scene/match prototype on the RoomServer role. Do not implement Cell spatial partitioning, seamless maps, or the ghost model.
- [ ] Consolidate needed player-data, online-routing, and supervision functions from `base-app` and `baseappmgr`; do not add a standalone Manager process by default.
- [ ] Align executable names, directories, class names, namespaces, config, protocol identities, log/metric identities, launch scripts, tests, and docs wherever needed.
  - Acceptance: source, build artifacts, development scripts, and user documentation use the same target names; no dead routes/config or empty shells remain solely for the obsolete topology.

## P2: Implement room runtime and player-authority transfer

- [ ] Implement Room lifecycle: a RoomServer can host multiple independent Rooms with clear create, join, run, settle, destroy, and failure paths.
- [ ] Ensure each player has one authoritative runtime object in a Room; define the boundary between persistent player data and temporary match state.
- [ ] Implement player-object transfer between RoomServers: source freeze/export, destination restore/accept, online-route and Gateway switch, then source cleanup; failures roll back to one authority.
- [ ] Add idempotency/deduplication for transfer requests, settlement, and persistence writes; reject late source messages and dual-authority writes.
  - Acceptance: automated tests cover normal transfer, destination rejection, timeout, source/destination crash, route-update failure, duplicate requests, and late messages; at most one endpoint can advance authoritative state at any time.

## P3: Complete Gateway routing and update behavior

- [ ] Connect real Gateway upstream/downstream traffic to multiple RoomServers using online routes; support unique Gateway instance identities and route updates.
- [ ] Implement Gateway draining: stop accepting new connections while existing connections continue until natural disconnect or a defined deadline.
- [ ] Implement connection recovery or cross-process connection handoff as required by the P0 update goals. If the goal is service availability only, specify client reconnect behavior and session-recovery constraints.
- [ ] Implement RoomServer rolling updates: stop assigning new Rooms and exit after active Rooms finish. If active Rooms must continue across versions, implement Room-state transfer, version compatibility, and rollback.
  - Acceptance: run upgrade drills with real clients and multiple Gateways/RoomServers. Record new-connection availability, existing TCP connections, re-login counts, active Room continuity, and message/settlement correctness separately; Ping or process liveness alone is not sufficient.

## P4: Deployment and failure acceptance

- [ ] Define single/multi-machine topologies, startup dependencies, health checks, overload rejection, and graceful shutdown for Login, Gateway, and RoomServer.
- [ ] Decide the actual owner, storage, and recovery method for online routing data. Fit the target scale without introducing an unnecessary standalone process.
- [ ] Establish repeatable automated or operational drills for cross-server transfer, Gateway draining, RoomServer draining, and update rollback.
  - Acceptance: after forced process termination, network interruption, or update timeout, players never have split authority or unrecoverable state; availability and client-connection guarantees each have observable evidence.
