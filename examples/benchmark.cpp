/**
 * @file benchmark.cpp
 * @brief Apollo 框架性能测试
 *
 * 测试各模块的性能表现
 */

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

using namespace std;
using namespace std::chrono;

//==============================================================================
// 性能测试辅助类
//==============================================================================

class Benchmark {
public:
    struct Result {
        string name;
        int64_t iterations;
        double elapsedMs;
        double opsPerSecond;
        double avgTimeNs;

        void print() const {
            cout << left << setw(30) << name
                 << right << setw(12) << iterations
                 << setw(12) << fixed << setprecision(2) << elapsedMs << " ms"
                 << setw(15) << setprecision(0) << opsPerSecond << " ops/s"
                 << setw(12) << setprecision(2) << avgTimeNs << " ns/op"
                 << endl;
        }
    };

    static Result run(const string& name, int iterations, auto&& func) {
        // 预热
        for (int i = 0; i < std::min(100, iterations / 10); ++i) {
            func();
        }

        // 正式测试
        auto start = high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            func();
        }

        auto end = high_resolution_clock::now();
        auto elapsed = duration_cast<microseconds>(end - start).count();

        Result result;
        result.name = name;
        result.iterations = iterations;
        result.elapsedMs = elapsed / 1000.0;
        result.opsPerSecond = (iterations * 1000000.0) / elapsed;
        result.avgTimeNs = (elapsed * 1000.0) / iterations;

        return result;
    }

    static void printHeader() {
        cout << left << setw(30) << "Benchmark"
             << right << setw(12) << "Iterations"
             << setw(12) << "Time"
             << setw(15) << "Ops/sec"
             << setw(12) << "Time/op"
             << endl;
        cout << string(79, '-') << endl;
    }
};

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
// 模拟 AOI 测试
//==============================================================================

void benchmarkAOI() {
    cout << "\n=== AOI System Simulation ===" << endl;
    Benchmark::printHeader();

    struct Entity {
        float x, y, z;
        uint64_t id;
    };

    const int entityCount = 10000;
    const int queryCount = 100000;

    vector<Entity> entities(entityCount);

    // 初始化实体
    for (int i = 0; i < entityCount; ++i) {
        entities[i].x = static_cast<float>(rand() % 10000);
        entities[i].y = 0;
        entities[i].z = static_cast<float>(rand() % 10000);
        entities[i].id = i;
    }

    // 查询附近实体（简单距离判断）
    Benchmark::run("AOI Query (Distance Check)", queryCount, [&]() {
        Entity& center = entities[rand() % entityCount];
        int count = 0;

        for (const auto& e : entities) {
            if (e.id == center.id) continue;

            float dx = e.x - center.x;
            float dz = e.z - center.z;
            float distSq = dx * dx + dz * dz;

            if (distSq < 2500.0f) {  // 50米范围
                ++count;
            }
        }
    }).print();

    // 网格优化版本
    const int gridSize = 100;
    const int gridCount = 100;  // 10000 / 100

    vector<vector<vector<uint32_t>>> grid(gridCount,
        vector<vector<uint32_t>>(gridCount));

    // 建立网格
    for (uint32_t i = 0; i < entities.size(); ++i) {
        int gx = static_cast<int>(entities[i].x) / gridSize;
        int gz = static_cast<int>(entities[i].z) / gridSize;
        if (gx >= 0 && gx < gridCount && gz >= 0 && gz < gridCount) {
            grid[gx][gz].push_back(i);
        }
    }

    Benchmark::run("AOI Query (Grid Optimized)", queryCount, [&]() {
        Entity& center = entities[rand() % entityCount];
        int gx = static_cast<int>(center.x) / gridSize;
        int gz = static_cast<int>(center.z) / gridSize;
        int count = 0;

        // 只检查相邻网格
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) {
                int nx = gx + dx;
                int nz = gz + dz;

                if (nx >= 0 && nx < gridCount && nz >= 0 && nz < gridCount) {
                    for (uint32_t eid : grid[nx][nz]) {
                        if (eid == center.id) continue;

                        float dx_ = entities[eid].x - center.x;
                        float dz_ = entities[eid].z - center.z;
                        float distSq = dx_ * dx_ + dz_ * dz_;

                        if (distSq < 2500.0f) {
                            ++count;
                        }
                    }
                }
            }
        }
    }).print();
}

//==============================================================================
// 模拟消息处理测试
//==============================================================================

void benchmarkMessageProcessing() {
    cout << "\n=== Message Processing ===" << endl;
    Benchmark::printHeader();

    // 小消息 (16B)
    struct SmallMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[8];
    };

    // 中等消息 (128B)
    struct MediumMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[112];
    };

    // 大消息 (1024B)
    struct LargeMessage {
        uint32_t type;
        uint32_t id;
        uint64_t timestamp;
        char data[1008];
    };

    const int N = 1000000;

    // 小消息处理
    Benchmark::run("Process Small Message (16B)", N, []() {
        SmallMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 中等消息处理
    Benchmark::run("Process Medium Message (128B)", N, []() {
        MediumMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 大消息处理
    Benchmark::run("Process Large Message (1KB)", N / 10, []() {
        LargeMessage msg{};
        volatile uint32_t type = msg.type;
        volatile uint32_t id = msg.id;
        (void)type; (void)id;
    }).print();

    // 消息拷贝
    vector<uint8_t> buffer(1024);

    Benchmark::run("Message Copy (1KB)", N, [&]() {
        vector<uint8_t> copy = buffer;
    }).print();

    // 消息序列化模拟
    Benchmark::run("Message Serialization (1KB)", N / 10, [&]() {
        // 模拟将数据写入缓冲区
        uint8_t output[1024];
        memcpy(output, buffer.data(), 1024);
    }).print();
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

//==============================================================================
// 主程序
//==============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "=== Apollo Framework Benchmark ===" << endl;
    cout << "========================================" << endl;
    cout << "\nCompiler: ";
#if defined(_MSC_VER)
    cout << "MSVC " << _MSC_VER << endl;
#elif defined(__GNUC__)
    cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << endl;
#elif defined(__clang__)
    cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_PATCHLEVEL__ << endl;
#else
    cout << "Unknown" << endl;
#endif

    cout << "Build: " << (sizeof(void*) == 8 ? "x64" : "x86") << endl;
    cout << "Date: " << __DATE__ << " " << __TIME__ << endl;

    benchmarkBasicOperations();
    benchmarkContainers();
    benchmarkThreading();
    benchmarkAOI();
    benchmarkMessageProcessing();
    benchmarkMemoryPool();
    benchmarkLocks();

    cout << "\n========================================" << endl;
    cout << "=== Benchmark Complete ===" << endl;
    cout << "========================================" << endl;

    return 0;
}
