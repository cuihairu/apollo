# Apollo Architecture Status

Apollo is a lightweight room-based multiplayer game server engine for matches such as tower defense, co-op, arenas, and dungeons. It does not target a persistent seamless world or spatially partitioned simulation processes.

The target runtime path is `Client → Login / Gateway → RoomServer → Room`. A Room is one match, a RoomServer can host multiple Rooms, and each player has one authoritative gameplay object in a Room at a time. Cross-server play requires an explicit authority transfer and route update.

## Current implementation boundary

The repository contains prototypes, not a complete production deployment: `login-app`, `gateway-app`, `room-server`, `base-app`, `baseappmgr`, and `machined`. `room-server` is named for its target role, but currently starts a default Scene/Instance and does not yet provide complete Room creation, admission, settlement, and destruction. See [Server Applications](../apps/index.md) and the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md) for verified status and remaining work.

Gateway does not automatically provide no-downtime updates. Multiple Gateway instances can keep new connections available while an old instance drains existing sockets. Cross-process TCP handoff, session recovery without another login, and uninterrupted active Rooms each require separate mechanisms and validation.

## Update modes

- **Protocol update:** change XML/XSD under `sdks/contract/`, regenerate artifacts, and roll out with an old/new compatibility window. The contract sources, generator, and golden checks exist; protocol negotiation and end-to-end compatibility-window validation remain incomplete.
- **Code-only update:** preserve the protocol contract and wire format. Lua `ScriptRuntime` has preparation, tick-boundary commit, observation, and rollback primitives; production patch distribution is incomplete. C++ changes use process replacement after Room drain or a separately implemented state transfer.

BigWorld and KBEngine terms are reserved for external-engine reference material. They do not define Apollo target processes or imply that Apollo implements seamless-world mechanisms.
