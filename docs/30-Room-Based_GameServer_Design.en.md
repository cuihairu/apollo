[English](30-Room-Based_GameServer_Design.en.md) | [中文](30-Room-Based_GameServer_Design.md)

# Room-Based GameServer Design

This document defines Apollo's target runtime model for tower defense, co-op, arenas, and dungeons. A `Room` is one match and its temporary state; a `RoomServer` is the process that hosts rooms, and one process may run multiple rooms.

## 1. Process and object responsibilities

| Name | Responsibility |
|---|---|
| `Login` | Account authentication and login credentials. |
| `Gateway` | Client connections, session association, and message routing. |
| `RoomServer` | Creates, runs, and destroys rooms and executes authoritative game logic. |
| `Room` | One gameplay session, including scene entities, AOI, combat, and temporary match data. |
| Player persistence | Stores data across matches; provided by persistence modules and not required to be a separate process. |

`Player` is the player identity; `Entity` is a runtime gameplay object. After joining a match, the RoomServer hosting that room owns the player's single authoritative runtime object. At match end, settlement is saved and the temporary match object is destroyed according to game rules.

## 2. Runtime flow

```text
Client → Login (authentication)
Client → Gateway (game connection) → RoomServer → Room
Room: entities / AOI / game logic / settlement
```

Matchmaking or room creation selects a target RoomServer and Room. Gateway routes requests by session. When the room ends, it commits settlement results and releases temporary state.

## 3. Cross-server transfer

Cross-server play transfers authority for a player object from the source RoomServer to the destination: the source freezes the object and produces transfer state; the destination restores it and confirms ownership; the route directory and Gateway switch to the destination; then the source deletes its object. On failure, the source must retain or recover authority so two processes never accept operations for the same player. The transfer mechanism must deduplicate requests and define which business writes cannot be replayed.

“Copying the object” means transferring its state, not allowing two runnable player objects. A transfer snapshot may briefly exist at both ends, but only one process may execute authoritative operations at a time.

## 4. Scaling and updates

Scale capacity by adding RoomServer instances and assigning new rooms to available processes. There is no need to split one map or world across spatial processes.

A RoomServer can roll by stopping new room assignments, waiting for active rooms to finish, and then exiting. Keeping matches running through an update requires separate implementation and end-to-end validation of room-state transfer, version compatibility, route switching, and rollback. Multiple Gateway instances can continue accepting new connections; preserving existing TCP connections across Gateway processes requires a separate connection handoff mechanism and cannot be inferred from the presence of Gateway.

## 5. Current implementation boundary

This is the target model. The repository still has prototype executables such as `base-app`, `cell-app`, and `baseappmgr`; their names and current split do not define target concepts. See [Server Applications](apps/index.en.md) for implementation status and [Architecture Overview](guide/architecture.en.md) for the target model. BigWorld/KBEngine Base/Cell process names appear only in comparison material as external engine terminology.
