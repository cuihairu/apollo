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
| gateway | Connections, sessions, Base/cell forwarding | gateway-app has ingress, the old World outlet, and drain primitives; wire separate Base/executor routes. |
| loginServer | Authentication, service/online directories | login-app hosts OnlineDirectory; add Base ownership and manager/executor address projections. |
| baseServer | PlayerAnchor, persistent gameplay, settlement validation | apps/base-server builds baseServer with PlayerStore/PlayerAnchor and injected remote storage; directory and settlement wiring remain pending. |
| worldServer | Executor registration, Space/Cell placement, topology, migration | world-app / WorldApp currently runs Scene directly; split management from execution. |
| cell | Local Cell entities, AOI, combat, scripts, ticks | Dedicated entry point not implemented; reuse WorldHost/Scene/Avatar execution primitives, initially in one process. |
| database | Load/save, storage routing, durable results | apps/database builds database with file profiles, journal, and M1 load/save; full queues, write authority, and partitioning remain pending. |
| machined | Local supervision and restart | Reuse supervision primitives, without cross-server discovery or player placement. |

apps/common is a shared library; logger/admin/fleetctl are auxiliary tools. The table identifies real entry points; track default-fleet and business migration in root todo.md.

## Requests and ownership

<ArchitectureDiagram kind="system" />

Scene is the current local implementation of Space semantics. Cell is a spatial partition; Instance is a temporary gameplay lifecycle. One executor may host Cells from several Spaces, without one process per instance. Cross-server play retains original Base/storage affiliation.

## Existing evidence

HostedServicesTests, SquadInstanceTests, SquadWireE2eTests, UpdateContinuityTests, and recovery tests establish existing behavior. Validate dedicated Base/database, manager/executor separation, real routes, durable settlement, complete Instance transfer, and dual-version protocols in the target topology. Old whole-profile transfers between Worlds do not validate the new boundary.

## Standalone Base/database prototype

Build the `baseServer` and `database` targets, then run this independent storage path (current M1 TCP is POSIX):

```sh
./build-examples/apps/database/database --port 9200 --data-dir data/database/players --journal data/database/persist.log
./build-examples/apps/base-server/baseServer --port 9201 --database-host 127.0.0.1 --database-port 9200
```

Both accept `--route-json` for the existing contract routes; Base accepts `--id` for executor identity. PlayerStorage injects a remote client. Base loads PlayerAnchor on demand without scanning every player in shared storage or opening local profile/journal files. database reuses DB_LOAD/DB_SAVE, validates profile identity, serializes operations, and acknowledges after file-journal/profile application. It recovers pending entries before readiness. Storage/listener failures prevent readiness; Base fails startup when database is unreachable, without local fallback.

This is an independent prototype. Default WorldApp still embeds the old PlayerStore. Login/gateway wiring, Base ownership directories, cross-Base write generations, settlement, storage partitions, and full asynchronous queues remain pending. PlayerData is still a legacy adapter containing position/runtime fields, not the final Base business model; split those contracts and remove old save paths in subsequent tasks. Current durability covers flush/rename and process restart, not power-loss fsync guarantees. Protocol upgrades and continuous activity migration remain unfinished.

BaseDatabaseTests covers TCP, Base without local paths, shutdown/reload, identity rejection, unavailability, and journal recovery. BaseDatabaseProcessSmoke launches both executables independently and verifies restart/isolation, without claiming complete fleet acceptance.
