---
title: Runtime API
icon: play
prev: /api/README.md
---

# Runtime API

> 2026-10-10 对账：本页按 HEAD 实况重写（modules/runtime 七头：application_host/
> console_event_source/crash_capture/runtime_manifest/service_host/signal_source/
> world_host）。旧稿的
> `Console`/`Signal`/`HealthCheck` 单例类在仓库中不存在——控制台与信号是
> 注入式事件源接口（poll 模型），健康检查未实现（P3-3 立项）。

## apollo::runtime::ApplicationHost（application_host.hpp）

宿主内核：帧驱动托管 `IHostedService`（start/stop/tick），六阶段状态机
（Boot→ConfigLoaded→Initialized→Ready→Stopping→Stopped），控制台/信号事件源
注入 + 关闭钩子。入口应用一般用同头文件的 `ServiceHost` 薄包装（二选一）。

```cpp
namespace apollo::runtime {
enum class StopReason { Completed = 0, ConsoleRequested, SignalRequested, StartupFailed };

struct ConsoleEvent { std::string command; };
struct SignalEvent  { int value = 0; };

class IHostedService : public apollo::core::IApplicationLifecycle {
public:
    virtual std::string_view service_name() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual void tick() {}
};

class IConsoleEventSource { public: virtual bool poll(ConsoleEvent&) = 0; };
class ISignalSource      { public: virtual bool poll(SignalEvent&)  = 0; };

class ApplicationHost {
public:
    using ShutdownHook = std::function<void(StopReason)>;

    void add_service(std::shared_ptr<IHostedService> service);
    void add_shutdown_hook(ShutdownHook hook);
    void set_console_source(std::unique_ptr<IConsoleEventSource> source);
    void set_signal_source(std::unique_ptr<ISignalSource> source);
    void request_stop(StopReason reason);       // async-signal-safe 路径配用

    bool start();                               // 任一服务 start 失败 = false
    int run_once();                             // 单帧：服务 tick + 事件源 poll
    void stop();

    apollo::core::ApplicationPhase phase() const;
    StopReason stop_reason() const;
    bool is_running() const;
};
}
```

`ServiceHost`（同头文件）为其薄包装：`add_service` / `add_shutdown_hook` /
`set_console_source` / `set_signal_source` / `start` / `run_once` / `is_running` /
`stop` 逐一转发——apps/game-server 即用此形态。

**线程安全**: 条件安全（`request_stop` 可自信号处理器调用；装配期单线程）

---

## apollo::runtime::WorldHost（world_host.hpp）

世界服务宿主（`IHostedService` 实现）：固定拍率驱动 `IWorldService` 列表，
默认 10Hz（clock-and-time §3，BW/KBE 同型），拍率可调：

```cpp
namespace apollo::runtime {
struct WorldTickContext {
    std::uint64_t tick_index = 0;   // 进程内单调，重启归零，0 = 未启动哨兵
    double delta_seconds = 0.0;
    std::chrono::steady_clock::time_point now{};
};

class IWorldService {
public:
    virtual ~IWorldService() = default;
    virtual std::string_view service_name() const = 0;
    virtual bool initialize() = 0;
    virtual void tick(const WorldTickContext& context) = 0;
    virtual void shutdown() = 0;
};

class WorldHost final : public IHostedService {
public:
    static constexpr std::uint32_t kDefaultTickRate = 10;
    explicit WorldHost(std::uint32_t tick_rate_hz = kDefaultTickRate, NowFn now_fn = ...);

    void set_tick_rate(std::uint32_t tick_rate_hz);
    std::uint32_t tick_rate() const;
    void add_world_service(std::shared_ptr<IWorldService> service);
};
}
```

固定步长全序推进；追赶有界——单次 tick 至多连跑 kCatchUpMaxSteps 补积压，
超出跳拍 + 告警 + 计 dropped_ticks；时间源可注入（now_fn）便于确定性单测。

**线程安全**: 不安全（tick 归宿主线程单写者）

---

## apollo::runtime::init_crash_capture（crash_capture.hpp）

进程最早段崩溃采集初始化（Crashpad out-of-process handler，本地模式 fail-open）：

```cpp
namespace apollo::runtime {
struct CrashCaptureStatus {
    bool enabled = false;            // handler 是否成功启动
    bool compiled_in = false;        // 是否带 crashpad（APOLLO_HAS_CRASHPAD）
    std::string database_dir;        // dump 数据库目录
    std::string handler_path;        // 定位到的 handler 可执行文件
    std::size_t pending_reports = 0; // 上次运行遗留 Pending 报告数（sweep）
};

CrashCaptureStatus init_crash_capture(int argc, char* const argv[],
                                      const std::string& proc_name);
}
```

自扫 `--crash-dump-dir` / `--crash-handler-path`（其余参数原样透传）；缺省
db = `crashdumps/<proc_name>/`，handler 三级定位（显式 → 可执行文件同级 →
编译期缓存）。`enabled=false` 时进程照常继续——采集是增强不是依赖。
全 app main 最早段接线（七 app）。

---

## apollo::runtime::RuntimeManifest（runtime_manifest.hpp）

进程装配元数据：`struct RuntimeManifest { std::string_view name, layer, description; }`，
经 `runtime_manifest()` 访问器暴露本模块清单（模块自省面）。

---

**runtime 模块不含**：交互式控制台与默认信号源（`IConsoleEventSource`/
`ISignalSource` 仓库现状只有测试替身——apps 用 csignal 自管 + request_stop
先例见 quick-start）、健康检查设施（`health_check.hpp` 不存在，P3-3 立项）。
