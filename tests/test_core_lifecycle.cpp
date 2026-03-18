/**
 * @file test_core_lifecycle.cpp
 * @brief Core lifecycle module unit tests
 */

#include "apollo/core/application_lifecycle.hpp"
#include "apollo/core/module_manifest.hpp"
#include "apollo/core/lifecycle_component.hpp"
#include <iostream>
#include <string_view>

using namespace apollo::core;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// ApplicationPhase Tests
//==============================================================================

bool test_application_phase_values() {
    std::cout << "Running: test_application_phase_values..." << std::endl;

    // Test enum values
    TEST_ASSERT(static_cast<int>(ApplicationPhase::Boot) == 0, "Boot phase value");
    TEST_ASSERT(static_cast<int>(ApplicationPhase::ConfigLoaded) == 1, "ConfigLoaded phase value");
    TEST_ASSERT(static_cast<int>(ApplicationPhase::Initialized) == 2, "Initialized phase value");
    TEST_ASSERT(static_cast<int>(ApplicationPhase::Ready) == 3, "Ready phase value");
    TEST_ASSERT(static_cast<int>(ApplicationPhase::Stopping) == 4, "Stopping phase value");
    TEST_ASSERT(static_cast<int>(ApplicationPhase::Stopped) == 5, "Stopped phase value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_application_reload_context() {
    std::cout << "Running: test_application_reload_context..." << std::endl;

    ApplicationReloadContext ctx;
    TEST_ASSERT(ctx.reason.empty(), "Default reason is empty");

    ApplicationReloadContext ctx2{"config_changed"};
    TEST_ASSERT(ctx2.reason == "config_changed", "Reason can be set");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IApplicationLifecycle Tests
//==============================================================================

bool test_i_application_lifecycle_default_impl() {
    std::cout << "Running: test_i_application_lifecycle_default_impl..." << std::endl;

    // Create a concrete implementation
    class TestLifecycle : public IApplicationLifecycle {
    public:
        int boot_calls = 0;
        int config_calls = 0;
        int init_calls = 0;
        int ready_calls = 0;
        int reload_calls = 0;
        int stop_calls = 0;

        void on_application_boot() override { boot_calls++; }
        void on_application_config_loaded() override { config_calls++; }
        void on_application_initialized() override { init_calls++; }
        void on_application_ready() override { ready_calls++; }
        void on_application_reload(const ApplicationReloadContext&) override { reload_calls++; }
        void on_application_stop() override { stop_calls++; }
    };

    TestLifecycle lifecycle;

    // Call all methods - should not crash
    lifecycle.on_application_boot();
    lifecycle.on_application_config_loaded();
    lifecycle.on_application_initialized();
    lifecycle.on_application_ready();
    lifecycle.on_application_reload(ApplicationReloadContext{"test"});
    lifecycle.on_application_stop();

    TEST_ASSERT(lifecycle.boot_calls == 1, "Boot called");
    TEST_ASSERT(lifecycle.config_calls == 1, "Config loaded called");
    TEST_ASSERT(lifecycle.init_calls == 1, "Initialized called");
    TEST_ASSERT(lifecycle.ready_calls == 1, "Ready called");
    TEST_ASSERT(lifecycle.reload_calls == 1, "Reload called");
    TEST_ASSERT(lifecycle.stop_calls == 1, "Stop called");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ModuleManifest Tests
//==============================================================================

bool test_module_manifest_structure() {
    std::cout << "Running: test_module_manifest_structure..." << std::endl;

    ModuleManifest manifest;
    manifest.name = "test_module";
    manifest.layer = "core";
    manifest.description = "Test module description";

    TEST_ASSERT(manifest.name == "test_module", "Name set");
    TEST_ASSERT(manifest.layer == "core", "Layer set");
    TEST_ASSERT(manifest.description == "Test module description", "Description set");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_core_manifest() {
    std::cout << "Running: test_core_manifest..." << std::endl;

    const ModuleManifest& manifest = core_manifest();

    TEST_ASSERT(!manifest.name.empty(), "Core manifest has name");
    TEST_ASSERT(!manifest.layer.empty(), "Core manifest has layer");
    TEST_ASSERT(!manifest.description.empty(), "Core manifest has description");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LifecycleComponent Tests
//==============================================================================

bool test_lifecycle_component_interface() {
    std::cout << "Running: test_lifecycle_component_interface..." << std::endl;

    class TestComponent : public LifecycleComponent {
    public:
        std::string_view name() const override {
            return "TestComponent";
        }
    };

    TestComponent component;

    TEST_ASSERT(component.name() == "TestComponent", "Component name matches");
    TEST_ASSERT((std::is_base_of_v<IApplicationLifecycle, TestComponent>), "Inherits from IApplicationLifecycle");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_lifecycle_component_polymorphic() {
    std::cout << "Running: test_lifecycle_component_polymorphic..." << std::endl;

    class TestComponent : public LifecycleComponent {
    public:
        std::string_view name() const override {
            return "TestComponent";
        }

        int call_count = 0;
        void on_application_ready() override {
            call_count++;
        }
    };

    TestComponent component;
    LifecycleComponent* base = &component;

    // Polymorphic call
    base->on_application_ready();

    TEST_ASSERT(component.call_count == 1, "Polymorphic call works");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Core Lifecycle Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // ApplicationPhase tests
    run(test_application_phase_values);
    run(test_application_reload_context);

    // IApplicationLifecycle tests
    run(test_i_application_lifecycle_default_impl);

    // ModuleManifest tests
    run(test_module_manifest_structure);
    run(test_core_manifest);

    // LifecycleComponent tests
    run(test_lifecycle_component_interface);
    run(test_lifecycle_component_polymorphic);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
