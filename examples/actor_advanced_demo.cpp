/**
 * @file actor_advanced_demo.cpp
 * @brief Actor 框架高级功能演示
 *
 * 演示功能：
 * 1. Actor 系统配置和启动
 * 2. 线程池配置
 * 3. 消息发送和接收
 * 4. 定时器使用
 * 5. 监控和观测性
 * 6. 健康检查
 */

#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_manager.h"
#include "apollo/actor/timer_service.h"
#include "apollo/actor/monitoring.h"
#include "apollo/actor/actor_utils.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace apollo::actor;

//==============================================================================
// 消息定义
//==============================================================================

struct PingMsg {
    std::string content;
    int64_t timestamp;

    static const char* typeName() { return "PingMsg"; }
};

struct PongMsg {
    std::string reply;
    int64_t originalTimestamp;

    static const char* typeName() { return "PongMsg"; }
};

struct UserLoginRequest {
    std::string username;
    std::string password;

    static const char* typeName() { return "UserLoginRequest"; }
};

struct UserLoginResponse {
    bool success;
    std::string token;
    std::string error;

    static const char* typeName() { return "UserLoginResponse"; }
};

//==============================================================================
// Actor 实现
//==============================================================================

// PingPong Actor - 演示消息交互
class PingPongActor : public Actor {
public:
    PingPongActor() = default;

protected:
    void onStart() override {
        std::cout << "[" << path().name << "] Started" << std::endl;

        // 设置定时器，每秒发送一次 Ping
        if (path().name == "ping") {
            scheduleRepeated(1000, [this]() {
                PingMsg ping;
                ping.content = "Hello from Ping";
                ping.timestamp = currentTimeMs();

                // 发送给 pong
                auto pongRef = context()->lookup("pong").get();
                if (pongRef.isValid()) {
                    pongRef.tell(ping);
                    std::cout << "[Ping] Sent ping" << std::endl;
                }
            });
        }
    }

    void receive(const Message& msg) override {
        if (msg.is<PingMsg>()) {
            auto ping = msg.as<PingMsg>();
            std::cout << "[" << path().name << "] Received: " << ping.content << std::endl;

            if (path().name == "pong") {
                // 回复 Pong
                PongMsg pong;
                pong.reply = "Pong from " + path().name;
                pong.originalTimestamp = ping.timestamp;

                msg.sender.tell(pong);
            }

        } else if (msg.is<PongMsg>()) {
            auto pong = msg.as<PongMsg>();
            int64_t rtt = currentTimeMs() - pong.originalTimestamp;
            std::cout << "[Ping] Received pong: " << pong.reply
                      << ", RTT: " << rtt << "ms" << std::endl;
        }
    }
};

// 用户服务 Actor - 演示请求-响应模式
class UserServiceActor : public Actor {
protected:
    void receive(const Message& msg) override {
        if (msg.is<UserLoginRequest>()) {
            auto request = msg.as<UserLoginRequest>();

            std::cout << "[UserService] Login attempt: " << request.username << std::endl;

            // 模拟登录验证
            UserLoginResponse response;
            if (request.username == "admin" && request.password == "password") {
                response.success = true;
                response.token = "token_" + std::to_string(currentTimeMs());
            } else {
                response.success = false;
                response.error = "Invalid credentials";
            }

            // 回复
            // msg.sender.tell(response);
        }
    }

private:
    // 模拟用户数据库
    std::unordered_map<std::string, std::string> users_;
};

// 网关 Actor - 演示消息转发
class GatewayActor : public Actor {
protected:
    void onStart() override {
        std::cout << "[Gateway] Started" << std::endl;

        // 定期打印统计信息
        scheduleRepeated(5000, [this]() {
            std::cout << "[Gateway] Running, processed messages..." << std::endl;
        });
    }

    void receive(const Message& msg) override {
        // 转发到相应的服务
        if (msg.is<UserLoginRequest>()) {
            auto userServiceRef = context()->lookup("user-service").get();
            if (userServiceRef.isValid()) {
                // userServiceRef.tell(msg, self());
            }
        }
    }
};

