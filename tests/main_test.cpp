#include <gtest/gtest.h>
#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/ApplicationContext.h"
#include "apollo/ConfigManager.h"
#include "apollo/framework/ioc/ConfigEnvironment.h"
#include "apollo/DependencyManager.h"
#include "apollo/starter/ApolloApplication.h"
#include "apollo/starter/StarterRegistry.h"
#include <cstdlib>
#include <fstream>

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

    bool onDestroy() override {
        initialized = false;
        started = false;
        return true;
    }

    bool isInitialized() const { return initialized; }
    bool isStarted() const { return started; }

private:
    bool initialized = false;
    bool started = false;
};

// Test helper: expose dependency API for unit tests.
class TestComponentWithDeps : public TestComponent {
public:
    using TestComponent::TestComponent;
    using BaseComponent::addDependency;
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

class InfrastructurePhaseComponent : public BaseComponent {
public:
    InfrastructurePhaseComponent()
        : BaseComponent("InfrastructurePhaseComponent", LifecyclePhase::Infrastructure) {}

    DECLARE_COMPONENT(InfrastructurePhaseComponent, "InfrastructurePhaseComponent")
};

class GatewayPhaseComponent : public BaseComponent {
public:
    GatewayPhaseComponent()
        : BaseComponent("GatewayPhaseComponent", LifecyclePhase::Gateway) {}

    DECLARE_COMPONENT(GatewayPhaseComponent, "GatewayPhaseComponent")
};

class MissingDependencyComponent : public BaseComponent {
public:
    MissingDependencyComponent() : BaseComponent("MissingDependencyComponent") {
        addDependency("DefinitelyMissingDependency");
    }

    DECLARE_COMPONENT(MissingDependencyComponent, "MissingDependencyComponent")
};

class StarterRegisteredComponent : public BaseComponent {
public:
    StarterRegisteredComponent() : BaseComponent("StarterRegisteredComponent") {}

    DECLARE_COMPONENT(StarterRegisteredComponent, "StarterRegisteredComponent")
};

class LifecycleTrackedStarterComponent : public BaseComponent {
public:
    LifecycleTrackedStarterComponent()
        : BaseComponent("LifecycleTrackedStarterComponent") {}

    DECLARE_COMPONENT(LifecycleTrackedStarterComponent, "LifecycleTrackedStarterComponent")

    bool onInitialize() override {
        initialized_ = true;
        return true;
    }

    bool onStart() override {
        started_ = true;
        return true;
    }

    bool onStop() override {
        started_ = false;
        stopped_ = true;
        return true;
    }

    bool onDestroy() override {
        destroyed_ = true;
        return true;
    }

    bool wasInitialized() const { return initialized_; }
    bool wasStarted() const { return started_; }
    bool wasStopped() const { return stopped_; }
    bool wasDestroyed() const { return destroyed_; }

private:
    bool initialized_ = false;
    bool started_ = false;
    bool stopped_ = false;
    bool destroyed_ = false;
};

class TestApplicationStarter : public Apollo::Starter::ApolloStarter {
public:
    TestApplicationStarter() {
        StarterMetadata metadata;
        metadata.name = "test-application-starter";
        metadata.order = static_cast<int>(Apollo::Starter::StarterOrder::HIGH_PRECEDENCE);
        metadata.autoEnabled = true;
        setMetadata(metadata);
    }

    void registerBeans(Apollo::ApplicationContext& context) override {
        context.registerComponent<StarterRegisteredComponent>();
    }

    bool matches(const Apollo::Starter::ConditionContext& ctx) override {
        return ctx.getProperty("starter.test.enabled", true);
    }
};

class LifecycleApplicationStarter : public Apollo::Starter::ApolloStarter {
public:
    LifecycleApplicationStarter() {
        StarterMetadata metadata;
        metadata.name = "lifecycle-application-starter";
        metadata.order = static_cast<int>(Apollo::Starter::StarterOrder::HIGH_PRECEDENCE);
        metadata.autoEnabled = true;
        setMetadata(metadata);
    }

    void registerBeans(Apollo::ApplicationContext& context) override {
        context.registerComponent<LifecycleTrackedStarterComponent>();
    }
};

class RollbackInitSuccessComponent : public BaseComponent {
public:
    RollbackInitSuccessComponent() : BaseComponent("RollbackInitSuccessComponent") {}

    DECLARE_COMPONENT(RollbackInitSuccessComponent, "RollbackInitSuccessComponent")

    bool onInitialize() override {
        initialized_ = true;
        return true;
    }

    bool onDestroy() override {
        destroyed_ = true;
        return true;
    }

