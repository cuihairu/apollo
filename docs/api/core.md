---
title: Core API
icon: core
prev: /api/README.md
---

# Core API

> 2026-10-10 对账：本页按 HEAD 实况重写（modules/core 八头 + log 树）。旧稿的
> `apollo::core::Application`、`ConfigManager`、`EventBus`、`LOG_INFO(logger, ...)`
> 宏在仓库中不存在；定时器轮属 base 模块（TimerWheel，G-4）。

## apollo::core::IApplicationLifecycle（application_lifecycle.hpp）

六钩子生命周期接口——宿主（runtime 模块 ApplicationHost）按相位回调：

```cpp
namespace apollo::core {
enum class ApplicationPhase {
    Boot, ConfigLoaded, Initialized, Ready, Stopping, Stopped,
};

struct ApplicationReloadContext { /* 热更上下文 */ };

class IApplicationLifecycle {
public:
    virtual ~IApplicationLifecycle() = default;
    virtual void on_application_boot() = 0;
    virtual void on_application_config_loaded() = 0;
    virtual void on_application_initialized() = 0;
    virtual void on_application_ready() = 0;
    virtual void on_application_reload(const ApplicationReloadContext&) = 0;
    virtual void on_application_stop() = 0;
};
}
```

`lifecycle_component.hpp` 另有 `LifecycleComponent`（钩子默认空实现的便利基类）；
`module_manifest.hpp` 有 `ModuleManifest`（模块注册元数据结构）。

---

## apollo::core::config::ConfigRegistry（config/config_registry.hpp）

键值配置注册表（string / int64 / bool 三型），进程内读写互斥：

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

ConfigRegistry& global_config();   // 进程级实例
}
```

热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6，未实现）。
**线程安全**: 安全（shared_mutex）

---

## apollo::core::di（di/application_context.hpp）

极简构造注入容器：类型键 bean 图、拓扑序装配，仅 Singleton/Prototype 两档作用域。
装配只发生在应用入口（apps/），无注解无注册宏。

```cpp
#include <apollo/core/di/application_context.hpp>

apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<DatabaseService, ConfigDep>()   // 模板参 = Impl, Deps...
       .name("database")
       .as<IDatabase>()          // 暴露为基类接口（Impl 须派生 Base）
       .eager(true)              // initialize 时立即装配
       .tag("storage");
builder.add_prototype<TransientService>();
auto context = builder.build();
if (!context.initialize()) { /* 装配失败（缺依赖/环） */ }

auto& db   = context.get<IDatabase>();          // 取（缺 bean 抛错）
auto* opts = context.try_get<ConfigDep>();      // 取（可空）
auto& nm   = context.get_named<IDatabase>("database");
auto  all  = context.get_all<IDatabase>();      // 同类型多 bean
context.shutdown();
```

`BeanBuilder` 定制面：`name` / `tag` / `eager` / `as<Base>()` / `depends_on<Dep>()`。
**线程安全**: 不安全（装配期单线程纪律；`initialize` 后 `get` 读并发安全）

---

## apollo::core::log（log/ 八头树）

`LogManager` 单例（零配置自动初始化，`shutdown()` 显式收口）：

```cpp
namespace apollo::core::log {
enum class LogLevel : uint32_t {
    None = 0, Debug, Info, Warning, Error, Critical, All = 0xFFFF,
};
inline constexpr LogLevel Warn = LogLevel::Warning;   // 兼容别名

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

配置工厂：`LogManagerConfig::createDefault()` / `createConsoleOnly(level)` /
`createCombined(path, level)` / `createFileOnly(path, level)`。

**便捷宏（仅两个——格式化走 logger 方法链）**：

```cpp
APOLLO_LOG()->info("boot complete");          // 默认 logger
APOLLO_LOG()->warn("low disk: {}", n);        // fmt 直传
APOLLO_LOG_GET("network")->error("timeout");  // 具名 logger
```

> 注意：`APOLLO_LOG_INFO/LOG_ERROR(logger, ...)` 一类宏**不存在**（旧稿虚构）；
> `modules/core/log/include/apollo/core/log/log.h` 头部 doxygen 示例里的
> `APOLLO_LOG_INFO` 同为陈旧注释，以本页与 `log_manager.hpp` 宏定义为准。

**线程安全**: 安全（内部锁；structured.h 为结构化行格式出口，logging §5.1）

---

**core 模块不含**：EventBus（现有事件面是专用的 EventSink——PlayerDirectory
在线目录事件、SceneAoi AOI 三事件；通用总线经 glossary 立项后才落）、
健康检查（P3-3 立项）、定时器（归 base::TimerWheel）。
