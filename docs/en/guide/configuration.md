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

| Application | Configuration | Responsibility |
|---|---|---|
| Login | `login::LoginConfig` | Authentication, Gateway allocation, World profile access, and online directory |
| Gateway | `gateway::GatewayConfig` | Client listener, admission, directory queries, and World forwarding |
| World | `world_app::WorldAppConfig` | Simulation, scripts, player profiles, and persistence |

Each entry point parses its own CLI; consult that executable's `--help`. Library defaults used by tests can differ from services enabled by an executable entry point. Port 0 in a library configuration does not establish the executable's deployment port.

```bash
world-app --world-id 1 --m1-port 9101 --player-store-m1-port 9104 \
  --data-dir data/world-1/players --journal data/world-1/journal/persist.log
login-app --m1-port 9103 --world-store-m1-port 9104 --directory-port 9105 \
  --directory-epochs data/login/epochs.json --gateway tcp://127.0.0.1:9002
gateway-app --port 9002 --world-m1-port 9101 \
  --login-m1-port 9103 --directory-m1-port 9105
```

World derives separate profile and journal directories from `worldId`. Separate Worlds must not write the same local storage. Login stores directory authority epoch floors, not gameplay profiles. Its authenticator still uses prototype test accounts; a successful login also requires the corresponding profile in World. These commands are not a production account-provisioning or authentication solution.

Cross-server grouping, admission, and return policies are a [reserved design](../design/cross-server-scenes.md), without enabled deployment configuration.

## Data tables and boundaries

`TableLoader` in `include/apollo/config/table_loader.h` reads `*|*`-separated tables: description row, field-name row, then data rows. Subclasses consume rows through `onRowRead()`.

Unified CLI parsing, configuration schema validation, YAML/TOML loading, and gameplay configuration snapshot replacement are not wired. Database fields in a configuration structure do not establish a live MySQL or Redis persistence path. See the [architecture overview](./architecture.md) for update and drain boundaries.