    bool initialized() const { return initialized_; }
    bool destroyed() const { return destroyed_; }

private:
    bool initialized_ = false;
    bool destroyed_ = false;
};

class RollbackInitFailComponent : public BaseComponent {
public:
    RollbackInitFailComponent() : BaseComponent("RollbackInitFailComponent") {
        addDependency("RollbackInitSuccessComponent");
    }

    DECLARE_COMPONENT(RollbackInitFailComponent, "RollbackInitFailComponent")

    bool onInitialize() override {
        return false;
    }
};

class RollbackStartSuccessComponent : public BaseComponent {
public:
    RollbackStartSuccessComponent() : BaseComponent("RollbackStartSuccessComponent") {}

    DECLARE_COMPONENT(RollbackStartSuccessComponent, "RollbackStartSuccessComponent")

    bool onInitialize() override {
        initialized_ = true;
        return true;
    }

    bool onStart() override {
        started_ = true;
        return true;
    }

    bool onStop() override {
        stopped_ = true;
        started_ = false;
        return true;
    }

    bool onDestroy() override {
        destroyed_ = true;
        return true;
    }

    bool initialized() const { return initialized_; }
    bool started() const { return started_; }
    bool stopped() const { return stopped_; }
    bool destroyed() const { return destroyed_; }

private:
    bool initialized_ = false;
    bool started_ = false;
    bool stopped_ = false;
    bool destroyed_ = false;
};

class RollbackStartFailComponent : public BaseComponent {
public:
    RollbackStartFailComponent() : BaseComponent("RollbackStartFailComponent") {
        addDependency("RollbackStartSuccessComponent");
    }

    DECLARE_COMPONENT(RollbackStartFailComponent, "RollbackStartFailComponent")

    bool onInitialize() override {
        return true;
    }

