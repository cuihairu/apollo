#include "apollo/framework/base/BaseComponent.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include "apollo/framework/ioc/ComponentRegistry.h"
#include "apollo/framework/ioc/ConfigManager.h"
#include "apollo/utils/logging/logger.hpp"
#include "apollo/utils/threading/thread_pool.hpp"
#include "apollo/core/server_id.hpp"
#include "apollo/game/attributes/attribute.hpp"
#include "apollo/game/aoi/aoi.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace Apollo;        // IoC / BaseComponent / ConfigManager
using namespace apollo;        // Logger / ThreadPool / Attribute / AOI
using namespace apollo::core;  // ServerId

class DemoComponent : public BaseComponent {
public:
    DemoComponent() : BaseComponent("DemoComponent") {
        logger_ = LogManager::Instance().GetLogger("DemoComponent");
    }

    DECLARE_COMPONENT(DemoComponent, "DemoComponent")

protected:
    bool onInitialize() override {
        LOG_INFO(logger_, "DemoComponent initialized");
        return true;
    }

    bool onStart() override {
        LOG_INFO(logger_, "DemoComponent started");
        return true;
    }

    bool onStop() override {
        LOG_INFO(logger_, "DemoComponent stopped");
        return true;
    }

    bool onDestroy() override {
        LOG_INFO(logger_, "DemoComponent destroyed");
        return true;
    }

private:
    std::shared_ptr<Logger> logger_;
};

REGISTER_COMPONENT(DemoComponent)

class AOIListenerImpl : public IAOIListener {
public:
    void OnAOIEvent(const AOIEvent& event) override {
        std::cout << "AOI Event: entity=" << event.entityId
                  << " scene=" << event.sceneId
                  << " type=" << static_cast<int>(event.type) << std::endl;
    }
};

static void TestConfigSystem() {
    std::cout << "\n=== Testing Config System ===" << std::endl;

    auto* config = ConfigManager::getInstance();
    config->setValue("server.host", std::string("localhost"));
    config->setValue("server.port", 7700);
    config->setValue("game.max_players", 5000);

    std::string host = config->getValue<std::string>("server.host", "127.0.0.1");
    int port = config->getValue<int>("server.port", 8080);
    int maxPlayers = config->getValue<int>("game.max_players", 1000);

    std::cout << "Server Host: " << host << std::endl;
    std::cout << "Server Port: " << port << std::endl;
    std::cout << "Max Players: " << maxPlayers << std::endl;
}

static void TestServerId() {
    std::cout << "\n=== Testing ServerId ===" << std::endl;

    auto loginGate = ServerId::Create(1, 1, 1, 1);
    auto gameServer = ServerId::Create(1, 1, 6, 1);
    auto dbServer = ServerId::Create(1, 1, 3, 1);

    std::cout << "LoginGate ID: " << loginGate.ToString() << std::endl;
    std::cout << "GameServer ID: " << gameServer.ToString() << std::endl;
    std::cout << "DBServer ID: " << dbServer.ToString() << std::endl;

    ServerId parsed = ServerId::FromString("1-1-6-2");
    std::cout << "Parsed ID - Region: " << parsed.GetRegion()
              << ", Group: " << parsed.GetGroup()
              << ", TypeValue: " << parsed.GetTypeValue()
              << ", Instance: " << parsed.GetInstance() << std::endl;
}

static void TestLoggerSystem() {
    std::cout << "\n=== Testing Logger ===" << std::endl;
    auto logger = LogManager::Instance().GetLogger("all_features_demo");
    LOG_INFO(logger, "This is an info message");
    LOG_WARN(logger, "This is a warning message");
    LOG_ERROR(logger, "This is an error message: %d", 42);
}

static void TestThreadPool() {
    std::cout << "\n=== Testing ThreadPool ===" << std::endl;

    ThreadPool pool(4);
    std::vector<std::future<int>> results;

    for (int i = 0; i < 8; ++i) {
        results.emplace_back(
            pool.Submit([i]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return i * i;
            })
        );
    }

    for (auto& result : results) {
        std::cout << "Task result: " << result.get() << std::endl;
    }
}

static void TestAttributeSystem() {
    std::cout << "\n=== Testing Attribute System ===" << std::endl;

    auto& mgr = AttributeManager::Instance();

    AttributeDef hpDef;
    hpDef.id = 1;
    hpDef.name = "HP";
    hpDef.type = AttributeType::INT32;
    hpDef.defaultValue = int32_t(100);
    hpDef.minValue = int32_t(0);
    hpDef.maxValue = int32_t(9999);
    hpDef.persistent = true;
    hpDef.syncToClient = true;

    mgr.RegisterAttribute(hpDef);

    auto container = mgr.CreateContainer(1001);
    SET_ATTR(container, 1, int32_t(150));
    std::cout << "Player HP: " << GET_ATTR(container, 1, int32_t) << std::endl;
}

static void TestAOISystem() {
    std::cout << "\n=== Testing AOI System ===" << std::endl;

    AOIManager& aoi = AOIManager::Instance();
    aoi.Initialize(100.0f);

    auto listener = std::make_shared<AOIListenerImpl>();
    aoi.SetListener(listener);

    AOIEntity player1(1001, 1);
    player1.position = Vector3(50, 0, 50);
    player1.aoiRadius = 30.0f;

    AOIEntity player2(1002, 1);
    player2.position = Vector3(55, 0, 55);
    player2.aoiRadius = 30.0f;

    aoi.UpdateEntity(player1);
    aoi.UpdateEntity(player2);

    auto visible1 = aoi.GetVisibleEntities(1001);
    std::cout << "Player 1001 can see: ";
    for (auto id : visible1) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Apollo Framework Feature Demo ===" << std::endl;

    LogManager::Instance().Initialize();

    TestConfigSystem();
    TestServerId();
    TestLoggerSystem();
    TestThreadPool();
    TestAttributeSystem();
    TestAOISystem();

    // IoC container demo (lifecycle smoke test)
    auto& context = ApplicationContext::getInstance();
    context.initializeComponents();
    context.startComponents();
    context.stopComponents();
    context.destroyComponents();

    std::cout << "\n=== All Tests Completed ===" << std::endl;
    return 0;
}
