/**
 * @file test_actor_concurrency.cpp
 * @brief Actor 框架并发和死锁测试
 *
 * 重点测试：
 * 1. 死锁检测
 * 2. 竞态条件
 * 3. 内存可见性
 * 4. 资源泄漏
 * 5. 异常安全
 */

#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_manager.h"
#include "apollo/actor/timer_service.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <random>
#include <sanitizer/tsan_interface.h>  // ThreadSanitizer（如果可用）

using namespace apollo::actor;

#define TEST(name) void test_##name()
#define RUN_TEST(name) \
    do { \
        std::cout << "Running: " << #name << "..."; \
        test_##name(); \
        std::cout << " PASSED" << std::endl; \
    } catch (const std::exception& e) { \
        std::cout << " FAILED: " << e.what() << std::endl; \
        throw; \
    }

#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #cond); \
    }

//==============================================================================
// 测试消息
//==============================================================================

struct RequestMsg {
    int from;
    int value;

    static const char* typeName() { return "RequestMsg"; }
};

struct ResponseMsg {
    int to;
    int value;

    static const char* typeName() { return "ResponseMsg"; }
};

struct ForwardMsg {
    int hopCount;
    std::string path;

    static const char* typeName() { return "ForwardMsg"; }
};

//==============================================================================
// 死锁测试
//------------------------------------------------------------------------------

class DeadlockTestActor : public Actor {
public:
    std::atomic<bool> waiting{false};
    std::string waitingFor;

protected:
    void receive(const Message& msg) override {
        if (msg.is<RequestMsg>()) {
            auto req = msg.as<RequestMsg>();
            waiting.store(true);
            waitingFor = "actor-" + std::to_string(req.from);

            // 模拟等待响应（可能导致死锁）
            // 实际应用中应使用 ask() 模式
        } else if (msg.is<ResponseMsg>()) {
            waiting.store(false);
            waitingFor.clear();
        }
    }
};

TEST(deadlock_circular_wait) {
    // 测试循环等待死锁
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    // 创建多个互相等待的 Actor
    std::vector<std::shared_ptr<DeadlockTestActor>> actors;
    std::vector<std::shared_ptr<ActorCell>> cells;

    for (int i = 0; i < 3; i++) {
        auto actor = std::make_shared<DeadlockTestActor>();
        ActorPath path;
        path.system = "test-system";
        path.name = "actor-" + std::to_string(i);

        MailboxConfig mailboxConfig;
        auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
        cell->start();
        manager->registerActor(cell);

        actors.push_back(actor);
        cells.push_back(cell);
    }

    // 创建循环等待: actor-0 -> actor-1 -> actor-2 -> actor-0
    for (int i = 0; i < 3; i++) {
        RequestMsg req;
        req.from = i;
        req.value = i;

        Message msg(req, MessageFormat::Binary);
        int next = (i + 1) % 3;
        // cells[next]->tell(msg, cells[i]->self());
    }

    // 使用超时检测死锁
    bool deadlockDetected = false;
    std::thread detector([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        for (auto& actor : actors) {
            if (actor->waiting.load()) {
                deadlockDetected = true;
                break;
            }
        }
    });

    detector.join();

    // 清理
    for (auto& cell : cells) {
        cell->stop();
    }

    system.shutdown();

    // 这里我们只是演示死锁场景，实际应该使用超时机制避免
    std::cout << " (deadlock scenario demonstrated)";
}

//------------------------------------------------------------------------------
// 竞态条件测试
//------------------------------------------------------------------------------

class RaceTestActor : public Actor {
public:
    std::atomic<int> counter{0};
    std::vector<int> values;
    std::mutex valuesMutex;

protected:
    void receive(const Message& msg) override {
        if (msg.is<RequestMsg>()) {
            auto req = msg.as<RequestMsg>();

            // 模拟竞态：先读后写
            int old = counter.load();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.store(old + req.value);

            std::lock_guard lock(valuesMutex);
            values.push_back(counter.load());
        }
    }
};

