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

Apollo's target model uses familiar responsibility names: `Login`, `Gateway`, `World`, and `Instance`. These are architecture concepts; they do not imply that the repository already contains complete deployable processes with those names. The repository has several prototype executables. Their names appear here only to describe implementation status, not as a second target topology.

| Current executable | Current role | Relation to target model |
|---|---|---|
| `login-app` | Login service prototype | Corresponds to `Login` |
| `gateway-app` | Client ingress and message-routing prototype | Corresponds to `Gateway` |
| `world-app` | Scene/match runtime prototype using the target World name | World, Scene, and the Instance state machine exist; group creation, completion, and resource cleanup are not integrated end to end |
| `base-app` | Player state, session, and persistence prototype | These responsibilities still need to converge on the instance model; this is not a required standalone target process |
| `baseappmgr` | Player directory and placement prototype | The target architecture does not require a standalone process with this name |
| `machined` | Local process-supervision prototype | Optional for deployment; carries no game business logic |
| `game-server` | Single-process assembly example | Demonstrates module assembly, not a complete production topology |
| `logger`, `admin`, `fleetctl` | Operations and management prototypes | Outside the instance gameplay model |

The current `base-app` owns PlayerAnchor and persistence responsibilities; `baseappmgr` contains directory and placement prototypes. These existing responsibilities need an explicit consolidation plan and tests. Renaming the game-logic process does not move persistent writes into Login or add another target process. Persistent player data and the single authoritative in-match entity have distinct responsibilities.

An instance is one match; a `World` process hosts instances, and one process may host many instances. Each player has one authoritative runtime object during a match. Cross-server play transfers its authority and updates Gateway routing; it does not create a second authoritative object that can run concurrently.

BigWorld/KBEngine names such as `BaseApp`, `CellApp`, and `BaseAppMgr` appear only in engine comparison material. They are not Apollo target process names. See the [Architecture Overview](../guide/architecture.md).
