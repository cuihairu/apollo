/**
 * @file test_actor_framework.cpp
 * @brief Actor 框架全面测试套件
 *
 * 测试覆盖：
 * 1. 消息序列化
 * 2. Actor 生命周期
 * 3. 消息发送/接收
 * 4. 并发测试
 * 5. 调度器
 * 6. 定时器
 * 7. 线程池
 * 8. 监控
 * 9. 边界条件
 */

#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_manager.h"
#include "apollo/actor/timer_service.h"
#include "apollo/actor/monitoring.h"
#include "apollo/actor/message.h"
#include "apollo/actor/actor_utils.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>
#include <vector>
#include <random>

using namespace apollo::actor;

//==============================================================================
// 测试框架
//==============================================================================

#define TEST(name) void test_##name()

#define RUN_TEST(name) \
    do { \
        std::cout << "Running test: " << #name << "..."; \
        test_##name(); \
        std::cout << " PASSED" << std::endl; \
        testsPassed++; \
    } catch (const std::exception& e) { \
        std::cout << " FAILED: " << e.what() << std::endl; \
        testsFailed++; \
    }

#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #cond); \
    }

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))

static int testsPassed = 0;
static int testsFailed = 0;

//==============================================================================
// 测试消息定义
//==============================================================================

struct PingMsg {
    int value = 0;
    static const char* typeName() { return "PingMsg"; }
};

struct PongMsg {
    int value = 0;
    static const char* typeName() { return "PongMsg"; }
};

struct CountMsg {
    int count = 0;
    static const char* typeName() { return "CountMsg"; }
};

struct StopMsg {
    static const char* typeName() { return "StopMsg"; }
};

//==============================================================================
// 测试用例
//==============================================================================

TEST(message_basics) {
    // 测试消息基本功能
    PingMsg ping;
    ping.value = 42;

    Message msg(ping, MessageFormat::Binary);

    ASSERT_TRUE(msg.is<PingMsg>());
    ASSERT_FALSE(msg.is<PongMsg>());

    auto decoded = msg.as<PingMsg>();
    ASSERT_EQ(decoded.value, 42);
}

TEST(message_copy) {
    // 测试消息拷贝
    PingMsg ping;
    ping.value = 123;

    Message msg1(ping, MessageFormat::Binary);
    Message msg2 = msg1;  // 拷贝

    auto decoded1 = msg1.as<PingMsg>();
    auto decoded2 = msg2.as<PingMsg>();

    ASSERT_EQ(decoded1.value, 123);
    ASSERT_EQ(decoded2.value, 123);
}

TEST(actor_path_parsing) {
    // 测试 Actor 路径解析
    ActorPath path1 = ActorPath::fromString("system://host/actor");
    ASSERT_EQ(path1.system, "system");
    ASSERT_EQ(path1.address, "host");
    ASSERT_EQ(path1.name, "actor");

    ActorPath path2 = ActorPath::fromString("system://actor");
    ASSERT_EQ(path2.system, "system");
    ASSERT_TRUE(path2.address.empty() || path2.address == "");
    ASSERT_EQ(path2.name, "actor");

    ActorPath path3 = ActorPath::fromString("simple-actor");
    ASSERT_EQ(path3.name, "simple-actor");
}

TEST(actor_path_to_string) {
    // 测试 Actor 路径序列化
    ActorPath path;
    path.system = "my-system";
    path.address = "server-001";
    path.name = "user-service";

    std::string str = path.toString();
    ASSERT_EQ(str, "my-system://server-001/user-service");
}

TEST(mailbox_basic) {
    // 测试 Mailbox 基本功能
    MailboxConfig config;
    config.capacity = 10;

    Mailbox mailbox(config);

    ASSERT_TRUE(mailbox.isEmpty());
    ASSERT_EQ(mailbox.size(), 0);

    Envelope env;
    env.message = Message(PingMsg{}, MessageFormat::Binary);

    // 发送消息
    ASSERT_TRUE(mailbox.send(env));
    ASSERT_EQ(mailbox.size(), 1);
    ASSERT_FALSE(mailbox.isEmpty());

    // 接收消息
    Envelope received;
    ASSERT_TRUE(mailbox.receive(received, 100));
    ASSERT_EQ(mailbox.size(), 0);
    ASSERT_TRUE(mailbox.isEmpty());
}