    bool onStart() override {
        return false;
    }
};

REGISTER_COMPONENT(TestComponent)
REGISTER_COMPONENT(ServiceWithDependency)
REGISTER_COMPONENT(InfrastructurePhaseComponent)
REGISTER_COMPONENT(GatewayPhaseComponent)

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
    auto component = std::make_shared<TestComponentWithDeps>();

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

TEST(ContextTest, OrdersComponentsByPhaseWhenIndependent) {
    auto& context = ApplicationContext::getInstance();

    const auto order = context.getDependencyOrder();

    auto infraIt = std::find(order.begin(), order.end(), "InfrastructurePhaseComponent");
    auto testIt = std::find(order.begin(), order.end(), "TestComponent");
    auto gatewayIt = std::find(order.begin(), order.end(), "GatewayPhaseComponent");

    ASSERT_NE(infraIt, order.end());
    ASSERT_NE(testIt, order.end());
    ASSERT_NE(gatewayIt, order.end());

    EXPECT_LT(infraIt, testIt);
    EXPECT_LT(testIt, gatewayIt);
}

TEST(ContextTest, MissingDependencyFailsDuringOrdering) {
    auto& context = ApplicationContext::getInstance();

    const bool registered = context.registerComponent<MissingDependencyComponent>();
    EXPECT_TRUE(registered);

    EXPECT_THROW(context.getDependencyOrder(), std::runtime_error);
    EXPECT_TRUE(context.unregisterComponent("MissingDependencyComponent"));
}

TEST(ContextTest, RegistersExplicitBeanDefinition) {
    auto& context = ApplicationContext::getInstance();

    BeanDefinition definition;
    definition.name = "ExplicitDefinitionComponent";
    definition.phase = static_cast<int>(LifecyclePhase::Infrastructure);
    definition.dependencies = {"TestComponent"};
    definition.metadata["source"] = "unit-test";
    definition.factory = []() -> ComponentPtr {
        return std::make_shared<TestComponent>();
    };

    EXPECT_TRUE(context.registerBeanDefinition(definition));
    EXPECT_TRUE(context.hasBeanDefinition("ExplicitDefinitionComponent"));

    const auto definitions = context.getBeanDefinitions();
    const auto it = std::find_if(definitions.begin(), definitions.end(),
        [](const BeanDefinition& item) {
            return item.name == "ExplicitDefinitionComponent";
        });

    ASSERT_NE(it, definitions.end());
    EXPECT_EQ(it->phase, static_cast<int>(LifecyclePhase::Infrastructure));
    ASSERT_EQ(it->dependencies.size(), 1u);
    EXPECT_EQ(it->dependencies.front(), "TestComponent");
    EXPECT_EQ(it->metadata.at("source"), "unit-test");

    const auto order = context.getDependencyOrder();
    auto explicitIt = std::find(order.begin(), order.end(), "ExplicitDefinitionComponent");
    auto testIt = std::find(order.begin(), order.end(), "TestComponent");

    ASSERT_NE(explicitIt, order.end());
    ASSERT_NE(testIt, order.end());
    EXPECT_LT(testIt, explicitIt);

    EXPECT_TRUE(context.unregisterComponent("ExplicitDefinitionComponent"));
}

TEST(ContextTest, RuntimeInfoTracksRegisteredAndInstantiatedBeans) {
    auto& context = ApplicationContext::getInstance();

    BeanRuntimeInfo info;
    ASSERT_TRUE(context.getBeanRuntimeInfo("TestComponent", info));
    EXPECT_EQ(info.name, "TestComponent");
    EXPECT_EQ(info.stage, BeanLifecycleStage::Registered);
    EXPECT_FALSE(info.instantiated);
    EXPECT_EQ(info.lastOperation, "register");

    auto component = context.getComponent<TestComponent>();
    ASSERT_NE(component, nullptr);

    ASSERT_TRUE(context.getBeanRuntimeInfo("TestComponent", info));
    EXPECT_TRUE(info.instantiated);
    EXPECT_EQ(info.stage, BeanLifecycleStage::Instantiated);
    EXPECT_EQ(info.lastOperation, "instantiate");
}

TEST(ContextTest, InitializeRollbackDestroysPreviouslyInitializedComponents) {
    auto& context = ApplicationContext::getInstance();

    EXPECT_TRUE(context.registerComponent<RollbackInitSuccessComponent>());
    EXPECT_TRUE(context.registerComponent<RollbackInitFailComponent>());

    EXPECT_FALSE(context.initializeComponents());

    auto success = context.getComponent<RollbackInitSuccessComponent>();
    ASSERT_NE(success, nullptr);
    EXPECT_TRUE(success->initialized());
    EXPECT_TRUE(success->destroyed());
    EXPECT_EQ(success->getState(), ComponentState::DESTROYED);

    EXPECT_TRUE(context.unregisterComponent("RollbackInitSuccessComponent"));
    EXPECT_TRUE(context.unregisterComponent("RollbackInitFailComponent"));
    context.destroyComponents();
}

TEST(ContextTest, StartRollbackStopsAndDestroysPreviouslyStartedComponents) {
    auto& context = ApplicationContext::getInstance();

    EXPECT_TRUE(context.registerComponent<RollbackStartSuccessComponent>());
    EXPECT_TRUE(context.registerComponent<RollbackStartFailComponent>());

    EXPECT_TRUE(context.initializeComponents());
    EXPECT_FALSE(context.startComponents());

    auto success = context.getComponent<RollbackStartSuccessComponent>();
    ASSERT_NE(success, nullptr);
    EXPECT_TRUE(success->initialized());
    EXPECT_TRUE(success->stopped());
    EXPECT_TRUE(success->destroyed());
    EXPECT_EQ(success->getState(), ComponentState::DESTROYED);

    EXPECT_TRUE(context.unregisterComponent("RollbackStartSuccessComponent"));
    EXPECT_TRUE(context.unregisterComponent("RollbackStartFailComponent"));
    context.destroyComponents();
}

TEST(ContextTest, RuntimeInfoTracksStartedBeans) {
    auto& context = ApplicationContext::getInstance();

    EXPECT_TRUE(context.initializeComponents());
    EXPECT_TRUE(context.startComponents());

    BeanRuntimeInfo info;
    ASSERT_TRUE(context.getBeanRuntimeInfo("TestComponent", info));
    EXPECT_EQ(info.stage, BeanLifecycleStage::Started);
    EXPECT_EQ(info.lastOperation, "start");
    EXPECT_EQ(info.componentState, ComponentState::STARTED);

    EXPECT_TRUE(context.stopComponents());
    EXPECT_TRUE(context.destroyComponents());

    ASSERT_TRUE(context.getBeanRuntimeInfo("TestComponent", info));
    EXPECT_EQ(info.stage, BeanLifecycleStage::Destroyed);
    EXPECT_EQ(info.lastOperation, "destroy");
    EXPECT_EQ(info.componentState, ComponentState::DESTROYED);
}

TEST(ContextTest, RuntimeInfoTracksRollbackAndFailure) {
    auto& context = ApplicationContext::getInstance();

    EXPECT_TRUE(context.registerComponent<RollbackStartSuccessComponent>());
    EXPECT_TRUE(context.registerComponent<RollbackStartFailComponent>());

    EXPECT_TRUE(context.initializeComponents());
    EXPECT_FALSE(context.startComponents());

    BeanRuntimeInfo failInfo;
    ASSERT_TRUE(context.getBeanRuntimeInfo("RollbackStartFailComponent", failInfo));
    EXPECT_EQ(failInfo.stage, BeanLifecycleStage::Failed);
    EXPECT_EQ(failInfo.lastOperation, "start");
    EXPECT_FALSE(failInfo.lastError.empty());

    BeanRuntimeInfo successInfo;
    ASSERT_TRUE(context.getBeanRuntimeInfo("RollbackStartSuccessComponent", successInfo));
    EXPECT_EQ(successInfo.stage, BeanLifecycleStage::Destroyed);
    EXPECT_EQ(successInfo.lastOperation, "rollback-destroy");
    EXPECT_EQ(successInfo.componentState, ComponentState::DESTROYED);

    EXPECT_TRUE(context.unregisterComponent("RollbackStartSuccessComponent"));
    EXPECT_TRUE(context.unregisterComponent("RollbackStartFailComponent"));
    context.destroyComponents();
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

    auto callCount = std::make_shared<std::atomic<int>>(0);
    config->addGlobalChangeListener([callCount](const std::string&) {
        callCount->fetch_add(1);
    });

    config->setValue("test.key", 1);
    EXPECT_EQ(1, callCount->load());

    config->setValue("test.key2", 2);
    EXPECT_EQ(2, callCount->load());
}

TEST(ConfigTest, EnvironmentPriorityResolution) {
    ConfigEnvironment env;

    env.set("defaults", ConfigSourcePriority::Defaults, "server.port", "7000");
    env.set("base", ConfigSourcePriority::BaseConfig, "server.port", "7100");
    env.set("env", ConfigSourcePriority::EnvironmentConfig, "server.port", "7200");
    env.set("cli", ConfigSourcePriority::CommandLine, "server.port", "7300");

    EXPECT_TRUE(env.has("server.port"));
    EXPECT_EQ(env.getInt("server.port", 0), 7300);
}

TEST(ConfigTest, EnvironmentExplainAndListParsing) {
    ConfigEnvironment env;

    env.set("defaults", ConfigSourcePriority::Defaults, "feature.flags", "a, b");
    env.set("memory", ConfigSourcePriority::InMemoryOverride, "feature.flags", "a, b, c");

    const auto values = env.getList("feature.flags");
    ASSERT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], "a");
    EXPECT_EQ(values[1], "b");
    EXPECT_EQ(values[2], "c");

    const auto explain = env.explain("feature.flags");
    ASSERT_EQ(explain.size(), 2u);
    EXPECT_EQ(explain[0].source, "memory");
    EXPECT_EQ(explain[0].value, "a, b, c");
    EXPECT_GT(explain[0].priority, explain[1].priority);
}

