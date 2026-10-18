---
title: BigWorld Process Architecture and Player Lifecycle
icon: flow
order: 31
category:
  - Architecture
  - BigWorld
tag:
  - BigWorld
  - Process Architecture
  - Player Lifecycle
  - Login Flow
---

# BigWorld Process Architecture and Player Lifecycle

This is an external-engine responsibility comparison, not another Apollo login/deployment topology. Apollo uses gateway, loginServer, baseServer, worldServer, cell, database, and machined. See [architecture](../guide/architecture.md).

## Process responsibilities

BigWorld LoginApp provides login ingress, BaseApp hosts non-spatial entities and client-proxy behavior, CellApp hosts spatial simulation, and database applications handle entity storage work. Client proxies and storage access are distinct responsibilities; BaseApp is not simply a database process.

## Lifecycle

Study authentication, persistent-entity loading, spatial entry, synchronization, disconnect handling, and persistence completion separately. Exact messages, objects, and process names depend on the external source version.

Apollo uses gateway for connections, Base for persistent-player gameplay, World for Space/Cell management, cell for simulation/Instance, and database for storage access. Scene transfer retains Base/storage affiliation; protocol upgrades replace Base, cell, or World management independently. Reserve same-Space partitions/seamless capabilities within this architecture; see the [mapping](../design/bigworld-apollo-mapping.md).

## Related reading

- [Player objects](../design/player-object-model.md)
- [Base Cell Proxy comparison](./base-cell-proxy-model.md)
- [Cross-server scenes](../design/cross-server-scenes.md)
