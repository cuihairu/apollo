/**
 * @file simple_server.cpp
 * @brief 使用 GameServer 启动器创建简单服务器
 */

#include "apollo/server/game_server.h"
#include "apollo/core/metrics.h"
#include "apollo/core/service_discovery.h"
#include "apollo/utils/time.h"

using namespace apollo::server;
using namespace apollo::core;
using namespace apollo::core::metrics;

//==============================================================================
// 自定义服务器
//==============================================================================

class MyGameServer : public GameServer {
public:
    MyGameServer() = default;

protected:
    bool onInitialize() override {
        std::cout << "[MyGameServer] Custom initialization..." << std::endl;

        // 注册自定义指标
        METRIC_COUNTER("player.login")->increment();
        METRIC_GAUGE("player.online")->set(0);

        return true;
    }

    bool onStart() override {
        std::cout << "[MyGameServer] Starting game logic..." << std::endl;

        // 模拟一些初始化的游戏数据
        for (int i = 0; i < 100; ++i) {
            monsters_.push_back({i, 100 + i, 100 + i, 100});
        }

        std::cout << "[MyGameServer] Spawned " << monsters_.size() << " monsters" << std::endl;
        return true;
    }

    void onStop() override {
        std::cout << "[MyGameServer] Shutting down game logic..." << std::endl;
    }

    void onUpdate() override {
        // 游戏循环
        tickCount_++;

        // 每秒更新一次
        static auto lastPrint = Time::now();
        uint64_t now = Time::now();

        if (now - lastPrint >= 1000) {
            lastPrint = now;
            printStats();
        }
    }

private:
    struct Monster {
        int id;
        float x, y, z;
        int hp;
    };

    void printStats() {
        std::cout << "[MyGameServer] Tick: " << tickCount_
                  << ", Online: " << METRIC_GAUGE("player.online")->get()
                  << ", Logins: " << METRIC_COUNTER("player.login")->get()
                  << std::endl;
    }

    std::vector<Monster> monsters_;
    uint64_t tickCount_ = 0;
};

//==============================================================================
// 示例：构建并启动服务器
//==============================================================================

void example1_BasicServer() {
    std::cout << "\n=== Example 1: Basic Server ===" << std::endl;

    DefaultLifecycleListener listener;

    auto server = ServerBuilder()
        .name("MyGameServer")
        .bind("0.0.0.0", 8080)
        .threads(4)
        .maxConnections(1000)
        .lifecycleListener(&listener)
        .build();

    if (!server->initialize()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return;
    }

    if (!server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return;
    }

    std::cout << "Server running. Press Ctrl+C to stop." << std::endl;

    // 运行 3 秒后停止
    std::this_thread::sleep_for(std::chrono::seconds(3));

    server->stop();
}

void example2_WithComponents() {
    std::cout << "\n=== Example 2: Server with Components ===" << std::endl;

    auto server = ServerBuilder()
        .name("ComponentServer")
        .bind("0.0.0.0", 8081)
        .component(std::make_shared<LoggingComponent>("./logs"))
        .component(std::make_shared<TimerComponent>())
        .component(std::make_shared<NetworkComponent>(8081))
        .component(std::make_shared<RpcComponent>())
        .component(std::make_shared<DatabaseComponent>())
        .build();

    if (!server->initialize() || !server->start()) {
        std::cerr << "Failed to start server" << std::endl;
        return;
    }

    std::cout << "Server with components running..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(2));

    server->stop();
}

void example3_CustomServer() {
    std::cout << "\n=== Example 3: Custom Game Server ===" << std::endl;

    MyGameServer server;

    ServerConfig config;
    config.name = "CustomGameServer";
    config.host = "0.0.0.0";
    config.port = 8082;
    config.threadCount = 4;
    config.maxConnections = 500;

    server.setConfig(config);

    DefaultLifecycleListener listener;
    server.setLifecycleListener(&listener);

    if (!server.initialize()) {
        std::cerr << "Failed to initialize" << std::endl;
        return;
    }

    if (!server.start()) {
        std::cerr << "Failed to start" << std::endl;
        return;
    }

    std::cout << "Custom game server running..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(3));

    server.stop();
}

