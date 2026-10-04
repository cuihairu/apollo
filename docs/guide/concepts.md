---
title: 核心概念
icon: lightbulb
order: 4
prev: /guide/quick-start
next: /guide/module-system
---

# 核心概念

## 应用 (Application)

Application 是 Apollo 的基本运行单元，代表一个可启动/停止的服务。业务侧实现
`IHostedService`（start/stop/tick），宿主 `ApplicationHost`/`ServiceHost` 帧驱动托管。

```cpp
class MyService : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override { return "my-service"; }
    bool start() override { running_ = true; return true; }
    void stop() override { running_ = false; }
    bool is_running() const override { return running_; }
    void tick() override { /* 定帧业务 */ }

private:
    bool running_ = false;
};
```

## 模块 (Module)

Apollo 采用模块化架构，每个模块提供特定的功能。

| 模块 | 功能 |
|------|------|
| `apollo::base` | 基础工具类 |
| `apollo::core` | 核心框架 |
| `apollo::runtime` | 运行时宿主 |
| `apollo::net` | 网络通信 |
| `apollo::data` | 数据访问 |
| `apollo::game` | 游戏逻辑 |

## 依赖注入 (DI)

Apollo 提供构造注入容器 `apollo::core::di`：类型键 bean 图、拓扑序装配、
仅 Singleton/Prototype 两档作用域。装配只发生在应用入口（apps/）。

```cpp
#include <apollo/core/di/application_context.hpp>

apollo::core::di::ApplicationContextBuilder builder;
builder.add_singleton<GameClockService>().name("game_clock");
builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
auto context = builder.build();
if (!context.initialize()) {
    return 1;
}
```

## 配置 (Config)

`apollo::core::config::ConfigRegistry` 键值注册表（string/int64/bool）；
热更规划走 tick 边界换 ConfigSnapshot（architecture-review §17.6，未实现）。

```cpp
#include <apollo/core/config/config_registry.hpp>

apollo::core::config::ConfigRegistry config;
config.set("server.port", 8888);
int64_t port = config.get_int64("server.port");
```

## 日志 (Log)

分级日志（`apollo::core::log`，自行开发多级别；文件/控制台输出）。

```cpp
#include <apollo/core/log/log_manager.hpp>

APOLLO_LOG_INFO("这是一条信息");
APOLLO_LOG_WARN("这是一条警告");
APOLLO_LOG_ERROR("这是一条错误");
```

## 实体 (Entity)

游戏世界中的基本对象：`apollo::game::core::Entity`（`EntityId` 强分型，
`PlayerId`/`EntityId` 互不隐式转换），支持组件挂载（`IEntityComponent`）。

```cpp
#include <apollo/game/core/entity.hpp>

apollo::game::core::Entity entity(apollo::game::core::EntityId{12345}, "Player");
entity.add_component<HealthComponent>();
auto health = entity.get_component<HealthComponent>();
```

## AOI (Area of Interest)

视野管理：`apollo::game::world::SceneAoi` 九宫格（每个 Scene 独享一个实例，
天然按 scene 隔离），`ViewerState` 记录逐观察者视野水位，Enter/Sync/Leave
事件面经 sink 分发（下发网关面留 P3-2）。

```cpp
#include <apollo/game/world/scene_aoi.hpp>

apollo::game::world::SceneAoi aoi(1000.0f, 1000.0f, 100.0f, 200.0f);
aoi.enter(apollo::game::core::EntityId{1001}, {100.0f, 0.0f, 100.0f});
auto viewers = aoi.viewers_at({100.0f, 0.0f, 100.0f});
```

## Actor 模型

> **未实现**（2026-10-04 对账）：仓库无 Actor 代码。现网并发模型 = 定帧
> 场景线程 + 单写者纪律（concurrency C-4：tick 归场景线程，IO 不持游戏
> 数据锁）；「每 Actor 独立消息队列」的示例代码为旧稿残留，随 P4-1 清理。

## 下一步

了解 [模块系统](./module-system.md)。
