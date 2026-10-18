# Apollo Architecture: Current Code and Target

Keep one target, with responsibilities and actual gaps defined by the [BigWorld–Apollo mapping](../design/bigworld-apollo-mapping.md).

## Conclusions

Apollo prioritizes tower defense and temporary group activities while reserving seamless maps, without claiming complete BigWorld capabilities. Retain six agreed role names and add the discussed cell executor; Cell means a partition.

worldServer overlaps CellAppMgr registration, Space/Cell placement, topology, and migration management. cell overlaps CellApp simulation. Base persistent objects and Cell entities are distinct; database owns storage. loginServer hosts authentication, service directories, and location projections. gateway retains sockets and forwards directly to Base/cell; machined supervises locally.

Space matches current Scene semantics and Instance is temporary gameplay, without a synonymous Scene→Space layer. Initially use one executor and one Cell per Space; reserve multiple Cells, Real/Ghost, continuous views, and complete authority handoff.

## Unfinished boundaries

WorldApp, World, and Scene currently combine management/execution; split profile/storage duties in PlayerStore. Base/database prototypes provide remote load/save and a file journal. cell, registration/routing, complete business commits, cross-server associations, dual-version decoders, and three state handoffs remain unfinished. SceneTransfer recreates Avatar and supports explicit scene changes rather than same-Space seamless handoff.

Cross-server play retains original Base/storage affiliation with idempotent durable settlement. Base, cell, and World management transfer persistent objects, simulation, and management records respectively. Gateway survival/replacement have separate TCP outcomes; activity continuity needs complete necessary Instance state.

Dedicated Profiler and OpenTelemetry SDK/OTLP exporter are not integrated. Custom logging/MetricRegistry/span primitives are not integration evidence.

## Implementation

[Architecture](../guide/architecture.md), [applications](../apps/index.md), [objects](../design/player-object-model.md), [routing](../design/session-and-online-directory.md), [cross-server play](../design/cross-server-scenes.md), and [updates](../design/gateway-topology-survey.md) follow these conclusions. Root todo.md tracks implementation/acceptance; planning is not completion evidence.
