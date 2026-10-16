---
title: Configuration
icon: settings
order: 6
prev: /guide/module-system
---

[English](configuration.en.md) | [中文](configuration.md)

# Configuration

> 2026-10-04 reconciliation: this page was rewritten against the actual state of HEAD. The old draft's `load()`/`loadFromEnv()`/`get<T>()`/`onChange()`/`setOverrideStrategy()`/`parseArgs()`/`defineSchema()`/`validate()` do not exist;
> YAML/TOML is not supported. Environment-variable overrides, unified command-line parsing, and schema validation are all unimplemented.

Configuration is split into three layers, each with its own role:

| Layer | Location | Capability | Status |
| --- | --- | --- | --- |
| `ConfigRegistry` | `modules/core` | In-process key-value table (string/int64/bool) | Active; no file I/O |
| App self-read | `apps/*` | CLI argument parsing, design-table loading | Managed per app; no unified entry point |

## ConfigRegistry (in-process key-value)

`apollo::core::config::ConfigRegistry` (`modules/core`) is the thinner in-process registry:
`set` / `get_string` / `get_int64` / `get_bool` / `has`; no file I/O, no events.
Suited for registering in-code defaults in one place.

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));

int64_t port = config.get_int64("server.port");
bool has = config.has("server.port");
```

## App self-read

### CLI arguments

There is no unified command-line parsing layer; each app manages its own. base-app's approach is to parse into a `BaseConfig` struct at the entry point
(`apps/base-app/src/main.cpp`):

```cpp
BaseConfig loadConfig(int argc, char* argv[]) {
    BaseConfig config;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::atoi(argv[++i]));
        } else if (arg == "--db" && i + 1 < argc) {
            config.dbName = argv[++i];
        }
    }
    return config;
}
```

P3-1 has been closed out (configuration unified, legacy `ConfigManager` deleted, `d36529c5`); the active configuration surface = `ConfigRegistry` + app self-read.

### Design tables

`include/apollo/config/table_loader.h`: the `TableLoader` base class loads `*|*`-delimited text tables
(line 1 description, line 2 field names, data rows from line 3); subclasses override `onRowRead()` to consume row by row.

## Not-implemented list

The following capabilities appeared in the old draft but do not exist in the repository; start separate items as needed:

- Environment-variable overrides and priority strategies (EnvFirst and the like)
- Unified command-line parsing (`--server.port=8888` style)
- Schema definition and validation
- YAML/TOML formats (CMake detects yaml-cpp, but the parser is not wired in)
- `ConfigSnapshot` hot reload for the game hot path (planned in architecture-review §17.6)
