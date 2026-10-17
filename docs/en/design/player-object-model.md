
# Player Object Model

Apollo's target model keeps one authoritative runtime object for a player in a match. See [Concepts and Names](./concept-glossary.md) for target terms. `Player` is the player identity, `Entity` is a runtime object, and `Avatar` represents a player as an entity during a match.

## Ownership

- Before login, the account is authenticated by `Login`; login credentials do not carry the player game object.
- After the player joins a room, the RoomServer creates or restores one player entity and becomes its sole authoritative owner.
- On disconnect, the RoomServer may retain the entity briefly for reconnection according to game rules; the session policy determines whether and how long.
- At match end, commit settlement/persistence data and destroy the player's entity for that match.

The target model does not split one player into a persistent Base object and a scene Cell object. Disconnects, server transfers, or Gateway changes do not create a second authoritative player object.

## Cross-server transfer

When a player needs to join a room on another RoomServer, authority for the same logical object is transferred:

```text
Source RoomServer: freeze and export state
Destination RoomServer: restore and confirm takeover
Route directory/Gateway: switch to destination
Source RoomServer: remove old object
```

Until the destination confirms takeover, the source remains authoritative. After confirmation, the destination becomes the sole authority. A state snapshot may briefly be copied during transfer, but both sides must never accept input, advance simulation, or commit authoritative writes for the same player. On failure, roll back to one source authority or report failure while preserving recoverable state.

## Boundaries with related concepts

- `Session` describes the session associated with a client connection; it is not the player object.
- `Gateway` owns network connections, not simulation authority for players.
- `Room` is a match and its temporary state; RoomServer is the process that hosts it.
- `Scene` may describe map/space data inside a room; it is not another process that hosts player objects.
- `BaseApp`, `RoomServer`, Proxy, and ghost belong to external engine models, not Apollo's target object hierarchy.

## Current implementation

The repository contains existing code components named `PlayerAnchor`, `Avatar`, `Scene`, and `Instance`. They do not imply that the target model keeps two player objects or splits Base/Cell processes. The current prototypes have not completed object-state transfer between RoomServers or end-to-end route switching.
