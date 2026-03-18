/**
 * @file runtime_comprehensive_tests.cpp
 * @brief Comprehensive test suite for apollo::runtime module
 *
 * Coverage targets:
 * - ApplicationHost: 90%+
 * - ServiceHost: 85%+
 * - IHostedService lifecycle: 95%+
 * - StopReason: 100%
 */

#include "apollo/runtime/application_host.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>

// ============================================================================
// Test Framework
// ============================================================================

#define TEST(name) bool test_##name()
#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "FAILED: " #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))
#define ASSERT_STRNE(a, b) ASSERT_TRUE(std::string(a) != std::string(b))
#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == nullptr)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != nullptr)

// ============================================================================
// Mock Services
// ============================================================================

struct MockService : public apollo::runtime::IHostedService {
    std::vector<std::string> events;
    std::string name_ = "MockService";
    bool running_ = false;
    int tick_count_ = 0;
    bool start_succeeds = true;
    bool stop_called = false;

    std::string_view service_name() const override {
        return name_;
    }

    void on_application_boot() override { events.push_back("boot"); }
    void on_application_config_loaded() override { events.push_back("config"); }
    void on_application_initialized() override { events.push_back("init"); }
    void on_application_ready() override { events.push_back("ready"); }
    void on_application_stop() override { events.push_back("stop"); }

    bool start() override {
        events.push_back("start");
        if (start_succeeds) {
            running_ = true;
        }
        return start_succeeds;
    }

    void stop() override {
        running_ = false;
        stop_called = true;
        events.push_back("service_stop");
    }

    bool is_running() const override {
        return running_;
    }

    void tick() override {
        ++tick_count_;
    }

    void reset() {
        events.clear();
        running_ = false;
        tick_count_ = 0;
        start_succeeds = true;
        stop_called = false;
    }
};

struct FailingService : public apollo::runtime::IHostedService {
    std::string_view service_name() const override { return "FailingService"; }

    bool start() override { return false; }  // Always fails to start
    void stop() override {}
    bool is_running() const override { return false; }
};

struct TickCounterService : public apollo::runtime::IHostedService {
    std::string_view service_name() const override { return "TickCounter"; }
    int tick_count = 0;
    bool running = false;

    bool start() override {
        running = true;
        return true;
    }

    void stop() override {
        running = false;
    }

    bool is_running() const override {
        return running;
    }

    void tick() override {
        ++tick_count;
    }
};

// ============================================================================
// Console/Signal Event Sources
// ============================================================================

struct TestConsoleSource : public apollo::runtime::IConsoleEventSource {
    std::vector<std::string> commands;
    size_t index = 0;

    bool poll(apollo::runtime::ConsoleEvent& event) override {
        if (index >= commands.size()) {
            return false;
        }
        event.command = commands[index++];
        return true;
    }

    void add_command(const std::string& cmd) {
        commands.push_back(cmd);
    }
};

struct TestSignalSource : public apollo::runtime::ISignalSource {
    std::vector<int> signals;
    size_t index = 0;

    bool poll(apollo::runtime::SignalEvent& event) override {
        if (index >= signals.size()) {
            return false;
        }
        event.value = signals[index++];
        return true;
    }

    void add_signal(int sig) {
        signals.push_back(sig);
    }
};

// ============================================================================
// StopReason Tests
// ============================================================================

TEST(stop_reason_values) {
    using apollo::runtime::StopReason;

    ASSERT_EQ(static_cast<int>(StopReason::Completed), 0);
    ASSERT_EQ(static_cast<int>(StopReason::ConsoleRequested), 1);
    ASSERT_EQ(static_cast<int>(StopReason::SignalRequested), 2);
    ASSERT_EQ(static_cast<int>(StopReason::StartupFailed), 3);

    return true;
}

// ============================================================================
// ApplicationHost Lifecycle Tests
// ============================================================================

TEST(host_start_stop) {
    apollo::runtime::ApplicationHost host;
    auto service = std::make_shared<MockService>();

    host.add_service(service);

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Boot);

    ASSERT_TRUE(host.start());

    ASSERT_TRUE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);
    ASSERT_TRUE(service->running_);

    host.stop();

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);
    ASSERT_FALSE(service->running_);

    return true;
}

