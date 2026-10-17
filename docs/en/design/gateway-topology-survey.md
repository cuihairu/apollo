# Gateway Responsibilities and Update Boundaries

This document defines the Gateway role in Apollo’s instance-based architecture. See the [architecture overview](../guide/architecture) for the target topology.

## Gateway responsibilities

1. Accept client connections and manage their lifecycle.
2. Validate login tickets, associate sessions, and apply connection-level rate limits.
3. Route client messages to the player’s current World and return responses.
4. Report disconnects and support session recovery over a new connection.

A Gateway does not own match objects or simulate instances. One World can host multiple Instances. The Gateway forwards messages using the player’s current route.

## Multiple Gateways

Multiple Gateway instances can share new connections and network load. Each existing TCP socket remains owned by the process that accepted it. Changing the World route changes where later messages go; it does not move the socket. Every instance needs a unique identity in service discovery and routing.

## Two update modes

Apollo distinguishes protocol/contract updates from code-only updates. Both may use rolling deployment, but compatibility and draining determine whether existing matches and connections remain uninterrupted.

| Mode | Change | Rollout | Impact and boundary |
|---|---|---|---|
| Protocol update | Change XML/XSD under `sdks/contract/` and regenerate protocol artifacts; server and client SDKs usually change too | Deploy a server that accepts old and new contracts, then publish client/protocol artifacts; remove old support after the compatibility window | `APOLLO_ROUTE_COMPAT` provides handshake compatibility windows. `drill_proto_update.sh` tests opening the window, rejection outside it, and rollback. A permitted hash does not decode an old payload: field layout, defaults, and business semantics require version-specific validation. A breaking change cannot be fixed by an allowlist. |
| Code-only update | Keep the contract, message IDs, field types, and wire format unchanged; update Lua gameplay modules or roll C++ binaries | Lua modules can be prepared, committed at a World tick boundary, observed, and rolled back. C++ changes require draining or migrating instances before process replacement. | `fleetctl module` coordinates preparation, canary commit, observation, promotion, and failure rollback across Worlds; `drill_module_update.sh` tests two Worlds. C++ uses process replacement with Gateway/World drain controls and explicit forced-stop deadlines. |

Neither mode makes TCP sockets survive a Gateway process replacement automatically. During a rolling update, the old process can keep serving existing sockets while the new instance accepts new ones. Cross-process socket handoff requires a separate implementation. Reconnect without another login also requires a session recovery protocol.

## Current implementation status

Current controls include Gateway/World begin/status drain, multi-World Lua rollout, and contract-version handshake windows. GatewayDrainTests, WorldDrainTests, and drills cover natural drain and deadline force-stop. These do not establish a production multi-instance deployment, client session recovery, or cross-process socket handoff.

MigrationChainTests covers Login coordinating source freeze, export, destination adoption, and directory switching between two Worlds. The source object remains frozen; complete object removal and failure recovery remain unfinished.

### Continuity acceptance

| Path | Verified behavior | Remaining boundary |
|---|---|---|
| Lua module update | UpdateContinuityTests advances an active Instance through prepare/commit to settlement and destruction; its real client socket case checks the original connection/session and new client acceptance | Prototype authentication does not validate a production account service; cross-Gateway recovery, a multi-instance load balancer, and full fault-rollout continuity require separate acceptance |
| World C++ replacement | Old World drains and stops; a replacement listens at the same address. Gateway retains bounded unsent inputs and sends them in order after reconnecting | The internal TCP connection is rebuilt and old Instances do not survive process replacement. This does not establish active-Instance continuity or real player-session recovery |
| Protocol update | Process drills cover handshake window publication, rejection, and rollback | Version-specific business decoders, real client upgrades, and active Instances spanning protocol versions |

After a World process-generation change, a sent request without a business acknowledgement has an unknown outcome. Gateway does not automatically replay it: the operation may already have executed. Unsent requests remain in a bounded inbox, preserve admission order, and are sent after reconnecting; a full inbox rejects new inputs. Business idempotency and session recovery require their own recovery workflow.

See the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md) for remaining acceptance. Durable settlement-ledger storage and restart recovery are not integrated; in-memory reward and deduplication tests do not prove successful persistence.
