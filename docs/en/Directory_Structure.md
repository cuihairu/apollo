
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
| `login-app/` | Login entry point |
| `gateway-app/` | Edge access layer |
| `baseappmgr/` | Fleet scheduling and recovery/directory mirroring |
| `base-app/` | Player anchor host |
| `room-server/` | RoomServer runtime prototype |
| `game-server/` | Compatibility or demo game service |
| `machined/` | Process supervision (roster spawn/backoff restart/death reporting) |

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
