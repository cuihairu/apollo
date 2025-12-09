#include "apollo/framework/ioc/IComponent.h"
#include "apollo/framework/ioc/ApplicationContext.h"
#include "apollo/ConfigManager.h"
#include "apollo/utils/logging/logger.hpp"
#include "apollo/utils/threading/thread_pool.hpp"
#include "apollo/game/attributes/attribute.hpp"
#include "apollo/game/aoi/aoi.hpp"
#include "apollo/db/redis.hpp"
#include "apollo/core/server_id.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace apollo;
using namespace apollo::db;
using namespace apollo::battle;

// 自定义组件示例
class GameLogicComponent : public BaseComponent {
public:
    bool initialize() override {
        logger.Info("GameLogicComponent initialized");
        return true;
    }

    bool start() override {
        logger.Info("GameLogicComponent started");
        return true;
    }

    bool stop() override {
        logger.Info("GameLogicComponent stopped");
        return true;
    }

    bool destroy() override {
        logger.Info("GameLogicComponent destroyed");
        return true;
    }

private:
    Logger logger{"GameLogicComponent"};
};

// AOI监听器示例
class AOIListenerImpl : public IAOIListener {
public:
    void OnAOIEvent(const AOIEvent& event) override {
        std::cout << "AOI Event: Entity " << event.entityId
                  << " " << (event.type == AOIEventType::ENTER ? "entered" : "left")
                  << " scene " << event.sceneId << std::endl;
    }
};

// 测试配置系统
void TestConfigSystem() {
    std::cout << "\n=== Testing Config System ===" << std::endl;

    ConfigManager& config = ConfigManager::getInstance();

    // 设置一些测试配置
    config.Set("server.host", "localhost");
    config.Set("server.port", 7700);
    config.Set("game.max_players", 5000);

    // 读取配置
    std::string host = config.Get("server.host", "127.0.0.1");
    int port = config.GetInt("server.port", 8080);
    int maxPlayers = config.GetInt("game.max_players", 1000);

    std::cout << "Server Host: " << host << std::endl;
    std::cout << "Server Port: " << port << std::endl;
    std::cout << "Max Players: " << maxPlayers << std::endl;
}

// 测试ServerID
void TestServerID() {
    std::cout << "\n=== Testing ServerID ===" << std::endl;

    // 创建不同类型的服务器ID
    auto loginGate = ServerId::Create(1, 1, 1, 1);
    auto gameServer = ServerId::Create(1, 1, 6, 1);
    auto dbServer = ServerId::Create(1, 1, 3, 1);

    std::cout << "LoginGate ID: " << loginGate.ToString() << std::endl;
    std::cout << "GameServer ID: " << gameServer.ToString() << std::endl;
    std::cout << "DBServer ID: " << dbServer.ToString() << std::endl;

    // 解析ServerID
    ServerId parsed = ServerId::Parse("1-1-6-2");
    std::cout << "Parsed ID - Region: " << parsed.GetRegion()
              << ", Group: " << parsed.GetGroup()
              << ", Type: " << parsed.GetType()
              << ", Instance: " << parsed.GetInstance() << std::endl;
}

// 测试日志系统
void TestLogger() {
    std::cout << "\n=== Testing Logger System ===" << std::endl;

    Logger logger("TestModule");

    logger.Info("This is an info message");
    logger.Warning("This is a warning message");
    logger.Error("This is an error message");

    // 测试格式化日志
    int playerId = 12345;
    std::string playerName = "TestPlayer";
    logger.Info("Player %s(ID: %d) logged in", playerName, playerId);
}

// 测试线程池
void TestThreadPool() {
    std::cout << "\n=== Testing Thread Pool ===" << std::endl;

    ThreadPool pool(4);

    std::vector<std::future<int>> results;

    // 提交一些任务
    for (int i = 0; i < 10; ++i) {
        results.emplace_back(
            pool.Submit([i] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return i * i;
            })
        );
    }

    // 获取结果
    for (auto& result : results) {
        std::cout << "Task result: " << result.get() << std::endl;
    }
}

// 测试属性系统
void TestAttributeSystem() {
    std::cout << "\n=== Testing Attribute System ===" << std::endl;

    AttributeContainer playerAttr;

    // 设置属性
    playerAttr.Set("level", 50);
    playerAttr.Set("hp", 1000);
    playerAttr.Set("maxHp", 1500);
    playerAttr.Set("speed", 5.5f);
    playerAttr.Set("name", "Hero");
    playerAttr.Set("vip", true);

    // 获取属性
    int level = playerAttr.Get<int>("level");
    int hp = playerAttr.Get<int>("hp");
    float speed = playerAttr.Get<float>("speed");
    std::string name = playerAttr.Get<std::string>("name");
    bool isVip = playerAttr.Get<bool>("vip");

    std::cout << "Player Attributes:" << std::endl;
    std::cout << "  Level: " << level << std::endl;
    std::cout << "  HP: " << hp << "/" << playerAttr.Get<int>("maxHp") << std::endl;
    std::cout << "  Speed: " << speed << std::endl;
    std::cout << "  Name: " << name << std::endl;
    std::cout << "  VIP: " << (isVip ? "Yes" : "No") << std::endl;

    // 属性修改
    playerAttr.Add("hp", 100);
    playerAttr.Mul("speed", 1.2f);
    std::cout << "After buff - HP: " << playerAttr.Get<int>("hp")
              << ", Speed: " << playerAttr.Get<float>("speed") << std::endl;
}

