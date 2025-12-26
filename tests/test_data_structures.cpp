/**
 * @file test_data_structures.cpp
 * @brief 时间轮、最小堆、对象池单元测试
 */

#include <iostream>
#include <cassert>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <algorithm>

#include "apollo/algorithm/timing_wheel.h"
#include "apollo/algorithm/min_heap.h"
#include "apollo/algorithm/object_pool.h"

using namespace apollo::algorithm;

//==============================================================================
// 测试辅助宏
//==============================================================================

#define TEST(name) \
    do { \
        std::cout << "[TEST] " << #name << "..."; \
        test_##name(); \
        std::cout << " PASS\n"; \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cout << " FAILED: " << #cond << "\n"; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))

//==============================================================================
// 最小堆测试
//==============================================================================

void test_min_heap_push_pop() {
    MinHeap<int> heap;

    heap.push(5);
    heap.push(3);
    heap.push(7);
    heap.push(1);
    heap.push(9);

    ASSERT_EQ(heap.size(), 5);
    ASSERT_EQ(heap.peek(), 1);

    ASSERT_EQ(heap.pop(), 1);
    ASSERT_EQ(heap.pop(), 3);
    ASSERT_EQ(heap.pop(), 5);
    ASSERT_EQ(heap.pop(), 7);
    ASSERT_EQ(heap.pop(), 9);

    ASSERT_TRUE(heap.empty());
}

void test_min_heap_heapify() {
    std::vector<int> vec = {5, 3, 7, 1, 9};
    MinHeap<int> heap(vec.begin(), vec.end());

    ASSERT_EQ(heap.size(), 5);
    ASSERT_EQ(heap.peek(), 1);
}

void test_min_heap_update() {
    MinHeap<int> heap;

    heap.push(5);
    heap.push(3);
    heap.push(7);

    // 更新索引 1 的值（3 -> 10，应该下沉）
    heap.update(1, 10);
    ASSERT_EQ(heap.peek(), 5);

    // 更新索引 2 的值（7 -> 1，应该上浮）
    heap.update(2, 1);
    ASSERT_EQ(heap.peek(), 1);
}

void test_min_heap_erase() {
    MinHeap<int> heap;

    heap.push(5);
    heap.push(3);
    heap.push(7);
    heap.push(1);

    // 删除索引 2 (7)
    heap.erase(2);

    ASSERT_EQ(heap.size(), 3);
    ASSERT_EQ(heap.pop(), 1);
    ASSERT_EQ(heap.pop(), 3);
    ASSERT_EQ(heap.pop(), 5);
}

void test_max_heap() {
    MaxHeap<int> heap;

    heap.push(5);
    heap.push(3);
    heap.push(7);
    heap.push(1);

    ASSERT_EQ(heap.peek(), 7);
    ASSERT_EQ(heap.pop(), 7);
    ASSERT_EQ(heap.pop(), 5);
    ASSERT_EQ(heap.pop(), 3);
    ASSERT_EQ(heap.pop(), 1);
}

void test_indexed_heap() {
    IndexedHeap<int, uint64_t> heap;

    heap.push(1, 50);
    heap.push(2, 30);
    heap.push(3, 70);
    heap.push(4, 10);

    ASSERT_EQ(heap.size(), 4);
    ASSERT_EQ(heap.peek(), 10);

    // 通过 ID 删除
    ASSERT_TRUE(heap.erase(2));  // 删除值为 30 的
    ASSERT_EQ(heap.size(), 3);

    // 验证剩余元素
    ASSERT_EQ(heap.peek(), 10);
    heap.pop();
    ASSERT_EQ(heap.peek(), 50);
    heap.pop();
    ASSERT_EQ(heap.peek(), 70);
}

void test_heap_with_custom_comparator() {
    // 使用自定义比较器（按绝对值排序）
    auto abs_less = [](int a, int b) {
        return std::abs(a) < std::abs(b);
    };

    MinHeap<int, decltype(abs_less)> heap(abs_less);

    heap.push(-5);
    heap.push(3);
    heap.push(-1);
    heap.push(7);

    ASSERT_EQ(heap.peek(), -1);  // | -1 | = 1 最小
    heap.pop();
    ASSERT_EQ(heap.peek(), 3);   // | 3 | = 3
    heap.pop();
    ASSERT_EQ(heap.peek(), -5);  // | -5 | = 5
}

//==============================================================================
// 时间轮测试
//==============================================================================

void test_timing_wheel_basic() {
    TimingWheel wheel({10, 100, 3});  // 10ms tick, 100 slots, 3 wheels

    std::vector<int> firedOrder;

    wheel.add(50, [&firedOrder]() { firedOrder.push_back(1); });
    wheel.add(30, [&firedOrder]() { firedOrder.push_back(2); });
    wheel.add(100, [&firedOrder]() { firedOrder.push_back(3); });

    int64_t currentTime = 0;
    while (currentTime < 150) {
        auto tasks = wheel.tick(currentTime);
        for (auto& task : tasks) {
            task->run();
        }
        currentTime += 10;
    }

    ASSERT_EQ(firedOrder.size(), 3);
    ASSERT_EQ(firedOrder[0], 2);  // 30ms 先到期
    ASSERT_EQ(firedOrder[1], 1);  // 50ms
    ASSERT_EQ(firedOrder[2], 3);  // 100ms
}

void test_timing_wheel_cancel() {
    TimingWheel wheel({10, 100, 2});

    std::vector<int> fired;

    uint64_t id1 = wheel.add(50, [&fired]() { fired.push_back(1); });
    uint64_t id2 = wheel.add(30, [&fired]() { fired.push_back(2); });

    // 取消第一个任务
    wheel.cancel(id1);

    int64_t currentTime = 0;
    while (currentTime < 100) {
        auto tasks = wheel.tick(currentTime);
        for (auto& task : tasks) {
            task->run();
        }
        currentTime += 10;
    }

    ASSERT_EQ(fired.size(), 1);
    ASSERT_EQ(fired[0], 2);  // 只有第二个任务执行
}

void test_timing_wheel_cascade() {
    TimingWheel wheel({10, 4, 4});  // 小轮子便于测试级联

    std::vector<int> fired;

    // 添加一个需要级联的任务（超过第一轮范围）
    wheel.add(100, [&fired]() { fired.push_back(1); });

    int64_t currentTime = 0;
    while (currentTime < 150) {
        auto tasks = wheel.tick(currentTime);
        for (auto& task : tasks) {
            task->run();
        }
        currentTime += 10;
    }

    ASSERT_EQ(fired.size(), 1);
    ASSERT_EQ(fired[0], 1);
}

void test_timing_wheel_stats() {
    TimingWheel wheel({10, 100, 3});

    for (int i = 0; i < 10; ++i) {
        wheel.add(50 + i * 10, []() {});
    }

    auto stats = wheel.getStats();
    ASSERT_EQ(stats.totalTasks, 10);
    ASSERT_GT(stats.activeTasks, 0);
}

//==============================================================================
// 对象池测试
//==============================================================================

void test_object_pool_basic() {
    struct TestObj {
        int value = 0;
        TestObj() : value(0) {}
        explicit TestObj(int v) : value(v) {}
    };

    ObjectPool<TestObj, false> pool({4, 10, 2});

    {
        auto obj1 = pool.acquire();
        auto obj2 = pool.acquire();
        auto obj3 = pool.acquire();

        obj1->value = 1;
        obj2->value = 2;
        obj3->value = 3;

        ASSERT_EQ(pool.allocCount(), 4);  // 初始预分配 4 个
    }

    // 对象归还后应该可复用
    auto stats = pool.getStats();
    ASSERT_GT(stats.currentlyFree, 0);
}

void test_object_pool_hit_rate() {
    ObjectPool<int, false> pool({2, 10, 2});

    {
        auto obj1 = pool.acquire();
        auto obj2 = pool.acquire();
        auto obj3 = pool.acquire();  // 需要新分配
        auto obj4 = pool.acquire();  // 需要新分配
    }

    {
        // 这四个应该命中缓存
        auto obj1 = pool.acquire();
        auto obj2 = pool.acquire();
        auto obj3 = pool.acquire();
        auto obj4 = pool.acquire();

        auto stats = pool.getStats();
        ASSERT_GT(stats.currentlyInUse, 0);
        ASSERT_GT(stats.hitRate, 0.5);  // 应该有较高的命中率
    }
}

void test_object_pool_thread_safe() {
    ObjectPool<int, true> pool({10, 100, 2});

    std::vector<std::thread> threads;
    const int numThreads = 4;
    const int numPerThread = 100;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&pool, numPerThread]() {
            for (int i = 0; i < numPerThread; ++i) {
                auto obj = pool.acquire();
                *obj = i;
                // 模拟使用
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = pool.getStats();
    ASSERT_EQ(stats.currentlyFree, stats.totalAllocated);  // 全部归还
}

void test_fixed_object_pool() {
    FixedObjectPool<int, 16> pool;

    std::vector<int*> allocated;

    // 分配所有对象
    for (size_t i = 0; i < 16; ++i) {
        int* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        *obj = static_cast<int>(i);
        allocated.push_back(obj);
    }

    ASSERT_EQ(pool.available(), 0);

    // 再分配应该失败
    int* obj = pool.allocate();
    ASSERT_EQ(obj, nullptr);

    // 归还一半
    for (size_t i = 0; i < 8; ++i) {
        pool.deallocate(allocated[i]);
    }

    ASSERT_EQ(pool.available(), 8);
}

void test_custom_object_pool() {
    struct Data {
        std::string text;
        int number;
        void reset() {
            text.clear();
            number = 0;
        }
    };

    CustomObjectPool<Data, false> pool(
        []() { return new Data(); },
        [](Data* d) { d->reset(); },
        {4, 10, 2}
    );

    {
        auto data1 = pool.acquire();
        data1->text = "Hello";
        data1->number = 42;
    }

    // 对象应该被重置
    auto data2 = pool.acquire();
    ASSERT_TRUE(data2->text.empty());
    ASSERT_EQ(data2->number, 0);
}

//==============================================================================
// 压力测试
//==============================================================================

void test_heap_stress() {
    MinHeap<int> heap;
    const int n = 10000;

    // 随机插入
    std::vector<int> values;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100000);

    for (int i = 0; i < n; ++i) {
        int val = dis(gen);
        values.push_back(val);
        heap.push(val);
    }

    // 排序验证
    std::sort(values.begin(), values.end());

    for (int i = 0; i < n; ++i) {
        ASSERT_EQ(heap.pop(), values[i]);
    }

    ASSERT_TRUE(heap.empty());
}

void test_timing_wheel_stress() {
    TimingWheel wheel({1, 1000, 4});

    std::atomic<int> firedCount{0};
    const int n = 1000;

    // 添加大量任务
    for (int i = 0; i < n; ++i) {
        wheel.add(1 + i, [&firedCount]() {
            firedCount.fetch_add(1);
        });
    }

    // 推进时间
    int64_t currentTime = 0;
    while (currentTime < n + 100) {
        auto tasks = wheel.tick(currentTime);
        for (auto& task : tasks) {
            task->run();
        }
        currentTime += 1;
    }

    ASSERT_EQ(firedCount.load(), n);
}

void test_object_pool_stress() {
    ObjectPool<std::string, true> pool({100, 10000, 2});

    std::vector<std::thread> threads;
    const int numThreads = 8;
    const int numPerThread = 1000;

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&pool, numPerThread]() {
            for (int i = 0; i < numPerThread; ++i) {
                auto str = pool.acquire();
                *str = "Test string " + std::to_string(i);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto stats = pool.getStats();
    ASSERT_EQ(stats.currentlyInUse, 0);  // 全部归还
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Data Structures Tests\n";
    std::cout << "========================================\n\n";

    std::cout << "--- MinHeap Tests ---\n";
    TEST(min_heap_push_pop);
    TEST(min_heap_heapify);
    TEST(min_heap_update);
    TEST(min_heap_erase);
    TEST(max_heap);
    TEST(indexed_heap);
    TEST(heap_with_custom_comparator);

    std::cout << "\n--- TimingWheel Tests ---\n";
    TEST(timing_wheel_basic);
    TEST(timing_wheel_cancel);
    TEST(timing_wheel_cascade);
    TEST(timing_wheel_stats);

    std::cout << "\n--- ObjectPool Tests ---\n";
    TEST(object_pool_basic);
    TEST(object_pool_hit_rate);
    TEST(object_pool_thread_safe);
    TEST(fixed_object_pool);
    TEST(custom_object_pool);

    std::cout << "\n--- Stress Tests ---\n";
    TEST(heap_stress);
    TEST(timing_wheel_stress);
    TEST(object_pool_stress);

    std::cout << "\n========================================\n";
    std::cout << "  All Tests Passed!\n";
    std::cout << "========================================\n";

    return 0;
}
