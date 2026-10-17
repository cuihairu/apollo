---
title: Modules Overview
---

# Modules Overview

Apollo's module split follows one principle:

`Stabilize the underlying capabilities first, then assemble by game domain, and never let BigWorld semantics pollute the base layer in reverse.`

Current modules, by responsibility:

| Module | Role |
|------|------|
| [Base](/en/modules/base) | Time, thread pool, memory, strings, IDs and other infrastructure |
| [Core](/en/modules/core) | Config, logging, lifecycle, DI, module registration and other framework kernel pieces |
| [Runtime](/en/modules/runtime) | ApplicationHost, WorldHost, ServiceHost and other host runtimes |
| [Script](/en/modules/script) | Lua sandbox host (scripting-lua.md: whitelist / budget / fault isolation / module injection) |
| [Data](/en/modules/data) | Connection abstraction, SQL templates, write-behind journal, in-process cache |
| [Net](/en/modules/net) | Transport, protocol, message codec, session communication |
| [Game](/en/modules/game) | Entities, AOI, battle, attributes, session directory and world logic |
| [BigWorld](/en/modules/bigworld) | BigWorld API compatibility layer (thin migration surface for entities/callbacks/timers) |
| [Contract](/en/design/sdk-contract) | Contract message definition and generation (messages.xml → C++/Lua/JSON) |
| [Protocol](/en/api/net) | Message codec and URL routing (codec + url) |

Recommended reading order:

1. [Base](/en/modules/base)
2. [Core](/en/modules/core)
3. [Runtime](/en/modules/runtime)
4. [Net](/en/modules/net)
5. [Data](/en/modules/data)
6. [Game](/en/modules/game)
7. [BigWorld](/en/modules/bigworld)
