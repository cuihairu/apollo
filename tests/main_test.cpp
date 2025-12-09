#include <gtest/gtest.h>
#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/ApplicationContext.h"
#include "apollo/ConfigManager.h"
#include "apollo/DependencyManager.h"

using namespace Apollo;

// Test components
class TestComponent : public BaseComponent {
public:
    TestComponent() : BaseComponent("TestComponent"), initialized(false) {}

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

    bool isInitialized() const { return initialized; }
    bool isStarted() const { return started; }

private:
    bool initialized = false;
    bool started = false;
};

class ServiceWithDependency : public BaseComponent {
public:
    ServiceWithDependency() : BaseComponent("ServiceWithDependency") {
        addDependency("TestComponent");
    }

    DECLARE_COMPONENT(ServiceWithDependency, "ServiceWithDependency")

    bool onInitialize() override {
        auto dep = getComponent<TestComponent>();
        return dep != nullptr && dep->isInitialized();
    }
};

REGISTER_COMPONENT(TestComponent)
REGISTER_COMPONENT(ServiceWithDependency)

// Component Tests
TEST(ComponentTest, BasicLifecycle) {
    auto component = std::make_shared<TestComponent>();

    EXPECT_EQ(component->getState(), ComponentState::UNINITIALIZED);
    EXPECT_FALSE(component->isInitialized());

    EXPECT_TRUE(component->initialize());
    EXPECT_EQ(component->getState(), ComponentState::INITIALIZED);
    EXPECT_TRUE(component->isInitialized());

    EXPECT_TRUE(component->start());
    EXPECT_EQ(component->getState(), ComponentState::STARTED);
    EXPECT_TRUE(component->isStarted());

    EXPECT_TRUE(component->stop());
    EXPECT_EQ(component->getState(), ComponentState::STOPPED);
    EXPECT_FALSE(component->isStarted());

    EXPECT_TRUE(component->destroy());
    EXPECT_EQ(component->getState(), ComponentState::DESTROYED);
    EXPECT_FALSE(component->isInitialized());
}

TEST(ComponentTest, DependencyManagement) {
    auto component = std::make_shared<TestComponent>();

    component->addDependency("Dep1");
    component->addDependency("Dep2");

    auto deps = component->getDependencies();
    EXPECT_EQ(deps.size(), 2);
    EXPECT_TRUE(component->dependsOn("Dep1"));
    EXPECT_TRUE(component->dependsOn("Dep2"));
    EXPECT_FALSE(component->dependsOn("NonExistent"));
}

// Context Tests
TEST(ContextTest, ComponentRegistration) {
    auto& context = ApplicationContext::getInstance();

    auto names = context.getComponentNames();
    EXPECT_GE(names.size(), 2);

    bool hasTestComponent = false;
    bool hasServiceWithDep = false;
    for (const auto& name : names) {
        if (name == "TestComponent") hasTestComponent = true;
        if (name == "ServiceWithDependency") hasServiceWithDep = true;
    }

    EXPECT_TRUE(hasTestComponent);
    EXPECT_TRUE(hasServiceWithDep);
}

TEST(ContextTest, ComponentRetrieval) {
    auto& context = ApplicationContext::getInstance();

    auto component = context.getComponent("TestComponent");
    EXPECT_NE(component, nullptr);
    EXPECT_EQ(component->getName(), "TestComponent");

    auto typedComponent = context.getComponent<TestComponent>();
    EXPECT_NE(typedComponent, nullptr);
    EXPECT_EQ(typedComponent->getName(), "TestComponent");

    auto nonExistent = context.getComponent("NonExistent");
    EXPECT_EQ(nonExistent, nullptr);
}

TEST(ContextTest, DependencyResolution) {
    auto& context = ApplicationContext::getInstance();

    EXPECT_TRUE(context.initializeComponents());

    auto testComp = context.getComponent<TestComponent>();
    EXPECT_TRUE(testComp != nullptr && testComp->isInitialized());

    auto service = context.getComponent<ServiceWithDependency>();
    EXPECT_NE(service, nullptr);

    EXPECT_TRUE(context.startComponents());
    EXPECT_TRUE(context.stopComponents());
    EXPECT_TRUE(context.destroyComponents());
}

