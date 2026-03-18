#include "apollo/runtime/application_host.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct RecordingService final : apollo::runtime::IHostedService {
    std::vector<std::string> events;
    bool running = false;
    int ticks = 0;

    std::string_view service_name() const override {
        return "recording";
    }

    void on_application_boot() override { events.emplace_back("boot"); }
    void on_application_config_loaded() override { events.emplace_back("config"); }
    void on_application_initialized() override { events.emplace_back("init"); }
    void on_application_ready() override { events.emplace_back("ready"); }
    void on_application_stop() override { events.emplace_back("stop"); }

    bool start() override {
        running = true;
        events.emplace_back("start");
        return true;
    }

    void stop() override {
        running = false;
        events.emplace_back("service_stop");
    }

    bool is_running() const override {
        return running;
    }

    void tick() override {
        ++ticks;
    }
};

struct OneShotConsoleSource final : apollo::runtime::IConsoleEventSource {
    bool consumed = false;

    bool poll(apollo::runtime::ConsoleEvent& event) override {
        if (consumed) {
            return false;
        }
        consumed = true;
        event.command = "quit";
        return true;
    }
};

struct OneShotSignalSource final : apollo::runtime::ISignalSource {
    bool consumed = false;

    bool poll(apollo::runtime::SignalEvent& event) override {
        if (consumed) {
            return false;
        }
        consumed = true;
        event.value = 2;
        return true;
    }
};

bool test_application_host_console_stop() {
    auto service = std::make_shared<RecordingService>();
    apollo::runtime::ApplicationHost host;
    host.add_service(service);
    host.set_console_source(std::make_unique<OneShotConsoleSource>());

    if (!host.start()) {
        return false;
    }

    host.run_once();

    return host.phase() == apollo::core::ApplicationPhase::Stopped &&
           host.stop_reason() == apollo::runtime::StopReason::ConsoleRequested &&
           service->ticks == 0 &&
           service->events == std::vector<std::string>({"boot", "config", "init", "start", "ready", "stop", "service_stop"});
}

bool test_application_host_tick() {
    auto service = std::make_shared<RecordingService>();
    apollo::runtime::ApplicationHost host;
    host.add_service(service);

    if (!host.start()) {
        return false;
    }

    host.run_once();
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    return service->ticks == 1 &&
           host.phase() == apollo::core::ApplicationPhase::Stopped;
}

bool test_application_host_signal_and_shutdown_hook() {
    auto service = std::make_shared<RecordingService>();
    apollo::runtime::ApplicationHost host;
    host.add_service(service);
    host.set_signal_source(std::make_unique<OneShotSignalSource>());

    bool hook_called = false;
    apollo::runtime::StopReason hook_reason = apollo::runtime::StopReason::Completed;
    host.add_shutdown_hook([&](apollo::runtime::StopReason reason) {
        hook_called = true;
        hook_reason = reason;
    });

    if (!host.start()) {
        return false;
    }

    host.run_once();

    return host.phase() == apollo::core::ApplicationPhase::Stopped &&
           host.stop_reason() == apollo::runtime::StopReason::SignalRequested &&
           hook_called &&
           hook_reason == apollo::runtime::StopReason::SignalRequested;
}

bool test_service_host_wrapper() {
    auto service = std::make_shared<RecordingService>();
    apollo::runtime::ServiceHost host;
    host.add_service(service);
    if (!host.start()) {
        return false;
    }
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();
    return host.phase() == apollo::core::ApplicationPhase::Stopped;
}

} // namespace

int main() {
    const bool ok =
        test_application_host_console_stop() &&
        test_application_host_tick() &&
        test_application_host_signal_and_shutdown_hook() &&
        test_service_host_wrapper();

    if (!ok) {
        std::cerr << "apollo_runtime_tests failed" << std::endl;
        return 1;
    }

    std::cout << "apollo_runtime_tests passed" << std::endl;
    return 0;
}
