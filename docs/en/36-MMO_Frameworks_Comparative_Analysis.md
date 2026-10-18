
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

Base/Cell belong to KBEngine's entity/spatial model. Apollo keeps profiles in Base, executor management in worldServer, and simulation in cell. Reuse Space/Cell semantics, initially one Cell per Space, reserving future seamless capabilities.

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

BigWorld's Cell/ghost mechanisms support continuous space across Cells; see the [Apollo mapping](design/bigworld-apollo-mapping.md) for management/execution/spatial correspondence, without claiming full implementation.

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

Colyseus uses the actual `Room` type for a stateful client group; see [official Rooms documentation](https://docs.colyseus.io/room). Photon Master Server and Game Server are service roles. External session names do not change Apollo Instance terminology.

### Agones

Agones uses `GameServer`, `Fleet`, and `FleetAutoscaler` for game-server instances and orchestration resources on Kubernetes. It addresses deployment and capacity orchestration; it does not define the business model for player objects or instances.

## Apollo names

| Target name | Responsibility | External responsibility reference |
|---|---|---|
| loginServer | Authentication, credentials, directories | loginapp/authserver authentication boundaries |
| gateway | Client connections and forwarding | Pomelo connector, Ryzom FES, Skynet Gate |
| baseServer | Persistent-player objects and base gameplay | Non-spatial BaseApp behavior; not a database process |
| worldServer | Executor registration, Space/Cell placement and topology | Relevant CellAppMgr management duties |
| cell executor | Partition simulation, entities, AOI and ticks | CellApp; one process may host several Cells |
| database | Profile loading/saving and storage access | dbmgr/dbapp storage responsibilities |
| machined | Local process lifecycle/supervision | Local process management, not player ownership |
| Space (existing Scene semantics) / Cell / Instance | Runtime space / partition / temporary gameplay | Reuse BigWorld Space/Cell; Instance is Apollo gameplay |

Cross-server play retains Base/storage affiliation; cell associates each player with its Base. See [concepts](design/concept-glossary.md). Similar responsibilities do not imply identical engine topologies.

## Gateway and updates

`Gateway` is a common ingress responsibility, not a guarantee of zero-downtime updates. Multiple Gateways can accept new players after an old instance stops receiving new connections. Existing TCP connections require connection handoff or the old instance retaining them until drained. Keeping active instances running across versions additionally requires instance-state transfer, version compatibility, authority handoff, route switching, and rollback. These mechanisms must be verified separately.

## Sources

- [KBEngine entity definitions](https://kbengine.github.io/docs/programming/entitydef.html) and [startup/shutdown](https://kbengine.github.io/docs/startup_shutdown.html)
- [TrinityCore networking](https://trinitycore.info/install/Networking)
- [EQEmu server installation](https://github.com/EQEmu/eqemu-docs-v2/blob/main/docs/server/installation/server-installation-windows.md)
- [Pomelo terminology](https://github.com/NetEase/pomelo/wiki/Terminologies) and [LordOfPomelo server introduction](https://github.com/NetEase/pomelo/wiki/LordOfPomelo-server-introduction)
- [Ryzom Core services](https://github.com/ryzom/ryzomcore/wiki/architecture_ryzom_services)
- [Skynet GateServer](https://github.com/cloudwu/skynet/wiki/GateServer)
- [Colyseus Rooms](https://docs.colyseus.io/room)
- [Agones concepts](https://agones.dev/site/docs/)
