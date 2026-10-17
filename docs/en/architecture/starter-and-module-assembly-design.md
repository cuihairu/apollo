---
title: Application Module Assembly
---

# Application Module Assembly

Entry points explicitly select modules, construct dependencies, and register hosts. Assembly does not add business processes or require an unimplemented runtime Starter container.

## Assembly and deployment

CMake targets define build/link dependencies. Application configuration selects enabled services. Hosts manage registered service lifecycles. These are distinct responsibilities.

| Executable | In-process services | Boundary |
|---|---|---|
| Login | Authentication, OnlineDirectory, admission links | Does not own gameplay profiles |
| Gateway | Client connections, admission client, directory lookup, World egress | Does not advance gameplay simulation |
| World | WorldHost, World/Scene/Instance, PlayerStore, Lua | Owns current simulation and player profile write authority |

`apps/common` provides PlayerStore and OnlineDirectory implementations as `apollo_app_services`, not another business executable. Operations tools and machined run separately when deployment requires them.

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

Local and cross-server scenes reuse the same World executable and Scene/Instance types. Deployment may later select a World pool for cross-server activities without another process role or synonymous object. See [cross-server scenes](../design/cross-server-scenes.md).

## Related documentation

- [Hosts and dependency injection](./host-builder-and-di-design.md)
- [Server applications](../apps/index.md)
- [Active architecture decisions](./adr.md)
