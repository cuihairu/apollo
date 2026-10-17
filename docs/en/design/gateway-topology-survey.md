# Gateway Responsibilities and Update Boundaries

This document defines the Gateway role in Apollo’s room-based architecture. See the [architecture overview](../../guide/architecture) for the target topology.

## Gateway responsibilities

1. Accept client connections and manage their lifecycle.
2. Validate login tickets, associate sessions, and apply connection-level rate limits.
3. Route client messages to the player’s current RoomServer and return responses.
4. Report disconnects and support session recovery over a new connection.

A Gateway does not own match objects or simulate rooms. One RoomServer can host multiple Rooms. The Gateway forwards messages using the player’s current route.

## Multiple Gateways

Multiple Gateway instances can share new connections and network load. Each existing TCP socket remains owned by the process that accepted it. Changing the RoomServer route changes where later messages go; it does not move the socket. Every instance needs a unique identity in service discovery and routing.

## Two update modes

Apollo distinguishes protocol/contract updates from code-only updates. Both may use rolling deployment, but compatibility and draining determine whether existing matches and connections remain uninterrupted.

| Mode | Change | Rollout | Impact and boundary |
|---|---|---|---|
| Protocol update | Change XML/XSD under `sdks/contract/` and regenerate protocol artifacts; server and client SDKs usually change too | Deploy a server that accepts old and new contracts, then publish client/protocol artifacts; remove old support after the compatibility window | Contract sources, generator, and golden artifacts exist. Dual-version negotiation, an N/N-1 compatibility window, and end-to-end rolling validation are incomplete. This is a designed rollout mode, not a proven live protocol hot update. Breaking changes require client upgrades or a maintenance window. |
| Code-only update | Keep the contract, message IDs, field types, and wire format unchanged; update Lua gameplay modules or roll C++ binaries | Lua modules can be prepared, committed at a RoomServer tick boundary, observed, and rolled back. C++ changes require draining or migrating rooms before process replacement. | `ScriptRuntime` has module preparation, tick-boundary commit, observation, and rollback primitives. A control plane that reliably distributes updates to every process is incomplete. C++ cannot be loaded in-process. |

Neither mode makes TCP sockets survive a Gateway process replacement automatically. During a rolling update, the old process can keep serving existing sockets while the new instance accepts new ones. Cross-process socket handoff requires a separate implementation. Reconnect without another login also requires a session recovery protocol.

## Current implementation status

The repository has a `gateway-app` prototype, but no validated multi-instance production topology or cross-process TCP handoff. End-to-end route switching and RoomServer authority transfer are also not validated. Process count, successful startup, or reliable inter-process transport alone do not prove seamless client updates. Lua module replacement primitives and production rollout orchestration are separate layers: the former has runtime foundations; the latter remains incomplete.