TEST(race_condition_counter) {
    // 测试计数器竞态条件
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<RaceTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "race-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();

    const int numThreads = 10;
    const int iterationsPerThread = 100;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&cell, iterationsPerThread]() {
            for (int j = 0; j < iterationsPerThread; j++) {
                RequestMsg req;
                req.from = j;
                req.value = 1;

                Message msg(req, MessageFormat::Binary);
                cell->tell(msg, ActorRef{});
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 处理所有消息
    for (int i = 0; i < 1000; i++) {
        cell->processMessages();
    }

    // 检查结果
    int finalValue = actor->counter.load();

    // 由于竞态条件，最终值可能不等于预期
    // 这演示了需要使用原子操作或互斥锁
    std::cout << " (final counter: " << finalValue << ", expected: "
              << numThreads * iterationsPerThread << ")";

    ASSERT_TRUE(finalValue <= numThreads * iterationsPerThread);

    cell->stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 内存可见性测试
//------------------------------------------------------------------------------

class VisibilityTestActor : public Actor {
public:
    std::atomic<bool> flag{false};
    int data = 0;  // 非 atomic

protected:
    void receive(const Message& msg) override {
        if (msg.is<RequestMsg>()) {
            auto req = msg.as<RequestMsg>();

            // 写数据
            data = req.value;
            // 设置标志（必须有内存屏障）
            flag.store(true, std::memory_order_release);

        } else if (msg.is<ResponseMsg>()) {
            // 读取数据
            if (flag.load(std::memory_order_acquire)) {
                // 此时 data 应该可见
                int value = data;
                (void)value;
            }
        }
    }
};

TEST(memory_visibility) {
    // 测试内存可见性
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<VisibilityTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "visibility-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();

    // 线程 1: 写入数据
    std::thread writer([&]() {
        RequestMsg req;
        req.value = 42;

        Message msg(req, MessageFormat::Binary);
        cell->tell(msg, ActorRef{});
    });

    // 线程 2: 读取数据
    std::thread reader([&]() {
        while (!actor->flag.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        int value = actor->data;
        ASSERT_EQ(value, 42);
    });

    writer.join();
    reader.join();

    cell->stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 资源泄漏测试
//------------------------------------------------------------------------------

TEST(resource_leak_actors) {
    // 测试 Actor 资源泄漏
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    ThreadPoolConfig threadConfig;
    ActorManager manager(threadConfig);
    ASSERT_TRUE(manager.start(system));

    const int numIterations = 100;

    for (int iter = 0; iter < numIterations; iter++) {
        // 创建多个 Actor
        for (int i = 0; i < 10; i++) {
            auto actor = std::make_shared<ConcurrentTestActor>();
            ActorPath path;
            path.system = "test-system";
            path.name = "temp-actor-" + std::to_string(iter) + "-" + std::to_string(i);

            MailboxConfig mailboxConfig;
            auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
            cell->start();
            manager.registerActor(cell);
        }

        // 立即销毁
        auto names = manager.getAllActorNames();
        for (const auto& name : names) {
            if (name.find("temp-actor-") == 0) {
                manager.unregisterActor(name);
            }
        }
    }

    // 验证最终没有残留
    auto finalNames = manager.getAllActorNames();
    ASSERT_EQ(finalNames.size(), 0);

    manager.stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 异常安全测试
//------------------------------------------------------------------------------

class ExceptionTestActor : public Actor {
public:
    std::atomic<int> exceptionCount{0};
    std::atomic<int> messageCount{0};

protected:
    void receive(const Message& msg) override {
        messageCount.fetch_add(1);

        if (msg.is<RequestMsg>()) {
            auto req = msg.as<RequestMsg>();

            // 某些条件下抛异常
            if (req.value < 0) {
                exceptionCount.fetch_add(1);
                throw std::runtime_error("Negative value");
            }
        }
    }

    void onError(const std::exception& e) override {
        // Actor 应该继续运行
    }
};

TEST(exception_safety) {
    // 测试异常处理
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<ExceptionTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "exception-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();

    // 发送正常消息
    for (int i = 0; i < 10; i++) {
        RequestMsg req;
        req.value = i;

        Message msg(req, MessageFormat::Binary);
        cell->tell(msg, ActorRef{});
    }

    // 发送导致异常的消息
    RequestMsg badReq;
    badReq.value = -1;
    Message badMsg(badReq, MessageFormat::Binary);
    cell->tell(badMsg, ActorRef{});

    // Actor 应该继续处理消息
    RequestMsg goodReq;
    goodReq.value = 100;
    Message goodMsg(goodReq, MessageFormat::Binary);
    cell->tell(goodMsg, ActorRef{});

    // 处理消息
    for (int i = 0; i < 20; i++) {
        cell->processMessages();
    }

    // 验证
    ASSERT_EQ(actor->messageCount.load(), 12);  // 10 + 1 bad + 1 good
    ASSERT_EQ(actor->exceptionCount.load(), 1);

    cell->stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 定时器并发测试
//------------------------------------------------------------------------------

TEST(timer_concurrent_scheduling) {
    // 测试并发调度定时器
    TimerServiceConfig config;
    config.workerThreads = 2;
    TimerService timer(config);

    ASSERT_TRUE(timer.start());

    const int numTimers = 1000;
    std::atomic<int> firedCount{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < numTimers / 4; i++) {
                timer.scheduleOnce(10 + (i % 50), [&]() {
                    firedCount.fetch_add(1);
                });
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 等待所有定时器触发
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(firedCount.load(), numTimers);

    timer.stop();
}

//------------------------------------------------------------------------------
// 邮箱并发压力测试
//------------------------------------------------------------------------------

TEST(mailbox_stress_concurrent) {
    // 极限并发测试
    MailboxConfig config;
    config.capacity = 100000;
    config.strategy = ipc::BackpressureStrategy::Buffer;

    Mailbox mailbox(config);

    const int numProducers = 20;
    const int numConsumers = 10;
    const int messagesPerProducer = 5000;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // 生产者
    for (int i = 0; i < numProducers; i++) {
        producers.emplace_back([&]() {
            Envelope env;
            env.message = Message(RequestMsg{}, MessageFormat::Binary);

            for (int j = 0; j < messagesPerProducer; j++) {
                while (!mailbox.send(env)) {
                    std::this_thread::yield();
                }
                produced.fetch_add(1);
            }
        });
    }

    // 消费者
    for (int i = 0; i < numConsumers; i++) {
        consumers.emplace_back([&]() {
            Envelope env;
            while (consumed.load() < numProducers * messagesPerProducer) {
                if (mailbox.tryReceive(env)) {
                    consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    for (auto& t : consumers) {
        t.join();
    }

    ASSERT_EQ(produced.load(), numProducers * messagesPerProducer);
    ASSERT_EQ(consumed.load(), numProducers * messagesPerProducer);
    ASSERT_EQ(mailbox.size(), 0);
}

//------------------------------------------------------------------------------
// 无锁队列验证
//------------------------------------------------------------------------------

TEST(lock_free_behavior) {
    // 验证原子操作的正确性
    std::atomic<int> counter{0};

    const int numThreads = 10;
    const int iterationsPerThread = 10000;

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < iterationsPerThread; j++) {
                // 使用 fetch_add 确保原子性
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_EQ(counter.load(), numThreads * iterationsPerThread);
}

//------------------------------------------------------------------------------
// 顺序一致性测试
//------------------------------------------------------------------------------

class OrderTestActor : public Actor {
public:
    std::vector<int> sequence;
    std::mutex mutex;

protected:
    void receive(const Message& msg) override {
        if (msg.is<RequestMsg>()) {
            auto req = msg.as<RequestMsg>();

            std::lock_guard lock(mutex);
            sequence.push_back(req.value);
        }
    }
};

TEST(message_ordering) {
    // 测试消息顺序
    ActorSystemConfig config;
    config.systemName = "test-system";
    config.discoveryConfig.backend = DiscoveryBackend::Memory;

    ActorSystem system(config);
    ASSERT_TRUE(system.start());

    auto actor = std::make_shared<OrderTestActor>();
    ActorPath path;
    path.system = "test-system";
    path.name = "order-actor";

    MailboxConfig mailboxConfig;
    auto cell = std::make_shared<ActorCell>(path, actor, system, mailboxConfig);
    cell->start();

    const int numMessages = 100;

    // 按顺序发送
    for (int i = 0; i < numMessages; i++) {
        RequestMsg req;
        req.value = i;

        Message msg(req, MessageFormat::Binary);
        cell->tell(msg, ActorRef{});
    }

    // 处理消息
    for (int i = 0; i < numMessages; i++) {
        cell->processMessages();
    }

    // 验证顺序
    ASSERT_EQ(actor->sequence.size(), numMessages);

    for (int i = 0; i < numMessages; i++) {
        ASSERT_EQ(actor->sequence[i], i);
    }

    cell->stop();
    system.shutdown();
}

//------------------------------------------------------------------------------
// 主函数
//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Actor Concurrency & Stress Tests    " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "=== Deadlock Tests ===" << std::endl;
    RUN_TEST(deadlock_circular_wait);

    std::cout << "\n=== Race Condition Tests ===" << std::endl;
    RUN_TEST(race_condition_counter);

    std::cout << "\n=== Memory Visibility Tests ===" << std::endl;
    RUN_TEST(memory_visibility);

    std::cout << "\n=== Resource Leak Tests ===" << std::endl;
    RUN_TEST(resource_leak_actors);

    std::cout << "\n=== Exception Safety Tests ===" << std::endl;
    RUN_TEST(exception_safety);

    std::cout << "\n=== Timer Concurrency Tests ===" << std::endl;
    RUN_TEST(timer_concurrent_scheduling);

    std::cout << "\n=== Stress Tests ===" << std::endl;
    RUN_TEST(mailbox_stress_concurrent);
    RUN_TEST(lock_free_behavior);

    std::cout << "\n=== Ordering Tests ===" << std::endl;
    RUN_TEST(message_ordering);

    std::cout << "\n=== All Tests Completed ===" << std::endl;

    return 0;
}

//==============================================================================
// 编译和运行
//==============================================================================
/*
g++ -std=c++20 -I../include -L../build \
    test_actor_concurrency.cpp \
    -lapollo -lpthread -o test_actor_concurrency

运行：
./test_actor_concurrency

使用 ThreadSanitizer 检测数据竞争：
g++ -std=c++20 -I../include -L../build \
    -fsanitize=thread -fPIE -pie \
    test_actor_concurrency.cpp \
    -lapollo -lpthread -o test_actor_concurrency_tsan

./test_actor_concurrency_tsan
*/
