---
title: Runtime Module
icon: play
order: 3
category:
  - Modules
tag:
  - runtime
---

# Runtime Module

The Runtime module provides the host runtime that manages the application lifecycle.

## ApplicationHost

The application host, driving `IHostedService` by frames (start/stop/tick), with a
six-phase state machine (Boot→…→Stopped) and shutdown hooks. Business services implement
the interface; the host owns the post-assembly lifecycle.

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
        APOLLO_LOG()->info("启动中...");
        // 初始化逻辑
        running_ = true;
        return true;
    }

    void stop() override {
        APOLLO_LOG()->info("关闭中...");
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
        APOLLO_LOG()->info("关闭钩子执行");
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

The ApplicationHost core (the host kernel) and ServiceHost share one header; ServiceHost is
its thin wrapper, and host entries pick one of the two (current ServiceHost users are on
the test side: runtime_comprehensive_tests / test_runtime).

## ServiceHost

The service host: multiple services hang off the same host and are driven uniformly.

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

## Console Input

The host does not build in a console: inject `IConsoleEventSource` (`poll(ConsoleEvent&)`
returns whether there is an input event); the host pulls each frame. The repository
currently has only the test double (`TestConsoleEventSource` in tests/test_runtime.cpp);
the interactive console is not implemented.

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

## Signal Handling

The same injection pattern with `ISignalSource` (`poll(SignalEvent&)`); the repository
currently has only the test double — the default signal source
(SIGINT/SIGTERM → request_stop) is not implemented.

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

## Health Check

> **Not implemented** (2026-10-04 reconciliation): `apollo/runtime/health_check.hpp` does not
> exist; the repository has no health-check facility. The observability minimum
> (metrics/tracing) closed out with P3-3 (2026-10-13); the health_check interface is still
> not on the roadmap.

## Dependencies

- apollo::core

## Linking

```cmake
find_package(apollo-runtime REQUIRED)
target_link_libraries(my_app apollo::runtime)
```