//==============================================================================
// 监控演示
//==============================================================================

void demonstrateMonitoring(ActorSystem& system, ActorManager& manager) {
    std::cout << "\n=== 监控演示 ===" << std::endl;

    ActorMonitor monitor(manager);

    // 获取统计信息
    auto stats = monitor.stats();
    std::cout << "Total Actors: " << stats.totalActors << std::endl;
    std::cout << "Running Actors: " << stats.runningActors << std::endl;
    std::cout << "Processed Messages: " << stats.processedMessages << std::endl;

    // 获取所有 Actor 信息
    auto actors = monitor.listActors();
    std::cout << "\nActor List:" << std::endl;
    for (const auto& name : actors) {
        auto info = monitor.getActor(name);
        std::cout << "  - " << name
                  << " (state: " << stateToString(info.state) << ")" << std::endl;
    }

    // 导出 JSON
    std::cout << "\nJSON Export:" << std::endl;
    std::cout << monitor.exportStatsAsJson() << std::endl;
}

//==============================================================================
// 健康检查演示
//==============================================================================

void demonstrateHealthCheck(ActorManager& manager) {
    std::cout << "\n=== 健康检查演示 ===" << std::endl;

    ActorMonitor monitor(manager);
    auto health = monitor.health();

    std::cout << "System Healthy: " << (health.healthy ? "Yes" : "No") << std::endl;

    if (!health.unhealthyActors.empty()) {
        std::cout << "Unhealthy Actors:" << std::endl;
        for (const auto& actor : health.unhealthyActors) {
            std::cout << "  - " << actor << std::endl;
        }
    }

    if (!health.warnings.empty()) {
        std::cout << "Warnings:" << std::endl;
        for (const auto& warning : health.warnings) {
            std::cout << "  - " << warning << std::endl;
        }
    }
}

//==============================================================================
// 定时器演示
//==============================================================================

void demonstrateTimers() {
    std::cout << "\n=== 定时器演示 ===" << std::endl;

    // 初始化定时器服务
    timer::init();

    // 单次定时器
    std::cout << "Scheduling one-shot timer (2 seconds)..." << std::endl;
    timer::scheduleOnce(2000, []() {
        std::cout << "[Timer] One-shot timer fired!" << std::endl;
    });

    // 重复定时器
    std::cout << "Scheduling repeated timer (every 3 seconds)..." << std::endl;
    timer::scheduleRepeated(3000, []() {
        std::cout << "[Timer] Repeated timer fired!" << std::endl;
    });

    // 倒计时
    std::cout << "Starting countdown (10 seconds)..." << std::endl;
    timer::countdown(10000,
        [](int64_t remaining) {
            std::cout << "[Countdown] " << (remaining / 1000) << "s remaining" << std::endl;
        },
        []() {
            std::cout << "[Countdown] Complete!" << std::endl;
        }
    );
}

//==============================================================================
// 观察者演示
//==============================================================================

