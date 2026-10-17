---
title: KBEngine Reference Principles
---

# KBEngine Reference Principles

Reuse mechanisms and engineering boundaries from KBEngine. Apollo's responsibilities and names are defined by its [active architecture decisions](./adr.md). External-engine process names retain their original meanings and are not candidate Apollo process roles.

## Runtime and lifecycle

A common entry point, shared server runtime, and explicit startup/shutdown order are useful principles. Apollo reuses its host and module-assembly facilities and composes services into applications, instead of reimplementing lifecycle and dependencies in each entry point.

Business process roles remain Login, Gateway, and World. World hosts PlayerStore for player data and persistence. Login hosts OnlineDirectory for routing, placement, and migration coordination. Shared application services are not extra business processes.

## Entities, contracts, and scripting

Reuse metadata-driven properties, remote calls, bindings, and client generation. Apollo uses XML/XSD contracts and a generator so server and client artifacts derive from the same source.

Keep wire contracts, storage representation, and script execution distinct. A declaration or generated file does not prove that the real message path uses an encoding, that version negotiation works, or that a database is integrated.

Lua uses the existing ScriptRuntime and C API bindings. Scripts must not mutate Scene from network or worker threads or bypass startup injection through string-based container lookup.

## Simulation and visibility

Reuse the ownership boundary in which a runtime spatial container holds entities, AOI, and visibility state. Apollo's existing object is Scene. Instance associates with Scene and controls a temporary activity through its existing eight-state lifecycle.

Cross-server gameplay also uses World, Scene, and Instance, retaining home-server affiliation and transferring authority explicitly. A cross-server Scene may persist or belong to a temporary Instance. It does not require Ghosts, spatial partitioning, or seamless maps. See [cross-server scenes](../design/cross-server-scenes.md).

## Communication and authority

Transport delivery, acceptance, simulation execution, and durable confirmation are separate outcomes requiring correlation and failure semantics. Reliable links do not guarantee exactly-once business effects. Transfers and settlement still require deduplication and authority epochs.

A player has one runtime authority accepting input, advancing simulation, and submitting authoritative writes. Transfer snapshots may coexist; a prepared destination remains inactive. Reconcile ambiguous commits before recovery instead of unfreezing both sides.

## Operations and recovery

Reuse supervision, structured observation, and controlled administration through machined, logging, metrics, and introspection primitives. Supervised restart does not recover active gameplay. Rebuilding a directory does not alone reestablish exclusive player write authority.

Drills must exercise the current applications and validate profiles, ownership, routing, and gameplay outcomes. Records from another process topology cannot establish current acceptance.

## Excluded targets

Apollo does not offer another Base/Cell operating mode or target seamless maps, Ghost replicas, and distributed spatial topology scheduling. It does not add synonymous objects or processes to resemble an external engine.

Adoption follows gameplay, capacity, and recovery requirements. Reserved cross-server deployment, external storage, and observation infrastructure retain the activation conditions recorded in the root backlog.
