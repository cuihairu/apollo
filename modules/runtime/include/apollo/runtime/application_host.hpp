#pragma once

#include "apollo/core/application_lifecycle.hpp"

#include <memory>
#include <functional>
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
    int run_once();
    void stop();

    apollo::core::ApplicationPhase phase() const { return phase_; }
    StopReason stop_reason() const { return stop_reason_; }
    bool is_running() const { return running_; }

private:
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
};

class ServiceHost {
public:
    ServiceHost() = default;

    void add_service(std::shared_ptr<IHostedService> service) {
        application_host_.add_service(std::move(service));
    }

    void add_shutdown_hook(ApplicationHost::ShutdownHook hook) {
        application_host_.add_shutdown_hook(std::move(hook));
    }

    void set_console_source(std::unique_ptr<IConsoleEventSource> console_source) {
        application_host_.set_console_source(std::move(console_source));
    }

    void set_signal_source(std::unique_ptr<ISignalSource> signal_source) {
        application_host_.set_signal_source(std::move(signal_source));
    }

    bool start() {
        return application_host_.start();
    }

    int run_once() {
        return application_host_.run_once();
    }

    void request_stop(StopReason reason) {
        application_host_.request_stop(reason);
    }

    void stop() {
        application_host_.stop();
    }

    apollo::core::ApplicationPhase phase() const {
        return application_host_.phase();
    }

    StopReason stop_reason() const {
        return application_host_.stop_reason();
    }

private:
    ApplicationHost application_host_;
};

} // namespace apollo::runtime
