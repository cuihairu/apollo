---
title: Server Applications
icon: server
order: 5
category:
  - Applications
tag:
  - Server
---

# Server Applications

Apollo's target model uses familiar responsibility names: `Login`, `Gateway`, `RoomServer`, and `Room`. These are architecture concepts; they do not imply that the repository already contains complete deployable processes with those names. The repository has several prototype executables. Their names appear here only to describe implementation status, not as a second target topology.

| Current executable | Current role | Relation to target model |
|---|---|---|
| `login-app` | Login service prototype | Corresponds to `Login` |
| `gateway-app` | Client ingress and message-routing prototype | Corresponds to `Gateway` |
| `room-server` | Scene/match runtime prototype using the target RoomServer name | It currently creates a default Scene/Instance; full Room lifecycle management remains unimplemented |
| `base-app` | Player state, session, and persistence prototype | These responsibilities still need to converge on the room model; this is not a required standalone target process |
| `baseappmgr` | Player directory and placement prototype | The target architecture does not require a standalone process with this name |
| `machined` | Local process-supervision prototype | Optional for deployment; carries no game business logic |
| `game-server` | Single-process assembly example | Demonstrates module assembly, not a complete production topology |
| `logger`, `admin`, `fleetctl` | Operations and management prototypes | Outside the room gameplay model |

The responsibilities in `base-app` and `baseappmgr` map to existing target roles rather than new process names: account authentication and login credentials belong to `Login`; client connections and current-match routing belong to `Gateway`; the owning `Room` holds the authoritative in-match player object; and `Login`-side data access handles player data between matches. The online directory is routing state, not a separate `BaseAppMgr` target process. This is the target responsibility map; the current prototypes have not all migrated yet.

A room is one match; a `RoomServer` process hosts rooms, and one process may host many rooms. Each player has one authoritative runtime object during a match. Cross-server play transfers its authority and updates Gateway routing; it does not create a second authoritative object that can run concurrently.

BigWorld/KBEngine names such as `BaseApp`, `CellApp`, and `BaseAppMgr` appear only in engine comparison material. They are not Apollo target process names. See the [Architecture Overview](../guide/architecture.md).
