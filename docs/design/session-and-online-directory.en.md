[English](session-and-online-directory.en.md) | [中文](session-and-online-directory.md)

# Sessions and Online Routing

This document defines session and player-location data for the room-based architecture. The [Architecture Overview](../guide/architecture.en.md) and [Concepts and Names](./concept-glossary.en.md) define target topology and terminology.

## Responsibilities

- `Gateway` maintains client connections and sessions and sends player messages to the current RoomServer.
- `RoomServer` is the authoritative owner of a player's object in the current room.
- Online routing answers “which Gateway, RoomServer, and Room currently host this player,” for forwarding and cross-server transfer.
- `Login` authenticates accounts and issues credentials; it does not host active matches.

Online routing is a data responsibility; it does not imply a standalone `Manager` process. The route-table owner, storage, and high-availability protocol must be decided before cluster wiring. The old `baseappmgr` prototype does not determine the target topology.

## Route record

A record needs to associate `player_id` with the current `session_id`, `gateway_id`, `room_server_id`, and `room_id`. Re-login or connection recovery uses a session generation/version to reject late messages from an old connection. Remove the record when the player goes offline or the match ends.

The directory stores routing state only; it is not a second authoritative copy of player game data. The current RoomServer owns the player object, while persistence stores data that spans matches.

## Cross-server transfer

Cross-server transfer is one authority handoff:

1. The source RoomServer pauses authoritative processing for the player object and produces transfer state.
2. The destination RoomServer restores the state and confirms ownership.
3. Update online routing and the Gateway forwarding target.
4. The source removes the old object and confirms completion.

Transfer state may briefly exist at both ends, but the source must remain frozen and must not process player input or commit authoritative writes. If the destination rejects or times out, restore source authority; a failed route update must never expose both endpoints as active.

Give each transfer request a unique identifier so the destination can deduplicate it. Business writes after transfer still need their own idempotency rules. Reliable transport does not mean exactly-once business execution.

## Gateway boundary

Gateway forwards messages using online routing. Multiple Gateways can serve different client connections; each connection remains owned by the Gateway that accepted it. During a Gateway update, an old instance can stop accepting new connections and keep serving existing ones until drained. Moving an existing TCP connection to another Gateway requires a dedicated connection-handoff mechanism; changing the route table does not move a socket.

## Implementation status

The repository contains prototype code for `SessionLocator`, `PlayerDirectory`, `WorldAssignment`, and `baseappmgr`. These are implementation components; they do not mean the target model has selected a directory process or storage design. Cross-process message routing, Gateway takeover, and end-to-end transfer still need validation against this target.
