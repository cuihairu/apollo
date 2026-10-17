---
title: Architecture Overview
icon: layers
order: 7
prev: /guide/configuration
---

# Architecture Overview

Apollo targets lightweight multiplayer games organized around rooms and matches, such as tower defense, co-op, arenas, and dungeons. A room hosts one gameplay session; one `RoomServer` process can host multiple rooms. Apollo is not designed around a persistent seamless world or spatially partitioned processes.

## Target model

```mermaid
flowchart LR
    Client[Client] --> Login[Login<br/>account authentication and login ticket]
    Client <-->|game connection| Gateway[Gateway<br/>connection and message routing]
    Gateway --> RoomServer[RoomServer<br/>room hosting and match simulation]
    RoomServer --> Room[Room<br/>one gameplay session]
    Room --> Player[Player / Entity<br/>single authoritative runtime object]
    Room --> AOI[AOI]
    Room --> Battle[game logic and settlement]
    RoomServer --> Store[player persistence]
```

| Name | Responsibility |
|---|---|
| `Login` | Authenticates accounts and issues credentials to enter the game. |
| `Gateway` | Accepts game connections and routes messages by session. It does not own authoritative match state. |
| `RoomServer` | Creates, runs, and destroys rooms; executes authoritative game logic in each room. One process can host multiple rooms. |
| `Room` | The lifecycle and temporary state of one match. It is reclaimed when the match ends. |
| `Player` / `Entity` | A player's runtime object in the current match. Each player has one authoritative object at a time. |

Cross-server play transfers the player's authoritative object. The source `RoomServer` pauses authoritative processing, the destination receives the state and confirms ownership, Gateway routing is updated, and the source object is then removed. Transfer data may briefly exist at both ends, but only one process may handle requests, advance state, or commit authoritative writes.

## Update boundary

Gateway provides connections and routing; it does not by itself provide zero-downtime updates. A rolling `RoomServer` update can keep the service available by stopping new room assignments, letting existing rooms finish, and then exiting the old process. This does not guarantee uninterrupted TCP connections or preserve active rooms across versions. Continuing active rooms across an update requires room-state transfer, route switching, and rollback to be implemented and verified separately.

## Repository status

The diagram describes the target conceptual model, not the current deployment topology. The repository still contains prototype executables such as `login-app`, `gateway-app`, `base-app`, `room-server`, `baseappmgr`, and `machined`. `room-server` now uses the target responsibility name, though the prototype still lacks a complete multi-Room lifecycle. See [Server Applications](../apps/index.md) for current implementation status. BigWorld/KBEngine terms are retained only as references in [compatibility and engine comparison documentation](/33-BigWorld_Compatibility).
