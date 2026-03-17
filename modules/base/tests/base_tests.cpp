#include "apollo/base/id_pool.hpp"
#include "apollo/base/thread_pool.hpp"
#include "apollo/base/time.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

bool test_id_pool() {
    apollo::base::IdPool pool(100, 102);
    const auto id0 = pool.allocate();
    const auto id1 = pool.allocate();
    if (id0 != 100 || id1 != 101) {
        return false;
    }
    if (!pool.is_allocated(100) || !pool.is_valid(102)) {
        return false;
    }
    pool.release(100);
    return !pool.is_allocated(100) && pool.allocated_count() == 1;
}

bool test_time() {
    const auto now = apollo::base::Time::now();
    if (now <= 0) {
        return false;
    }
    const auto formatted = apollo::base::Time::format_now();
    if (formatted.size() < 19) {
        return false;
    }

    apollo::base::Timer timer;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return timer.elapsed_millis() >= 5.0;
}

bool test_thread_pool() {
    apollo::base::ThreadPool pool(2);
    std::atomic<int> counter{0};

    auto future = pool.submit([](int a, int b) { return a + b; }, 19, 23);
    if (future.get() != 42) {
        return false;
    }

    for (int i = 0; i < 8; ++i) {
        pool.enqueue([&counter]() { ++counter; });
    }

    pool.wait_for_all();
    return counter == 8 && pool.pending_task_count() == 0;
}

} // namespace

int main() {
    const bool ok =
        test_id_pool() &&
        test_time() &&
        test_thread_pool();

    if (!ok) {
        std::cerr << "apollo_base_tests failed" << std::endl;
        return 1;
    }

    std::cout << "apollo_base_tests passed" << std::endl;
    return 0;
}
