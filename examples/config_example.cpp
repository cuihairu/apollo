#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include "apollo/ConfigProperty.h"
#include "apollo/ConfigManager.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace Apollo;

class WebServerComponent : public BaseComponent {
public:
    WebServerComponent() : BaseComponent("WebServerComponent") {}

    DECLARE_COMPONENT(WebServerComponent, "WebServerComponent")

    bool onInitialize() override {
        auto config = ConfigManager::getInstance();

        port.set(config->getValue<int>("server.port", 8080));
        maxConnections.set(config->getValue<int>("server.max_connections", 100));
        enableSsl.set(config->getValue<bool>("server.ssl.enabled", false));

        std::cout << "WebServerComponent: Initializing with port=" << port
                  << ", max_connections=" << maxConnections
                  << ", ssl=" << (enableSsl ? "enabled" : "disabled") << std::endl;

        port.addChangeListener([this](int newPort) {
            std::cout << "WebServerComponent: Port changed to " << newPort << std::endl;
        });

        return true;
    }

    bool onStart() override {
        std::cout << "WebServerComponent: Starting server on port " << port << std::endl;
        return true;
    }

private:
    CONFIG_PROPERTY(int, port, "server.port", 8080);
    CONFIG_PROPERTY(int, maxConnections, "server.max_connections", 100);
    CONFIG_PROPERTY(bool, enableSsl, "server.ssl.enabled", false);
};

class CacheComponent : public BaseComponent {
public:
    CacheComponent() : BaseComponent("CacheComponent") {}

    DECLARE_COMPONENT(CacheComponent, "CacheComponent")

    bool onInitialize() override {
        auto config = ConfigManager::getInstance();

        maxSize.set(config->getValue<int>("cache.max_size", 1000));
        ttl.set(config->getValue<int>("cache.ttl_seconds", 3600));

        std::cout << "CacheComponent: Initializing with max_size=" << maxSize
                  << ", ttl=" << ttl << "s" << std::endl;

        return true;
    }

    bool onStart() override {
        std::cout << "CacheComponent: Starting cache with size " << maxSize << std::endl;
        return true;
    }

private:
    CONFIG_PROPERTY(int, maxSize, "cache.max_size", 1000);
    CONFIG_PROPERTY(int, ttl, "cache.ttl_seconds", 3600);
};

REGISTER_COMPONENT(WebServerComponent)
REGISTER_COMPONENT(CacheComponent)

void createSampleConfig(const std::string& filename) {
    auto config = ConfigManager::getInstance();

    config->setValue("server.port", 9090);
    config->setValue("server.max_connections", 200);
    config->setValue("server.ssl.enabled", true);
    config->setValue("cache.max_size", 5000);
    config->setValue("cache.ttl_seconds", 7200);

    config->saveToFile(filename);
    std::cout << "Created sample config file: " << filename << std::endl;
}

int main() {
    const std::string configFile = "config.json";

    createSampleConfig(configFile);

    auto& context = ApplicationContext::getInstance();
    auto config = ConfigManager::getInstance();

    std::cout << "=== Config Demo ===" << std::endl;

    config->addGlobalChangeListener([](const std::string& key) {
        std::cout << "Config changed: " << key << std::endl;
    });

    if (!context.initializeComponents()) {
        std::cerr << "Failed to initialize components!" << std::endl;
        return 1;
    }

    if (!context.startComponents()) {
        std::cerr << "Failed to start components!" << std::endl;
        return 1;
    }

    std::cout << "\n=== Changing Configuration ===" << std::endl;

    std::cout << "Changing server port to 9999..." << std::endl;
    config->setValue("server.port", 9999);

    std::cout << "Waiting 2 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n=== Enabling Auto Reload ===" << std::endl;

    if (config->enableAutoReload(configFile)) {
        std::cout << "Auto-reload enabled for " << configFile << std::endl;
        std::cout << "Try editing the file manually (change server.port to 7777)" << std::endl;
        std::cout << "Press Enter to continue..." << std::endl;
        std::cin.get();
    }

    config->disableAutoReload();

    context.stopComponents();
    context.destroyComponents();

    return 0;
}