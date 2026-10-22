---
title: Server Applications
icon: server
order: 5
category:
  - Applications
tag:
  - Server
---

# Server Applications

Target roles and current code are distinct. The [BigWorld–Apollo mapping](../design/bigworld-apollo-mapping.md) defines the responsibility boundaries; unfinished entry points do not define another architecture.

## Roles and implementation mapping

| Role | Responsibility | Existing code and remaining work |
|---|---|---|
| gateway | Connections, sessions, Base/cell forwarding | gateway has ingress, the old World outlet, and drain primitives; wire separate Base/executor routes. |
| loginServer | Authentication, service/online directories | login hosts OnlineDirectory; add Base ownership and manager/executor address projections. |
| baseServer | Avatar, persistent gameplay, settlement validation | apps/base builds base with PlayerStore/Avatar and injected remote storage; settlement admission is wired, directory ownership wiring remains pending. |
| worldServer | Executor registration, Space/Cell placement, topology, migration | world / WorldApp currently runs Scene directly; split management from execution. |
| cell | Local Cell entities, AOI, combat, scripts, ticks | Dedicated entry point not implemented; reuse WorldHost/Scene/Avatar execution primitives, initially in one process. |
| database | Load/save, storage routing, durable results | apps/database builds database with file profiles, journal, and M1 load/save; full queues, write authority, and partitioning remain pending. |
| machined | Local supervision and restart | Reuse supervision primitives, without cross-server discovery or player placement. |

apps/common is a shared library; logger/admin/fleetctl are auxiliary tools; apolloc is the project scaffold CLI (`apolloc init <path>`, template pending P0.2). The table identifies real entry points; track default-fleet and business migration in root todo.md.

## Requests and ownership

<ArchitectureDiagram kind="system" />

Scene is the current local implementation of Space semantics. Cell is a spatial partition; Instance is a temporary gameplay lifecycle. One executor may host Cells from several Spaces, without one process per instance. Cross-server play retains original Base/storage affiliation.

## Existing evidence

HostedServicesTests, SquadInstanceTests, SquadWireE2eTests, UpdateContinuityTests, and recovery tests establish existing behavior. Validate dedicated Base/database, manager/executor separation, real routes, durable settlement, complete Instance transfer, and dual-version protocols in the target topology. Old whole-profile transfers between Worlds do not validate the new boundary.

## Standalone Base/database prototype

Build the `base` and `database` targets, then run this independent storage path (current M1 TCP is POSIX):

```sh
./build-examples/apps/database/database --port 9200 --data-dir data/database/players --journal data/database/persist.log
./build-examples/apps/base/base --port 9201 --database-host 127.0.0.1 --database-port 9200
```

Both accept `--route-json` for the existing contract routes; Base accepts `--id` for executor identity. PlayerStorage injects a remote client. Base loads Avatar on demand without scanning every player in shared storage or opening local profile/journal files. database reuses DB_LOAD/DB_SAVE, validates profile identity, serializes operations, and acknowledges after file-journal/profile application. It recovers pending entries before readiness. Storage/listener failures prevent readiness; Base fails startup when database is unreachable, without local fallback.

This is an independent prototype. Persistent-profile ownership has moved to base, and the bounded save queue, storage routing, and write generations have landed; login/gateway and Base ownership directories, the full cross-process settlement chain, and storage partitions remain pending. PlayerData is still a legacy adapter containing position/runtime fields, not the final Base business model; split those contracts and remove old save paths in subsequent tasks. Current durability covers flush/rename and process restart, not power-loss fsync guarantees. Protocol upgrades and continuous activity migration remain unfinished.

BaseDatabaseTests covers TCP, Base without local paths, shutdown/reload, identity rejection, unavailability, and journal recovery. BaseDatabaseProcessSmoke launches both executables independently and verifies restart/isolation, without claiming complete fleet acceptance.
