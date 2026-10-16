---
title: Starter and Module Assembly Design
icon: plug
order: 13
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - Starter
  - Assembly
  - MonoRepo
---

[English](starter-and-module-assembly-design.en.md) | [中文](starter-and-module-assembly-design.md)

# Starter and Module Assembly Design

> **Status (2026-09-30, C-76 same-type extension)**: **Reference document (historical source of the assembly idea)** — the current state of the starter/manifest implementation (double INTERFACE shells + phantom tests, C-68) and the current assembly semantics (explicitly registered assembly units, main as the closure point) are governed by `docs/analysis/architecture-review.md` §17/§21; this document's value = the affirmative-statement source for "no Spring clone, explicit builder, lightweight registration" (cited in §1/§4). Status table = `docs/architecture/README.md` tier A. **Update (2026-10-08)**: the empty on-disk shell of `modules/starter` has been deleted with the whole directory (C-68 written off, decision = delete — fleshing it out would rebuild the runtime Starter object graph / condition evaluator already rejected in §17 and would overlap with the runtime ApplicationHost); from this point on this document exists only as a statement of ideas, with no implementation counterpart; the §14 reference list synced accordingly.

This document settles a question that must be answered as the Apollo overall framework keeps moving forward:

`How should Apollo do C++-style modular assembly without introducing a runtime reflection model.`

Let me state the conclusion up front:

- Apollo fits the idea of starter-based composite assembly
- But it will not do runtime auto-scanning or implicit injection

The more reasonable route:

- `mono-repo + multiple modules + starter assembly + profile configuration + explicit registration`

## 1. Conclusion First

For Apollo's assembly layer, the recommended model is:

```text
Module
    -> Starter
    -> Profile
    -> App Bootstrap
    -> Topology
```

That is:

- `Module` provides capabilities
- `Starter` composes capabilities
- `Profile` selects the default assembly plan
- `App Bootstrap` starts the process
- `Topology` decides the final deployment shape

This is steadier than "every app hand-writes a pile of initialization code," and better suited to C++ than "building a heavyweight reflective auto-assembly scheme."

## 2. Why This Idea Fits Apollo

### 1. Apollo is by nature a progressive framework

You have already made it clear that Apollo is not a fixed-shape MMO framework, but one that is:

- Usable for light online projects
- Usable for ordinary MMOs
- Incrementally upgradeable to BigWorld mode

Such goals naturally require:

- The ability to assemble by project type

### 2. Apollo is no longer a single world server

Apollo will gradually come to include:

- runtime
- ops
- platform
- script
- world
- session
- battle
- social
- activity
- distributed world

Without a unified assembly model, this ends up as:

- Every app hand-writing its own copy of "which modules I need"

That gets out of control fast.

### 3. C++ is better suited to explicit assembly

Unlike Java, C++ is not good at:

- Large-scale runtime scanning
- Large-scale reflective injection
- Large-scale implicit assembly

But it is very well suited to:

- Explicit dependencies
- Compile-time registration
- manifest
- factory
- starter composition

## 3. What Apollo Must Not Copy from Runtime Containers

This point needs to be settled firmly first.

### Not recommended to copy

- Package scanning
- Reflection-based auto-discovery
- Heavy runtime injection
- Every component independently and dynamically hot-pluggable

### Worth absorbing

- starter
- profile
- the auto-config idea
- convention over configuration
- default assembly + explicit override

What Apollo should build is:

- `A C++-style explicit assembly framework`

not:

- `A heavyweight runtime container clone`

## 4. Core Concepts

## 5. `Module`

A `Module` is the minimal capability unit.

It represents:

- A code module with one clearly defined responsibility

For example:

- `base`
- `core`
- `runtime`
- `runtime.ops`
- `platform.redis`
- `platform.sql`
- `game.session`
- `game.world`
- `game.battle`
- `script.lua`
- `script.python`

### Metadata a `Module` should carry

```text
ModuleManifest
    name
    version
    dependencies
    optional_dependencies
    capabilities
    default_config
```

### The responsibility boundary of a `Module`

- Provide capabilities
- Do not perform final assembly
- Do not directly decide the deployment topology

## 6. `Starter`

A `Starter` is not a capability module; it is an assembly template.

It is responsible for:

- Selecting a set of modules
- Providing default configuration
- Conventions for how modules combine

For example there could be:

- `starter-core`
- `starter-gateway`
- `starter-login`
- `starter-world`
- `starter-mmo`
- `starter-bigworld`

### The value of a `Starter`

It solves:

- Consumers no longer need to assemble all modules from scratch every time

### Example

`starter-mmo` could assemble by default:

- `runtime`
- `runtime.ops`
- `platform.redis`
- `platform.sql`
- `game.session`
- `game.world`
- `game.battle`
- `script.lua` or `script.python`

