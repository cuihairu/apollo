---
title: Application Module Assembly
---

# Application Module Assembly

Entry points explicitly select modules, construct dependencies, and register hosts. Assembly does not add business processes or require an unimplemented runtime Starter container.

## Assembly and deployment

CMake targets define build/link dependencies. Application configuration selects enabled services. Hosts manage registered service lifecycles. These are distinct responsibilities.

| Role | Owned services | Boundary |
|---|---|---|
| loginServer | Authentication, service/online directories | No persistent gameplay profiles |
| gateway | Connections, admission, separate Base/cell routes | No simulation or profile writes |
| baseServer | `base::Avatar`, base gameplay, settlement validation | Storage requests go to database |
| worldServer | Executor registration, Space/Cell placement, topology, migrations | CellAppMgr-like management |
| cell | WorldHost, Scene/Instance, Avatar and Lua execution | Gameplay snapshots/Base associations; dedicated entry pending |
| database | Storage operations, queues, journal, connections | No reward or combat rules |
| machined | Local process launch, supervision, restart | No player ownership or cross-server placement |

apps/common is a reusable implementation library. Dedicated Base/database prototypes exist; default-fleet ownership wiring remains incomplete; see [applications](../apps/index.md). Sharing code does not merge roles. Standalone World registration does not require local machined.

## Dependencies

Applications link existing CMake targets. Dependencies must be explicit at build/startup rather than selected by string lookup during message handling or runtime scanning of process topology.

Use ApplicationContextBuilder for constructor injection and ApplicationHost / ServiceHost or WorldHost for lifecycle. Dependency injection does not change exclusive service ownership. Applications still contain explicit construction and shutdown logic; uniform container integration is not complete.

## Startup and shutdown

1. Validate application configuration and contract artifacts.
2. Initialize logging, crash capture, and required facilities.
3. Construct owned services and dependencies; start listeners and execution threads.
4. Open admission according to readiness.
5. Close new admission, drain work, release business state, then stop dependencies in reverse order.

These are assembly rules. Each entry point needs verified readiness, deadlines, and compensation. Linking successfully does not establish endpoint readiness or complete startup-failure recovery.

## Lua and updates

The current scripting backend uses the Lua C API and ScriptRuntime. Commit patches at the owning logic boundary without adding/removing C++ services at runtime. Validate code-only and protocol updates separately.

## Cross-server reservation

Local and cross-server scenes reuse World management, cell execution, and Scene/Instance primitives. Deployment may later select a World pool for cross-server activities without another process role or synonymous object. See [cross-server scenes](../design/cross-server-scenes.md).

## Related documentation

- [Hosts and dependency injection](./host-builder-and-di-design.md)
- [Server applications](../apps/index.md)
- [Active architecture decisions](./adr.md)
