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

Runtime 模块提供宿主运行时，负责服务生命周期、停止请求、控制台事件和进程信号处理。

## ApplicationHost

应用程序宿主，管理一组 `IHostedService` 的生命周期。

```cpp
#include <apollo/runtime/application_host.hpp>
#include <apollo/runtime/console_event_source.hpp>

class GameServerService final : public apollo::runtime::IHostedService {
public:
    std::string_view service_name() const override {
        return "game_server";
    }

    bool start() override {
        running_ = true;
        return true;
    }

    void stop() override {
        running_ = false;
    }

    bool is_running() const override {
        return running_;
    }

private:
    bool running_ = false;
};

int main() {
    apollo::runtime::ApplicationHost host;
    host.add_service(std::make_shared<GameServerService>());

    auto console = std::make_unique<apollo::runtime::QueueConsoleEventSource>();
    console->push_command("quit");
    host.set_console_source(std::move(console));

    host.add_shutdown_hook([](apollo::runtime::StopReason reason) {
        (void)reason;
    });

    return host.run();
}
```

## ServiceHost

`ServiceHost` 是 `ApplicationHost` 的服务侧包装，接口保持一致，便于 app 入口直接表达“这是一个服务宿主”。

```cpp
#include <apollo/runtime/service_host.hpp>

int main() {
    apollo::runtime::ServiceHost host;
    host.add_service(std::make_shared<GameServerService>());
    return host.run();
}
```

## 控制台事件源

```cpp
#include <apollo/runtime/console_event_source.hpp>

apollo::runtime::QueueConsoleEventSource console;
console.push_command("quit");
```

## 信号处理

```cpp
#include <apollo/runtime/signal_source.hpp>

apollo::runtime::ProcessSignalSource signals{SIGINT, SIGTERM};
```

`ApplicationHost::request_stop()` 可由其他线程调用，停止会在当前或下一次 `run_once()` 中收敛，并触发 shutdown hooks。

## 依赖

- apollo::core

## 链接

```cmake
find_package(apollo-runtime REQUIRED)
target_link_libraries(my_app apollo::runtime)
```
