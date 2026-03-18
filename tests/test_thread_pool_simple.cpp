#include "apollo/base/thread_pool.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

using namespace apollo::base;

int main() {
    std::cout << "Testing ThreadPool..." << std::endl;
    
    ThreadPool pool(2);
    std::atomic<int> value{0};
    
    // Test enqueue
    pool.enqueue([&value]() {
        value.store(42);
    });
    
    pool.wait_for_all();
    
    if (value.load() == 42) {
        std::cout << "PASSED: enqueue test" << std::endl;
    } else {
        std::cout << "FAILED: enqueue test" << std::endl;
        return 1;
    }
    
    // Test submit with no args
    auto f1 = pool.submit([]() {
        return 123;
    });
    
    if (f1.get() == 123) {
        std::cout << "PASSED: submit no args" << std::endl;
    } else {
        std::cout << "FAILED: submit no args" << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
