---
title: RemoteEntityCall Design
icon: arrows-turn-right
order: 21
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - RemoteEntityCall
  - Message
  - RPC
---

# RemoteEntityCall Design

This page documents existing remote-reference primitives and target call boundaries, reserving same-Space Cells and Ghost interfaces without implementing replication. Resolve base gameplay separately from simulation; see [sessions/routing](../design/session-and-online-directory.md).

## Call boundaries

- gateway forwards base gameplay to the owning Base and simulation input to the current cell.
- Base requests admission/placement from World and supplies gameplay snapshots to cell. cell submits settlement to each owning Base.
- Base loads/commits through database without exposing storage locations to cell.
- loginServer directories resolve services/player associations; normal messages can use cached direct routes.

## Existing implementation

modules/net/remoteentity provides RemoteEntityRef, RemoteMethodSchema, RouteResolver, and RpcTrace. Validate their real application wiring separately. Library types do not establish business directories, complete RPC codecs, or version migration.

## 5. References, methods, and resolution

### 5.1 RemoteEntityRef

Source: modules/net/remoteentity/include/apollo/net/remote/entity_ref.hpp. Fields include entity_id, domain, target_app, role, route_version, and shard_key. Trusted directories/admission establish targets and versions; clients cannot declare arbitrary data ownership.

### 5.2 RemoteMethodSchema

The adjacent method_schema.hpp defines method names/aliases, modes, domains, parameter types, timeout, and idempotency declarations. A declaration does not implement deduplication; business operations require durable IDs/results.

### 5.3 RpcTrace

The adjacent rpc_trace.hpp and src/rpc_trace.cpp reuse linktrace contexts and span emission. These self-built primitives do not integrate OpenTelemetry SDK/OTLP exporters and do not provide a sampling Profiler.

### 5.4 RouteResolver

The adjacent route_resolver.hpp and src/route_resolver.cpp implement local resolve, update_route, clear, and version queries. The owner serializes updates. The map does not implement distributed directories or separate Base/simulation player associations and cannot replace business routing.

## 7. Stale routes and outcome semantics

Refresh endpoints/generations and reject stale profile writes, sessions, and simulation authority. Base replacement and executor transfer update their own associations independently.

Distinguish delivery, acceptance, execution, and durable confirmation. Query ambiguous operations or retry with the same ID; generation changes must not allocate fresh IDs and duplicate rewards. Traces correlate failures but do not prove durable execution.

## Current work

Integrate Base/database contracts, trusted registration, separate routes, version decoders, and recovery. See [applications](../apps/index.md) and root todo.md. No alternative operating mode or object-layer roadmap is retained.
