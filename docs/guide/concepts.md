---
title: 核心概念
icon: lightbulb
order: 4
prev: /guide/quick-start
next: /guide/module-system
---

# 核心概念

## 应用 (Application)

Application 是 Apollo 的基本运行单元，代表一个可启动/停止的服务。

```cpp
class MyApplication : public core::Application {
public:
    void start() override {
        // 初始化逻辑
    }

    void stop() override {
        // 清理逻辑
    }
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

Apollo 提供 IoC 容器，支持依赖注入。

```cpp
// 注册服务
container().registerSingleton<DatabaseService>();

// 解析服务
auto db = container().resolve<DatabaseService>();
```

## 配置 (Config)

支持多种配置格式（JSON、YAML、TOML）。

```cpp
// 加载配置
auto& config = core::ConfigManager::instance();
config.load("config.json");

// 读取配置
int port = config.get<int>("server.port");
```

## 日志 (Log)

分级日志系统。

```cpp
LOG_INFO("LoggerName", "这是一条信息");
LOG_WARN("LoggerName", "这是一条警告");
LOG_ERROR("LoggerName", "这是一条错误");
```

## 实体 (Entity)

游戏世界中的基本对象。

```cpp
auto entity = std::make_shared<game::Entity>();
entity->id = 12345;
entity->position = {100, 200};
```

## AOI (Area of Interest)

视野管理系统，用于高效查询附近的实体。

```cpp
// 创建 AOI 管理器
auto aoi = std::make_shared<game::AOIManager>(1000, 1000, 100);

// 实体进入世界
aoi->enter(entity, {100, 100});

// 查询视野内的实体
auto viewers = aoi->getViewers({100, 100}, 200);
```

## Actor 模型

基于 Actor 的并发模型，每个 Actor 有独立的消息队列。

```cpp
class MyActor : public actor::Actor {
protected:
    void onMessage(const actor::Message& msg) override {
        // 处理消息
    }
};
```

## 下一步

了解 [模块系统](./module-system.md)。
