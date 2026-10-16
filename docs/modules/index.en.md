---
title: Modules Overview
---

[English](index.en.md) | [中文](index.md)

# Modules Overview

Apollo's module split follows one principle:

`Stabilize the underlying capabilities first, then assemble by game domain, and never let BigWorld semantics pollute the base layer in reverse.`

Current modules, by responsibility:

| Module | Role |
|------|------|
| [Base](/modules/base) | Time, thread pool, memory, strings, IDs and other infrastructure |
| [Core](/modules/core) | Config, logging, lifecycle, DI, module registration and other framework kernel pieces |
| [Runtime](/modules/runtime) | ApplicationHost, WorldHost, ServiceHost and other host runtimes |
| [Script](/modules/script) | Lua sandbox host (scripting-lua.md: whitelist / budget / fault isolation / module injection) |
| [Data](/modules/data) | Connection abstraction, SQL templates, write-behind journal, in-process cache |
| [Net](/modules/net) | Transport, protocol, message codec, session communication |
| [Game](/modules/game) | Entities, AOI, battle, attributes, session directory and world logic |
| [BigWorld](/modules/bigworld) | BigWorld API compatibility layer (thin migration surface for entities/callbacks/timers) |
| [Contract](/modules/contract) | Contract message definition and generation (messages.xml → C++/Lua/JSON) |
| [Protocol](/modules/protocol) | Message codec and URL routing (codec + url) |

Recommended reading order:

1. [Base](/modules/base)
2. [Core](/modules/core)
3. [Runtime](/modules/runtime)
4. [Net](/modules/net)
5. [Data](/modules/data)
6. [Game](/modules/game)
7. [BigWorld](/modules/bigworld)