void example4_ServiceDiscovery() {
    std::cout << "\n=== Example 4: Service Discovery ===" << std::endl;

    auto& discovery = LocalServiceDiscovery::instance();

    ServiceDiscoveryConfig config;
    config.heartbeatIntervalMs = 5000;
    config.heartbeatTimeoutMs = 15000;

    discovery.start(config);

    // 注册游戏服务器
    auto gameNode1 = ServiceHelper::createDefaultNode(
        ServerType::Game, 1001, "192.168.1.10", 7003);

    auto gameNode2 = ServiceHelper::createDefaultNode(
        ServerType::Game, 1002, "192.168.1.11", 7003);

    discovery.registerServer(gameNode1);
    discovery.registerServer(gameNode2);

    // 注册网关服务器
    auto gatewayNode = ServiceHelper::createDefaultNode(
        ServerType::Gateway, 2001, "192.168.1.20", 7002);

    discovery.registerServer(gatewayNode);

    std::cout << "Registered " << discovery.getServerCount() << " servers" << std::endl;

    // 发现游戏服务器
    auto gameServers = discovery.discoverServers(ServerType::Game);
    std::cout << "Found " << gameServers.size() << " game servers:" << std::endl;
    for (const auto& server : gameServers) {
        std::cout << "  - " << server.name << " (" << server.host << ":" << server.port << ")" << std::endl;
    }

    // 选择负载最低的服务器
    ServerNode selected;
    if (discovery.selectLeastLoaded(ServerType::Game, selected)) {
        std::cout << "Selected server: " << selected.name << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    discovery.shutdown();
}

void example5_Metrics() {
    std::cout << "\n=== Example 5: Performance Metrics ===" << std::endl;

    auto& registry = MetricRegistry::instance();

    // 创建各种指标
    auto* requestCounter = registry.counter("http_requests_total", "Total HTTP requests");
    auto* activeGauge = registry.gauge("http_active", "Active connections");
    auto* latencyHist = registry.histogram("http_latency_us", "Request latency");
    auto* latencySummary = registry.summary("http_latency_summary", "Request latency summary", 1000);
    auto* requestMeter = registry.meter("http_requests_per_second", "Request rate");

    // 模拟请求
    std::cout << "Simulating requests..." << std::endl;

    for (int i = 0; i < 1000; ++i) {
        requestCounter->mark();
        activeGauge->set(i % 100);

        // 模拟延迟（微秒）
        int latency = 100 + (rand() % 500);
        latencyHist->record(latency);
        latencySummary->record(latency);
        requestMeter->mark();
    }

    // 打印统计
    std::cout << "\n--- Request Statistics ---" << std::endl;
    std::cout << "Total requests: " << requestCounter->get() << std::endl;
    std::cout << "Active connections: " << activeGauge->get() << std::endl;
    std::cout << "Latency avg: " << latencyHist->getAvg() << " us" << std::endl;
    std::cout << "Latency min: " << latencyHist->getMin() << " us" << std::endl;
    std::cout << "Latency max: " << latencyHist->getMax() << " us" << std::endl;
    std::cout << "Latency p50: " << latencySummary->getP50() << " us" << std::endl;
    std::cout << "Latency p95: " << latencySummary->getP95() << " us" << std::endl;
    std::cout << "Latency p99: " << latencySummary->getP99() << " us" << std::endl;
    std::cout << "Request rate: " << requestMeter->getMeanRate() << " req/s" << std::endl;
}

void example6_TimerMeasurement() {
    std::cout << "\n=== Example 6: Timer Measurement ===" << std::endl;

    auto& registry = MetricRegistry::instance();
    auto* processSummary = registry.summary("process_time_ns");

    // 使用定时器测量代码执行时间
    {
        ScopedTimer timer(processSummary);

        // 模拟一些工作
        std::vector<int> data;
        for (int i = 0; i < 10000; ++i) {
            data.push_back(i);
        }
        std::sort(data.begin(), data.end());
    }

    std::cout << "Process time statistics:" << std::endl;
    std::cout << "  Avg: " << processSummary->getAvg() << " ns" << std::endl;
    std::cout << "  Min: " << processSummary->getMin() << " ns" << std::endl;
    std::cout << "  Max: " << processSummary->getMax() << " ns" << std::endl;
    std::cout << "  P50: " << processSummary->getP50() << " ns" << std::endl;
    std::cout << "  P95: " << processSummary->getP95() << " ns" << std::endl;
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Server Examples ===" << std::endl;
    std::cout << "========================================" << std::endl;

    example1_BasicServer();
    example2_WithComponents();
    example3_CustomServer();
    example4_ServiceDiscovery();
    example5_Metrics();
    example6_TimerMeasurement();

    std::cout << "\n========================================" << std::endl;
    std::cout << "=== All Examples Complete ===" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
