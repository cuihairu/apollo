/**
 * @file test_runtime.cpp
 * @brief Runtime module unit tests
 */

#include "apollo/runtime/application_host.hpp"
#include "apollo/runtime/runtime_manifest.hpp"
#include "apollo/core/application_lifecycle.hpp"
#include <iostream>
#include <memory>
#include <string_view>

using namespace apollo::runtime;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// StopReason Tests
//==============================================================================

bool test_stop_reason_values() {
    std::cout << "Running: test_stop_reason_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(StopReason::Completed) == 0, "Completed value");
    TEST_ASSERT(static_cast<int>(StopReason::ConsoleRequested) == 1, "ConsoleRequested value");
    TEST_ASSERT(static_cast<int>(StopReason::SignalRequested) == 2, "SignalRequested value");
    TEST_ASSERT(static_cast<int>(StopReason::StartupFailed) == 3, "StartupFailed value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ConsoleEvent Tests
//==============================================================================

bool test_console_event_default() {
    std::cout << "Running: test_console_event_default..." << std::endl;

    ConsoleEvent event;
    TEST_ASSERT(event.command.empty(), "Default command is empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_console_event_command() {
    std::cout << "Running: test_console_event_command..." << std::endl;

    ConsoleEvent event;
    event.command = "exit";
    TEST_ASSERT(event.command == "exit", "Command set correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// SignalEvent Tests
//==============================================================================

bool test_signal_event_default() {
    std::cout << "Running: test_signal_event_default..." << std::endl;

    SignalEvent event;
    TEST_ASSERT(event.value == 0, "Default value is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_signal_event_value() {
    std::cout << "Running: test_signal_event_value..." << std::endl;

    SignalEvent event;
    event.value = 2;  // SIGINT
    TEST_ASSERT(event.value == 2, "Value set correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IHostedService Tests
//==============================================================================

class TestHostedService : public IHostedService {
public:
    std::string name_ = "TestService";
    bool started = false;
    bool stopped = false;
    int boot_calls = 0;
    int config_calls = 0;
    int init_calls = 0;
    int ready_calls = 0;
    int stop_calls = 0;
    int tick_calls = 0;

    std::string_view service_name() const override {
        return name_;
    }

    bool start() override {
        started = true;
        return true;
    }

    void stop() override {
        stopped = true;
    }

    bool is_running() const override {
        return started && !stopped;
    }

    void tick() override {
        tick_calls++;
    }

    void on_application_boot() override {
        boot_calls++;
    }

    void on_application_config_loaded() override {
        config_calls++;
    }

    void on_application_initialized() override {
        init_calls++;
    }

    void on_application_ready() override {
        ready_calls++;
    }

    void on_application_stop() override {
        stop_calls++;
    }
};

bool test_hosted_service_interface() {
    std::cout << "Running: test_hosted_service_interface..." << std::endl;

    TestHostedService service;

    TEST_ASSERT(service.service_name() == "TestService", "Service name");
    TEST_ASSERT(!service.started, "Not started initially");
    TEST_ASSERT(!service.is_running(), "Not running initially");

    service.start();
    TEST_ASSERT(service.started, "Started after start()");
    TEST_ASSERT(service.is_running(), "Running after start()");

    service.stop();
    TEST_ASSERT(service.stopped, "Stopped after stop()");
    TEST_ASSERT(!service.is_running(), "Not running after stop()");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_hosted_service_lifecycle_callbacks() {
    std::cout << "Running: test_hosted_service_lifecycle_callbacks..." << std::endl;

    TestHostedService service;

    service.on_application_boot();
    service.on_application_config_loaded();
    service.on_application_initialized();
    service.on_application_ready();
    service.on_application_stop();

    TEST_ASSERT(service.boot_calls == 1, "Boot callback called");
    TEST_ASSERT(service.config_calls == 1, "Config callback called");
    TEST_ASSERT(service.init_calls == 1, "Init callback called");
    TEST_ASSERT(service.ready_calls == 1, "Ready callback called");
    TEST_ASSERT(service.stop_calls == 1, "Stop callback called");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_hosted_service_tick() {
    std::cout << "Running: test_hosted_service_tick..." << std::endl;

    TestHostedService service;

    service.tick();
    service.tick();
    service.tick();

    TEST_ASSERT(service.tick_calls == 3, "Tick called 3 times");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IConsoleEventSource Tests
//==============================================================================

class TestConsoleEventSource : public IConsoleEventSource {
public:
    std::vector<ConsoleEvent> events_to_return;
    int poll_count = 0;

    bool poll(ConsoleEvent& event) override {
        poll_count++;
        if (!events_to_return.empty()) {
            event = events_to_return.back();
            events_to_return.pop_back();
            return true;
        }
        return false;
    }
};

bool test_console_event_source_poll() {
    std::cout << "Running: test_console_event_source_poll..." << std::endl;

    TestConsoleEventSource source;

    ConsoleEvent event1;
    event1.command = "cmd1";
    source.events_to_return.push_back(event1);

    ConsoleEvent result;
    TEST_ASSERT(source.poll(result), "Poll returns true");
    TEST_ASSERT(result.command == "cmd1", "Event received");
    TEST_ASSERT(source.poll_count == 1, "Poll count incremented");

    TEST_ASSERT(!source.poll(result), "No more events");
    TEST_ASSERT(source.poll_count == 2, "Poll count incremented again");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ISignalSource Tests
//==============================================================================

class TestSignalSource : public ISignalSource {
public:
    std::vector<SignalEvent> events_to_return;
    int poll_count = 0;

    bool poll(SignalEvent& event) override {
        poll_count++;
        if (!events_to_return.empty()) {
            event = events_to_return.back();
            events_to_return.pop_back();
            return true;
        }
        return false;
    }
};

bool test_signal_source_poll() {
    std::cout << "Running: test_signal_source_poll..." << std::endl;

    TestSignalSource source;

    SignalEvent event1;
    event1.value = 2;
    source.events_to_return.push_back(event1);

    SignalEvent result;
    TEST_ASSERT(source.poll(result), "Poll returns true");
    TEST_ASSERT(result.value == 2, "Signal value correct");
    TEST_ASSERT(source.poll_count == 1, "Poll count incremented");

    TEST_ASSERT(!source.poll(result), "No more events");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ApplicationHost Tests
//==============================================================================

bool test_application_host_default_state() {
    std::cout << "Running: test_application_host_default_state..." << std::endl;

    ApplicationHost host;

    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Boot,
                "Initial phase is Boot");
    TEST_ASSERT(host.stop_reason() == StopReason::Completed,
                "Default stop reason is Completed");
    TEST_ASSERT(!host.is_running(), "Not running initially");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_add_service() {
    std::cout << "Running: test_application_host_add_service..." << std::endl;

    ApplicationHost host;
    auto service = std::make_shared<TestHostedService>();

    host.add_service(service);

    // Service is added but not started until host.start() is called
    TEST_ASSERT(!service->started, "Service not started before host.start()");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_add_null_service() {
    std::cout << "Running: test_application_host_add_null_service..." << std::endl;

    ApplicationHost host;
    std::shared_ptr<TestHostedService> null_service = nullptr;

    // Should not crash
    host.add_service(null_service);

    TEST_ASSERT(true, "Adding null service doesn't crash");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_start() {
    std::cout << "Running: test_application_host_start..." << std::endl;

    ApplicationHost host;
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);

    bool started = host.start();

    TEST_ASSERT(started, "Host started successfully");
    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Ready,
                "Phase is Ready after start");
    TEST_ASSERT(host.is_running(), "Host is running");
    TEST_ASSERT(service->started, "Service started");
    TEST_ASSERT(service->boot_calls == 1, "Boot callback called");
    TEST_ASSERT(service->config_calls == 1, "Config callback called");
    TEST_ASSERT(service->init_calls == 1, "Init callback called");
    TEST_ASSERT(service->ready_calls == 1, "Ready callback called");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_start_with_failing_service() {
    std::cout << "Running: test_application_host_start_with_failing_service..." << std::endl;

    class FailingService : public IHostedService {
    public:
        std::string_view service_name() const override { return "FailingService"; }
        bool start() override { return false; }
        void stop() override {}
        bool is_running() const override { return false; }
    };

    ApplicationHost host;
    auto service = std::make_shared<FailingService>();
    host.add_service(service);

    bool started = host.start();

    TEST_ASSERT(!started, "Host start fails when service fails");
    TEST_ASSERT(host.stop_reason() == StopReason::StartupFailed,
                "Stop reason is StartupFailed");
    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Stopped,
                "Phase is Stopped after failed start");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_run_once() {
    std::cout << "Running: test_application_host_run_once..." << std::endl;

    ApplicationHost host;
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);

    host.start();

    host.run_once();
    TEST_ASSERT(service->tick_calls == 1, "Service ticked once");

    host.run_once();
    TEST_ASSERT(service->tick_calls == 2, "Service ticked twice");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_stop() {
    std::cout << "Running: test_application_host_stop..." << std::endl;

    ApplicationHost host;
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);

    host.start();
    TEST_ASSERT(service->started, "Service started");
    TEST_ASSERT(!service->stopped, "Service not stopped");

    host.stop();
    TEST_ASSERT(!host.is_running(), "Host not running after stop");
    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Stopped,
                "Phase is Stopped");
    TEST_ASSERT(service->stopped, "Service stopped");
    TEST_ASSERT(service->stop_calls == 1, "Stop callback called");

    // Calling stop again should be idempotent
    host.stop();
    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Stopped,
                "Still Stopped after second stop");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_request_stop() {
    std::cout << "Running: test_application_host_request_stop..." << std::endl;

    ApplicationHost host;
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);

    host.start();

    host.request_stop(StopReason::ConsoleRequested);
    TEST_ASSERT(!host.is_running(), "Not running after request_stop");
    TEST_ASSERT(host.stop_reason() == StopReason::ConsoleRequested,
                "Stop reason set");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_shutdown_hooks() {
    std::cout << "Running: test_application_host_shutdown_hooks..." << std::endl;

    ApplicationHost host;
    int hook_call_count = 0;
    StopReason captured_reason = StopReason::Completed;

    auto hook1 = [&](StopReason reason) {
        hook_call_count++;
        captured_reason = reason;
    };

    auto hook2 = [&](StopReason) {
        hook_call_count += 10;
    };

    host.add_shutdown_hook(hook1);
    host.add_shutdown_hook(hook2);

    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);
    host.start();

    host.request_stop(StopReason::SignalRequested);
    host.stop();

    // Hooks are called in reverse order
    TEST_ASSERT(hook_call_count == 11, "Both hooks called (reverse order)");
    TEST_ASSERT(captured_reason == StopReason::SignalRequested,
                "Reason captured correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_console_source() {
    std::cout << "Running: test_application_host_console_source..." << std::endl;

    ApplicationHost host;
    auto console_source = std::make_unique<TestConsoleEventSource>();

    ConsoleEvent exit_event;
    exit_event.command = "exit";
    console_source->events_to_return.push_back(exit_event);

    host.set_console_source(std::move(console_source));
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);
    host.start();

    host.run_once();

    TEST_ASSERT(!host.is_running(), "Stopped after console event");
    TEST_ASSERT(host.stop_reason() == StopReason::ConsoleRequested,
                "Stop reason is ConsoleRequested");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_signal_source() {
    std::cout << "Running: test_application_host_signal_source..." << std::endl;

    ApplicationHost host;
    auto signal_source = std::make_unique<TestSignalSource>();

    SignalEvent sig_event;
    sig_event.value = 2;  // SIGINT
    signal_source->events_to_return.push_back(sig_event);

    host.set_signal_source(std::move(signal_source));
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);
    host.start();

    host.run_once();

    TEST_ASSERT(!host.is_running(), "Stopped after signal event");
    TEST_ASSERT(host.stop_reason() == StopReason::SignalRequested,
                "Stop reason is SignalRequested");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_host_multiple_services() {
    std::cout << "Running: test_application_host_multiple_services..." << std::endl;

    ApplicationHost host;

    auto service1 = std::make_shared<TestHostedService>();
    service1->name_ = "Service1";

    auto service2 = std::make_shared<TestHostedService>();
    service2->name_ = "Service2";

    auto service3 = std::make_shared<TestHostedService>();
    service3->name_ = "Service3";

    host.add_service(service1);
    host.add_service(service2);
    host.add_service(service3);

    host.start();

    // All services should be started
    TEST_ASSERT(service1->started, "Service1 started");
    TEST_ASSERT(service2->started, "Service2 started");
    TEST_ASSERT(service3->started, "Service3 started");

    // All services should receive lifecycle callbacks
    TEST_ASSERT(service1->ready_calls == 1, "Service1 ready callback");
    TEST_ASSERT(service2->ready_calls == 1, "Service2 ready callback");
    TEST_ASSERT(service3->ready_calls == 1, "Service3 ready callback");

    host.run_once();
    TEST_ASSERT(service1->tick_calls == 1, "Service1 ticked");
    TEST_ASSERT(service2->tick_calls == 1, "Service2 ticked");
    TEST_ASSERT(service3->tick_calls == 1, "Service3 ticked");

    host.stop();

    // Services are stopped in reverse order
    TEST_ASSERT(service1->stopped, "Service1 stopped");
    TEST_ASSERT(service2->stopped, "Service2 stopped");
    TEST_ASSERT(service3->stopped, "Service3 stopped");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ServiceHost Tests
//==============================================================================

bool test_service_host_default_state() {
    std::cout << "Running: test_service_host_default_state..." << std::endl;

    ServiceHost host;

    TEST_ASSERT(host.phase() == apollo::core::ApplicationPhase::Boot,
                "Initial phase is Boot");
    TEST_ASSERT(host.stop_reason() == StopReason::Completed,
                "Default stop reason is Completed");
    TEST_ASSERT(!host.is_running(), "Not running initially");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_service_host_delegates_to_application_host() {
    std::cout << "Running: test_service_host_delegates_to_application_host..." << std::endl;

    ServiceHost host;
    auto service = std::make_shared<TestHostedService>();
    host.add_service(service);

    bool started = host.start();

    TEST_ASSERT(started, "ServiceHost started");
    TEST_ASSERT(host.is_running(), "ServiceHost is running");
    TEST_ASSERT(service->started, "Service started");

    host.run_once();
    TEST_ASSERT(service->tick_calls == 1, "Service ticked");

    host.request_stop(StopReason::ConsoleRequested);
    TEST_ASSERT(host.stop_reason() == StopReason::ConsoleRequested,
                "Stop reason propagated");

    host.stop();

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// RuntimeManifest Tests
//==============================================================================

bool test_runtime_manifest_structure() {
    std::cout << "Running: test_runtime_manifest_structure..." << std::endl;

    RuntimeManifest manifest;
    manifest.name = "runtime";
    manifest.layer = "core";
    manifest.description = "Runtime module";

    TEST_ASSERT(manifest.name == "runtime", "Name set");
    TEST_ASSERT(manifest.layer == "core", "Layer set");
    TEST_ASSERT(manifest.description == "Runtime module", "Description set");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_runtime_manifest_function() {
    std::cout << "Running: test_runtime_manifest_function..." << std::endl;

    const RuntimeManifest& manifest = runtime_manifest();

    TEST_ASSERT(!manifest.name.empty(), "Runtime manifest has name");
    TEST_ASSERT(!manifest.layer.empty(), "Runtime manifest has layer");
    TEST_ASSERT(!manifest.description.empty(), "Runtime manifest has description");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Runtime Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // StopReason tests
    run(test_stop_reason_values);

    // ConsoleEvent tests
    run(test_console_event_default);
    run(test_console_event_command);

    // SignalEvent tests
    run(test_signal_event_default);
    run(test_signal_event_value);

    // IHostedService tests
    run(test_hosted_service_interface);
    run(test_hosted_service_lifecycle_callbacks);
    run(test_hosted_service_tick);

    // IConsoleEventSource tests
    run(test_console_event_source_poll);

    // ISignalSource tests
    run(test_signal_source_poll);

    // ApplicationHost tests
    run(test_application_host_default_state);
    run(test_application_host_add_service);
    run(test_application_host_add_null_service);
    run(test_application_host_start);
    run(test_application_host_start_with_failing_service);
    run(test_application_host_run_once);
    run(test_application_host_stop);
    run(test_application_host_request_stop);
    run(test_application_host_shutdown_hooks);
    run(test_application_host_console_source);
    run(test_application_host_signal_source);
    run(test_application_host_multiple_services);

    // ServiceHost tests
    run(test_service_host_default_state);
    run(test_service_host_delegates_to_application_host);

    // RuntimeManifest tests
    run(test_runtime_manifest_structure);
    run(test_runtime_manifest_function);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
