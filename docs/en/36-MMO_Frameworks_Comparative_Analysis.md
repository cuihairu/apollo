
# 36. Common Names in Multiplayer Game Servers

This document explains names used by other game server projects and the responsibilities they carry. “Similar” means overlapping responsibilities, not equivalent implementations. See [Concepts and Names](design/concept-glossary.md) for Apollo's target terms and [Architecture Overview](guide/architecture.md) for its target topology.

## Project terminology

### KBEngine

| Name | Responsibility |
|---|---|
| `loginapp` | Login entry and authentication-flow coordination. |
| `baseapp` | Base entities and client proxies; hosts non-spatial entities. |
| `cellapp` | Entities and spatial logic inside a Space. |
| `baseappmgr` | BaseApp selection, load information, and login allocation. |
| `cellappmgr` | CellApp selection and Space/Cell creation and recovery coordination. |
| `dbmgr` | Database access and entity-persistence coordination. |
| `machine` | Machine discovery and process-management support. |

Base/Cell are part of KBEngine's entity and spatial model. Apollo does not use these words for its target processes.

### BigWorld

| Name | Responsibility |
|---|---|
| `loginapp` | Login entry. |
| `baseapp` | Base entities, client proxies, and non-spatial player state. |
| `cellapp` | Cell entities and Space simulation. |
| `baseappmgr` | BaseApp allocation and load management. |
| `cellappmgr` | CellApp/Space management and recovery coordination. |
| `dbapp` / `dbappmgr` | Database task processing and management. |
| `bwmachined` | Machine discovery, monitoring, and process-management support. |

BigWorld's Cell/ghost mechanisms support continuous space across Cells; these names are used here only as BigWorld terminology.

### TrinityCore / AzerothCore

| Name | Responsibility |
|---|---|
| `authserver` | Account authentication and Realm entry. |
| `worldserver` | Realm runtime, including player connections and much of the game-world logic. |

These names describe public open-source implementations and do not establish the internal topology of commercial MMOs.

### EQEmu

| Name | Responsibility |
|---|---|
| `loginserver` | Login service. |
| `world` | World-level coordination and client/Zone connection management. |
| `zone` | Runs a game area and its related gameplay. |
| `ucs` | Standalone communication service. |

`zone` is EQEmu's name for its area runtime; it is not an Apollo process name.

### Pomelo / LordOfPomelo

| Name | Responsibility |
|---|---|
| `gate` | Initial ingress and Connector selection. |
| `connector` | Maintains client connections and sessions and forwards messages. |
| `area` | Entities and game logic for a map area. |
| `master` | Configures and manages service processes. |

Pomelo's `gate` and `connector` are separate roles. `connector` is closer to the common client-Gateway responsibility.

### Ryzom Core

| Name | Responsibility |
|---|---|
| `FES` (Front End Service) | Client ingress and message front end. |
| `EGS` (Entities Game Service) | Entities and core game logic. |
| `AIS` (AI Service) | AI logic. |
| `MS` (Mirror Service) | Inter-service entity data mirroring. |
| `NS` (Naming Service) | Service discovery/naming. |
| `AES` (Administration Executor Service) | Service start/stop and administration. |

These abbreviations belong to Ryzom's service layout and should not be copied into Apollo as extra concepts.

### Skynet

Skynet is a general service runtime and does not prescribe an MMO process topology. Its Gate example handles client connections and message forwarding; names such as Agent and Watchdog come from example service arrangements, with exact roles defined by the application.

### Colyseus and Photon

Colyseus uses `Instance` for a stateful multiplayer session/match hosted by a game-server process. Photon commonly uses `Master Server` for session discovery/allocation and `Game Server` to host instances and matches. In both cases, `Instance` is a gameplay session concept, not a process name.

### Agones

Agones uses `GameServer`, `Fleet`, and `FleetAutoscaler` for game-server instances and orchestration resources on Kubernetes. It addresses deployment and capacity orchestration; it does not define the business model for player objects or instances.

## Apollo's common names

| Apollo target name | Responsibility | Similar concepts elsewhere |
|---|---|---|
| `Login` | Account authentication and login credentials. | `loginapp`, `authserver`, `loginserver` |
| `Gateway` | Client game connections and message routing. | Pomelo `connector`, Ryzom `FES`, Skynet Gate |
| `World` | Hosts multiple instances and runs authoritative match logic. | Colyseus game-server process, Photon `Game Server` |
| `Instance` | One match and its temporary state. | Colyseus/Photon `Instance`, match instance |

Apollo does not use `World`, `ZoneServer`, `AreaServer`, and `SceneServer` as parallel names for the same instance/match runtime. `Scene` may describe a spatial object in code; the public gameplay and hosting units are `Instance` and `World`. One World can host multiple Instances.

## Gateway and updates

`Gateway` is a common ingress responsibility, not a guarantee of zero-downtime updates. Multiple Gateways can accept new players after an old instance stops receiving new connections. Existing TCP connections require connection handoff or the old instance retaining them until drained. Keeping active instances running across versions additionally requires instance-state transfer, version compatibility, authority handoff, route switching, and rollback. These mechanisms must be verified separately.

## Sources

- [KBEngine entity definitions](https://kbengine.github.io/docs/programming/entitydef.html) and [startup/shutdown](https://kbengine.github.io/docs/startup_shutdown.html)
- [TrinityCore networking](https://trinitycore.info/install/Networking)
- [EQEmu server installation](https://github.com/EQEmu/eqemu-docs-v2/blob/main/docs/server/installation/server-installation-windows.md)
- [Pomelo terminology](https://github.com/NetEase/pomelo/wiki/Terminologies) and [LordOfPomelo server introduction](https://github.com/NetEase/pomelo/wiki/LordOfPomelo-server-introduction)
- [Ryzom Core services](https://github.com/ryzom/ryzomcore/wiki/architecture_ryzom_services)
- [Skynet GateServer](https://github.com/cloudwu/skynet/wiki/GateServer)
- [Colyseus instances](https://docs.colyseus.io/instance/)
- [Agones concepts](https://agones.dev/site/docs/)
