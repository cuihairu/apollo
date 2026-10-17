# Apollo Architecture Status

Apollo is a lightweight instance-based multiplayer game server engine for matches such as tower defense, co-op, arenas, and dungeons. It does not target a persistent seamless world or spatially partitioned simulation processes.

The target runtime path is `Client → Login / Gateway → World → Instance`. An Instance is one match, a World can host multiple Instances, and each player has one authoritative gameplay object in an Instance at a time. Cross-server play requires an explicit authority transfer and route update.

## Current implementation boundary

The repository contains prototypes, not a complete production deployment: `login-app`, `gateway-app`, `world-app`, and optional `machined` supervision. World hosts PlayerStore for data and persistence; Login hosts OnlineDirectory for routing and placement. `world-app` integrates group creation, admission, objectives, settlement, and temporary Scene/Instance cleanup; durable settlement-ledger recovery remains incomplete. See [Server Applications](../apps/index.md) and the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md) for verified status and remaining work.

Gateway does not automatically provide no-downtime updates. Multiple Gateway instances can keep new connections available while an old instance drains existing sockets. Cross-process TCP handoff, session recovery without another login, and uninterrupted active Instances each require separate mechanisms and validation.

## Update modes

- **Protocol update:** change XML/XSD under `sdks/contract/`, regenerate artifacts, and roll out with an old/new compatibility window. Handshake compatibility windows and publish/rollback drills exist. Business payload compatibility and version-specific decoding must still be verified for each encoding and change.
- **Code-only update:** preserve the protocol contract and wire format. Lua `ScriptRuntime` has preparation, tick-boundary commit, observation, and rollback primitives; fleetctl distributes module operations across Worlds with canary observation and failure rollback. C++ changes use process replacement after Instance drain or a separately implemented state transfer.

BigWorld and KBEngine terms are reserved for external-engine reference material. They do not define Apollo target processes or imply that Apollo implements seamless-world mechanisms.

## Temporary instance lifecycle

A group requests an activity → World creates an Instance and temporary Scene → members pass admission and enter → clear enemies and complete objectives → settle → players leave → destroy the Instance, scene, and temporary resources.

The existing eight-state Instance lifecycle is integrated into World. Scene owns entities, AOI, and ticks; Instance owns gameplay start and completion. SquadInstanceTests and SquadWireE2eTests cover group admission, creation, gameplay objectives, settlement, cleanup, isolation, and protocol replies. Durable settlement-ledger recovery and cross-Gateway session recovery remain incomplete.