// Config Tests
TEST(ConfigTest, BasicOperations) {
    auto config = ConfigManager::getInstance();

    config->setValue("test.int", 42);
    config->setValue("test.string", "hello");
    config->setValue("test.bool", true);

    EXPECT_EQ(42, config->getValue<int>("test.int"));
    EXPECT_EQ("hello", config->getValue<std::string>("test.string"));
    EXPECT_TRUE(config->getValue<bool>("test.bool"));

    EXPECT_EQ(100, config->getValue<int>("nonexistent", 100));
    EXPECT_FALSE(config->hasValue("nonexistent"));

    config->removeValue("test.int");
    EXPECT_FALSE(config->hasValue("test.int"));
}

TEST(ConfigTest, ChangeListeners) {
    auto config = ConfigManager::getInstance();

    int callCount = 0;
    config->addGlobalChangeListener([&](const std::string& key) {
        callCount++;
    });

    config->setValue("test.key", 1);
    EXPECT_EQ(1, callCount);

    config->setValue("test.key2", 2);
    EXPECT_EQ(2, callCount);
}

// Dependency Tests
TEST(DependencyTest, SimpleDependencies) {
    auto& depMgr = DependencyManager::getInstance();

    depMgr.addDependency("A", "B");
    depMgr.addDependency("B", "C");

    auto order = depMgr.getInitializationOrder();
    EXPECT_GE(order.size(), 3);

    auto posA = std::find(order.begin(), order.end(), "A");
    auto posB = std::find(order.begin(), order.end(), "B");
    auto posC = std::find(order.begin(), order.end(), "C");

    EXPECT_TRUE(posC < posB);
    EXPECT_TRUE(posB < posA);
}

TEST(DependencyTest, CircularDependency) {
    auto& depMgr = DependencyManager::getInstance();

    depMgr.addDependency("CircularA", "CircularB");
    depMgr.addDependency("CircularB", "CircularC");
    depMgr.addDependency("CircularC", "CircularA");

    EXPECT_TRUE(depMgr.hasCircularDependency());

    auto cycle = depMgr.findCircularDependency();
    EXPECT_FALSE(cycle.empty());
}

TEST(DependencyTest, TransitiveDependencies) {
    auto& depMgr = DependencyManager::getInstance();

    depMgr.addDependency("TransA", "TransB");
    depMgr.addDependency("TransB", "TransC");
    depMgr.addDependency("TransC", "TransD");

    auto transitive = depMgr.getTransitiveDependencies("TransA");

    EXPECT_TRUE(std::find(transitive.begin(), transitive.end(), "TransB") != transitive.end());
    EXPECT_TRUE(std::find(transitive.begin(), transitive.end(), "TransC") != transitive.end());
    EXPECT_TRUE(std::find(transitive.begin(), transitive.end(), "TransD") != transitive.end());
    EXPECT_TRUE(std::find(transitive.begin(), transitive.end(), "TransA") == transitive.end());
}

// Integration Tests
TEST(IntegrationTest, FullWorkflow) {
    auto& context = ApplicationContext::getInstance();
    auto config = ConfigManager::getInstance();

    // Configure some values
    config->setValue("app.name", "TestApp");
    config->setValue("app.version", 1);

    // Initialize components
    EXPECT_TRUE(context.initializeComponents());

    // Start components
    EXPECT_TRUE(context.startComponents());

    // Verify components are running
    auto component = context.getComponent<TestComponent>();
    EXPECT_TRUE(component != nullptr);
    EXPECT_TRUE(component->isStarted());

    // Shutdown
    EXPECT_TRUE(context.stopComponents());
    EXPECT_TRUE(context.destroyComponents());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}