---
title: Runtime API
icon: play
prev: /api/README.md
---

# Runtime API

## apollo::runtime::ApplicationHost

```cpp
namespace apollo::runtime {
enum class StopReason {
    Completed = 0,
    ConsoleRequested,
    SignalRequested,
    StartupFailed,
};

class IHostedService : public apollo::core::IApplicationLifecycle {
public:
    virtual std::string_view service_name() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual void tick() {}
};

class ApplicationHost {
public:
    using ShutdownHook = std::function<void(StopReason)>;

    void add_service(std::shared_ptr<IHostedService> service);
    void add_shutdown_hook(ShutdownHook hook);
    void set_console_source(std::unique_ptr<IConsoleEventSource> console_source);
    void set_signal_source(std::unique_ptr<ISignalSource> signal_source);
    void request_stop(StopReason reason);

    bool start();
    int run();
    int run_once();
    void stop();

    apollo::core::ApplicationPhase phase() const;
    StopReason stop_reason() const;
    bool is_running() const;
};
}
```

`request_stop()` 设计为可从非宿主线程投递停止请求。

---

## apollo::runtime::ServiceHost

```cpp
namespace apollo::runtime {
class ServiceHost {
public:
    void add_service(std::shared_ptr<IHostedService> service);
    void add_shutdown_hook(ApplicationHost::ShutdownHook hook);
    void set_console_source(std::unique_ptr<IConsoleEventSource> console_source);
    void set_signal_source(std::unique_ptr<ISignalSource> signal_source);

    bool start();
    int run();
    int run_once();
    void request_stop(StopReason reason);
    void stop();
};
}
```

---

## apollo::runtime::QueueConsoleEventSource

```cpp
namespace apollo::runtime {
class QueueConsoleEventSource : public IConsoleEventSource {
public:
    void push_command(std::string command);
    bool poll(ConsoleEvent& event) override;
};
}
```

---

## apollo::runtime::ProcessSignalSource

```cpp
namespace apollo::runtime {
class ProcessSignalSource : public ISignalSource {
public:
    ProcessSignalSource();
    explicit ProcessSignalSource(std::initializer_list<int> signals);
    bool poll(SignalEvent& event) override;
};
}
```
