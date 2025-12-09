#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/ApplicationContext.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

using namespace Apollo;

// External test runner from component_test_simple.cpp
extern class TestRunner {
public:
    static void assert_true(bool condition, const std::string& message);
    static void assert_equals(int expected, int actual, const std::string& message);
    static void print_summary();
    static int get_result();
};

// Test components for context testing
class ServiceA : public BaseComponent {
public:
    ServiceA() : BaseComponent("ServiceA"), initialized(false) {}

    DECLARE_COMPONENT(ServiceA, "ServiceA")

    bool onInitialize() override {
        initialized = true;
        return true;
    }

    bool isInitialized() const { return initialized; }

private:
    bool initialized;
};

class ServiceB : public BaseComponent {
public:
    ServiceB() : BaseComponent("ServiceB") {
        addDependency("ServiceA");
    }

    DECLARE_COMPONENT(ServiceB, "ServiceB")

    bool onInitialize() override {
        auto serviceA = getComponent<ServiceA>();
        if (!serviceA || !serviceA->isInitialized()) {
            return false;
        }
        return true;
    }
};

class ServiceC : public BaseComponent {
public:
    ServiceC() : BaseComponent("ServiceC") {
        addDependency("ServiceB");
    }

    DECLARE_COMPONENT(ServiceC, "ServiceC")

    bool onInitialize() override {
        auto serviceB = getComponent<ServiceB>();
        if (!serviceB) {
            return false;
        }
        return true;
    }
};

REGISTER_COMPONENT(ServiceA)
REGISTER_COMPONENT(ServiceB)
REGISTER_COMPONENT(ServiceC)

void test_component_registration() {
    std::cout << "\nTesting Component Registration..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Check that components are registered
    auto names = context.getComponentNames();

    bool foundServiceA = false;
    bool foundServiceB = false;
    bool foundServiceC = false;

    for (const auto& name : names) {
        if (name == "ServiceA") foundServiceA = true;
        if (name == "ServiceB") foundServiceB = true;
        if (name == "ServiceC") foundServiceC = true;
    }

    TestRunner::assert_true(foundServiceA, "ServiceA should be registered");
    TestRunner::assert_true(foundServiceB, "ServiceB should be registered");
    TestRunner::assert_true(foundServiceC, "ServiceC should be registered");
}

void test_component_retrieval() {
    std::cout << "\nTesting Component Retrieval..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Test getting components by name
    auto serviceA = context.getComponent("ServiceA");
    TestRunner::assert_true(serviceA != nullptr, "Should be able to get ServiceA");

    // Test getting components by type
    auto serviceATyped = context.getComponent<ServiceA>();
    TestRunner::assert_true(serviceATyped != nullptr, "Should be able to get ServiceA by type");
    TestRunner::assert_true(serviceATyped->getName() == "ServiceA", "Component should have correct name");

    // Test getting non-existent component
    auto nonExistent = context.getComponent("NonExistent");
    TestRunner::assert_true(nonExistent == nullptr, "Should not be able to get non-existent component");
}

void test_dependency_resolution() {
    std::cout << "\nTesting Dependency Resolution..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Initialize all components
    TestRunner::assert_true(context.initializeComponents(),
        "Should be able to initialize all components with dependencies");

    // Verify ServiceA is initialized
    auto serviceA = context.getComponent<ServiceA>();
    TestRunner::assert_true(serviceA != nullptr && serviceA->isInitialized(),
        "ServiceA should be initialized");

    // Start components
    TestRunner::assert_true(context.startComponents(),
        "Should be able to start all components");

    // Cleanup
    context.stopComponents();
    context.destroyComponents();
}

void test_duplicate_registration() {
    std::cout << "\nTesting Duplicate Registration..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Try to register an already registered component
    bool result = context.registerComponent<ServiceA>();
    TestRunner::assert_true(!result,
        "Should not be able to register the same component twice");
}

void test_manual_registration() {
    std::cout << "\nTesting Manual Registration..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Create a new component type
    class ManualComponent : public BaseComponent {
    public:
        ManualComponent() : BaseComponent("ManualComponent") {}

        static const std::string& getStaticName() {
            static std::string name = "ManualComponent";
            return name;
        }
    };

    // Register manually
    bool registered = context.registerComponent<ManualComponent>();
    TestRunner::assert_true(registered, "Should be able to register component manually");

    // Retrieve the manually registered component
    auto component = context.getComponent<ManualComponent>();
    TestRunner::assert_true(component != nullptr, "Should be able to retrieve manually registered component");
}

void test_context_cleanup() {
    std::cout << "\nTesting Context Cleanup..." << std::endl;

    auto& context = ApplicationContext::getInstance();

    // Initialize and start components
    context.initializeComponents();
    context.startComponents();

    // Check that components are running
    auto serviceA = context.getComponent<ServiceA>();
    TestRunner::assert_true(serviceA != nullptr && serviceA->getState() == ComponentState::STARTED,
        "ServiceA should be started");

    // Destroy all components
    TestRunner::assert_true(context.destroyComponents(),
        "Should be able to destroy all components");

    // Check that component instances are cleared
    serviceA = context.getComponent<ServiceA>();
    TestRunner::assert_true(serviceA == nullptr,
        "Component instances should be cleared after destroy");
}

void run_context_tests() {
    std::cout << "\n=== Context Tests ===" << std::endl;

    test_component_registration();
    test_component_retrieval();
    test_dependency_resolution();
    test_duplicate_registration();
    test_manual_registration();
    test_context_cleanup();

    std::cout << "Context tests completed." << std::endl;
}