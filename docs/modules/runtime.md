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

应用程序宿主，帧驱动托管 `IHostedService`（start/stop/tick），带六阶段
状态机（Boot→…→Stopped）与关闭钩子。业务服务实现接口，宿主负责装配后的
生命周期。

```cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/core/log/log_manager.hpp>

using apollo::runtime::IHostedService;
using apollo::runtime::ServiceHost;

// 定义服务
class GameServer : public IHostedService {
public:
    std::string_view service_name() const override { return "game-server"; }

    bool start() override {
        APOLLO_LOG_INFO("启动中...");
        // 初始化逻辑
        running_ = true;
        return true;
    }

    void stop() override {
        APOLLO_LOG_INFO("关闭中...");
        running_ = false;
    }

    bool is_running() const override { return running_; }
    void tick() override { /* 定帧业务 */ }

private:
    bool running_ = false;
};

int main() {
    ServiceHost host;
    host.add_service(std::make_shared<GameServer>());

    // 关闭钩子（入参 StopReason）
    host.add_shutdown_hook([](apollo::runtime::StopReason) {
        APOLLO_LOG_INFO("关闭钩子执行");
    });

    if (!host.start()) {
        return 1;
    }
    while (host.is_running()) {
        host.run_once();          // 单帧驱动：各服务 tick + 事件源 poll
    }
    host.stop();
    return 0;
}
```

ApplicationHost 本体（宿主内核）与 ServiceHost 同头文件；ServiceHost 是其
薄包装，入口应用按需二选一（apps/game-server 用 ServiceHost）。

## ServiceHost

服务宿主，多个服务挂同一宿主统一驱动。

```cpp
#include <apollo/runtime/application_host.hpp>

int main() {
    apollo::runtime::ServiceHost host;
    host.add_service(std::make_shared<NetworkService>());
    host.add_service(std::make_shared<DatabaseService>());
    host.add_service(std::make_shared<GameService>());

    if (!host.start()) {
        return 1;
    }
    while (host.is_running()) {
        host.run_once();
    }
    return 0;
}
```

## 控制台输入

宿主不内置控制台实现：注入 `IConsoleEventSource`（`poll(ConsoleEvent&)`
返回是否有输入事件），宿主在每帧 pull。仓库目前只有测试替身
（tests/test_runtime.cpp 的 `TestConsoleEventSource`），交互式控制台未实现。

```cpp
#include <apollo/runtime/console_event_source.hpp>

class MyConsoleSource : public apollo::runtime::IConsoleEventSource {
public:
    bool poll(apollo::runtime::ConsoleEvent& event) override {
        // 非阻塞读取一行输入，填充 event；无输入返回 false
        return false;
    }
};

int main() {
    apollo::runtime::ServiceHost host;
    host.set_console_source(std::make_unique<MyConsoleSource>());
    // ...
}
```

## 信号处理

同型注入 `ISignalSource`（`poll(SignalEvent&)`）；仓库现状只有测试替身，
默认信号源（SIGINT/SIGTERM → request_stop）未实现。

```cpp
#include <apollo/runtime/signal_source.hpp>

class MySignalSource : public apollo::runtime::ISignalSource {
public:
    bool poll(apollo::runtime::SignalEvent& event) override {
        // 非阻塞查询待处理信号；无信号返回 false
        return false;
    }
};

int main() {
    apollo::runtime::ServiceHost host;
    host.set_signal_source(std::make_unique<MySignalSource>());
    // ...
}
```

## 健康检查

> **未实现**（2026-10-04 对账）：`apollo/runtime/health_check.hpp` 不存在，
> 仓库无健康检查设施；接口面立项随 P3-3（可观测：metrics/tracing 最小集）。

## 依赖

- apollo::core

## 链接

```cmake
find_package(apollo-runtime REQUIRED)
target_link_libraries(my_app apollo::runtime)
```
