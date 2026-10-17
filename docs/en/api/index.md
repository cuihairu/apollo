---
title: API Reference
icon: code
order: 1
---

# API Reference

This section contains the API reference documentation for Apollo.

## Module APIs

- [Base API](./base.md) - base utility class API
- [Core API](./core.md) - core framework API
- [Runtime API](./runtime.md) - runtime API
- [Network API](./net.md) - network communication API
- [Data API](./data.md) - data access API
- [Game API](./game.md) - game logic API
- [BigWorld API](./bigworld.md) - BigWorld compatibility layer API

## Namespaces

All Apollo APIs live under the `apollo` namespace:

```cpp
#include <apollo/base/time.hpp>
#include <apollo/core/log/log_manager.hpp>
#include <apollo/runtime/application_host.hpp>

using apollo::base::Time;
using apollo::runtime::ServiceHost;
```

## Error Handling

Apollo has no unified exception base class (`apollo::Exception` does not exist); conventions are split three ways by domain:

```cpp
// 1) Exceptions: host/container classes throw std::runtime_error on
//    programming-error paths (e.g. submit after ThreadPool has stopped)
try {
    pool.submit(task);
} catch (const std::runtime_error& e) {
    APOLLO_LOG()->error("submit failed: {}", e.what());
}

// 2) Sentinel return values: pool exhaustion returns UINT32_MAX / nullptr
uint32_t id = pool.allocate();
if (id == UINT32_MAX) { /* pool exhausted */ }

// 3) Result structs: the data layer's QueryResult carries ok/error/affected_rows/rows
auto result = conn->execute_query(sql);
if (!result.ok) {
    APOLLO_LOG()->error("query failed: {}", result.error);
}
```

There are only two logging macros: `APOLLO_LOG()` (default logger) and
`APOLLO_LOG_GET(name)` (named logger); formatting goes through the logger
method chain (`->info/warn/error(...)`) — macros like
`LOG_ERROR(logger, ...)` do not exist.

## Thread Safety

Apollo API thread safety is classified in three levels:

- **Safe** - can be called safely from multiple threads
- **Conditionally safe** - requires external synchronization
- **Unsafe** - must be called on the same thread

The thread-safety level of each API is annotated in the documentation.