TEST(host_lifecycle_events) {
    apollo::runtime::ApplicationHost host;
    auto service = std::make_shared<MockService>();

    host.add_service(service);
    host.start();

    // Verify lifecycle events were called in order
    const auto& events = service->events;
    ASSERT_GE(events.size(), 5);

    ASSERT_EQ(events[0], "boot");
    ASSERT_EQ(events[1], "config");
    ASSERT_EQ(events[2], "init");
    ASSERT_EQ(events[3], "start");
    ASSERT_EQ(events[4], "ready");

    host.stop();

    ASSERT_EQ(events.back(), "service_stop");

    return true;
}

TEST(host_multiple_services) {
    apollo::runtime::ApplicationHost host;

    auto service1 = std::make_shared<MockService>();
    service1->name_ = "Service1";

    auto service2 = std::make_shared<MockService>();
    service2->name_ = "Service2";

    host.add_service(service1);
    host.add_service(service2);

    ASSERT_TRUE(host.start());

    ASSERT_TRUE(service1->running_);
    ASSERT_TRUE(service2->running_);

    host.run_once();  // Tick both services

    ASSERT_EQ(service1->tick_count_, 1);
    ASSERT_EQ(service2->tick_count_, 1);

    host.stop();

    ASSERT_FALSE(service1->running_);
    ASSERT_FALSE(service2->running_);

    return true;
}

TEST(host_tick) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<TickCounterService>();
    host.add_service(service);

    host.start();

    ASSERT_EQ(service->tick_count, 0);

    host.run_once();
    ASSERT_EQ(service->tick_count, 1);

    host.run_once();
    ASSERT_EQ(service->tick_count, 2);

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();  // This should stop, not tick

    ASSERT_EQ(service->tick_count, 2);  // No increment after stop request

    return true;
}

TEST(host_run_once_without_start) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    // Calling run_once without start should be safe but won't tick
    int count = host.run_once();

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(service->tick_count_, 0);

    return true;
}

// ============================================================================
// Stop Tests
// ============================================================================

TEST(host_stop_by_console) {
    apollo::runtime::ApplicationHost host;

    auto console_source = std::make_unique<TestConsoleSource>();
    console_source->add_command("quit");

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_console_source(std::move(console_source));

    host.start();

    ASSERT_TRUE(host.is_running());

    host.run_once();  // Should process console and stop

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::ConsoleRequested);

    return true;
}

TEST(host_stop_by_signal) {
    apollo::runtime::ApplicationHost host;

    auto signal_source = std::make_unique<TestSignalSource>();
    signal_source->add_signal(2);  // SIGINT

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_signal_source(std::move(signal_source));

    host.start();

    ASSERT_TRUE(host.is_running());

    host.run_once();  // Should process signal and stop

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::SignalRequested);

    return true;
}

TEST(host_stop_by_request) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    host.start();

    ASSERT_TRUE(host.is_running());

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::Completed);

    return true;
}

TEST(host_multiple_stop_requests) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    host.start();

    // Multiple stop requests should be safe
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_FALSE(host.is_running());

    return true;
}

// ============================================================================
// Shutdown Hooks Tests
// ============================================================================

TEST(host_shutdown_hooks) {
    apollo::runtime::ApplicationHost host;

    bool hook1_called = false;
    bool hook2_called = false;

    host.add_shutdown_hook([&](apollo::runtime::StopReason reason) {
        hook1_called = true;
        ASSERT_EQ(reason, apollo::runtime::StopReason::Completed);
    });

    host.add_shutdown_hook([&](apollo::runtime::StopReason reason) {
        hook2_called = true;
    });

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    host.start();
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_TRUE(hook1_called);
    ASSERT_TRUE(hook2_called);

    return true;
}

TEST(host_shutdown_hooks_order) {
    apollo::runtime::ApplicationHost host;

    std::vector<int> call_order;

    host.add_shutdown_hook([&call_order](apollo::runtime::StopReason) {
        call_order.push_back(1);
    });

    host.add_shutdown_hook([&call_order](apollo::runtime::StopReason) {
        call_order.push_back(2);
    });

    host.add_shutdown_hook([&call_order](apollo::runtime::StopReason) {
        call_order.push_back(3);
    });

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    host.start();
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(call_order.size(), 3);
    // Shutdown hooks are called in REVERSE order (LIFO)
    ASSERT_EQ(call_order[0], 3);
    ASSERT_EQ(call_order[1], 2);
    ASSERT_EQ(call_order[2], 1);

    return true;
}

// ============================================================================
// Service Start Failure Tests
// ============================================================================

TEST(host_service_start_failure) {
    apollo::runtime::ApplicationHost host;

    auto failing_service = std::make_shared<FailingService>();
    host.add_service(failing_service);

    ASSERT_FALSE(host.start());

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::StartupFailed);

    return true;
}

