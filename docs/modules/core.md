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

```cpp
#include <apollo/core/di/application_context.hpp>

using namespace apollo::core;

// 定义服务
class DatabaseService {
public:
    void connect() { /* ... */ }
};

// 注册服务
auto& container = ApplicationContext::instance();
container.registerSingleton<DatabaseService>();

// 解析服务
auto db = container.resolve<DatabaseService>();
db->connect();
```

### 配置 (Config)

```cpp
#include <apollo/core/config.hpp>

auto& config = ConfigManager::instance();
config.load("config.json");

int port = config.get<int>("server.port");
```

### 日志 (Log)

```cpp
#include <apollo/core/log.hpp>

using namespace apollo::core;

// 基础使用
LOG_INFO("MyLogger", "这是一条信息");
LOG_WARN("MyLogger", "这是一条警告: {}", errorCode);
LOG_ERROR("MyLogger", "错误: {}", error);

// 配置日志
LogManager::instance().configure({
    .level = LogLevel::INFO,
    .file = "logs/server.log",
    .console = true
});
```

### 生命周期 (Lifecycle)

```cpp
#include <apollo/core/application_lifecycle.hpp>

class MyApplication : public Application {
public:
    void start() override {
        LOG_INFO("App", "启动中...");
        // 初始化逻辑
    }

    void stop() override {
        LOG_INFO("App", "关闭中...");
        // 清理逻辑
    }

    State getState() const override {
        return state_;
    }

private:
    State state_ = State::STOPPED;
};
```

### 事件 (Event)

```cpp
#include <apollo/core/event_bus.hpp>

// 定义事件
struct PlayerLoginEvent {
    uint64_t playerId;
    std::string playerName;
};

// 订阅事件
EventBus::instance().subscribe<PlayerLoginEvent>([](const PlayerLoginEvent& e) {
    LOG_INFO("Event", "玩家 {} 登录", e.playerName);
});

// 发布事件
EventBus::instance().publish(PlayerLoginEvent{12345, "Player1"});
```

### 定时器 (Timer)

```cpp
#include <apollo/core/timer.hpp>

// 一次性定时器
Timer::setTimeout(1000, []() {
    LOG_INFO("Timer", "1秒后执行");
});

// 周期定时器
Timer::setInterval(5000, []() {
    LOG_INFO("Timer", "每5秒执行");
});

// 取消定时器
auto id = Timer::setTimeout(1000, callback);
Timer::cancel(id);
```

## 依赖

- apollo::base

## 链接

```cmake
find_package(apollo-core REQUIRED)
target_link_libraries(my_app apollo::core)
```