TEST(ConfigTest, ApplicationContextConfigEnvironmentSync) {
    auto& context = ApplicationContext::getInstance();
    auto config = ConfigManager::getInstance();
    config->clear();

    auto env = std::make_shared<ConfigEnvironment>();
    env->set("defaults", ConfigSourcePriority::Defaults, "app.mode", "dev");
    env->set("cli", ConfigSourcePriority::CommandLine, "app.mode", "prod");
    env->set("memory", ConfigSourcePriority::InMemoryOverride, "feature.toggle", "true");

    context.setConfigEnvironment(env);
    context.syncConfigEnvironmentToManager();

    EXPECT_EQ(config->getValue<std::string>("app.mode"), "prod");
    EXPECT_TRUE(config->getValue<bool>("feature.toggle"));
}

TEST(StarterTest, ApolloApplicationRegistersStarterBeans) {
    auto& registry = Apollo::Starter::StarterRegistry::getInstance();
    registry.clear();
    ASSERT_TRUE(registry.registerStarter(std::make_shared<TestApplicationStarter>()));

    auto& context = ApplicationContext::getInstance();
    context.unregisterComponent("StarterRegisteredComponent");

    auto app = Apollo::Starter::ApolloApplication::Builder()
        .withProperty("starter.test.enabled", true)
        .build();

    ASSERT_NE(app, nullptr);
    EXPECT_TRUE(context.hasBeanDefinition("StarterRegisteredComponent"));

    auto component = context.getComponent<StarterRegisteredComponent>();
    EXPECT_NE(component, nullptr);

    registry.clear();
    context.unregisterComponent("StarterRegisteredComponent");
}