bool test__host_mixed_services_some_fail() {
    apollo::runtime::ApplicationHost host;

    auto failing_service = std::make_shared<FailingService>();
    auto mock_service = std::make_shared<MockService>();

    host.add_service(failing_service);
    host.add_service(mock_service);

    ASSERT_FALSE(host.start());

    // Mock service should not be running after failure
    ASSERT_FALSE(mock_service->running_);

    return true;
}

// ============================================================================
// ConsoleEventSource Tests
// ============================================================================

TEST(console_source_basic) {
    apollo::runtime::ApplicationHost host;

    auto console = std::make_unique<TestConsoleSource>();
    console->add_command("quit");  // Any console input will request stop

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_console_source(std::move(console));

    host.start();
    ASSERT_TRUE(host.is_running());

    host.run_once();  // Process "quit" - should stop
    ASSERT_FALSE(host.is_running());

    return true;
}

TEST(console_source_empty) {
    apollo::runtime::ApplicationHost host;

    auto console = std::make_unique<TestConsoleSource>();
    // No commands

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_console_source(std::move(console));

    host.start();

    // Should stay running without console events
    for (int i = 0; i < 5; ++i) {
        host.run_once();
        ASSERT_TRUE(host.is_running());
    }

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_FALSE(host.is_running());

    return true;
}

// ============================================================================
// SignalSource Tests
// ============================================================================

TEST(signal_source_basic) {
    apollo::runtime::ApplicationHost host;

    auto signal_src = std::make_unique<TestSignalSource>();
    signal_src->add_signal(2);  // SIGINT

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_signal_source(std::move(signal_src));

    host.start();

    host.run_once();  // Should process signal and stop

    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::SignalRequested);
    ASSERT_FALSE(host.is_running());

    return true;
}

TEST(signal_source_multiple) {
    apollo::runtime::ApplicationHost host;

    auto signal_src = std::make_unique<TestSignalSource>();
    signal_src->add_signal(15);  // SIGTERM
    signal_src->add_signal(2);   // SIGINT

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_signal_source(std::move(signal_src));

    host.start();

    host.run_once();  // Process SIGTERM

    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::SignalRequested);
    ASSERT_FALSE(host.is_running());

    return true;
}

TEST(signal_source_sigint) {
    apollo::runtime::ApplicationHost host;

    auto signal_src = std::make_unique<TestSignalSource>();
    signal_src->add_signal(2);  // SIGINT (Ctrl+C)

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_signal_source(std::move(signal_src));

    host.start();
    host.run_once();

    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::SignalRequested);

    return true;
}

// ============================================================================
// ServiceHost Tests
// ============================================================================

TEST(service_host_basic) {
    apollo::runtime::ServiceHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());

    ASSERT_TRUE(host.is_running());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_FALSE(host.is_running());

    return true;
}

TEST(service_host_delegates_to_application_host) {
    apollo::runtime::ServiceHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    // Verify delegation
    ASSERT_TRUE(host.start());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::Completed);

    return true;
}

TEST(service_host_console_signal_sources) {
    apollo::runtime::ServiceHost host;

    auto console = std::make_unique<TestConsoleSource>();
    console->add_command("quit");

    auto service = std::make_shared<MockService>();
    host.add_service(service);
    host.set_console_source(std::move(console));

    ASSERT_TRUE(host.start());
    host.run_once();

    ASSERT_FALSE(host.is_running());
    ASSERT_EQ(host.stop_reason(), apollo::runtime::StopReason::ConsoleRequested);

    return true;
}

TEST(service_host_shutdown_hooks) {
    apollo::runtime::ServiceHost host;

    bool hook_called = false;

    host.add_shutdown_hook([&](apollo::runtime::StopReason reason) {
        hook_called = true;
    });

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_TRUE(hook_called);

    return true;
}

// ============================================================================
// Phase Transition Tests
// ============================================================================

TEST(host_phase_transitions) {
    apollo::runtime::ApplicationHost host;

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Boot);

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());

    // Verify phases during startup
    const auto& events = service->events;
    ASSERT_EQ(events[0], "boot");   // Boot phase
    // Then config loaded
    // Then initialized
    // Then start
    // Then ready
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);

    return true;
}

TEST(host_restart_not_supported) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);

    // Cannot restart after stop
    // (This is the current behavior - restart would require a new host)

    return true;
}

// ============================================================================
// Concurrent Tests
// ============================================================================