TEST(mailbox_capacity) {
    // 测试 Mailbox 容量限制
    MailboxConfig config;
    config.capacity = 5;
    config.strategy = ipc::BackpressureStrategy::Drop;

    Mailbox mailbox(config);

    Envelope env;
    env.message = Message(PingMsg{}, MessageFormat::Binary);

    // 发送满容量
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(mailbox.send(env));
    }

    // 超过容量应该被丢弃
    ASSERT_FALSE(mailbox.send(env));
    ASSERT_EQ(mailbox.size(), 5);
}

TEST(mailbox_watermark) {
    // 测试水位线
    MailboxConfig config;
    config.capacity = 100;
    config.lowWatermark = 30;
    config.highWatermark = 70;

    Mailbox mailbox(config);

    Envelope env;
    env.message = Message(PingMsg{}, MessageFormat::Binary);

    // 低于低水位线
    ASSERT_FALSE(mailbox.isHighWatermark());

    // 发送到高水位线以上
    for (int i = 0; i < 80; i++) {
        mailbox.send(env);
    }

    ASSERT_TRUE(mailbox.isHighWatermark());
    ASSERT_FALSE(mailbox.isLowWatermark());

    // 消费到低水位线以下
    Envelope received;
    for (int i = 0; i < 60; i++) {
        mailbox.tryReceive(received);
    }

    ASSERT_FALSE(mailbox.isHighWatermark());
    ASSERT_TRUE(mailbox.isLowWatermark());
}