TEST(StarterTest, ApolloApplicationWritesPropertiesToConfigEnvironment) {
    auto& registry = Apollo::Starter::StarterRegistry::getInstance();
    registry.clear();

    auto& context = ApplicationContext::getInstance();
    auto env = std::make_shared<ConfigEnvironment>();
    context.setConfigEnvironment(env);

    auto app = Apollo::Starter::ApolloApplication::Builder()
        .withProperty("runtime.profile", "test")
        .withProperty("server.port", 8123)
        .build();

    ASSERT_NE(app, nullptr);
    auto configEnv = context.getConfigEnvironment();
    ASSERT_NE(configEnv, nullptr);
    EXPECT_EQ(configEnv->getString("runtime.profile"), "test");
    EXPECT_EQ(configEnv->getInt("server.port", 0), 8123);
}

TEST(StarterTest, ApolloApplicationMergesFileEnvAndBuilderProperties) {
    auto& registry = Apollo::Starter::StarterRegistry::getInstance();
    registry.clear();

    const std::string configPath = "/tmp/apollo_starter_config_test.ini";
    {
        std::ofstream out(configPath);
        out << "server.port=7000\n";
        out << "runtime.profile=file\n";
    }

#if defined(_WIN32)
    _putenv_s("APOLLO_SERVER_PORT", "7100");
    _putenv_s("APOLLO_RUNTIME_PROFILE", "env");
#else
    setenv("APOLLO_SERVER_PORT", "7100", 1);
    setenv("APOLLO_RUNTIME_PROFILE", "env", 1);
#endif

    auto& context = ApplicationContext::getInstance();
    auto env = std::make_shared<ConfigEnvironment>();
    context.setConfigEnvironment(env);

    auto app = Apollo::Starter::ApolloApplication::Builder()
        .withConfigFile(configPath)
        .withProperty("server.port", 7200)
        .build();

    ASSERT_NE(app, nullptr);
    auto configEnv = context.getConfigEnvironment();
    ASSERT_NE(configEnv, nullptr);

    EXPECT_EQ(configEnv->getInt("server.port", 0), 7200);
    EXPECT_EQ(configEnv->getString("runtime.profile"), "env");

    const auto explainPort = configEnv->explain("server.port");
    ASSERT_EQ(explainPort.size(), 3u);
    EXPECT_EQ(explainPort[0].source, "starter.builder");
    EXPECT_EQ(explainPort[1].source, "starter.env");
    EXPECT_EQ(explainPort[2].source, "starter.file");

#if defined(_WIN32)
    _putenv_s("APOLLO_SERVER_PORT", "");
    _putenv_s("APOLLO_RUNTIME_PROFILE", "");
#else
    unsetenv("APOLLO_SERVER_PORT");
    unsetenv("APOLLO_RUNTIME_PROFILE");
#endif
    std::remove(configPath.c_str());
}

TEST(StarterTest, ApolloApplicationClearsPreviousBuilderSourceBeforeRebuild) {
    auto& registry = Apollo::Starter::StarterRegistry::getInstance();
    registry.clear();

    auto& context = ApplicationContext::getInstance();
    auto env = std::make_shared<ConfigEnvironment>();
    context.setConfigEnvironment(env);

    auto app1 = Apollo::Starter::ApolloApplication::Builder()
        .withProperty("ephemeral.key", "first")
        .build();
    ASSERT_NE(app1, nullptr);
    ASSERT_NE(context.getConfigEnvironment(), nullptr);
    EXPECT_EQ(context.getConfigEnvironment()->getString("ephemeral.key"), "first");

    auto app2 = Apollo::Starter::ApolloApplication::Builder().build();
    ASSERT_NE(app2, nullptr);
    EXPECT_FALSE(context.getConfigEnvironment()->has("ephemeral.key"));
}

TEST(StarterTest, ApolloApplicationDrivesComponentLifecycle) {
    auto& registry = Apollo::Starter::StarterRegistry::getInstance();
    registry.clear();
    ASSERT_TRUE(registry.registerStarter(std::make_shared<LifecycleApplicationStarter>()));

    auto& context = ApplicationContext::getInstance();
    context.unregisterComponent("LifecycleTrackedStarterComponent");

    auto app = Apollo::Starter::ApolloApplication::Builder().build();
    ASSERT_NE(app, nullptr);

    app->start();

    auto component = context.getComponent<LifecycleTrackedStarterComponent>();
    ASSERT_NE(component, nullptr);
    EXPECT_TRUE(component->wasInitialized());
    EXPECT_TRUE(component->wasStarted());
    EXPECT_EQ(component->getState(), ComponentState::STARTED);

    app->stop();

    EXPECT_TRUE(component->wasStopped());
    EXPECT_TRUE(component->wasDestroyed());
    EXPECT_EQ(component->getState(), ComponentState::DESTROYED);

    registry.clear();
    context.unregisterComponent("LifecycleTrackedStarterComponent");
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
