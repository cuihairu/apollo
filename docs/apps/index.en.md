---
title: Server Applications
icon: server
order: 5
category:
  - Application
tag:
  - BigWorld
  - Server
  - Application
---

[English](index.en.md) | [中文](index.md)

# Server Applications

The Apollo project provides a set of assemblable server applications for realizing two MMO main topologies:

- `Standard MMO`
- `Distributed World`

Two boundaries should be clarified up front:

- `BaseApp` is not a database server
- `GatewayApp` is not the default mandatory core in the BigWorld / KBEngine sense

More precisely:

- The default main chain of a standard MMO is `LoginApp -> GatewayApp -> BaseApp(PlayerAnchor) -> WorldApp`
- The default main chain of a distributed world is `LoginApp -> BaseApp(Proxy + PlayerAnchor) -> CellApp`

> **Status note (2026-10-07 audit)**: `WorldApp` currently has no standalone process
> directory — the world runtime lives as library components in `modules/game/world`
> (Scene/AOI/Instance); see `apps/game-server` for a single-machine assembly example.
> A standalone world-app process will be scoped with the topology wiring batch.

## Application List

| Application | Description | Docs |
|------|------|------|
| **GatewayApp** | Edge access layer: handles client connections, admission validation, and message forwarding | [Details](/apps/BigWorld服务器应用实现.en) |
| **LoginApp** | Login entry: handles account authentication, entry assignment, and session ticket generation | [Details](/apps/BigWorld服务器应用实现.en) |
| **BaseApp** | Player anchor host: handles `PlayerAnchor`, session ownership, and reconnect recovery | [Details](/apps/BigWorld服务器应用实现.en) |
| **CellApp** | World runtime host: handles AOI, entities, combat, and spatial logic | [Details](/apps/BigWorld服务器应用实现.en) |
| **BaseAppMgr** | Scheduling plane: global directory (player→SessionBinding/WorldAssignment), placement arbitration, and route resolution | [Details](/apps/BigWorld服务器应用实现.en) |
| **Machined** | Supervisor daemon: fleet directory (UDP broadcast discovery) + roster bring-up and backoff restart supervision | [Details](/apps/BigWorld服务器应用实现.en) |
| **game-server** | Monolithic assembly sample (RuntimeManifest/ServiceHost/DI/data-source demo), not a topology member | [Details](/apps/BigWorld服务器应用实现.en) |

## Related Docs

- [BigWorld Server Application Implementation](BigWorld服务器应用实现.en.md) - full implementation details and usage guide
- [BigWorld Architecture Deep Dive](/architecture/bigworld) - architecture overview
- [BigWorld Process Architecture and Player Lifecycle](/architecture/bigworld-lifecycle) - process responsibilities and lifecycle
- [AOI 3x3 Grid System](/architecture/aoi) - AOI system in depth
