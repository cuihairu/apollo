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
| `login-app` | Authentication, online directory, and placement control | Hosts `OnlineDirectory`; does not persist player gameplay profiles |
| `gateway-app` | Client ingress and message-routing prototype | Corresponds to `Gateway` |
| `world-app` | Simulation runtime and player profile host | Hosts `PlayerStore` and reuses World, Scene, and Instance; group Instance creation, gameplay, settlement, and cleanup are integrated |
| `machined` | Local process-supervision prototype | Optional for deployment; carries no game business logic |
| `game-server` | Single-process assembly example | Demonstrates module assembly, not a complete production topology |
| `logger`, `admin`, `fleetctl` | Operations and management prototypes | Outside the instance gameplay model |

`apps/common` is a shared application library, not another process. World hosts `player_store::PlayerStore`, which owns PlayerAnchor, local profiles, SaveQueue, and PersistJournal. Login hosts `online_directory::OnlineDirectory`, which owns online routing, placement projections, and migration coordination. Gateway holds client connections and caches routing. Each hosted service has its own link endpoint and follows its application lifecycle.

HostedServicesTests validates activation, session binding, directory lookup, and profile retention over real TCP. SquadWireE2eTests covers Instance inputs and replies. MigrationChainTests covers freeze, export, destination adoption, and directory switching; source-object deletion, complete failure recovery, and cross-Gateway client recovery remain incomplete.

An instance is one match; a `World` process hosts instances, and one process may host many instances. Each player has one authoritative runtime object during a match. Cross-server play transfers its authority and updates Gateway routing; it does not create a second authoritative object that can run concurrently.

BigWorld/KBEngine names such as `BaseApp`, `CellApp`, and `BaseAppMgr` appear only in engine comparison material. They are not Apollo target process names. See the [Architecture Overview](../guide/architecture.md).