### What a `Starter` must not do

- Write business logic directly
- Carry app state directly

## 7. `Profile`

A `Profile` is the assembly plan along the runtime-configuration dimension.

What it answers is not "which modules exist," but:

- In which mode these modules are enabled

For example:

- `dev`
- `test`
- `prod`
- `single-world`
- `mmo`
- `bigworld`

### What a `Profile` is responsible for

- Turning certain modules on or off
- Choosing the script backend
- Choosing the storage backend
- Choosing ops switches
- Choosing distributed-capability switches

### Example

```yaml
profile: mmo

script:
  backend: lua

distributed_world:
  enabled: false

ops:
  otel: true
  console: true
```

## 8. `App Bootstrap`

This is the real startup entry point of every executable.

It is responsible for:

- Reading the profile
- Loading the starter
- Parsing the module manifest
- Building the dependency graph
- Starting the host

### Recommended structure

```text
AppBootstrap
    ├── ConfigLoader
    ├── StarterResolver
    ├── ModuleRegistry
    ├── DependencyGraph
    └── HostBuilder
```

### Key point

Apollo's final apps should no longer each hand-write their own initialization order; everything should converge into a unified bootstrap.

## 9. `Topology`

`Topology` is the deployment and runtime-shape layer.

It is responsible for:

- Which apps things finally split into
- Which starters each app assembles
- Which profile each app runs

For example:

### Light online projects

- `gateway-app`
- `login-app`
- `game-app`
- `platform-app`

### Ordinary MMO

- `gateway-app`
- `login-app`
- `base-app`
  The target semantics are `BaseApp(PlayerAnchor Host)`
- `world-app`
  In the current repo, `apps/cell-app` can serve as its assembly shell for now
- `chat-app`
- `db-app / persistence-app`

### BigWorld mode

- `gateway-app`
- `login-app`
- `base-app`
  The target semantics are `BaseApp(PlayerAnchor Host)`
- `base-app-mgr`
- `cell-app`
  Only here does it denote the real `CellApp(Distributed World Node)`
- `cell-app-mgr`
- `db-app / persistence-app`

## 10. Recommended Assembly Flow

The recommended standard assembly flow for Apollo:

1. Select the `app`
2. Load the `profile`
3. Resolve the `starter`
4. Expand `module dependencies`
5. Build the `host builder`
6. Initialize the `runtime host`
7. Initialize the `ops host`
8. Initialize the `platform foundation`
9. Initialize the `game foundation`
10. Initialize `domain components`

If BigWorld is enabled, continue with:

11. Initialize the `distributed world extension`

## 11. Recommended Manifest Structure

It is recommended that every module carry at least a manifest like this:

```yaml
name: game.world
version: 0.1.0
dependencies:
  - runtime
  - runtime.ops
  - game.foundation
optional_dependencies:
  - script.lua
  - script.python
capabilities:
  - world_host
  - map_instance
  - aoi
```

### Why the manifest must be explicit

Because Apollo will keep gaining more and more modules.

Without explicit manifests, starters eventually degrade into:

- A pile of hand-written if/else

## 12. Recommended Starter Categories

### Foundation starters

- `starter-core`
- `starter-runtime`
- `starter-ops`
- `starter-platform`

### Game starters

- `starter-session`
- `starter-world`
- `starter-battle`
- `starter-social`

### Shape starters

- `starter-online`
- `starter-mmo`
- `starter-bigworld`

### Backend starters

- `starter-script-lua`
- `starter-script-python`
- `starter-platform-redis`
- `starter-platform-sql`

## 13. Relationship with Script Backends

This layer should also align with the unified script-layer design.

### Principle

- A project usually picks exactly one primary business script backend

So the starter layer should support:

- `starter-script-lua`
- `starter-script-python`

but enabling both by default is not recommended.

## 14. Relationship with the Current Apollo Repository

Apollo currently has:

- `modules/starter` (**deleted as of 2026-10-08** — C-68 empty-shell write-off; assembly semantics = explicitly registered assembly units + main as the closure point, host = the `runtime` ApplicationHost)
- Multiple `apps/*`

But a truly unified starter + bootstrap system has not yet taken shape.

The current state is closer to:

- Prototype apps assembling themselves

The more reasonable next step:

- Reduce apps to thin assembly shells
- Promote starter and bootstrap into the formal assembly layer

## 15. Conclusion

If Apollo keeps pushing toward an overall framework, the most reasonable assembly route is:

- One mono-repo
- Multiple clearly bounded modules
- Starters compose the modules
- Profiles select the mode
- Bootstrap starts everything uniformly

This is the most C++-appropriate landing of explicit assembly.

## Related Reading
