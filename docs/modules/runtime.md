---
title: Runtime 模块
icon: play
order: 3
category:
  - 模块
tag:
  - runtime
  - 运行时
---

# Runtime 模块

Runtime 模块提供宿主运行时，管理应用程序的生命周期。

## ApplicationHost

应用程序宿主，管理应用的启动和关闭。

```cpp
#include <apollo/runtime/application_host.hpp>

using namespace apollo::runtime;

// 定义应用
class GameServer : public core::Application {
public:
    void start() override {
        LOG_INFO("GameServer", "启动中...");
        // 初始化逻辑
    }

    void stop() override {
        LOG_INFO("GameServer", "关闭中...");
        // 清理逻辑
    }
};

int main() {
    ApplicationHost host;

    // 注册应用
    host.registerApplication<GameServer>();

    // 添加关闭钩子
    host.addShutdownHook([]() {
        LOG_INFO("Host", "关闭钩子执行");
    });

    // 运行
    return host.run();
}
```

## ServiceHost

服务宿主，支持多个服务并行运行。

```cpp
#include <apollo/runtime/application_host.hpp>

int main() {
    ApplicationHost host;

    // 注册多个服务
    host.registerApplication<NetworkService>();
    host.registerApplication<DatabaseService>();
    host.registerApplication<GameService>();

    return host.run();
}
```

## 控制台输入

```cpp
#include <apollo/runtime/console.hpp>

// 启动控制台监听
Console::instance().start();

// 添加命令处理器
Console::instance().addCommand("help", []() {
    std::cout << "可用命令:" << std::endl;
    std::cout << "  help - 显示帮助" << std::endl;
    std::cout << "  stop - 停止服务器" << std::endl;
});

Console::instance().addCommand("stop", [&host]() {
    host.stop();
});
```

## 信号处理

```cpp
#include <apollo/runtime/signal.hpp>

// 注册信号处理器
Signal::instance().on(SIGINT, [&host]() {
    LOG_INFO("Signal", "收到 SIGINT，准备关闭");
    host.stop();
});

Signal::instance().on(SIGTERM, [&host]() {
    LOG_INFO("Signal", "收到 SIGTERM，准备关闭");
    host.stop();
});
```

## 健康检查

```cpp
#include <apollo/runtime/health_check.hpp>

// 注册健康检查
HealthCheck::instance().add("database", []() {
    return database->ping() ? HealthStatus::HEALTHY : HealthStatus::UNHEALTHY;
});

// 检查状态
auto status = HealthCheck::instance().check();
if (status == HealthStatus::HEALTHY) {
    LOG_INFO("Health", "系统健康");
}
```

## 依赖

- apollo::core

## 链接

```cmake
find_package(apollo-runtime REQUIRED)
target_link_libraries(my_app apollo::runtime)
```
