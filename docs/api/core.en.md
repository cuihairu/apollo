---
title: Core API
icon: core
prev: /api/README.md
---

[English](core.en.md) | [中文](core.md)

# Core API

> 2026-10-10 audit: this page was rewritten against the actual state of HEAD
> (seven `modules/core` headers + the nine-piece log header tree). The old
> draft's `apollo::core::Application`, `ConfigManager`, `EventBus`,
> `LOG_INFO(logger, ...)` macros do not exist in the repository; the timer
> wheel belongs to the base module (TimerWheel, G-4).

## apollo::core::IApplicationLifecycle（application_lifecycle.hpp）

A six-hook lifecycle interface — the host (the runtime module's
ApplicationHost) calls back by phase:

```cpp
namespace apollo::core {
enum class ApplicationPhase {
    Boot, ConfigLoaded, Initialized, Ready, Stopping, Stopped,
};

struct ApplicationReloadContext { std::string_view reason; };  // hot-reload reason

class IApplicationLifecycle {
public:
    virtual ~IApplicationLifecycle() = default;
    virtual void on_application_boot() {}
    virtual void on_application_config_loaded() {}
    virtual void on_application_initialized() {}
    virtual void on_application_ready() {}
    virtual void on_application_reload(const ApplicationReloadContext&) {}
    virtual void on_application_stop() {}
};
}
```

`lifecycle_component.hpp` also provides `LifecycleComponent` (a convenience
base with empty hook implementations); `module_manifest.hpp` has
`ModuleManifest` (the module registration metadata struct).

---

## apollo::core::config::ConfigRegistry（config/config_registry.hpp）

A key-value configuration registry (string / int64 / bool), with in-process
read/write mutual exclusion:

```cpp
namespace apollo::core::config {
class ConfigRegistry {
public:
    void set(std::string key, std::string value);
    void set(std::string key, const char* value);
    void set(std::string key, int64_t value);
    void set(std::string key, bool value);

    bool has(const std::string& key) const;
    std::string get_string(const std::string& key, std::string default_value = {}) const;
    int64_t get_int64(const std::string& key, int64_t default_value = 0) const;
    bool get_bool(const std::string& key, bool default_value = false) const;
};

ConfigRegistry& global_config();   // process-level instance
}
```

Hot-reload planning goes through swapping ConfigSnapshot at tick boundaries
(architecture-review §17.6, not implemented).
**Thread safety**: safe (shared_mutex)

---

## apollo::core::di（di/application_context.hpp）

A minimal constructor-injection container: type-keyed bean graph, topological
assembly, only Singleton/Prototype scopes. Assembly happens only at the
application entry (apps/); no annotations, no registration macros.

```cpp
#include <apollo/core/di/application_context.hpp>

apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<DatabaseService, ConfigDep>()   // template args = Impl, Deps...
       .name("database")
       .as<IDatabase>()          // expose as a base interface (Impl must derive Base)
       .eager(true)              // assemble immediately at initialize
       .tag("storage");
builder.add_prototype<TransientService>();
auto context = builder.build();
if (!context.initialize()) { /* assembly failed (missing dep / cycle) */ }

auto& db   = context.get<IDatabase>();          // fetch (missing bean: debug assert aborts / release UB)
auto* opts = context.try_get<ConfigDep>();      // fetch (nullable)
auto& nm   = context.get_named<IDatabase>("database");
auto  all  = context.get_all<IDatabase>();      // multiple beans of one type
context.shutdown();
```

`BeanBuilder` customization surface: `name` / `tag` / `eager` / `as<Base>()` / `depends_on<Dep>()`.
**Thread safety**: unsafe (single-thread discipline during assembly; `get` reads are concurrency-safe after `initialize`)

---

## apollo::core::log（log/ nine-piece header tree）

The `LogManager` singleton (auto-initializes with zero config, `shutdown()`
closes it explicitly):

```cpp
namespace apollo::core::log {
enum class LogLevel : uint32_t {
    None = 0, Debug, Info, Warning, Error, Critical, All = 0xFFFF,
};
inline constexpr LogLevel Warn = LogLevel::Warning;   // compatibility alias

class LogManager {
public:
    static LogManager& instance();
    void initialize(const LogManagerConfig& config = LogManagerConfig::createDefault());
    void shutdown();

    LoggerPtr getDefaultLogger();
    LoggerPtr getLogger(const std::string& name);
    LoggerPtr createLogger(const std::string& name, LogLevel level = LogLevel::All);
    void removeLogger(const std::string& name);
    bool hasLogger(const std::string& name) const;
    std::vector<std::string> getLoggerNames() const;

    void setDefaultLevel(LogLevel level);
    void flushAll();
    void write(LogLevel level, std::string logger, std::string message);
};
}
```

Config factories: `LogManagerConfig::createDefault()` / `createConsoleOnly(level)` /
`createCombined(path, level)` / `createFileOnly(path, level)`.

**Convenience macros (only two — formatting goes through the logger method chain)**:

```cpp
APOLLO_LOG()->info("boot complete");          // default logger
APOLLO_LOG()->warn("low disk: {}", n);        // fmt passed straight through
APOLLO_LOG_GET("network")->error("timeout");  // named logger
```

> Note: macros like `APOLLO_LOG_INFO/LOG_ERROR(logger, ...)` **do not exist**
> (fictional in the old draft); the `APOLLO_LOG_INFO` in the doxygen example at
> the top of `modules/core/log/include/apollo/core/log/log.h` is likewise a
> stale comment — this page and the `log_manager.hpp` macro definitions are
> authoritative.

**Thread safety**: safe (internal locks; structured.h is the structured
line-format sink, logging §5.1)

---

**The core module does not include**: EventBus (the existing event surface is
the purpose-built EventSink — PlayerDirectory online-directory events, the
three SceneAoi AOI events; a general bus lands only after a glossary charter),
health checks (chartered in P3-3), timers (belongs to base::TimerWheel).
