---
title: KBEngine Reference Principles
---

# KBEngine Reference Principles

Reuse mechanisms and engineering boundaries from KBEngine. Apollo's responsibilities and names are defined by its [active architecture decisions](./adr.md). External-engine process names retain their original meanings and are not candidate Apollo process roles.

## Runtime and lifecycle

A common entry point, shared server runtime, and explicit startup/shutdown order are useful principles. Apollo reuses its host and module-assembly facilities and composes services into applications, instead of reimplementing lifecycle and dependencies in each entry point.

The fixed roles are gateway, loginServer, baseServer, worldServer, cell, database, and machined. Base owns persistent gameplay, World owns Space/Cell management and cell owns simulation, database owns storage access, and loginServer hosts authentication/directories. See [server applications](../apps/index.md) for wiring.

## Entities, contracts, and scripting

Reuse metadata-driven properties, remote calls, bindings, and client generation. Apollo uses XML/XSD contracts and a generator so server and client artifacts derive from the same source.

Keep wire contracts, storage representation, and script execution distinct. A declaration or generated file does not prove that the real message path uses an encoding, that version negotiation works, or that a database is integrated.

Lua uses the existing ScriptRuntime and C API bindings. Scripts must not mutate Scene from network or worker threads or bypass startup injection through string-based container lookup.

## Simulation and visibility

Reuse the ownership boundary in which a runtime spatial container holds entities, AOI, and visibility state. Apollo's existing object is Scene. Instance associates with Scene and controls a temporary activity through its existing eight-state lifecycle.

Cross-server gameplay also uses World, Scene, and Instance, retaining home-server affiliation and transferring authority explicitly. A cross-server Scene may persist or belong to a temporary Instance. It does not require Ghosts, spatial partitioning, or seamless maps. See [cross-server scenes](../design/cross-server-scenes.md).

## Communication and authority

Transport delivery, acceptance, simulation execution, and durable confirmation are separate outcomes requiring correlation and failure semantics. Reliable links do not guarantee exactly-once business effects. Transfers and settlement still require deduplication and authority epochs.

Base exclusively modifies persistent-player data while cell exclusively advances the scene entity. Cross-server transfer retains Base/storage affiliation. Prepared simulation destinations remain inactive; reconcile ambiguous commits before activation.

## Operations and recovery

Reuse supervision, structured observation, and controlled administration through machined, logging, metrics, and introspection primitives. Supervised restart does not recover active gameplay. Rebuilding a directory does not alone reestablish exclusive player write authority.

Drills must exercise the current applications and validate profiles, ownership, routing, and gameplay outcomes. Records from another process topology cannot establish current acceptance.

## Current scope and reservation

Base owns persistent gameplay, World manages Space/Cell topology, and cell executes simulation. Initially one Cell per Space; reserve future seamless maps and Ghost interfaces while deferring splitting/replication. Scene retains its code name with Space semantics, without duplicate containers.

Adoption follows gameplay, capacity, and recovery requirements. Reserved cross-server deployment, external storage, and observation infrastructure retain the activation conditions recorded in the root backlog.
