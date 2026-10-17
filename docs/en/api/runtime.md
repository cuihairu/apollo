---
title: Runtime API
icon: play
prev: /en/api/index.md
---

# Runtime API

> 2026-10-10 audit: this page was rewritten against the actual state of HEAD
> (seven `modules/runtime` headers: application_host/console_event_source/
> crash_capture/runtime_manifest/service_host/signal_source/world_host). The
> old draft's `Console`/`Signal`/`HealthCheck` singleton classes do not exist
> in the repository — console and signal are injected event-source interfaces
> (poll model), and health checks are not implemented (chartered in P3-3).

## apollo::runtime::ApplicationHost (application_host.hpp)

The host kernel: frame-driven hosting of `IHostedService` (start/stop/tick),
a six-phase state machine
(Boot→ConfigLoaded→Initialized→Ready→Stopping→Stopped), console/signal
event-source injection + shutdown hooks. Entry-point applications usually use
`ServiceHost` from the same header instead (a thin wrapper — pick one of the
two).

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
    void request_stop(StopReason reason);       // pair with async-signal-safe paths

    bool start();                               // any service start failure = false
    int run_once();                             // single frame: service ticks + event-source polls
    void stop();

    apollo::core::ApplicationPhase phase() const;
    StopReason stop_reason() const;
    bool is_running() const;
};
}
```

`ServiceHost` (same header) is its thin wrapper: `add_service` /
`add_shutdown_hook` / `set_console_source` / `set_signal_source` / `start` /
`run_once` / `is_running` / `stop` each forward directly — apps/game-server
uses this form.

**Thread safety**: conditionally safe (`request_stop` may be called from a
signal handler; single-threaded during assembly)

---

## apollo::runtime::WorldHost (world_host.hpp)

The world service host (an `IHostedService` implementation): drives a list of
`IWorldService` at a fixed tick rate, default 10Hz (clock-and-time §3, same
shape as BW/KBE), tick rate adjustable:

```cpp
namespace apollo::runtime {
struct WorldTickContext {
    std::uint64_t tick_index = 0;   // monotonic within the process, resets on restart, 0 = not-started sentinel
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

Fixed-step total-order advancement; bounded catch-up — a single tick runs at
most kCatchUpMaxSteps consecutive steps to drain backlog, beyond that it skips
ticks + alerts + counts dropped_ticks; the time source is injectable (now_fn)
for deterministic unit tests.

**Thread safety**: unsafe (ticks belong to the host thread, single writer)

---

## apollo::runtime::init_crash_capture (crash_capture.hpp)

Earliest-stage process crash capture initialization (Crashpad out-of-process
handler, local mode fail-open):

```cpp
namespace apollo::runtime {
struct CrashCaptureStatus {
    bool enabled = false;            // whether the handler started successfully
    bool compiled_in = false;        // whether crashpad is included (APOLLO_HAS_CRASHPAD)
    std::string database_dir;        // dump database directory
    std::string handler_path;        // the located handler executable
    std::size_t pending_reports = 0; // leftover Pending reports from the previous run (sweep)
};

CrashCaptureStatus init_crash_capture(int argc, char* const argv[],
                                      const std::string& proc_name);
}
```

Self-scans `--crash-dump-dir` / `--crash-handler-path` (other arguments pass
through untouched); default db = `crashdumps/<proc_name>/`, handler located in
three tiers (explicit → same directory as the executable → compile-time
cache). With `enabled=false` the process continues as normal — capture is an
enhancement, not a dependency.
Wired in the earliest segment of all app mains (eight apps).

---

## apollo::runtime::RuntimeManifest (runtime_manifest.hpp)

Process assembly metadata: `struct RuntimeManifest { std::string_view name, layer, description; }`,
exposed via the `runtime_manifest()` accessor as this module's manifest (the
module introspection surface).

---

**The runtime module does not include**: an interactive console or default
signal sources (`IConsoleEventSource`/`ISignalSource` currently have only test
doubles in the repo — apps self-manage via csignal + request_stop, precedent in
quick-start), or health-check facilities (`health_check.hpp` does not exist,
chartered in P3-3).
