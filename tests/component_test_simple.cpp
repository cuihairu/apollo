#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

using namespace Apollo;

// Simple test framework
class TestRunner {
public:
    static void assert_true(bool condition, const std::string& message) {
        tests_total++;
        if (condition) {
            tests_passed++;
        } else {
            std::cerr << "FAIL: " << message << std::endl;
            failed_tests.push_back(message);
        }
    }

    static void assert_equals(int expected, int actual, const std::string& message) {
        assert_true(expected == actual, message + " (expected: " + std::to_string(expected) + ", actual: " + std::to_string(actual) + ")");
    }

    static void print_summary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total: " << tests_total << std::endl;
        std::cout << "Passed: " << tests_passed << std::endl;
        std::cout << "Failed: " << (tests_total - tests_passed) << std::endl;

        if (!failed_tests.empty()) {
            std::cout << "\nFailed tests:" << std::endl;
            for (const auto& test : failed_tests) {
                std::cout << "  - " << test << std::endl;
            }
        }
    }

    static int get_result() {
        return (tests_total == tests_passed) ? 0 : 1;
    }

private:
    static int tests_total;
    static int tests_passed;
    static std::vector<std::string> failed_tests;
};

int TestRunner::tests_total = 0;
int TestRunner::tests_passed = 0;
std::vector<std::string> TestRunner::failed_tests;

// Test components
class TestComponent : public BaseComponent {
public:
    TestComponent() : BaseComponent("TestComponent"), initialized(false), started(false) {}

    DECLARE_COMPONENT(TestComponent, "TestComponent")

    bool onInitialize() override {
        initialized = true;
        return true;
    }

    bool onStart() override {
        started = true;
        return true;
    }

    bool onStop() override {
        started = false;
        return true;
    }

    bool onDestroy() override {
        initialized = false;
        return true;
    }

    bool isInitialized() const { return initialized; }
    bool isStarted() const { return started; }

private:
    bool initialized;
    bool started;
};

class FailingComponent : public BaseComponent {
public:
    FailingComponent() : BaseComponent("FailingComponent") {}

    DECLARE_COMPONENT(FailingComponent, "FailingComponent")

    bool onInitialize() override {
        return false; // Always fail to initialize
    }
};

REGISTER_COMPONENT(TestComponent)
REGISTER_COMPONENT(FailingComponent)

void test_component_lifecycle() {
    std::cout << "\nTesting Component Lifecycle..." << std::endl;

    auto component = std::make_shared<TestComponent>();

    // Test initial state
    TestRunner::assert_true(component->getState() == ComponentState::UNINITIALIZED,
        "Component should start in UNINITIALIZED state");
    TestRunner::assert_true(!component->isInitialized(),
        "Component should not be initialized initially");

    // Test initialize
    TestRunner::assert_true(component->initialize(),
        "Component should initialize successfully");
    TestRunner::assert_true(component->getState() == ComponentState::INITIALIZED,
        "Component should be in INITIALIZED state");
    TestRunner::assert_true(component->isInitialized(),
        "Component should be initialized");

    // Test start
    TestRunner::assert_true(component->start(),
        "Component should start successfully");
    TestRunner::assert_true(component->getState() == ComponentState::STARTED,
        "Component should be in STARTED state");
    TestRunner::assert_true(component->isStarted(),
        "Component should be started");

    // Test stop
    TestRunner::assert_true(component->stop(),
        "Component should stop successfully");
    TestRunner::assert_true(component->getState() == ComponentState::STOPPED,
        "Component should be in STOPPED state");
    TestRunner::assert_true(!component->isStarted(),
        "Component should not be started after stop");

    // Test destroy
    TestRunner::assert_true(component->destroy(),
        "Component should destroy successfully");
    TestRunner::assert_true(component->getState() == ComponentState::DESTROYED,
        "Component should be in DESTROYED state");
    TestRunner::assert_true(!component->isInitialized(),
        "Component should not be initialized after destroy");
}

void test_component_dependencies() {
    std::cout << "\nTesting Component Dependencies..." << std::endl;

    auto component = std::make_shared<TestComponent>();

    // Test adding dependencies
    component->addDependency("Dep1");
    component->addDependency("Dep2");

    auto deps = component->getDependencies();
    TestRunner::assert_equals(2, deps.size(),
        "Component should have 2 dependencies");

    TestRunner::assert_true(component->dependsOn("Dep1"),
        "Component should depend on Dep1");
    TestRunner::assert_true(component->dependsOn("Dep2"),
        "Component should depend on Dep2");
    TestRunner::assert_true(!component->dependsOn("NonExistentDep"),
        "Component should not depend on NonExistentDep");
}

void test_failing_component() {
    std::cout << "\nTesting Failing Component..." << std::endl;

    auto component = std::make_shared<FailingComponent>();

    // Test that initialize fails
    TestRunner::assert_true(!component->initialize(),
        "FailingComponent should fail to initialize");
    TestRunner::assert_true(component->getState() == ComponentState::UNINITIALIZED,
        "FailingComponent should remain in UNINITIALIZED state");

    // Test that start fails if not initialized
    TestRunner::assert_true(!component->start(),
        "FailingComponent should fail to start when not initialized");
}

void test_state_transitions() {
    std::cout << "\nTesting State Transitions..." << std::endl;

    auto component = std::make_shared<TestComponent>();

    // Test invalid transitions
    component->initialize();
    TestRunner::assert_true(!component->initialize(),
        "Should not be able to initialize twice");

    component->start();
    TestRunner::assert_true(!component->initialize(),
        "Should not be able to initialize after start");
    TestRunner::assert_true(!component->start(),
        "Should not be able to start twice");

    component->stop();
    TestRunner::assert_true(!component->stop(),
        "Should not be able to stop twice");
    TestRunner::assert_true(!component->start(),
        "Should not be able to start after stop without re-initialization");
}

void run_component_tests() {
    std::cout << "=== Component Tests ===" << std::endl;

    test_component_lifecycle();
    test_component_dependencies();
    test_failing_component();
    test_state_transitions();

    std::cout << "Component tests completed." << std::endl;
}