void demonstrateObservers(ActorManager& manager) {
    std::cout << "\n=== 观察者演示 ===" << std::endl;

    // 添加观察者，监听所有事件
    manager.addObserver("demo-observer",
        [](const Observation& event) {
            std::cout << "[Observer] Event: ";
            switch (event.event) {
                case ObservationEvent::ActorCreated:
                    std::cout << "ActorCreated";
                    break;
                case ObservationEvent::ActorStarted:
                    std::cout << "ActorStarted";
                    break;
                case ObservationEvent::ActorStopped:
                    std::cout << "ActorStopped";
                    break;
                case ObservationEvent::MessageReceived:
                    std::cout << "MessageReceived";
                    break;
                case ObservationEvent::MessageProcessed:
                    std::cout << "MessageProcessed";
                    break;
                default:
                    std::cout << "Other";
            }
            std::cout << " - " << event.actorPath.name << std::endl;
        }
    );
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   Apollo Actor Framework Demo        " << std::endl;
    std::cout << "========================================" << std::endl;

    //==========================================================================
    // 1. 配置 Actor 系统
    //==========================================================================

    std::cout << "\n=== 配置系统 ===" << std::endl;

    ActorSystemConfig systemConfig;
    systemConfig.systemName = "demo-system";
    systemConfig.address = "127.0.0.1";
    systemConfig.dataCenter = "shanghai";
    systemConfig.hostId = "server-001";

    // 配置线程池
    systemConfig.dispatcherThreads = 4;      // 4 个调度线程
    systemConfig.mailboxSize = 256;          // 邮箱大小

    // 配置服务发现（使用内存模式进行测试）
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    //==========================================================================
    // 2. 配置线程池
    //==========================================================================

    ThreadPoolConfig threadPoolConfig;
    threadPoolConfig.dispatcherThreads = 4;
    threadPoolConfig.ioThreads = 2;          // IO 线程池
    threadPoolConfig.backgroundThreads = 2;  // 后台线程池
    threadPoolConfig.dispatchStrategy = DispatchStrategy::RoundRobin;

    //==========================================================================
    // 3. 配置可观测性
    //==========================================================================

    ObservabilityConfig obsConfig;
    obsConfig.enableHttpApi = false;         // 禁用 HTTP API（演示环境）
    obsConfig.enableLoggingObserver = true;  // 启用日志观察者
    obsConfig.enableMetricsCollector = true; // 启用指标收集

    //==========================================================================
    // 4. 创建并启动系统
    //==========================================================================

    std::cout << "\n=== 启动系统 ===" << std::endl;

    ActorSystem system(systemConfig);

    // 创建 Actor 管理器
    auto manager = std::make_unique<ActorManager>(threadPoolConfig);
    if (!manager->start(system)) {
        std::cerr << "Failed to start ActorManager" << std::endl;
        return 1;
    }

    if (!system.start()) {
        std::cerr << "Failed to start ActorSystem" << std::endl;
        return 1;
    }

    std::cout << "System started successfully!" << std::endl;

    // 启动可观测性
    Observability observability(*manager, obsConfig);
    observability.start();

    //==========================================================================
    // 5. 创建 Actor
    //==========================================================================

    std::cout << "\n=== 创建 Actor ===" << std::endl;

    auto pingActor = system.spawn<PingPongActor>("ping");
    auto pongActor = system.spawn<PingPongActor>("pong");
    auto userService = system.spawn<UserServiceActor>("user-service");
    auto gateway = system.spawn<GatewayActor>("gateway");

    std::cout << "Actors created!" << std::endl;

    //==========================================================================
    // 6. 演示功能
    //==========================================================================

    // 观察者
    demonstrateObservers(*manager);

    // 定时器
    demonstrateTimers();

    // 等待一段时间让 Actor 运行
    std::cout << "\n=== 运行中... (15秒) ===" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(15));

    // 监控
    demonstrateMonitoring(system, *manager);

    // 健康检查
    demonstrateHealthCheck(*manager);

    //==========================================================================
    // 7. 演示 Actor 控制
    //==========================================================================

    std::cout << "\n=== Actor 控制演示 ===" << std::endl;

    // 暂停 Actor
    std::cout << "Suspending ping actor..." << std::endl;
    manager->suspendActor("ping");

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 恢复 Actor
    std::cout << "Resuming ping actor..." << std::endl;
    manager->resumeActor("ping");

    std::this_thread::sleep_for(std::chrono::seconds(3));

    //==========================================================================
    // 8. 关闭系统
    //==========================================================================

    std::cout << "\n=== 关闭系统 ===" << std::endl;

    observability.stop();
    system.shutdown();
    manager->stop();
    timer::shutdown();

    std::cout << "System shut down successfully!" << std::endl;

    return 0;
}

//==============================================================================
// 编译说明
//==============================================================================
/*
g++ -std=c++20 -I../include -L../build \
    actor_advanced_demo.cpp \
    -lapollo -lpthread -o actor_advanced_demo

或使用 CMake:
add_executable(actor_advanced_demo examples/actor_advanced_demo.cpp)
target_link_libraries(actor_advanced_demo PRIVATE apollo)
*/
