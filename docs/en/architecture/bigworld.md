---
title: BigWorld Architecture Deep Dive
icon: server
order: 30
category:
  - Architecture
  - BigWorld
tag:
  - BigWorld
  - Compatibility Layer
  - BaseApp
  - CellApp
---

# BigWorld Architecture Deep Dive

This is a wrap-up entry page.

Apollo's BigWorld compatibility module provides a familiar C++ API facade; it does not implement BigWorld's distributed seamless-world runtime. Apollo's target architecture is a room-based multiplayer game server; see the [Architecture Overview](../guide/architecture.md).

## Semantic Boundaries

- `BaseApp` is closer to `PlayerAnchor + Proxy Host`
- `CellApp` is closer to a spatial real-time authority node
- `LoginApp` handles authentication and entry assignment
- `GatewayApp` is only enabled in Apollo's edge-access assembly

## Compatibility Layer

Apollo already has a BigWorld compatibility layer implementation, see:

- [BigWorld Compatibility Layer](/33-BigWorld_Compatibility)

## Recommended Reading

- [BigWorld Process Architecture and Player Lifecycle](./bigworld-lifecycle.md)
- [Witness and Ghost Design](./witness-ghost-design.md)
