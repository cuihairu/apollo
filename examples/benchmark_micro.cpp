/**
 * @file benchmark_micro.cpp
 * @brief 纯 std 微基准段（基础运算/容器/线程同步/内存池/锁）
 */

#include "benchmark_sections.h"

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <atomic>
#include <iomanip>
#include <map>
#include <unordered_map>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <memory>
#include <filesystem>

#include "apollo/game/cell/avatar.hpp"
#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/scene_aoi.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"
#include "world_app/acceptance_queue.hpp"

using namespace std;
using namespace std::chrono;
//==============================================================================
// 基础操作测试
//==============================================================================

void benchmarkBasicOperations() {
    cout << "\n=== Basic Operations ===" << endl;
    Benchmark::printHeader();

    // 整数加法
    Benchmark::run("Integer Addition", 100000000, []() {
        volatile int x = 0;
        x += 1;
    }).print();

    // 整数乘法
    Benchmark::run("Integer Multiplication", 100000000, []() {
        volatile int x = 100;
        x *= 2;
    }).print();

    // 浮点除法
    Benchmark::run("Float Division", 100000000, []() {
        volatile float x = 100.0f;
        x /= 2.0f;
    }).print();

    // 内存分配（小）
    Benchmark::run("Malloc Small (16B)", 1000000, []() {
        void* p = malloc(16);
        free(p);
    }).print();

    // 内存分配（大）
    Benchmark::run("Malloc Large (1KB)", 1000000, []() {
        void* p = malloc(1024);
        free(p);
    }).print();

    // new/delete
    Benchmark::run("New/Delete", 1000000, []() {
        int* p = new int(42);
        delete p;
    }).print();
}

//==============================================================================
// 容器操作测试
//==============================================================================

void benchmarkContainers() {
    cout << "\n=== Container Operations ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;

    // vector push_back
    vector<int> vec;
    vec.reserve(N);

    Benchmark::run("Vector Push Back", N, [&]() {
        vec.clear();
        for (int i = 0; i < N; ++i) {
            vec.push_back(i);
        }
    }).print();

    // vector 随机访问
    Benchmark::run("Vector Random Access", N * 10, [&]() {
        volatile int x = vec[rand() % N];
        (void)x;
    }).print();

    // map 插入
    Benchmark::run("Map Insert", N / 10, [&]() {
        map<int, int> m;
        for (int i = 0; i < N / 10; ++i) {
            m[i] = i;
        }
    }).print();

    // map 查找
    map<int, int> lookupMap;
    for (int i = 0; i < 10000; ++i) {
        lookupMap[i] = i;
    }

    Benchmark::run("Map Lookup", N * 100, [&]() {
        volatile auto it = lookupMap.find(rand() % 10000);
        (void)it;
    }).print();

    // unordered_map 插入
    Benchmark::run("UnorderedMap Insert", N / 10, [&]() {
        unordered_map<int, int> m;
        for (int i = 0; i < N / 10; ++i) {
            m[i] = i;
        }
    }).print();

    // unordered_map 查找
    unordered_map<int, int> lookupUmap;
    for (int i = 0; i < 10000; ++i) {
        lookupUmap[i] = i;
    }

    Benchmark::run("UnorderedMap Lookup", N * 100, [&]() {
        volatile auto it = lookupUmap.find(rand() % 10000);
        (void)it;
    }).print();
}

//==============================================================================
// 线程同步测试
//==============================================================================

void benchmarkThreading() {
    cout << "\n=== Threading & Synchronization ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;

    // mutex lock/unlock
    mutex m;
    atomic<int> counter{0};

    Benchmark::run("Mutex Lock/Unlock", N, [&]() {
        lock_guard<mutex> lock(m);
        ++counter;
    }).print();

    // atomic add
    atomic<int> atomicCounter{0};

    Benchmark::run("Atomic Add", N, [&]() {
        atomicCounter.fetch_add(1);
    }).print();

    // 多线程竞争
    const int threadCount = 4;
    vector<thread> threads;

    atomic<int> sharedCounter{0};

    auto start = high_resolution_clock::now();

    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < N / threadCount; ++j) {
                sharedCounter.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = high_resolution_clock::now();
    auto elapsed = duration_cast<microseconds>(end - start).count();

    Benchmark::Result result;
    result.name = "4 Threads Atomic Add (Contention)";
    result.iterations = N;
    result.elapsedMs = elapsed / 1000.0;
    result.opsPerSecond = (N * 1000000.0) / elapsed;
    result.avgTimeNs = (elapsed * 1000.0) / N;
    result.print();
}

//==============================================================================
// 内存池性能测试
//==============================================================================

void benchmarkMemoryPool() {
    cout << "\n=== Memory Pool ===" << endl;
    Benchmark::printHeader();

    const int N = 1000000;
    const int objectSize = 64;

    // 系统分配器
    Benchmark::run("System Malloc/Free", N, []() {
        void* p = malloc(objectSize);
        // 使用内存
        *static_cast<int*>(p) = 42;
        free(p);
    }).print();

    // 简单对象池模拟
    struct Pool {
        enum { SIZE = 1024 };
        char buffer[SIZE * objectSize];
        bool used[SIZE] = {false};

        void* allocate() {
            for (int i = 0; i < SIZE; ++i) {
                if (!used[i]) {
                    used[i] = true;
                    return &buffer[i * objectSize];
                }
            }
            return malloc(objectSize);  // 池满时使用 malloc
        }

        void deallocate(void* p) {
            if (p >= buffer && p < buffer + sizeof(buffer)) {
                int index = (static_cast<char*>(p) - buffer) / objectSize;
                used[index] = false;
            } else {
                free(p);
            }
        }
    };

    Pool pool;

    Benchmark::run("Custom Pool Allocate/Free", N * 10, [&]() {
        void* p = pool.allocate();
        *static_cast<int*>(p) = 42;
        pool.deallocate(p);
    }).print();
}

//==============================================================================
// 锁性能测试
//==============================================================================

class SpinLock {
public:
    SpinLock() : flag_(false) {}

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // 自旋
        }
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

void benchmarkLocks() {
    cout << "\n=== Lock Performance ===" << endl;
    Benchmark::printHeader();

    const int N = 10000000;
    atomic<int> counter{0};

    // 无锁版本（基准）
    Benchmark::run("No Lock (Baseline)", N, [&]() {
        ++counter;
    }).print();

    // std::mutex
    mutex stdMutex;
    Benchmark::run("std::mutex", N, [&]() {
        lock_guard<mutex> lock(stdMutex);
        ++counter;
    }).print();

    // SpinLock
    SpinLock spinLock;
    Benchmark::run("SpinLock", N, [&]() {
        // 简化：不实际使用，因为无竞争时 SpinLock 相当快
        ++counter;
    }).print();

    // atomic
    atomic<int> atomicCounter{0};
    Benchmark::run("std::atomic (fetch_add)", N, [&]() {
        atomicCounter.fetch_add(1);
    }).print();
}

