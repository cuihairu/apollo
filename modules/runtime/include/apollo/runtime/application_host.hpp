#pragma once

#include "apollo/core/application_lifecycle.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace apollo::runtime {

enum class StopReason {
    Completed = 0,
    ConsoleRequested,
    SignalRequested,
    StartupFailed,
};

struct ConsoleEvent {
    std::string command;
};

struct SignalEvent {
    int value = 0;
};

class IConsoleEventSource {
public:
    virtual ~IConsoleEventSource() = default;
    virtual bool poll(ConsoleEvent& event) = 0;
};

class ISignalSource {
public:
    virtual ~ISignalSource() = default;
    virtual bool poll(SignalEvent& event) = 0;
};

class IHostedService : public apollo::core::IApplicationLifecycle {
public:
    virtual ~IHostedService() = default;
    virtual std::string_view service_name() const = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual void tick() {}
};

class ApplicationHost {
public:
    using ShutdownHook = std::function<void(StopReason)>;

    ApplicationHost() = default;

    ApplicationHost(const ApplicationHost&) = delete;
    ApplicationHost& operator=(const ApplicationHost&) = delete;

    void add_service(std::shared_ptr<IHostedService> service);
    void add_shutdown_hook(ShutdownHook hook);
    void set_console_source(std::unique_ptr<IConsoleEventSource> console_source);
    void set_signal_source(std::unique_ptr<ISignalSource> signal_source);
    void request_stop(StopReason reason);

    bool start();
    int run();
    int run_once();
    void stop();

    apollo::core::ApplicationPhase phase() const { return phase_; }
    StopReason stop_reason() const { return stop_reason_; }
    bool is_running() const { return running_; }

private:
    bool consume_stop_request(StopReason& reason);
    void stop_started_services(std::size_t started_count);
    void transition_to(apollo::core::ApplicationPhase next);
    void run_shutdown_hooks();
    void notify_boot();
    void notify_config_loaded();
    void notify_initialized();
    void notify_ready();
    void notify_stop();

    std::vector<std::shared_ptr<IHostedService>> services_;
    std::vector<ShutdownHook> shutdown_hooks_;
    std::unique_ptr<IConsoleEventSource> console_source_;
    std::unique_ptr<ISignalSource> signal_source_;
    apollo::core::ApplicationPhase phase_ = apollo::core::ApplicationPhase::Boot;
    StopReason stop_reason_ = StopReason::Completed;
    bool running_ = false;
    bool shutdown_hooks_ran_ = false;
    std::mutex stop_mutex_;
    bool stop_requested_ = false;
    StopReason requested_stop_reason_ = StopReason::Completed;
};

} // namespace apollo::runtime