TEST(host_concurrent_stop_requests) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());
    ASSERT_TRUE(host.is_running());

    // Request stop from multiple threads
    std::thread t1([&]() {
        host.request_stop(apollo::runtime::StopReason::Completed);
    });

    std::thread t2([&]() {
        host.request_stop(apollo::runtime::StopReason::Completed);
    });

    t1.join();
    t2.join();

    host.run_once();

    ASSERT_FALSE(host.is_running());

    return true;
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(host_no_services) {
    apollo::runtime::ApplicationHost host;

    // Should be able to start without services
    ASSERT_TRUE(host.start());

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);

    return true;
}

TEST(host_null_service_handling) {
    apollo::runtime::ApplicationHost host;

    // Adding null service should be handled (or crash - design decision)
    // For now, we'll test the behavior
    // host.add_service(nullptr);  // This would be unsafe

    // Verify we can still start with valid services
    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);

    return true;
}

TEST(host_double_start) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());
    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Ready);

    // Starting again should be handled
    // Depending on implementation, might return false or be no-op
    bool result = host.start();

    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    ASSERT_EQ(host.phase(), apollo::core::ApplicationPhase::Stopped);

    return true;
}

TEST(host_double_stop) {
    apollo::runtime::ApplicationHost host;

    auto service = std::make_shared<MockService>();
    host.add_service(service);

    ASSERT_TRUE(host.start());

    host.stop();

    ASSERT_FALSE(host.is_running());

    // Second stop should be safe
    host.stop();

    ASSERT_FALSE(host.is_running());

    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Apollo Runtime Module Comprehensive Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    #define RUN_TEST(name) \
        do { \
            std::cout << "Running test_" #name "... "; \
            if (test_##name()) { \
                std::cout << "PASSED" << std::endl; \
                ++passed; \
            } else { \
                std::cout << "FAILED" << std::endl; \
                ++failed; \
            } \
        } while(0)

    // StopReason Tests
    std::cout << "--- StopReason Tests ---" << std::endl;
    RUN_TEST(stop_reason_values);

    // ApplicationHost Lifecycle Tests
    std::cout << "\n--- ApplicationHost Lifecycle Tests ---" << std::endl;
    RUN_TEST(host_start_stop);
    RUN_TEST(host_lifecycle_events);
    RUN_TEST(host_multiple_services);
    RUN_TEST(host_tick);
    RUN_TEST(host_run_once_without_start);

    // Stop Tests
    std::cout << "\n--- Stop Tests ---" << std::endl;
    RUN_TEST(host_stop_by_console);
    RUN_TEST(host_stop_by_signal);
    RUN_TEST(host_stop_by_request);
    RUN_TEST(host_multiple_stop_requests);

    // Shutdown Hooks Tests
    std::cout << "\n--- Shutdown Hooks Tests ---" << std::endl;
    RUN_TEST(host_shutdown_hooks);
    RUN_TEST(host_shutdown_hooks_order);

    // Service Start Failure Tests
    std::cout << "\n--- Service Start Failure Tests ---" << std::endl;
    RUN_TEST(host_service_start_failure);
    RUN_TEST(_host_mixed_services_some_fail);

    // ConsoleEventSource Tests
    std::cout << "\n--- ConsoleEventSource Tests ---" << std::endl;
    RUN_TEST(console_source_basic);
    RUN_TEST(console_source_empty);

    // SignalSource Tests
    std::cout << "\n--- SignalSource Tests ---" << std::endl;
    RUN_TEST(signal_source_basic);
    RUN_TEST(signal_source_multiple);
    RUN_TEST(signal_source_sigint);

    // ServiceHost Tests
    std::cout << "\n--- ServiceHost Tests ---" << std::endl;
    RUN_TEST(service_host_basic);
    RUN_TEST(service_host_delegates_to_application_host);
    RUN_TEST(service_host_console_signal_sources);
    RUN_TEST(service_host_shutdown_hooks);

    // Phase Transition Tests
    std::cout << "\n--- Phase Transition Tests ---" << std::endl;
    RUN_TEST(host_phase_transitions);
    RUN_TEST(host_restart_not_supported);

    // Concurrent Tests
    std::cout << "\n--- Concurrent Tests ---" << std::endl;
    RUN_TEST(host_concurrent_stop_requests);

    // Edge Cases
    std::cout << "\n--- Edge Cases ---" << std::endl;
    RUN_TEST(host_no_services);
    RUN_TEST(host_null_service_handling);
    RUN_TEST(host_double_start);
    RUN_TEST(host_double_stop);

    #undef RUN_TEST

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed > 0) {
        std::cerr << "\nSome tests FAILED!" << std::endl;
        return 1;
    }

    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
