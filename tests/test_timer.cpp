/**
 * @file test_timer.cpp
 * @brief 定时器系统测试
 */

#include "apollo/core/timer/timer_manager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

using namespace apollo::core::timer;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// 测试回调类
//==============================================================================

class CounterTimer : public ITimerCallback {
public:
    std::atomic<int> count{0};
    TimerId lastTimerId = INVALID_TIMER_ID;

    void onTimer(TimerId timerId) override {
        count++;
        lastTimerId = timerId;
    }
};

//==============================================================================
// 测试用例
//==============================================================================

/**
 * @brief 测试定时器创建和销毁
 */
bool test_timer_create_destroy() {
    std::cout << "Running: test_timer_create_destroy..." << std::endl;

    TimerManager manager(1);  // 1ms精度

    TEST_ASSERT(manager.getTimerCount() == 0, "Initial timer count");

    TimerId id = manager.setTimer(100, [](TimerId, void*) {
        // 空回调
    });

    TEST_ASSERT(id != INVALID_TIMER_ID, "Timer created");
    TEST_ASSERT(manager.getTimerCount() == 1, "Timer count after create");
    TEST_ASSERT(manager.hasTimer(id), "Timer exists");

    manager.killTimer(id);
    TEST_ASSERT(!manager.hasTimer(id), "Timer removed");
    TEST_ASSERT(manager.getTimerCount() == 0, "Timer count after kill");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试单次定时器
 */
bool test_timer_once() {
    std::cout << "Running: test_timer_once..." << std::endl;

    TimerManager manager(1);
    std::atomic<bool> called{false};
    std::atomic<TimerId> receivedId{INVALID_TIMER_ID};

    TimerId id = manager.setTimer(50, [&](TimerId timerId, void*) {
        called = true;
        receivedId = timerId;
    }, 0);  // 0表示只执行一次

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (manager.update(100)) {
        // 处理所有事件
    }

    TEST_ASSERT(called, "Callback called");
    TEST_ASSERT(receivedId == id, "Correct timer ID");
    TEST_ASSERT(manager.getTimerCount() == 0, "Timer auto-removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试周期定时器
 */
bool test_timer_repeat() {
    std::cout << "Running: test_timer_repeat..." << std::endl;

    TimerManager manager(1);
    std::atomic<int> count{0};

    TimerId id = manager.setTimer(20, [&](TimerId, void*) {
        count++;
    }, 5);  // 执行5次

    // 等待足够时间
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    while (manager.update(100)) {
        // 处理所有事件
    }

    TEST_ASSERT(count == 5, "Repeat count");
    TEST_ASSERT(manager.getTimerCount() == 0, "Timer removed after completion");

    std::cout << "    Count: " << count << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试无限循环定时器
 */
bool test_timer_infinite() {
    std::cout << "Running: test_timer_infinite..." << std::endl;

    TimerManager manager(1);
    std::atomic<int> count{0};

    TimerId id = manager.setTimer(10, [&](TimerId, void*) {
        count++;
    }, 0xFFFFFFFF);  // 无限循环

    // 运行一段时间
    auto start = std::chrono::steady_clock::now();
    while (count < 5) {
        manager.update(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 500) {
            break;  // 超时
        }
    }

    manager.killTimer(id);

    TEST_ASSERT(count >= 5, "Multiple executions");
    TEST_ASSERT(!manager.hasTimer(id), "Timer killed");

    std::cout << "    Count: " << count << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试接口回调
 */
bool test_timer_interface_callback() {
    std::cout << "Running: test_timer_interface_callback..." << std::endl;

    TimerManager manager(1);
    CounterTimer counter;

    TimerId id = manager.setTimer(50, &counter, 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (manager.update(100)) {
        // 处理所有事件
    }

    TEST_ASSERT(counter.count == 1, "Callback executed");
    TEST_ASSERT(counter.lastTimerId == id, "Correct timer ID");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试用户数据
 */
bool test_timer_userdata() {
    std::cout << "Running: test_timer_userdata..." << std::endl;

    TimerManager manager(1);
    int testData = 42;
    std::atomic<int> receivedData{0};

    manager.setTimer(50, [&](TimerId, void* data) {
        receivedData = *static_cast<int*>(data);
    }, 1, &testData);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (manager.update(100)) {
        // 处理所有事件
    }

    TEST_ASSERT(receivedData == 42, "User data passed correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试多个定时器
 */
bool test_timer_multiple() {
    std::cout << "Running: test_timer_multiple..." << std::endl;

    TimerManager manager(1);
    std::atomic<int> count1{0};
    std::atomic<int> count2{0};
    std::atomic<int> count3{0};

    // 三个不同频率的定时器
    manager.setTimer(20, [&](TimerId, void*) { count1++; }, 3);
    manager.setTimer(30, [&](TimerId, void*) { count2++; }, 2);
    manager.setTimer(40, [&](TimerId, void*) { count3++; }, 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    while (manager.update(100)) {
        // 处理所有事件
    }

    TEST_ASSERT(count1 == 3, "Timer1 count");
    TEST_ASSERT(count2 == 2, "Timer2 count");
    TEST_ASSERT(count3 == 2, "Timer3 count");

    std::cout << "    Count1: " << count1 << ", Count2: " << count2 << ", Count3: " << count3 << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试定时器精度
 */
bool test_timer_precision() {
    std::cout << "Running: test_timer_precision..." << std::endl;

    TimerManager manager(1);
    std::atomic<uint64_t> actualInterval{0};
    uint64_t expectedInterval = 50;

    auto startTime = TimerManager::getCurrentTimeMs();

    manager.setTimer(expectedInterval, [&](TimerId, void*) {
        actualInterval = TimerManager::getCurrentTimeMs() - startTime;
    }, 1);

    while (manager.getTimerCount() > 0) {
        manager.update(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 允许一定的误差（+10ms）
    uint64_t error = (actualInterval > expectedInterval)
        ? (actualInterval - expectedInterval)
        : (expectedInterval - actualInterval);

    TEST_ASSERT(error <= 15, "Timer precision within 15ms");

    std::cout << "    Expected: " << expectedInterval << "ms, Actual: " << actualInterval << "ms" << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试取消定时器
 */
bool test_timer_cancel() {
    std::cout << "Running: test_timer_cancel..." << std::endl;

    TimerManager manager(1);
    std::atomic<int> count{0};

    TimerId id = manager.setTimer(10, [&](TimerId, void*) {
        count++;
    }, 100);  // 本应执行100次

    // 运行一小段时间后取消
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    manager.killTimer(id);

    TEST_ASSERT(manager.getTimerCount() == 0, "Timer removed");
    TEST_ASSERT(count < 100, "Timer canceled early");

    std::cout << "    Executed: " << count << " times before cancel" << std::endl;
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试线程安全性
 */
bool test_timer_thread_safety() {
    std::cout << "Running: test_timer_thread_safety..." << std::endl;

    TimerManager manager(1);
    std::atomic<int> totalCount{0};
    constexpr int threadCount = 4;
    constexpr int timersPerThread = 25;

    std::vector<std::thread> threads;
    std::vector<TimerId> timerIds;

    // 创建定时器的线程
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < timersPerThread; ++i) {
                TimerId id = manager.setTimer(10 + i, [&, t](TimerId, void*) {
                    totalCount++;
                }, 2);
                timerIds.push_back(id);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // 等待创建完成
    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();

    // 运行定时器
    for (int i = 0; i < 200; ++i) {
        manager.update(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::cout << "    Total timers: " << timerIds.size() << std::endl;
    std::cout << "    Total executions: " << totalCount << std::endl;

    TEST_ASSERT(timerIds.size() == threadCount * timersPerThread, "All timers created");
    TEST_ASSERT(totalCount > 0, "Some timers executed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试转储信息
 */
bool test_timer_dump() {
    std::cout << "Running: test_timer_dump..." << std::endl;

    TimerManager manager(1);

    manager.setTimer(100, [](TimerId, void*) {}, 5);
    manager.setTimer(200, [](TimerId, void*) {}, 3);

    std::string info = manager.dumpInfo();

    TEST_ASSERT(info.find("Timer count: 2") != std::string::npos, "Dump shows count");

    std::cout << info;

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Timer System Tests ===" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        total++;
        if (test()) passed++;
        else {
            std::cout << "  FAILED!" << std::endl;
        }
    };

    // 运行所有测试
    run("test_timer_create_destroy", test_timer_create_destroy);
    run("test_timer_once", test_timer_once);
    run("test_timer_repeat", test_timer_repeat);
    run("test_timer_infinite", test_timer_infinite);
    run("test_timer_interface_callback", test_timer_interface_callback);
    run("test_timer_userdata", test_timer_userdata);
    run("test_timer_multiple", test_timer_multiple);
    run("test_timer_precision", test_timer_precision);
    run("test_timer_cancel", test_timer_cancel);
    run("test_timer_thread_safety", test_timer_thread_safety);
    run("test_timer_dump", test_timer_dump);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_timer_main() {
    return main();
}
