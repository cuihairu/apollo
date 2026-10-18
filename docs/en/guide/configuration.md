---
title: Configuration
icon: settings
order: 6
prev: /en/guide/module-system
---

# Configuration

Apollo currently uses an in-process `ConfigRegistry` and application-specific configuration structures. There is no unified configuration-file loader, environment override precedence, or command-line parser.

## In-process configuration

`apollo::core::config::ConfigRegistry` exposes `set`, `get_string`, `get_int64`, `get_bool`, and `has`. It is a key/value registry; it does not read files, validate schemas, or publish changes.

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));
const auto port = config.get_int64("server.port");
```

## Application configuration

Configuration must reflect the target roles: loginServer configures authentication/directories; gateway configures ingress and separate Base/cell routes; Base configures ownership and database clients; World configures executor registration, capacity, topology, and admission; cell configures its World, simulation, and Base lookup; database configures storage partitions/connections/queues; machined configures only local supervision.

Logical identities and endpoints are separate. A player keeps its home server, Base, and storage affiliation when changing World. A standalone World needs service-directory configuration, not home database credentials or a mandatory local machined. Service generation, player ownership generation, simulation generation, session version, and storage schema version have distinct meanings.

Existing LoginConfig, GatewayConfig, and WorldAppConfig do not yet implement this complete split. Remove profile/direct-storage ownership from World and move it behind Base/database entry points. Current executable CLI flags are implementation details discoverable through --help, not a second deployment architecture. See [application mappings](../apps/index.md) and root todo.md.

The authenticator remains a prototype; test profiles do not establish production account registration. Cross-server grouping, admission, and return remain [reserved design](../design/cross-server-scenes.md), not enabled deployment options.

## Data tables and boundaries

`TableLoader` in `include/apollo/config/table_loader.h` reads `*|*`-separated tables: description row, field-name row, then data rows. Subclasses consume rows through `onRowRead()`.

Unified CLI parsing, configuration schema validation, YAML/TOML loading, and gameplay configuration snapshot replacement are not wired. Database fields in a configuration structure do not establish a live MySQL or Redis persistence path. See the [architecture overview](./architecture.md) for update and drain boundaries.
