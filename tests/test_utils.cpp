/**
 * @file test_utils.cpp
 * @brief Apollo 工具类综合测试
 */

#include "apollo/utils/thread_pool.h"
#include "apollo/utils/lock.h"
#include "apollo/utils/loop_buffer.h"
#include "apollo/utils/data_queue.h"
#include "apollo/utils/memory_pool.h"
#include "apollo/utils/time.h"
#include "apollo/utils/random.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <thread>

using namespace apollo::utils;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// ThreadPool 测试
//==============================================================================

bool test_threadpool_basic() {
    std::cout << "Running: test_threadpool_basic..." << std::endl;

    ThreadPool pool(4);

    // 提交任务
    auto result1 = pool.submit([]() { return 42; });
    TEST_ASSERT(result1.get() == 42, "Task result");

    // 多任务
    std::atomic<int> count{0};
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&count]() { ++count; });
    }

    pool.waitForAll();
    TEST_ASSERT(count == 10, "Task count");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_threadpool_submit() {
    std::cout << "Running: test_threadpool_submit..." << std::endl;

    ThreadPool pool(2);

    auto r1 = pool.submit([](int a, int b) { return a + b; }, 10, 32);
    TEST_ASSERT(r1.get() == 42, "Addition task");

    auto r2 = pool.submit([]() { return std::string("hello"); });
    TEST_ASSERT(r2.get() == "hello", "String task");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Lock 测试
//==============================================================================

bool test_spinlock() {
    std::cout << "Running: test_spinlock..." << std::endl;

    SpinLock lock;
    std::atomic<int> counter{0};

    lock.lock();
    TEST_ASSERT(lock.tryLock() == false, "tryLock fails when locked");
    lock.unlock();
    TEST_ASSERT(lock.tryLock() == true, "tryLock succeeds when unlocked");
    lock.unlock();

    // 并发测试
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) {
                ScopedSpinLock guard(lock);
                ++counter;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT(counter == 10000, "Counter value after concurrent access");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_atomic_flag() {
    std::cout << "Running: test_atomic_flag..." << std::endl;

    AtomicFlag flag;
    TEST_ASSERT(!flag.test(), "Initially not set");

    flag.set();
    TEST_ASSERT(flag.test(), "Set flag");
    TEST_ASSERT(flag.testAndSet() == true, "testAndSet true");
    flag.clear();
    TEST_ASSERT(!flag.test(), "Cleared flag");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// LoopBuffer 测试
//==============================================================================

bool test_loopbuffer_basic() {
    std::cout << "Running: test_loopbuffer_basic..." << std::endl;

    LoopBuffer<int> buffer(10);

    TEST_ASSERT(buffer.isEmpty(), "Initially empty");
    TEST_ASSERT(!buffer.isFull(), "Not full");
    TEST_ASSERT(buffer.capacity() == 10, "Capacity");

    // 写入
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT(buffer.push(i), "Push item");
    }

    TEST_ASSERT(buffer.availableRead() == 5, "5 items available");
    TEST_ASSERT(buffer.availableWrite() == 5, "5 slots available");

    // 读取
    int value;
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT(buffer.pop(value), "Pop item");
        TEST_ASSERT(value == i, "Correct value");
    }

    TEST_ASSERT(buffer.isEmpty(), "Empty after reading all");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_loopbuffer_wraparound() {
    std::cout << "Running: test_loopbuffer_wraparound..." << std::endl;

    LoopBuffer<int> buffer(4);

    // 填满
    buffer.push(1);
    buffer.push(2);
    buffer.push(3);
    buffer.push(4);

    TEST_ASSERT(buffer.isFull(), "Full");

    // 读取部分
    int value;
    buffer.pop(value);
    buffer.pop(value);

    // 再写入（应该环绕）
    buffer.push(5);
    buffer.push(6);

    // 验证内容
    buffer.pop(value);
    TEST_ASSERT(value == 3, "Third value");
    buffer.pop(value);
    TEST_ASSERT(value == 4, "Fourth value");
    buffer.pop(value);
    TEST_ASSERT(value == 5, "Fifth value");
    buffer.pop(value);
    TEST_ASSERT(value == 6, "Sixth value");

    TEST_ASSERT(buffer.isEmpty(), "Empty after all");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_byte_loop_buffer() {
    std::cout << "Running: test_byte_loop_buffer..." << std::endl;

    ByteLoopBuffer buffer(16);

    const char* data = "Hello, World!";
    size_t len = std::strlen(data);

    size_t written = buffer.write(data, len);
    TEST_ASSERT(written == len, "Write length");

    char out[32];
    size_t read = buffer.read(out, sizeof(out));
    TEST_ASSERT(read == len, "Read length");
    out[read] = '\0';
    TEST_ASSERT(std::strcmp(out, data) == 0, "Read content");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// DataQueue 测试
//==============================================================================

bool test_spsc_queue() {
    std::cout << "Running: test_spsc_queue..." << std::endl;

    SPSCQueue<int, 8> queue;

    TEST_ASSERT(queue.isEmpty(), "Initially empty");
    TEST_ASSERT(!queue.isFull(), "Not full");

    for (int i = 0; i < 7; ++i) {
        TEST_ASSERT(queue.push(i), "Push item");
    }

    TEST_ASSERT(queue.size() == 7, "Size");

    int value;
    for (int i = 0; i < 7; ++i) {
        TEST_ASSERT(queue.pop(value), "Pop item");
        TEST_ASSERT(value == i, "Correct value");
    }

    TEST_ASSERT(queue.isEmpty(), "Empty after all");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_lock_free_queue() {
    std::cout << "Running: test_lock_free_queue..." << std::endl;

    LockFreeQueue<int> queue;

    queue.enqueue(new int(1));
    queue.enqueue(new int(2));
    queue.enqueue(new int(3));

    int* p;
    p = queue.dequeue();
    TEST_ASSERT(p != nullptr && *p == 1, "First item");
    delete p;

    p = queue.dequeue();
    TEST_ASSERT(p != nullptr && *p == 2, "Second item");
    delete p;

    p = queue.dequeue();
    TEST_ASSERT(p != nullptr && *p == 3, "Third item");
    delete p;

    p = queue.dequeue();
    TEST_ASSERT(p == nullptr, "Empty queue");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// MemoryPool 测试
//==============================================================================

bool test_memory_pool() {
    std::cout << "Running: test_memory_pool..." << std::endl;

    MemoryPool pool(64, 4);

    TEST_ASSERT(pool.getFreeCount() == 4, "Initial free count");
    TEST_ASSERT(pool.getBlockSize() == 64, "Block size");

    void* p1 = pool.allocate();
    void* p2 = pool.allocate();
    void* p3 = pool.allocate();

    TEST_ASSERT(p1 != nullptr, "Allocate p1");
    TEST_ASSERT(p2 != nullptr, "Allocate p2");
    TEST_ASSERT(p3 != nullptr, "Allocate p3");
    TEST_ASSERT(pool.getAllocatedCount() == 3, "Allocated count");

    pool.deallocate(p2);
    TEST_ASSERT(pool.getFreeCount() == 1, "Free count after deallocate");

    void* p4 = pool.allocate();
    TEST_ASSERT(p4 == p2, "Reallocated block is same");

    pool.deallocate(p1);
    pool.deallocate(p3);
    pool.deallocate(p4);

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator() {
    std::cout << "Running: test_arena_allocator..." << std::endl;

    ArenaAllocator arena(1024);

    void* p1 = arena.allocate(100);
    void* p2 = arena.allocate(200);
    void* p3 = arena.allocate(300);

    TEST_ASSERT(p1 != nullptr, "Allocate p1");
    TEST_ASSERT(p2 != nullptr, "Allocate p2");
    TEST_ASSERT(p3 != nullptr, "Allocate p3");

    size_t used = arena.getUsedSize();
    TEST_ASSERT(used >= 600, "Used size at least 600");

    arena.reset();
    TEST_ASSERT(arena.getUsedSize() == 0, "Used size after reset");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Time 测试
//==============================================================================

bool test_time() {
    std::cout << "Running: test_time..." << std::endl;

    Time::Timestamp now = Time::now();
    TEST_ASSERT(now > 0, "Timestamp positive");

    std::string timeStr = Time::formatNow();
    TEST_ASSERT(!timeStr.empty(), "Format now not empty");
    TEST_ASSERT(timeStr.length() >= 19, "Time string length");

    std::string dateStr = Time::dateStr(Time::unixTime());
    TEST_ASSERT(dateStr.length() == 10, "Date string length");

    Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double elapsed = timer.elapsedMillis();
    TEST_ASSERT(elapsed >= 10.0, "Elapsed time at least 10ms");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fps_calculator() {
    std::cout << "Running: test_fps_calculator..." << std::endl;

    FpsCalculator fps(100);

    fps.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    fps.update();

    // FPS 应该大于 0
    TEST_ASSERT(fps.getFps() >= 0.0f, "FPS non-negative");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Random 测试
//==============================================================================

bool test_random_basic() {
    std::cout << "Running: test_random_basic..." << std::endl;

    Random rng(42);

    uint64_t r1 = rng.next();
    uint64_t r2 = rng.next();
    TEST_ASSERT(r1 != r2, "Different random values");

    // 范围测试
    uint32_t val = rng.nextUint32();
    TEST_ASSERT(val >= 0 && val <= UINT32_MAX, "Uint32 range");

    // 浮点数
    double d = rng.nextDouble();
    TEST_ASSERT(d >= 0.0 && d < 1.0, "Double range");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_random_range() {
    std::cout << "Running: test_random_range..." << std::endl;

    Random rng(12345);

    for (int i = 0; i < 100; ++i) {
        uint64_t val = rng.next(10, 20);
        TEST_ASSERT(val >= 10 && val <= 20, "Range check");
    }

    for (int i = 0; i < 100; ++i) {
        double val = rng.nextDouble(5.0, 10.0);
        TEST_ASSERT(val >= 5.0 && val < 10.0, "Double range check");
    }

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_random_static() {
    std::cout << "Running: test_random_static..." << std::endl;

    int32_t val = Random::randInt(1, 100);
    TEST_ASSERT(val >= 1 && val <= 100, "Static randInt range");

    double d = Random::randDouble();
    TEST_ASSERT(d >= 0.0 && d < 1.0, "Static randDouble range");

    bool b = Random::randBool();
    // 随机布尔值，不检查具体值

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_random_distribution() {
    std::cout << "Running: test_random_distribution..." << std::endl;

    Random rng;
    NormalDistribution normal(0.0, 1.0);

    double sum = 0.0;
    const int samples = 1000;
    for (int i = 0; i < samples; ++i) {
        sum += normal(rng);
    }

    double mean = sum / samples;
    // 正态分布的均值应该接近 0
    TEST_ASSERT(mean > -1.0 && mean < 1.0, "Normal distribution mean near 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_shuffle() {
    std::cout << "Running: test_shuffle..." << std::endl;

    std::vector<int> v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Random rng(42);
    shuffle(v.begin(), v.end(), [&](size_t max) { return rng.next(max); });

    // 检查元素数量不变
    int sum = 0;
    for (int val : v) {
        sum += val;
    }
    TEST_ASSERT(sum == 55, "Sum unchanged");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo Utils Tests ===" << std::endl;
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

    // ThreadPool 测试
    std::cout << "--- ThreadPool Tests ---" << std::endl;
    run("test_threadpool_basic", test_threadpool_basic);
    run("test_threadpool_submit", test_threadpool_submit);

    // Lock 测试
    std::cout << std::endl << "--- Lock Tests ---" << std::endl;
    run("test_spinlock", test_spinlock);
    run("test_atomic_flag", test_atomic_flag);

    // LoopBuffer 测试
    std::cout << std::endl << "--- LoopBuffer Tests ---" << std::endl;
    run("test_loopbuffer_basic", test_loopbuffer_basic);
    run("test_loopbuffer_wraparound", test_loopbuffer_wraparound);
    run("test_byte_loop_buffer", test_byte_loop_buffer);

    // DataQueue 测试
    std::cout << std::endl << "--- DataQueue Tests ---" << std::endl;
    run("test_spsc_queue", test_spsc_queue);
    run("test_lock_free_queue", test_lock_free_queue);

    // MemoryPool 测试
    std::cout << std::endl << "--- MemoryPool Tests ---" << std::endl;
    run("test_memory_pool", test_memory_pool);
    run("test_arena_allocator", test_arena_allocator);

    // Time 测试
    std::cout << std::endl << "--- Time Tests ---" << std::endl;
    run("test_time", test_time);
    run("test_fps_calculator", test_fps_calculator);

    // Random 测试
    std::cout << std::endl << "--- Random Tests ---" << std::endl;
    run("test_random_basic", test_random_basic);
    run("test_random_range", test_random_range);
    run("test_random_static", test_random_static);
    run("test_random_distribution", test_random_distribution);
    run("test_shuffle", test_shuffle);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}
