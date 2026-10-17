---
title: KBEngine Source Analysis
description: Runtime, entity, networking, and spatial responsibilities in KBEngine, and their relevance to Apollo.
---

# KBEngine Source Analysis

This page explains KBEngine's source organization and the responsibilities behind its process names. These are external-engine concepts. Apollo's current architecture remains Login, Gateway, and World, with Scene and Instance inside World; see [active architecture decisions](./adr.md).

## Reading order

Read the startup host, common server runtime, networking, and entity definitions before individual entity implementations. Then follow BaseApp/CellApp, spatial visibility, managers, and operational tools. Starting with `cellapp/entity.cpp` hides the common infrastructure behind entity-specific detail.

| Source area | Responsibility |
|---|---|
| `kbe/src/server/` | Process implementations: loginapp, baseapp, cellapp, their managers, dbmgr, machine, and tools |
| `kbe/src/lib/server/` | Shared application startup and server runtime |
| `kbe/src/lib/network/` | Interfaces, channels, bundles, and message dispatch |
| `kbe/src/lib/entitydef/` | Entity metadata, script definitions, and remote references |
| `kbe/src/lib/pyscript/` | Python runtime integration |
| `kbe/res/` | Server resources and scripts |

## Shared startup and runtime

The `kbeMainT` startup template and common `ServerApp` skeleton keep initialization and lifecycle work consistent across applications. `EntityApp<E>` provides a shared basis for entity-bearing applications.

The useful principle for Apollo is a common application host with explicit dependency and shutdown order. It does not require copying KBEngine's process topology or its global singleton structure.

## Networking and entity definitions

`NetworkInterface`, `Channel`, `Bundle`, and message handlers separate connection ownership, packet construction, and dispatch. Entity definitions connect property metadata, scripting, serialization, and remote calls. `ScriptDefModule`, `EntityCall`, and entity components participate in that model.

Apollo can reuse the separation of transport, contract metadata, and entity-call semantics. Protocols and bindings should share a contract source rather than evolve independently. See [EntityDef analysis](./kbengine-entitydef-analysis.md) and [remote entity calls](./remote-entity-call-design.md).

## What BaseApp and CellApp mean

BaseApp is more than a database process: its player/session role includes `Proxy` and non-spatial entity behavior. CellApp owns spatial simulation. `SpaceMemory` is a runtime spatial container, rather than only a numeric space identifier.

`Witness` and visibility triggers define what a client can observe. `GhostManager` belongs to distributed spatial authority and replica coordination. CellAppMgr manages spatial placement/topology, while BaseAppMgr manages entry placement and non-spatial load. These are specific responsibilities, not generic names for every multiplayer server.

Apollo does not target seamless spatial partitioning or Ghost replication. Its World hosts Scene simulation and temporary Instance lifecycles, and also hosts player data services. Its Login hosts the online directory and placement control. Do not reintroduce Base/Cell process names through this comparison.

## Operations and observability

Machine and operational tools are part of KBEngine's deployment model. Watcher exposes structured runtime state, complementing logs. These ideas motivate explicit supervision, introspection, and controlled administrative actions; they do not prove availability or recovery by themselves.

Apollo's supervision remains distinct from active gameplay recovery. Similarly, queue acceptance, authority transfer, and durable settlement need separate observable outcomes.

## Adoption boundaries

- Reuse a common host and explicit lifecycle before adding more process roles.
- Keep shared entity metadata and remote-reference semantics coherent.
- Keep AOI state and simulation authority inside the owning Scene.
- Use structured observability and bounded administrative operations.
- Avoid copying macro-heavy interfaces, pervasive singleton dependencies, or Python-specific coupling merely to resemble the external engine.
- Adopt mechanisms according to Apollo's documented requirements. Cross-server activities require explicit authority transfer and server affiliation; they do not require seamless-world spatial partitioning.

The detailed source walkthrough is available in the [Chinese source analysis](/architecture/kbe-source-analysis). Apollo implementation tasks are maintained in the root backlog, not in an alternative Base/Cell migration roadmap.
