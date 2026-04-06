#pragma once

#include "apollo/runtime/application_host.hpp"

namespace apollo::runtime {

class ServiceHost {
public:
    ServiceHost() = default;

    void add_service(std::shared_ptr<IHostedService> service);
    void add_shutdown_hook(ApplicationHost::ShutdownHook hook);
    void set_console_source(std::unique_ptr<IConsoleEventSource> console_source);
    void set_signal_source(std::unique_ptr<ISignalSource> signal_source);

    bool start();
    int run();
    int run_once();
    void request_stop(StopReason reason);
    void stop();

    apollo::core::ApplicationPhase phase() const;
    StopReason stop_reason() const;
    bool is_running() const;

    ApplicationHost& application_host() { return application_host_; }
    const ApplicationHost& application_host() const { return application_host_; }

private:
    ApplicationHost application_host_;
};

using DefaultServiceHost = ServiceHost;

} // namespace apollo::runtime
