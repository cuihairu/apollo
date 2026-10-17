
# Apollo Concepts and Names

This is the terminology authority for Apollo's target architecture. Public documentation uses familiar game-server concepts. Prototype code names identify implementation status only; they are not a second architecture vocabulary. External engine-specific terms are explained only in [engine comparison material](../36-MMO_Frameworks_Comparative_Analysis.md).

## Product positioning

Apollo is a lightweight room-based multiplayer game server engine for tower defense, co-op, arenas, and dungeons. MMO can describe a product form, but Apollo is not currently designed around a persistent seamless world.

## Target concepts

| Name | Definition | Boundary |
|---|---|---|
| `Login` | Account authentication and issuance of login credentials. | Does not host match state. |
| `Gateway` | Accepts game connections and routes messages by session. | Does not own authoritative game state; the name does not imply lossless connection migration. |
| `RoomServer` | Process role that creates, runs, and destroys rooms and executes their authoritative logic. | One process can host multiple Rooms. |
| `Room` | One gameplay session/match and its temporary state. | Reclaimed after the match; it is not a process. |
| `Player` | Player identity. | Has one authoritative runtime object during a match. |
| `Entity` | Runtime object participating in game simulation. | `Avatar` represents a player as an entity during a match. |
| `AOI` | Interest management that determines which entities a player should observe. | A room simulation capability, not a required standalone process. |
| `Persistence` | Stores data that survives across matches. | A capability/module; no separate service process is implied. |

## Player object transfer

Cross-server play transfers authority for a player object from the source RoomServer to the destination. State can be transmitted, but at any moment only one side may process player requests, advance authoritative state, or perform authoritative writes. After the destination confirms ownership, Gateway routing is switched and the source object is removed. Failure handling must avoid both split authority and loss of authority.

## Update semantics

`Gateway` is a connection and routing role, not a zero-downtime update solution. A RoomServer can stop accepting new rooms and roll out after active rooms complete. Continuing active matches across versions requires additional state-transfer and version-compatibility mechanisms; keeping existing connections across Gateway processes requires connection handoff. Service availability, no re-login, uninterrupted TCP, and uninterrupted matches are different outcomes and must not be conflated.

## Repository implementation names

| Code/executable name | Status |
|---|---|
| `login-app` | Login prototype. |
| `gateway-app` | Gateway prototype. |
| `room-server` | RoomServer prototype executable; currently hosts a default Scene and Instance, while full Room lifecycle management remains unimplemented. |
| `base-app` | Player state and persistence prototype; it does not imply a required standalone BaseApp target process. |
| `baseappmgr` | Directory/placement prototype; it does not imply a required BigWorld-style manager process. |
| `machined` | Local process-supervision prototype in the deployment/operations plane. |
| `game-server` | Single-process module-assembly example. |

The target process roles are named `Login`, `Gateway`, and `RoomServer`; `Room` is the gameplay object hosted by a RoomServer. BigWorld/KBEngine names such as `BaseApp`, `CellApp`, and `BaseAppMgr` are external engine terminology and must not be used as synonyms for Apollo target concepts.
