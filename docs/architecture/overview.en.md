---
title: Architecture Overview
icon: sitemap
order: 1
category:
  - Architecture
tag:
  - Overview
  - Data Flow
  - Responsibility Layers
---

[English](overview.en.md) | [中文](overview.md)

# Architecture Overview

From a client entering a match to the settlement being persisted, requests flow down through seven layers, each layer with one authoritative owner (all mapped to actual repo artifacts, reconciled 2026-10-07):

```text
Client
  ↓
Connection      the access face (gateway ingress)
  ↓
Session         session ownership and routing (BaseAppMgr directory + gateway session)
  ↓
Player          the player's long-term state (PlayerAnchor)
  ↓
Scene           the spatial runtime boundary (Scene + AOI)
  ↓
Instance        the instance lifecycle (eight-state machine)
  ↓
Entity / Battle  entity simulation and battle rulings (Avatar / BattleRuntime)
```

## Responsibilities of Each Layer

### Connection — the access face

The acceptance of client TCP/WebSocket connections and packet dispatch. The actual artifacts are in `apps/gateway-app` (the four `ingress/` pieces: `client_ingress_server` for acceptance, `client_packet_dispatcher` for dispatch, `gateway_connection_registry` for the connection registry, `session_admission_service` for admission). Boundary: it only understands connections and byte streams, not player semantics.

### Session — session ownership and routing

The single adjudication point for "which world is this player in now, and via which gateway". Actual artifacts: `BaseAppMgr` (`apps/baseappmgr`) maintains the global player→SessionBinding / player→WorldAssignment directory and makes placement rulings (`assignWorld` / `clearWorldAssignment`); the gateway side's `session_manager` maintains access sessions and heartbeats. Boundary: directory and rulings; it carries no player data.

### Player — the player's long-term state

The player profile and anchor that persist across instances. Actual artifact `PlayerAnchor` (`modules/game/session`): login activation (`AnchorManager::activate`, idempotent), long-term progress (`progress` accumulation, `mark_dirty` dirty on every change), logout reclamation (`deactivate`). Boundary: the sole write point for the resident state — battle settlement settles here one-way, and instances hold no long-term state.

### Scene — the spatial runtime boundary

AOI ownership belongs to the scene. Actual artifacts `Scene` + `SceneAoi` (`modules/game/world`): 3×3-grid diffing, `ViewerState` per-observer watermarks, the Enter/Sync/Leave event face. Boundary: the spatial visibility authority — who sees whom is the Scene's call; entities on the Scene carry the three responsibilities of movement/combat/AOI broadcast through `Avatar`.

### Instance — the instance lifecycle

A first-class citizen, one instance per match, an eight-state one-way state machine (only adjacent transitions are legal; the terminal state is sealed):

```text
Create → Initialize → Waiting → Running → Finishing → Rewarding → Draining → Destroyed
```

Actual artifact `Instance` (`modules/game/world`): the Waiting window carries the gameplay payload and pre-entry players, `start` validates internally and enters Running, and the settlement tail advances one-way through the last four states. Boundary: the lifecycle and participant collection — the ruling domain's tick space belongs to the Instance, and battle input injection goes through the explicit `BattleInput` face (the two must never be mixed or allowed to race for the beat).

### Entity / Battle — entity simulation and battle rulings

The deterministic battle domain. Actual artifact `BattleRuntime` (`modules/game/battle`): five phases (Created→Entering→Battling→Rewarding→Finished), strictly increasing ticks + ordered inputs, `CombatRoll` substream random numbers (the determinism fingerprint is a hash chain), and settlement summed per-player damage into `IRewardSink`. Entity identity is strongly typed via `EntityId`/`PlayerId`. Boundary: authority inside battle — everything after settlement belongs to the Player layer.

## Cross-Cutting Concerns

- **Persistence**: the Player layer is dirty on every change, and `SaveQueue`/`PersistJournal` write-behind persists it (append→quota drain→snapshot compaction→crash replay), see the [Data Architecture Whitepaper](/18-Data_Architecture_Whitepaper)
- **Reconnect recovery**: the Session-layer reconnect window + the Player anchor's session ownership, see [Process Architecture and Player Lifecycle](/architecture/bigworld-lifecycle)
- **Scene transfer**: entering and leaving a Scene boundary goes through `scene_transfer` (prepare→detach→attach→resume + failure rollback)
- **Fleet and supervision**: process-level fleets belong to machined (UDP broadcast discovery + roster pull-restart), see [Server Applications](/apps/)

## Related Docs

- [AOI 3×3 Grid System In Depth](/architecture/aoi)
- [BigWorld Architecture Deep Dive](/architecture/bigworld)
- [BigWorld Process Architecture and Player Lifecycle](/architecture/bigworld-lifecycle)
