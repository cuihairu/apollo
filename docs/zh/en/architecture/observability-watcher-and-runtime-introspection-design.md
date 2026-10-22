---
title: Observability, Watcher, and Runtime Introspection Design
icon: binoculars
order: 30
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - Observability
  - Watcher
  - Introspection
---

# Runtime Observability and Introspection Boundaries

Self-built logging, MetricRegistry, span/trace, log/metric push, and some management queries exist. No dedicated Profiler, OpenTelemetry SDK, or OTLP exporter is integrated. Design proposals do not establish deployed functionality.

## Existing implementation

| Capability | Source and boundary |
|---|---|
| Local logging | modules/core/log; see [logging](../design/logging.md) for output and push. |
| Metrics | MetricRegistry in modules/core/metrics; not CPU/stack sampling. |
| Span/trace | modules/net/linktrace and RpcTrace in remoteentity; self-built context/emission rather than OTel integration. |
| Push/management | modules/net/metricspush and apps/admin aggregation/HTTP metrics; deployment requires separate acceptance. |
| Crash capture | See [crash capture](../design/crash-capture.md); dumps are not a live Profiler. |

## Observation across roles

loginServer records authentication, directory registration/expiry, and ownership changes. gateway records connections, sessions, and Base/cell routes. Base records base gameplay, settlement, and pending storage results. World records registration, topology, placement, and transfer decisions. cell records Cells, entities, Instances, ticks, and state handoff. database records storage admission, durable outcomes, failure, and retry. machined records local process lifecycle.

Correlate player IDs, Base/World/Cell/executor identities, session/route versions, transfer/settlement IDs, and process/business generations. Machine addresses do not identify player ownership. Do not log credentials or database passwords.

## Diagnostic ownership

Obtain runtime snapshots at their owning execution boundaries rather than traversing mutating simulation/profile containers from management threads. Administrative changes still require authentication, authorization, and business intents; debug access must not bypass Base/World/cell/database authority.

Structured queries and Watcher-style trees are diagnostic design directions, not an implemented unified Watcher Tree or complete introspection platform. Profiler, OTel, and external collectors remain requirement-driven without new game process roles.

## Acceptance

Validate process liveness, readiness, admission, execution, and durable results separately. Observe Base associations, scene transfer, and update phases; a trace, restart, or successful log does not establish seamless upgrades. Validate sampling cost, exporter backpressure, and failure handling when an actual integration exists.

Track implementation only in root todo.md. See [architecture](../guide/architecture.md) and [update boundaries](../design/gateway-topology-survey.md).