TEST(mailbox_concurrent) {
    // 测试并发访问
    MailboxConfig config;
    config.capacity = 10000;

    Mailbox mailbox(config);

    const int numProducers = 4;
    const int messagesPerProducer = 1000;
    std::atomic<int> totalSent{0};
    std::atomic<int> totalReceived{0};

    // 生产者线程
    std::vector<std::thread> producers;
    for (int i = 0; i < numProducers; i++) {
        producers.emplace_back([&mailbox, &totalSent, messagesPerProducer]() {
            Envelope env;
            env.message = Message(PingMsg{}, MessageFormat::Binary);

            for (int j = 0; j < messagesPerProducer; j++) {
                while (!mailbox.send(env)) {
                    std::this_thread::yield();
                }
                totalSent.fetch_add(1);
            }
        });
    }

    // 消费者线程
    std::thread consumer([&mailbox, &totalReceived, numProducers, messagesPerProducer]() {
        Envelope env;
        int target = numProducers * messagesPerProducer;

        while (totalReceived.load() < target) {
            if (mailbox.tryReceive(env)) {
                totalReceived.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : producers) {
        t.join();
    }
    consumer.join();

    ASSERT_EQ(totalSent.load(), numProducers * messagesPerProducer);
    ASSERT_EQ(totalReceived.load(), numProducers * messagesPerProducer);
}

//------------------------------------------------------------------------------
// Actor 生命周期测试
//------------------------------------------------------------------------------

class LifecycleTestActor : public Actor {
public:
    std::atomic<bool> started{false};
    std::atomic<bool> stopped{false};
    std::atomic<int> messageCount{0};

protected:
    void onStart() override {
        started.store(true);
        Actor::onStart();
    }

    void onStop() override {
        stopped.store(true);
        Actor::onStop();
    }

    void receive(const Message& msg) override {
        messageCount.fetch_add(1);
    }
};

TEST(actor_lifecycle) {
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<LifecycleTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "test-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);

    ASSERT_FALSE(actor->started.load());
    ASSERT_FALSE(actor->stopped.load());

    cell->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(actor->started.load());
    ASSERT_FALSE(actor->stopped.load());

    cell->stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_TRUE(actor->stopped.load());

    system.shutdown();
}

//------------------------------------------------------------------------------
// 消息发送测试
//------------------------------------------------------------------------------

class EchoActor : public Actor {
public:
    std::vector<std::string> receivedMessages;
    std::mutex mutex;

protected:
    void receive(const Message& msg) override {
        if (msg.is<PingMsg>()) {
            std::lock_guard lock(mutex);
            receivedMessages.push_back("ping");

            // 回复
            if (!msg.sender.path().name.empty()) {
                // msg.sender.tell(PongMsg{});
            }
        } else if (msg.is<PongMsg>()) {
            std::lock_guard lock(mutex);
            receivedMessages.push_back("pong");
        } else if (msg.is<CountMsg>()) {
            std::lock_guard lock(mutex);
            receivedMessages.push_back("count");
        }
    }
};

TEST(actor_messaging) {
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto echoActor = std::make_shared<EchoActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "echo";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, echoActor, system, mailboxConfig);
    cell->start();

    // 发送消息
    PingMsg ping;
    ping.value = 1;

    Message msg(ping, MessageFormat::Binary);
    cell->tell(msg, ActorRef{});

    // 处理消息
    cell->processMessages();

    // 验证
    ASSERT_EQ(echoActor->receivedMessages.size(), 1);
    ASSERT_EQ(echoActor->receivedMessages[0], "ping");

    system.shutdown();
}

//------------------------------------------------------------------------------
// 并发测试
//------------------------------------------------------------------------------

class ConcurrentTestActor : public Actor {
public:
    std::atomic<int> count{0};

protected:
    void receive(const Message& msg) override {
        if (msg.is<CountMsg>()) {
            count.fetch_add(1);
        }
    }
};

TEST(actor_concurrent_send) {
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<ConcurrentTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "concurrent";

    MailboxConfig mailboxConfig;
    mailboxConfig.capacity = 10000;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();

    const int numThreads = 10;
    const int messagesPerThread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&cell, messagesPerThread]() {
            CountMsg msg;
            Message message(msg, MessageFormat::Binary);

            for (int j = 0; j < messagesPerThread; j++) {
                cell->tell(message, ActorRef{});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 处理所有消息
    for (int i = 0; i < 100; i++) {
        cell->processMessages();
        if (actor->count.load() >= numThreads * messagesPerThread) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(actor->count.load(), numThreads * messagesPerThread);

    system.shutdown();
}

//------------------------------------------------------------------------------
// 定时器测试
//------------------------------------------------------------------------------

TEST(timer_oneshot) {
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    std::atomic<bool> fired{false};
    std::atomic<int64_t> fireTime{0};

    int64_t startTime = currentTimeMs();
    uint64_t timerId = timer.scheduleOnce(100, [&]() {
        fired.store(true);
        fireTime.store(currentTimeMs());
    });

    ASSERT_NE(timerId, 0);

    // 等待触发
    while (!fired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_LT(currentTimeMs() - startTime, 5000);  // 超时保护
    }

    int64_t elapsed = fireTime.load() - startTime;
    ASSERT_GE(elapsed, 80);   // 至少 80ms (允许误差)
    ASSERT_LT(elapsed, 200);  // 最多 200ms

    timer.stop();
}

TEST(timer_repeated) {
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    std::atomic<int> count{0};

    uint64_t timerId = timer.scheduleRepeated(50, [&]() {
        count.fetch_add(1);
    });

    ASSERT_NE(timerId, 0);

    // 等待触发几次
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int c = count.load();
    ASSERT_GE(c, 3);  // 至少触发 3 次
    ASSERT_LE(c, 6);  // 最多 6 次（允许误差）

    // 取消定时器
    ASSERT_TRUE(timer.cancel(timerId));

    int oldCount = count.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 取消后不应再触发
    ASSERT_EQ(count.load(), oldCount);

    timer.stop();
}

TEST(timer_cancel) {
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    std::atomic<bool> fired{false};

    uint64_t timerId = timer.scheduleOnce(100, [&]() {
        fired.store(true);
    });

    // 立即取消
    ASSERT_TRUE(timer.cancel(timerId));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_FALSE(fired.load());  // 不应该触发

    // 取消不存在的定时器
    ASSERT_FALSE(timer.cancel(99999));

    timer.stop();
}

TEST(timer_concurrent) {
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    const int numTimers = 100;
    std::atomic<int> firedCount{0};

    for (int i = 0; i < numTimers; i++) {
        timer.scheduleOnce(10 + (i % 10), [&]() {
            firedCount.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_EQ(firedCount.load(), numTimers);

    timer.stop();
}

//------------------------------------------------------------------------------
// Actor 管理器测试
//------------------------------------------------------------------------------

TEST(actor_manager_registration) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    auto actor = std::make_shared<EchoActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "test-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);

    // 注册
    ASSERT_TRUE(manager.registerActor(cell));

    // 查找
    auto found = manager.getActor("test-actor");
    ASSERT_TRUE(found != nullptr);
    ASSERT_EQ(found->path().name, "test-actor");

    // 列出所有
    auto names = manager.getAllActorNames();
    ASSERT_EQ(names.size(), 1);
    ASSERT_EQ(names[0], "test-actor");

    // 注销
    ASSERT_TRUE(manager.unregisterActor("test-actor"));

    // 再次查找应该失败
    found = manager.getActor("test-actor");
    ASSERT_TRUE(found == nullptr);

    manager.stop();
    system.shutdown();
}

TEST(actor_manager_find_by_prefix) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    // 创建多个 Actor
    for (int i = 0; i < 5; i++) {
        auto actor = std::make_shared<EchoActor>();
        ActorPath path;
        path.system = "test-system";
        path.name = "service-" + std::to_string(i);

        MailboxConfig mailboxConfig;
        auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
        cell->start();
        manager.registerActor(cell);
    }

    // 按前缀查找
    auto results = manager.findActorsByPrefix("service-");
    ASSERT_EQ(results.size(), 5);

    results = manager.findActorsByPrefix("service-1");
    ASSERT_EQ(results.size(), 1);

    manager.stop();
    system.shutdown();
}

TEST(actor_manager_stats) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    // 创建 Actor
    auto actor = std::make_shared<ConcurrentTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "stats-test";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();
    manager.registerActor(cell);

    // 获取统计
    auto stats = manager.getStats();
    ASSERT_EQ(stats.totalActors, 1);
    ASSERT_EQ(stats.runningActors, 1);

    // 停止 Actor
    manager.stopActor("stats-test");

    stats = manager.getStats();
    ASSERT_EQ(stats.stoppedActors, 1);

    manager.stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 观察者测试
//------------------------------------------------------------------------------

TEST(observer_basic) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    std::vector<Observation> observations;
    std::mutex mutex;

    uint64_t observerId = manager.addObserver("test-observer",
        [&](const Observation& event) {
            std::lock_guard lock(mutex);
            observations.push_back(event);
        });

    ASSERT_NE(observerId, 0);

    // 创建 Actor（应该触发 ActorCreated 和 ActorStarted）
    auto actor = std::make_shared<EchoActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "observed-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    manager.registerActor(cell);
    cell->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 验证事件
    std::lock_guard lock(mutex);
    bool created = false, started = false;
    for (const auto& obs : observations) {
        if (obs.event == ObservationEvent::ActorCreated) created = true;
        if (obs.event == ObservationEvent::ActorStarted) started = true;
    }

    ASSERT_TRUE(created);
    ASSERT_TRUE(started);

    manager.removeObserver(observerId);
    manager.stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 健康检查测试
//------------------------------------------------------------------------------

TEST(health_check) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    ActorMonitor monitor(manager);
    auto health = monitor.health();

    ASSERT_TRUE(health.healthy);

    manager.stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 边界条件测试
//------------------------------------------------------------------------------

TEST(mailbox_close_and_send) {
    // 测试关闭后发送
    MailboxConfig config;
    Mailbox mailbox(config);

    mailbox.close();

    Envelope env;
    env.message = Message(PingMsg{}, MessageFormat::Binary);

    ASSERT_FALSE(mailbox.send(env));
}

TEST(actor_path_empty) {
    // 测试空路径
    ActorPath path;
    ASSERT_TRUE(path.name.empty());
    ASSERT_TRUE(path.address.empty());
    ASSERT_TRUE(path.system.empty());

    std::string str = path.toString();
    ASSERT_TRUE(str.empty() || str == "://");
}

TEST(timer_zero_delay) {
    // 测试零延迟定时器
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    std::atomic<bool> fired{false};

    uint64_t timerId = timer.scheduleOnce(0, [&]() {
        fired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_TRUE(fired.load());

    timer.stop();
}

TEST(timer_negative_interval) {
    // 测试负间隔
    TimerServiceConfig config;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    uint64_t timerId = timer.scheduleRepeated(-100, []() {});

    ASSERT_EQ(timerId, 0);  // 应该失败

    timer.stop();
}

TEST(message_empty) {
    // 测试空消息
    Message msg;
    ASSERT_TRUE(msg.getType().name.empty());
    ASSERT_EQ(msg.getFormat(), MessageFormat::FlatBuffers);
}

//------------------------------------------------------------------------------
// 压力测试
//------------------------------------------------------------------------------

TEST(stress_many_actors) {
    ActorSystemConfig systemConfig;
    systemConfig.systemName = "test-system";
    systemConfig.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(systemConfig);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    const int numActors = 100;
    std::vector<std::shared_ptr<ActorCell>> cells;

    for (int i = 0; i < numActors; i++) {
        auto actor = std::make_shared<ConcurrentTestActor>();
        ActorPath path;
        path.system = "test-system";
        path.name = "actor-" + std::to_string(i);

        MailboxConfig mailboxConfig;
        auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
        cell->start();
        manager.registerActor(cell);
        cells.push_back(cell);
    }

    auto stats = manager.getStats();
    ASSERT_EQ(stats.totalActors, numActors);

    // 发送消息
    for (auto& cell : cells) {
        CountMsg msg;
        Message message(msg, MessageFormat::Binary);
        cell->tell(message, ActorRef{});
    }

    // 处理消息
    for (auto& cell : cells) {
        cell->processMessages();
    }

    manager.stop();
    system.shutdown();
}

TEST(stress_message_burst) {
    // 测试突发大量消息
    MailboxConfig config;
    config.capacity = 10000;
    Mailbox mailbox(config);

    const int numMessages = 5000;
    std::vector<Envelope> envelopes(numMessages);

    for (int i = 0; i < numMessages; i++) {
        envelopes[i].message = Message(CountMsg{}, MessageFormat::Binary);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < numMessages; i++) {
        ASSERT_TRUE(mailbox.send(envelopes[i]));
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << " (sent " << numMessages << " messages in " << elapsed.count() << " us)";

    ASSERT_EQ(mailbox.size(), numMessages);

    // 清空
    Envelope recv;
    while (mailbox.tryReceive(recv)) {
        // 消费所有消息
    }

    ASSERT_EQ(mailbox.size(), 0);
}

//------------------------------------------------------------------------------
// 主函数
//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "   Apollo Actor Framework Tests       " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 消息测试
    std::cout << "=== Message Tests ===" << std::endl;
    RUN_TEST(message_basics);
    RUN_TEST(message_copy);
    RUN_TEST(actor_path_parsing);
    RUN_TEST(actor_path_to_string);

    // Mailbox 测试
    std::cout << "\n=== Mailbox Tests ===" << std::endl;
    RUN_TEST(mailbox_basic);
    RUN_TEST(mailbox_capacity);
    RUN_TEST(mailbox_watermark);
    RUN_TEST(mailbox_concurrent);

    // Actor 生命周期测试
    std::cout << "\n=== Actor Lifecycle Tests ===" << std::endl;
    RUN_TEST(actor_lifecycle);

    // 消息发送测试
    std::cout << "\n=== Actor Messaging Tests ===" << std::endl;
    RUN_TEST(actor_messaging);
    RUN_TEST(actor_concurrent_send);

    // 定时器测试
    std::cout << "\n=== Timer Tests ===" << std::endl;
    RUN_TEST(timer_oneshot);
    RUN_TEST(timer_repeated);
    RUN_TEST(timer_cancel);
    RUN_TEST(timer_concurrent);

    // 管理器测试
    std::cout << "\n=== Actor Manager Tests ===" << std::endl;
    RUN_TEST(actor_manager_registration);
    RUN_TEST(actor_manager_find_by_prefix);
    RUN_TEST(actor_manager_stats);

    // 观察者测试
    std::cout << "\n=== Observer Tests ===" << std::endl;
    RUN_TEST(observer_basic);

    // 健康检查测试
    std::cout << "\n=== Health Check Tests ===" << std::endl;
    RUN_TEST(health_check);

    // 边界条件测试
    std::cout << "\n=== Edge Case Tests ===" << std::endl;
    RUN_TEST(mailbox_close_and_send);
    RUN_TEST(actor_path_empty);
    RUN_TEST(timer_zero_delay);
    RUN_TEST(timer_negative_interval);
    RUN_TEST(message_empty);

    // 压力测试
    std::cout << "\n=== Stress Tests ===" << std::endl;
    RUN_TEST(stress_many_actors);
    RUN_TEST(stress_message_burst);

    // 汇总
    std::cout << "\n========================================" << std::endl;
    std::cout << "   Test Summary                       " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;

    return testsFailed > 0 ? 1 : 0;
}

//==============================================================================
// 编译说明
//==============================================================================
/*
g++ -std=c++20 -I../include -L../build \
    test_actor_framework.cpp \
    -lapollo -lpthread -o test_actor_framework

运行：
./test_actor_framework

或使用 CMake:
add_executable(test_actor_framework tests/test_actor_framework.cpp)
target_link_libraries(test_actor_framework PRIVATE apollo)

enable_testing()
add_test(NAME actor_tests COMMAND test_actor_framework)
*/
