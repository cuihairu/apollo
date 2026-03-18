/**
 * @file test_thread_pool.cpp
 * @brief ThreadPool unit tests
 */

#include "apollo/base/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace apollo::base;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

bool test_thread_pool_construction() {
    std::cout << "Running: test_thread_pool_construction..." << std::endl;
    ThreadPool pool(4);
    TEST_ASSERT(pool.thread_count() == 4, "Thread count matches");
    TEST_ASSERT(!pool.is_stopped(), "Pool not stopped initially");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_submit_void_task() {
    std::cout << "Running: test_submit_void_task..." << std::endl;
    ThreadPool pool(2);
    std::atomic<int> value{0};
    auto future = pool.submit([&value]() {
        value.store(42);
    });
    future.wait();
    TEST_ASSERT(value.load() == 42, "Task executed");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_submit_with_return() {
    std::cout << "Running: test_submit_with_return..." << std::endl;
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 123; });
    TEST_ASSERT(future.get() == 123, "Return value correct");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_enqueue_task() {
    std::cout << "Running: test_enqueue_task..." << std::endl;
    ThreadPool pool(2);
    std::atomic<int> value{0};
    pool.enqueue([&value]() { value.store(99); });
    pool.wait_for_all();
    TEST_ASSERT(value.load() == 99, "Enqueued task executed");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_enqueue_multiple() {
    std::cout << "Running: test_enqueue_multiple..." << std::endl;
    ThreadPool pool(4);
    std::atomic<int> count{0};
    for (int i = 0; i < 20; ++i) {
        pool.enqueue([&count]() { count.fetch_add(1); });
    }
    pool.wait_for_all();
    TEST_ASSERT(count.load() == 20, "All enqueued tasks executed");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_wait_for_all() {
    std::cout << "Running: test_wait_for_all..." << std::endl;
    ThreadPool pool(4);
    std::atomic<int> completed{0};
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&completed]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            completed.fetch_add(1);
        });
    }
    pool.wait_for_all();
    TEST_ASSERT(completed.load() == 10, "All tasks completed");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_stop_waits_for_tasks() {
    std::cout << "Running: test_stop_waits_for_tasks..." << std::endl;
    ThreadPool pool(2);
    std::atomic<int> completed{0};
    std::atomic<bool> running{true};
    pool.enqueue([&]() {
        while (running.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        completed.fetch_add(1);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running.store(false);
    pool.stop();
    TEST_ASSERT(completed.load() == 1, "Task completed before stop");
    TEST_ASSERT(pool.is_stopped(), "Pool stopped");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_submit_after_stop() {
    std::cout << "Running: test_submit_after_stop..." << std::endl;
    ThreadPool pool(2);
    pool.stop();
    bool threw = false;
    try {
        pool.submit([]() {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Submit after stop throws");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_enqueue_after_stop() {
    std::cout << "Running: test_enqueue_after_stop..." << std::endl;
    ThreadPool pool(2);
    pool.stop();
    bool threw = false;
    try {
        pool.enqueue([]() {});
    } catch (const std::runtime_error&) {
        threw = true;
    }
    TEST_ASSERT(threw, "Enqueue after stop throws");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_many_small_tasks() {
    std::cout << "Running: test_many_small_tasks..." << std::endl;
    ThreadPool pool(4);
    std::atomic<int> sum{0};
    for (int i = 0; i < 1000; ++i) {
        pool.enqueue([&sum]() { sum.fetch_add(1); });
    }
    pool.wait_for_all();
    TEST_ASSERT(sum.load() == 1000, "All 1000 tasks executed");
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    std::cout << "=== Apollo ThreadPool Test Suite ===" << std::endl;
    int total = 0;
    int passed = 0;
    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) passed++;
    };
    run(test_thread_pool_construction);
    run(test_submit_void_task);
    run(test_submit_with_return);
    run(test_enqueue_task);
    run(test_enqueue_multiple);
    run(test_wait_for_all);
    run(test_stop_waits_for_tasks);
    run(test_submit_after_stop);
    run(test_enqueue_after_stop);
    run(test_many_small_tasks);
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;
    return (total == passed) ? 0 : 1;
}
