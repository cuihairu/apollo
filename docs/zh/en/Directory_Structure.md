
# Apollo Project Directory Structure

## Overview

This document describes the actual directory layout of the current repository.

## Top-Level Directories

| Directory | Description |
|------|------|
| `apps/` | Executable services and process shells |
| `modules/` | Modular implementation |
| `docs/` | Documentation site |
| `examples/` | Example code |
| `tests/` | Test code |
| `include/` | Public headers and compatibility-layer headers |
| `src/` | Common implementation and compatibility-layer implementation |
| `sdks/` | Contract sources and client SDKs (contract/gen/cpp/unity) |
| `skds/` | Cocos / Laya / historical SDK workspaces |
| `cmake/` | CMake helper files |
| `scripts/` | Scripts |
| `build*/` | Build output |

## apps/

| Directory | Description |
|------|------|
| `login/` | Login entry point (authentication, service register/online directory, recovery fleet face) |
| `gateway/` | Edge access layer |
| `base/` | Standalone base entry with remote database access, owning persistent profiles and settlement admission |
| `database/` | Dedicated M1 load/save and file journal |
| `common/` | Reusable profile, storage, and directory implementations; hosts assemble along Base/database boundaries |
| `world/` | world entry (manager role by default; `world --role=cell` is explicitly rejected and the standalone `cellServer` is the sole executor entry) plus the cellServer executor entry (the execution kernel CellApp is extracted and runnable standalone; the cell side dials with `--register-host/--register-port/--cell-id`, link-grace freeze and epoch-watermark isolation use `--link-grace-ms`, and the multi-Space declaration is `--add-space`; the World manager also parses and honors `--add-space` (via the embedded CellApp's `config.spaces`, registered one-by-one at the startup window), so `--add-space` is a declaration face shared by both the World manager and the cellServer executor; the World-side management faces are cell registration admission 960/961 (including contract-compat adjudication with code 5), the heartbeat expiry window `--cell-expiry-ms`, World allocation 962 `--assign-spaces` (including expiry release, re-claim, revocation markers, and occupied-space revoke retry; entries may declare capability-bit requirements `<id>[:<needs>]`), the cross-restart persistent tombstone `--world-journal`, and disconnect suspect-marking; topology versioning/rebalancing orchestration follows later P2 items) |
| `machined/` | Process supervision (roster spawn/backoff restart/death reporting) |
| `admin/`, `fleetctl/`, `logger/` | Operations and measurement tools |
| `apolloc/` | Project scaffold CLI (`apolloc init <path>` creates a minimal skeleton; template pending P0.2) |

## modules/

| Directory | Description |
|------|------|
| `base/` | Base utilities and foundation capabilities |
| `core/` | Core framework capabilities |
| `contract/` | Contract sources and generator integration (XML contracts) |
| `data/` | Data access and cache abstraction |
| `game/` | Game logic domain |
| `net/` | Networking and protocols |
| `protocol/` | Protocol layer |
| `runtime/` | Runtime and hosting |
| `bigworld/` | BigWorld compatibility layer |

## docs/

| Directory | Description |
|------|------|
| `.vitepress/` | Site configuration and theme |
| `guide/` | Getting-started guides |
| `architecture/` | Architecture docs (ADRs and semantic reference material) |
| `analysis/` | Framework comparative analysis |
| `design/` | Design artifacts (designs and batch records archived by domain) |
| `apps/` | Application design docs |
| `modules/` | Module design docs |
| `api/` | API reference |
| `qa/` | Q&A index |
| `analysis/` | Current architecture status and gap analysis |
| `sdks/` | SDK docs |
| `public/` | Static assets |

## Notes

The repository keeps both `sdks/` and `skds/` directory names. `sdks/` is the main
directory for contract sources and client SDKs (contract/gen/cpp/unity), while
`skds/` is the multi-platform SDK workspace and a historical directory
(cocos/laya). The documentation is organized by actual usage, no longer laid out
according to the legacy single-tree structure.
