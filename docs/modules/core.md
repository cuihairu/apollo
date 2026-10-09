---
title: Core 模块
icon: core
order: 2
category:
  - 模块
tag:
  - core
  - 核心
---

# Core 模块

Core 模块提供框架内核，定义公共运行语义。

## 组件

### 依赖注入 (DI)

`apollo::core::di` 极简构造注入容器：类型键 bean 图、拓扑序装配、仅
Singleton/Prototype 两档作用域。装配只发生在应用入口（apps/），无注解无注册宏。

```cpp
#include <apollo/core/di/application_context.hpp>

// 定义服务（普通类，依赖走构造函数）
class DatabaseService {
public:
    void connect() { /* ... */ }
};

// 构建：builder 声明 bean，build 产出上下文，initialize 按拓扑序装配
apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<DatabaseService>().name("database");
auto context = builder.build();
if (!context.initialize()) {
    // 装配失败
}

// 解析：get<T>() 取按类型键；get_named<T>("...") 按名取
auto& db = context.get<DatabaseService>();
db.connect();
```

### 配置 (Config)

`apollo::core::config::ConfigRegistry` 键值注册表（string/int64/bool 三型）；
热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6，未实现）。

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
config.set("server.host", std::string("0.0.0.0"));

int64_t port = config.get_int64("server.port");
std::string host = config.get_string("server.host");
bool ok = config.has("server.port");
```

### 日志 (Log)

`apollo::core::log`：`LogManager` 单例（`initialize(config)` / `getLogger(name)` /
`createLogger(name, level)` / `removeLogger` / `hasLogger` / `flushAll` /
`shutdown`），配置工厂 `LogManagerConfig::createDefault/createConsoleOnly/
createCombined/createFileOnly`。**便捷宏只有两个**：`APOLLO_LOG()`（默认
logger）与 `APOLLO_LOG_GET(name)`（具名 logger）——格式化走 logger 方法链；
`APOLLO_LOG_INFO/LOG_WARN_F` 一类宏不存在（旧稿虚构，log.h 头部 doxygen
示例同为陈旧注释）。

```cpp
#include <apollo/core/log/log_manager.hpp>

APOLLO_LOG()->info("这是一条信息");
APOLLO_LOG()->warn("端口 {} 已变更", 8888);
APOLLO_LOG_GET("network")->error("timeout");

auto& mgr = apollo::core::log::LogManager::instance();
mgr.initialize(apollo::core::log::LogManagerConfig::createCombined("logs/server.log",
               apollo::core::log::LogLevel::Info));
auto logger = mgr.getLogger("MyLogger");
```

### 生命周期 (Lifecycle)

`IApplicationLifecycle` 六钩子（宿主按相位回调）：`on_application_boot` /
`on_application_config_loaded` / `on_application_initialized` /
`on_application_ready` / `on_application_reload` / `on_application_stop`。
`ApplicationHost` 的六阶段状态机（Boot→…→Stopped）见 runtime 模块。

```cpp
#include <apollo/core/application_lifecycle.hpp>

class MyApplication : public apollo::core::IApplicationLifecycle {
public:
    void on_application_ready() override {
        APOLLO_LOG()->info("应用就绪");
    }
    void on_application_stop() override {
        APOLLO_LOG()->info("应用停止");
    }
};
```

### 事件 (Event)

> **未实现**（2026-10-04 对账）：`apollo/core/event_bus.hpp` 不存在，仓库无
> 通用 EventBus。现有事件面是专用的：PlayerDirectory 经 `EventSink` 上报
> 在线目录事件（session_up/down/moved/kicked）、`SceneAoi::EventSink` 上报
> AOI 三事件——新事件先入 glossary 再按域落接口，不做通用总线。

### 定时器 (Timer)

时间轮在 base 模块：`apollo::base::TimerWheel`（G-4 库件，分层哈希、
`advance(now_ms)` 喂钟驱动，见 [Base 模块](/modules/base)）——core 模块本体
不含定时器。顶层遗留件 `include/apollo/core/timer/timer_manager.h`
（`TimerManager::setTimer/killTimer`）仅被 legacy GTest 批引用，不入门禁，
勿在新代码使用。

## 依赖

- apollo::base

## 链接

```cmake
find_package(apollo-core REQUIRED)
target_link_libraries(my_app apollo::core)
```
