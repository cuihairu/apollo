#include "apollo/framework/base/BaseComponent.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include "apollo/framework/ioc/DependencyManager.h"
#include <iostream>

using namespace Apollo;

class StorageComponent : public BaseComponent {
public:
    StorageComponent() : BaseComponent("StorageComponent") {}

    DECLARE_COMPONENT(StorageComponent, "StorageComponent")

    bool onInitialize() override {
        std::cout << "StorageComponent: Initialized" << std::endl;
        return true;
    }

    void store(const std::string& data) {
        std::cout << "StorageComponent: Storing " << data << std::endl;
    }
};

class NetworkComponent : public BaseComponent {
public:
    NetworkComponent() : BaseComponent("NetworkComponent") {}

    DECLARE_COMPONENT(NetworkComponent, "NetworkComponent")

    bool onInitialize() override {
        std::cout << "NetworkComponent: Initialized" << std::endl;
        return true;
    }

    void send(const std::string& data) {
        std::cout << "NetworkComponent: Sending " << data << std::endl;
    }
};

class AuthComponent : public BaseComponent {
public:
    AuthComponent() : BaseComponent("AuthComponent") {
        addDependency("StorageComponent");
        addDependency("NetworkComponent");
    }

    DECLARE_COMPONENT(AuthComponent, "AuthComponent")

    bool onInitialize() override {
        std::cout << "AuthComponent: Initialized with dependencies" << std::endl;
        return true;
    }

    bool authenticate(const std::string& user, const std::string& pass) {
        auto storage = getComponent<StorageComponent>();
        auto network = getComponent<NetworkComponent>();

        storage->store("Auth attempt for " + user);
        network->send("Auth request for " + user);

        return pass == "password123";
    }
};

class ApiServiceComponent : public BaseComponent {
public:
    ApiServiceComponent() : BaseComponent("ApiServiceComponent") {
        addDependency("AuthComponent");
        addDependency("NetworkComponent");
    }

    DECLARE_COMPONENT(ApiServiceComponent, "ApiServiceComponent")

    bool onInitialize() override {
        std::cout << "ApiServiceComponent: Initialized with dependencies" << std::endl;
        return true;
    }

    void handleRequest(const std::string& endpoint) {
        std::cout << "ApiServiceComponent: Handling " << endpoint << std::endl;

        if (endpoint == "/login") {
            auto auth = getComponent<AuthComponent>();
            if (auth && auth->authenticate("admin", "password123")) {
                std::cout << "ApiServiceComponent: Login successful" << std::endl;
            } else {
                std::cout << "ApiServiceComponent: Login failed" << std::endl;
            }
        }
    }
};

class MonitoringComponent : public BaseComponent {
public:
    MonitoringComponent() : BaseComponent("MonitoringComponent") {
        addDependency("ApiServiceComponent");
        addDependency("StorageComponent");
    }

    DECLARE_COMPONENT(MonitoringComponent, "MonitoringComponent")

    bool onInitialize() override {
        std::cout << "MonitoringComponent: Initialized with dependencies" << std::endl;
        return true;
    }

    void startMonitoring() {
        std::cout << "MonitoringComponent: Monitoring all services" << std::endl;
    }
};

REGISTER_COMPONENT(StorageComponent)
REGISTER_COMPONENT(NetworkComponent)
REGISTER_COMPONENT(AuthComponent)
REGISTER_COMPONENT(ApiServiceComponent)
REGISTER_COMPONENT(MonitoringComponent)

void demonstrateDependencyResolution() {
    std::cout << "\n=== Dependency Resolution Demo ===" << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    depMgr.addDependency("AuthComponent", "StorageComponent");
    depMgr.addDependency("AuthComponent", "NetworkComponent");
    depMgr.addDependency("ApiServiceComponent", "AuthComponent");
    depMgr.addDependency("ApiServiceComponent", "NetworkComponent");
    depMgr.addDependency("MonitoringComponent", "ApiServiceComponent");
    depMgr.addDependency("MonitoringComponent", "StorageComponent");

    std::cout << "Initialization order:" << std::endl;
    auto initOrder = depMgr.getInitializationOrder();
    for (const auto& name : initOrder) {
        std::cout << "  " << name << std::endl;
    }

    std::cout << "\nShutdown order:" << std::endl;
    auto shutdownOrder = depMgr.getShutdownOrder();
    for (const auto& name : shutdownOrder) {
        std::cout << "  " << name << std::endl;
    }

    std::cout << "\nValidating dependencies..." << std::endl;
    if (depMgr.validateDependencies()) {
        std::cout << "All dependencies are valid!" << std::endl;
    } else {
        std::cout << "Dependency validation failed!" << std::endl;
        auto missing = depMgr.getMissingDependencies();
        for (const auto& dep : missing) {
            std::cout << "  Missing: " << dep << std::endl;
        }
    }

    auto transitiveDeps = depMgr.getTransitiveDependencies("MonitoringComponent");
    std::cout << "\nTransitive dependencies of MonitoringComponent:" << std::endl;
    for (const auto& dep : transitiveDeps) {
        std::cout << "  " << dep << std::endl;
    }
}

void demonstrateCircularDependency() {
    std::cout << "\n=== Circular Dependency Demo ===" << std::endl;

    auto& depMgr = DependencyManager::getInstance();

    depMgr.addDependency("ComponentA", "ComponentB");
    depMgr.addDependency("ComponentB", "ComponentC");
    depMgr.addDependency("ComponentC", "ComponentA");

    if (depMgr.hasCircularDependency()) {
        std::cout << "Circular dependency detected!" << std::endl;
        auto cycle = depMgr.findCircularDependency();
        std::cout << "Cycle: ";
        for (size_t i = 0; i < cycle.size(); ++i) {
            if (i > 0) std::cout << " -> ";
            std::cout << cycle[i];
        }
        std::cout << std::endl;
    }
}

int main() {
    auto& context = ApplicationContext::getInstance();

    std::cout << "=== Apollo IoC Dependency Management Demo ===" << std::endl;

    demonstrateDependencyResolution();
    demonstrateCircularDependency();

    std::cout << "\n=== Component Lifecycle Demo ===" << std::endl;

    if (!context.initializeComponents()) {
        std::cerr << "Failed to initialize components!" << std::endl;
        return 1;
    }

    if (!context.startComponents()) {
        std::cerr << "Failed to start components!" << std::endl;
        return 1;
    }

    std::cout << "\n=== Application Running ===" << std::endl;
    auto api = context.getComponent<ApiServiceComponent>();
    if (api) {
        api->handleRequest("/login");
    }

    auto monitoring = context.getComponent<MonitoringComponent>();
    if (monitoring) {
        monitoring->startMonitoring();
    }

    std::cout << "\n=== Shutting Down ===" << std::endl;
    context.stopComponents();
    context.destroyComponents();

    return 0;
}