#include "apollo/core/config/config_registry.hpp"
#include "apollo/core/log/log_manager.hpp"
#include "apollo/runtime/application_host.hpp"

namespace apollo::runtime {

void ApplicationHost::add_service(std::shared_ptr<IHostedService> service) {
    if (service) {
        services_.push_back(std::move(service));
    }
}

void ApplicationHost::add_shutdown_hook(ShutdownHook hook) {
    if (hook) {
        shutdown_hooks_.push_back(std::move(hook));
    }
}

void ApplicationHost::set_console_source(std::unique_ptr<IConsoleEventSource> console_source) {
    console_source_ = std::move(console_source);
}

void ApplicationHost::set_signal_source(std::unique_ptr<ISignalSource> signal_source) {
    signal_source_ = std::move(signal_source);
}

void ApplicationHost::request_stop(StopReason reason) {
    running_ = false;
    stop_reason_ = reason;
}

bool ApplicationHost::start() {
    apollo::core::log::global_log_manager().write(
        apollo::core::log::LogLevel::Info, "runtime", "ApplicationHost starting");
    stop_reason_ = StopReason::Completed;
    transition_to(apollo::core::ApplicationPhase::Boot);
    notify_boot();

    transition_to(apollo::core::ApplicationPhase::ConfigLoaded);
    notify_config_loaded();

    transition_to(apollo::core::ApplicationPhase::Initialized);
    notify_initialized();

    for (const auto& service : services_) {
        if (!service->start()) {
            stop_reason_ = StopReason::StartupFailed;
            transition_to(apollo::core::ApplicationPhase::Stopping);
            notify_stop();
            transition_to(apollo::core::ApplicationPhase::Stopped);
            return false;
        }
    }

    running_ = true;
    transition_to(apollo::core::ApplicationPhase::Ready);
    notify_ready();
    apollo::core::log::global_log_manager().write(
        apollo::core::log::LogLevel::Info, "runtime", "ApplicationHost ready");
    return true;
}

int ApplicationHost::run_once() {
    if (!running_) {
        if (phase_ != apollo::core::ApplicationPhase::Stopped &&
            phase_ != apollo::core::ApplicationPhase::Stopping) {
            stop();
        }
        return 0;
    }

    ConsoleEvent console_event;
    if (console_source_ && console_source_->poll(console_event)) {
        request_stop(StopReason::ConsoleRequested);
    }

    SignalEvent signal_event;
    if (running_ && signal_source_ && signal_source_->poll(signal_event)) {
        (void)signal_event;
        request_stop(StopReason::SignalRequested);
    }

    if (running_) {
        for (const auto& service : services_) {
            service->tick();
        }
    }

    if (!running_) {
        stop();
    }

    return 0;
}

void ApplicationHost::stop() {
    if (phase_ == apollo::core::ApplicationPhase::Stopped) {
        return;
    }

    running_ = false;
    transition_to(apollo::core::ApplicationPhase::Stopping);
    notify_stop();

    for (auto it = services_.rbegin(); it != services_.rend(); ++it) {
        if ((*it)->is_running()) {
            (*it)->stop();
        }
    }

    run_shutdown_hooks();
    transition_to(apollo::core::ApplicationPhase::Stopped);
    apollo::core::log::global_log_manager().write(
        apollo::core::log::LogLevel::Info, "runtime", "ApplicationHost stopped");
}

void ApplicationHost::transition_to(apollo::core::ApplicationPhase next) {
    phase_ = next;
}

void ApplicationHost::run_shutdown_hooks() {
    for (auto it = shutdown_hooks_.rbegin(); it != shutdown_hooks_.rend(); ++it) {
        (*it)(stop_reason_);
    }
}

void ApplicationHost::notify_boot() {
    for (const auto& service : services_) {
        service->on_application_boot();
    }
}

void ApplicationHost::notify_config_loaded() {
    for (const auto& service : services_) {
        service->on_application_config_loaded();
    }
}

void ApplicationHost::notify_initialized() {
    for (const auto& service : services_) {
        service->on_application_initialized();
    }
}

void ApplicationHost::notify_ready() {
    for (const auto& service : services_) {
        service->on_application_ready();
    }
}

void ApplicationHost::notify_stop() {
    for (const auto& service : services_) {
        service->on_application_stop();
    }
}

} // namespace apollo::runtime
