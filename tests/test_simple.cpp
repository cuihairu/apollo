#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/ApplicationContext.h"
#include "apollo/ConfigManager.h"
#include "apollo/DependencyManager.h"
#include <iostream>

using namespace Apollo;

// Test components
class SimpleComponent : public BaseComponent {
public:
    SimpleComponent(const std::string& name) : BaseComponent(name) {}

    bool onInitialize() override {
        std::cout << getName() << ": Initialized" << std::endl;
        return true;
    }

    bool onStart() override {
        std::cout << getName() << ": Started" << std::endl;
        return true;
    }

    bool onStop() override {
        std::cout << getName() << ": Stopped" << std::endl;
        return true;
    }
};

void test_component_lifecycle() {
    std::cout << "\n=== Testing Component Lifecycle ===" << std::endl;

    auto component = std::make_shared<SimpleComponent>("TestComponent");

    // Test lifecycle
    std::cout << "Initial state: " << (int)component->getState() << std::endl;

    component->initialize();
    std::cout << "After initialize: " << (int)component->getState() << std::endl;

    component->start();
    std::cout << "After start: " << (int)component->getState() << std::endl;

    component->stop();
    std::cout << "After stop: " << (int)component->getState() << std::endl;
}

void test_config_manager() {
    std::cout << "\n=== Testing Config Manager ===" << std::endl;

    auto config = ConfigManager::getInstance();

    // Test basic operations
    config->setValue("test.int", 42);
    config->setValue("test.string", std::string("hello"));

    std::cout << "Int value: " << config->getValue<int>("test.int") << std::endl;
    std::cout << "String value: " << config->getValue<std::string>("test.string") << std::endl;

    // Test default value
    std::cout << "Default value: " << config->getValue<int>("nonexistent", 100) << std::endl;
}

void test_dependency_manager() {
    std::cout << "\n=== Testing Dependency Manager ===" << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    // Add dependencies
    depMgr.addDependency("A", "B");
    depMgr.addDependency("B", "C");

    // Get order
    auto order = depMgr.getInitializationOrder();
    std::cout << "Initialization order: ";
    for (const auto& name : order) {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    // Test circular dependency
    depMgr.addDependency("X", "Y");
    depMgr.addDependency("Y", "Z");
    depMgr.addDependency("Z", "X");

    if (depMgr.hasCircularDependency()) {
        std::cout << "Circular dependency detected!" << std::endl;
        auto cycle = depMgr.findCircularDependency();
        std::cout << "Cycle: ";
        for (const auto& name : cycle) {
            std::cout << name << " -> ";
        }
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "=== Apollo IoC Framework Simple Test ===" << std::endl;

    test_component_lifecycle();
    test_config_manager();
    test_dependency_manager();

    std::cout << "\n=== Tests Completed ===" << std::endl;
    return 0;
}