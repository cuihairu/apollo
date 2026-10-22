
# Design Docs Overview

> This directory contains design and implementation notes for individual capabilities. Apollo's target architecture, public terminology, and process boundaries are defined by the [Architecture Overview](../guide/architecture.md) and [Concepts and Names](./concept-glossary.md); subsystem documents must not introduce a second process topology.
> This directory contains implementation notes and subsystem designs. Do not infer current code status from old batch notes. External engine terminology is comparative material, not automatically an Apollo concept. The only current project backlog is the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).

## Foundations and Models

| Design doc | Status | One-line positioning |
|---|---|---|
| [BigWorld–Apollo mapping](./bigworld-apollo-mapping.md) | Agreed rules | Original terms, Apollo roles, code and gaps |
| [Can Apollo exceed BigWorld?](./apollo-vs-bigworld.md) | Auditable assessment | Potential advantages, unproven claims, gaps, and acceptance criteria |
| [World and cell management](./world-cell-management.md) | Review draft | Shared binary, role startup, local autoscaling, placement, recovery, and migration |
| [World Restart Reconciliation](./world-reconciliation.md) | Design draft (under review), not implemented | journal v2 record family, 963 self-report, per-item adjudication and conflict freezes |
| [Seamless map and multi-Cell Space](./seamless-map.md) | Target design, staged implementation | Partitioning, Ghosts, boundary AOI/combat, transfer, and takeover |
| [Concept glossary](./concept-glossary.md) | Agreed rules | Established names and responsibility boundaries |
| [Player object model](./player-object-model.md) | Subsystem implementation note | Base persistent objects, Cell entities, and Instance gameplay lifecycles |
| [Attribute system and sync](./attribute-sync.md) | Subsystem implementation note | Attribute data and visibility/synchronization mechanisms |
| [Session and online directory](./session-and-online-directory.md) | Subsystem implementation note | Base ownership, Space/Cell/executor location, and session routing |
| [Cross-server scenes](./cross-server-scenes.md) | Reserved design; disabled | Server affiliation, shared-scene admission, authority transfer, settlement, and return |
| [Login flow](./login-flow.md) | Subsystem design draft | loginServer/gateway authentication, Base/database loading, and World admission |
| [Clock and time](./clock-and-time.md) | Delivered | Wall clock/tick/frame budget semantics |

## Combat

| Design doc | Status | One-line positioning |
|---|---|---|
| [Battle determinism](./battle-determinism.md) | Delivered | Four constraints + recording/replay/hash chain |
| [Battle instance offload](./battle-instance-offload.md) | Subsystem implementation note | Battle-computation offload experiment; does not change Instance/World target terms |
| [Battle verification service](./battle-verification-service.md) | Design draft (under review) | Client-authoritative battle recomputation and reconciliation (verifier) |

## Networking and Operations

| Design doc | Status | One-line positioning |
|---|---|---|
| [Network abstraction and transport kernel](./net-abstraction.md) | Design draft | L0-L3 layering + M1 homegrown kernel (nng replacement route) |
| [Gateway responsibilities and update boundary](./gateway-topology-survey.md) | Architecture note | Gateway routing, multiple instances, and update limits |
| [Logging system](./logging.md) | Delivered (P3-3 batches A/B/C) | One unified closure + structured lines + three-prohibitions discipline |
| [Log collector](./logger-app.md) | Implementation note | Log collection executable and structured-log wiring |
| [Crash capture](./crash-capture.md) | Delivered | Crashpad out-of-process + symbolization pipeline |
| [Capacity and benchmark](./capacity-and-benchmark.md) | Delivered | 5000 CCU capacity model + three-model benchmark |

## Extension Surfaces

| Design doc | Status | One-line positioning |
|---|---|---|
| [Simulation single-writer](./world-single-writer.md) | Concurrency design | Concurrency boundary between network acceptance and instance simulation |
| [Scripting system (Lua)](./scripting-lua.md) | Design draft | Both-end shared battle logic and hot-update discipline |
| [SDK contract](./sdk-contract.md) | Living document | Client/server shared contract (contract.lua) |
| [XML contract generation](./xml-generation.md) | Delivered | entities.xml → three-language projections |
