---
title: Architecture Overview
icon: layers
order: 7
prev: /en/guide/configuration
---

# Architecture Overview

Apollo currently targets tower defense, cooperative play, and group instances, while reserving future seamless maps. The [BigWorld–Apollo mapping](../design/bigworld-apollo-mapping.md) retains the six agreed role names and adds the discussed cell executor. worldServer manages executors rather than running entity ticks.

## Target model

<ArchitectureDiagram kind="system" />

| Role | Responsibility and lifecycle |
|---|---|
| gateway | Client sockets, sessions, and Base/simulation routes; backend replacement does not move sockets. |
| loginServer | Authentication, credentials, service and online directories; location projections, not Cell topology decisions. |
| baseServer | PlayerAnchor persistent objects, base gameplay, and profile write authority, independent of instances and cross-server play. |
| worldServer | CellAppMgr-like management: executor registration, capacity, Space/Cell placement, topology, drain, and migration. |
| cell | CellApp-like execution: local Cells, Avatar/Entity, AOI, combat, Lua, and ticks. |
| database | Profile operations, storage routing, queues/journal, and durable results, without reward rules. |
| machined | Optional local supervision, without player ownership or spatial placement decisions. |

## Spatial and gameplay objects

Space is an independent runtime space, corresponding to the spatial meaning of the existing Scene type. Do not add a synonymous Scene→Space container chain. A Cell is a partition of a Space; a cell process hosts partitions. Initially use one executor and one full-range Cell per Space. One executor may host Cells from several Spaces.

Instance retains the temporary group gameplay lifecycle: members, objectives, clocks, settlement, and cleanup, with the existing eight states. It associates with Space rather than adding another partition. Persistent Spaces need no temporary Instance.

<ArchitectureDiagram kind="space" />

## Player and data ownership

Base owns profiles and supplies validated gameplay snapshots. A cell modifies authoritative simulation state in its Cells. Submit rewards, consumption, and quest results to each owning Base for validation and durable commit through database. Receipt ACK is not persistence. Cross-server play preserves home Base, server, and storage affiliation; destinations receive no home database credentials.

## Registration, addressing, and migration

Base/World register with loginServer; cell executors register readiness with their owning worldServer. worldServer is authoritative for Space/Cell placement and topology. The online directory stores projections of Base, World, Space, Cell, executor, and session associations. gateway caches Base and executor routes for direct business traffic.

Freeze the source executor at a simulation boundary; restore the destination without activation. worldServer conditionally commits ownership against the expected source and generation, then updates route projections, activates the target, and cleans the source. Cross-World transfer needs coordination and durable commit evidence, never independent activation by two managers. Reconcile ambiguous results before simulation resumes. Neither World nor cell requires local machined.

## Update boundary

Protocol updates prepare replacements, verify message/state compatibility, restore state, hand off exclusive authority and routes, then retire sources. Base transfers persistent objects and in-flight business; cell transfers necessary simulation state; worldServer transfers topology and outstanding placement/migration records. Their snapshots differ.

Code-only updates preserve message IDs, fields, and wire format. Lua changes commit at the owning execution boundary; C++ uses replacement processes. Keeping gateway retains its TCP sockets; replacing it requires drain or reconnect recovery. Complete Instance continuity includes members, enemies, objectives, clocks, and necessary script state. See [update boundaries](../design/gateway-topology-survey.md).

## Seamless-map reservation

Spatial splitting, Ghost replication, and cross-boundary combat are deferred. Reserve multiple Cells in one Space, global coordinates, stable entity IDs, adjacency/topology, authority generations, complete state handoff, and continuous visibility. Explicit Space changes differ from same-Space Cell handoff; departure resets cannot implement seamless transfer.

## Repository status

WorldApp, World, and Scene currently combine management and simulation. Dedicated Base/database prototypes provide remote load/save. The cell entry point, registration, business migration, and full routing remain unfinished. SceneTransfer changes scenes within one process, not complete Cell ownership. Existing primitives/tests establish their own scope only. See [applications](../apps/index.md) and root todo.md.