// 测试AOI系统
void TestAOISystem() {
    std::cout << "\n=== Testing AOI System ===" << std::endl;

    AOIManager& aoi = AOIManager::Instance();
    aoi.Initialize(100.0f);  // 100米网格

    // 设置监听器
    auto listener = std::make_shared<AOIListenerImpl>();
    aoi.SetListener(listener);

    // 创建一些实体
    AOIEntity player1(1001, 1);
    player1.position = Vector3(50, 0, 50);
    player1.aoiRadius = 30.0f;

    AOIEntity player2(1002, 1);
    player2.position = Vector3(55, 0, 55);
    player2.aoiRadius = 30.0f;

    AOIEntity monster1(2001, 1);
    monster1.position = Vector3(200, 0, 200);
    monster1.aoiRadius = 20.0f;

    // 更新实体到AOI
    aoi.UpdateEntity(player1);
    aoi.UpdateEntity(player2);
    aoi.UpdateEntity(monster1);

    // 查询可见实体
    auto visible1 = aoi.GetVisibleEntities(1001);
    std::cout << "Player 1001 can see: ";
    for (auto id : visible1) {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    auto visible2 = aoi.GetVisibleEntities(1002);
    std::cout << "Player 1002 can see: ";
    for (auto id : visible2) {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    // 获取范围内的实体
    auto inRange = aoi.GetEntitiesInRange(Vector3(50, 0, 50), 50.0f);
    std::cout << "Entities in range (50,0,50) radius 50: ";
    for (auto id : inRange) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
}

// 测试Redis（如果可用）
void TestRedis() {
    std::cout << "\n=== Testing Redis Client ===" << std::endl;

    RedisPoolConfig config;
    config.host = "localhost";
    config.port = 6379;
    config.minConnections = 2;
    config.maxConnections = 5;

    RedisClient client(config);

    if (client.Initialize()) {
        std::cout << "Connected to Redis" << std::endl;

        // 测试基本操作
        if (client.Set("test_key", "Hello Redis!")) {
            std::cout << "SET success" << std::endl;
        }

        std::string value = client.Get("test_key");
        if (!value.empty()) {
            std::cout << "GET value: " << value << std::endl;
        }

        // 测试计数器
        int64_t counter = client.Incr("test_counter");
        std::cout << "Counter value: " << counter << std::endl;

        // 测试哈希
        client.HSet("player:1001", "name", "TestPlayer");
        client.HSet("player:1001", "level", "50");
        auto playerData = client.HGetAll("player:1001");
        std::cout << "Player data: ";
        for (const auto& pair : playerData) {
            std::cout << pair.first << "=" << pair.second << " ";
        }
        std::cout << std::endl;

        client.Del("test_key");
        client.Del("test_counter");
        client.Del("player:1001");
    } else {
        std::cout << "Failed to connect to Redis (server not running?)" << std::endl;
    }
}

// 测试IoC容器
void TestIoCContainer() {
    std::cout << "\n=== Testing IoC Container ===" << std::endl;

    ApplicationContext& context = ApplicationContext::getInstance();

    // 注册组件
    context.registerComponent<GameLogicComponent>("GameLogic");

    // 初始化
    if (context.initialize()) {
        std::cout << "IoC Container initialized successfully" << std::endl;

        // 启动
        if (context.start()) {
            std::cout << "Components started" << std::endl;

            // 获取组件
            auto gameLogic = context.getComponent<GameLogicComponent>("GameLogic");
            if (gameLogic) {
                std::cout << "GameLogic component retrieved successfully" << std::endl;
            }

            // 停止
            context.stop();
            std::cout << "Components stopped" << std::endl;
        }

        // 销毁
        context.destroy();
        std::cout << "Components destroyed" << std::endl;
    }
}

int main() {
    std::cout << "=== Apollo Framework Feature Demo ===" << std::endl;

    // 初始化日志系统
    Logger::initialize();

    // 运行各种测试
    TestIoCContainer();
    TestConfigSystem();
    TestServerID();
    TestLogger();
    TestThreadPool();
    TestAttributeSystem();
    TestAOISystem();
    TestRedis();

    std::cout << "\n=== All Tests Completed ===" << std::endl;

    return 0;
}