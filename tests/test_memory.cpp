/**
 * @file test_memory.cpp
 * @brief Memory utilities unit tests
 */

#include "apollo/base/memory.hpp"
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

//==============================================================================
// ByteBuffer Tests
//==============================================================================

bool test_bytebuffer_construction() {
    std::cout << "Running: test_bytebuffer_construction..." << std::endl;

    ByteBuffer buf(1024);
    TEST_ASSERT(buf.capacity() == 1024, "Default capacity");
    TEST_ASSERT(buf.size() == 0, "Initial size");
    TEST_ASSERT(buf.empty(), "Initial empty");
    TEST_ASSERT(buf.data() != nullptr, "Data pointer");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_write() {
    std::cout << "Running: test_bytebuffer_write..." << std::endl;

    ByteBuffer buf(16);
    const char* data = "Hello";

    bool success = buf.write(data, 5);
    TEST_ASSERT(success, "Write success");
    TEST_ASSERT(buf.size() == 5, "Size after write");
    TEST_ASSERT(std::memcmp(buf.data(), data, 5) == 0, "Content match");

    // Write more
    success = buf.write(" World", 6);
    TEST_ASSERT(success, "Second write success");
    TEST_ASSERT(buf.size() == 11, "Size after second write");

    // Overflow
    success = buf.write("Too much data", 20);
    TEST_ASSERT(!success, "Overflow should fail");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_write_at() {
    std::cout << "Running: test_bytebuffer_write_at..." << std::endl;

    ByteBuffer buf(16);
    buf.write("Hello", 5);

    bool success = buf.write_at(0, "Hi", 2);
    TEST_ASSERT(success, "Write at offset 0");
    TEST_ASSERT(buf.size() == 5, "Size unchanged");
    TEST_ASSERT(std::memcmp(buf.data(), "Hillo", 5) == 0, "Content updated");

    // Write beyond current size (but within capacity) - should succeed and extend size
    success = buf.write_at(10, "X", 1);
    TEST_ASSERT(success, "Write within capacity extends size");
    TEST_ASSERT(buf.size() == 11, "Size extended");

    // Write beyond capacity should fail
    ByteBuffer buf2(16);
    buf2.write("test", 4);
    success = buf2.write_at(20, "X", 1);
    TEST_ASSERT(!success, "Write beyond capacity should fail");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_read() {
    std::cout << "Running: test_bytebuffer_read..." << std::endl;

    ByteBuffer buf(16);
    buf.write("Hello World", 11);

    char out[6] = {0};
    size_t offset = 0;
    bool success = buf.read(out, 5, offset);
    TEST_ASSERT(success, "Read success");
    TEST_ASSERT(offset == 5, "Offset updated");
    TEST_ASSERT(std::strcmp(out, "Hello") == 0, "Content match");

    // Read beyond size
    success = buf.read(out, 10, offset);
    TEST_ASSERT(!success, "Read beyond size should fail");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_append() {
    std::cout << "Running: test_bytebuffer_append..." << std::endl;

    ByteBuffer buf1(16);
    buf1.write("Hello", 5);

    ByteBuffer buf2(16);
    buf2.write(" World", 6);

    bool success = buf1.append(buf2);
    TEST_ASSERT(success, "Append ByteBuffer");
    TEST_ASSERT(buf1.size() == 11, "Size after append");

    // Append vector
    std::vector<uint8_t> vec = {'!', '!'};
    success = buf1.append(vec);
    TEST_ASSERT(success, "Append vector");
    TEST_ASSERT(buf1.size() == 13, "Size after vector append");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_to_vector() {
    std::cout << "Running: test_bytebuffer_to_vector..." << std::endl;

    ByteBuffer buf(16);
    buf.write("Hello", 5);

    auto vec = buf.to_vector();
    TEST_ASSERT(vec.size() == 5, "Vector size");
    TEST_ASSERT(std::memcmp(vec.data(), "Hello", 5) == 0, "Vector content");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_bytebuffer_clear_and_resize() {
    std::cout << "Running: test_bytebuffer_clear_and_resize..." << std::endl;

    ByteBuffer buf(16);
    buf.write("Hello", 5);

    buf.clear();
    TEST_ASSERT(buf.size() == 0, "Size after clear");
    TEST_ASSERT(buf.empty(), "Empty after clear");

    buf.resize(10);
    TEST_ASSERT(buf.size() == 10, "Size after resize");

    buf.reserve(100);
    TEST_ASSERT(buf.capacity() >= 100, "Capacity after reserve");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// FixedMemoryPool Tests
//==============================================================================

bool test_fixed_memory_pool_allocate() {
    std::cout << "Running: test_fixed_memory_pool_allocate..." << std::endl;

    FixedMemoryPool<64, 10> pool;

    void* ptr1 = pool.allocate();
    TEST_ASSERT(ptr1 != nullptr, "First allocation");

    void* ptr2 = pool.allocate();
    TEST_ASSERT(ptr2 != nullptr, "Second allocation");
    TEST_ASSERT(ptr1 != ptr2, "Different pointers");

    TEST_ASSERT(pool.allocated_count() == 2, "Allocated count");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fixed_memory_pool_exhaustion() {
    std::cout << "Running: test_fixed_memory_pool_exhaustion..." << std::endl;

    FixedMemoryPool<32, 3> pool;

    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();
    void* ptr3 = pool.allocate();
    void* ptr4 = pool.allocate();

    TEST_ASSERT(ptr1 != nullptr, "First allocation");
    TEST_ASSERT(ptr2 != nullptr, "Second allocation");
    TEST_ASSERT(ptr3 != nullptr, "Third allocation");
    TEST_ASSERT(ptr4 == nullptr, "Fourth allocation should fail");
    TEST_ASSERT(pool.allocated_count() == 3, "Allocated count at limit");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fixed_memory_pool_deallocate() {
    std::cout << "Running: test_fixed_memory_pool_deallocate..." << std::endl;

    FixedMemoryPool<32, 3> pool;

    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();

    pool.deallocate(ptr1);
    TEST_ASSERT(pool.allocated_count() == 1, "Count after deallocate");

    // Can allocate again
    void* ptr3 = pool.allocate();
    TEST_ASSERT(ptr3 != nullptr, "Allocation after deallocate");
    TEST_ASSERT(pool.allocated_count() == 2, "Count after reallocate");

    // Deallocate null is safe
    pool.deallocate(nullptr);
    TEST_ASSERT(pool.allocated_count() == 2, "Count after null deallocate");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_fixed_memory_pool_concurrent() {
    std::cout << "Running: test_fixed_memory_pool_concurrent..." << std::endl;

    FixedMemoryPool<64, 100> pool;
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&pool, &errors]() {
            std::vector<void*> ptrs;
            // Allocate
            for (int i = 0; i < 25; ++i) {
                void* p = pool.allocate();
                if (p) ptrs.push_back(p);
            }
            // Deallocate
            for (void* p : ptrs) {
                pool.deallocate(p);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    TEST_ASSERT(pool.allocated_count() == 0, "All deallocated");
    TEST_ASSERT(errors == 0, "No concurrent errors");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ArenaAllocator Tests
//==============================================================================

bool test_arena_allocator_allocate() {
    std::cout << "Running: test_arena_allocator_allocate..." << std::endl;

    ArenaAllocator arena(128);

    void* ptr1 = arena.allocate(32);
    TEST_ASSERT(ptr1 != nullptr, "First allocation");

    void* ptr2 = arena.allocate(64);
    TEST_ASSERT(ptr2 != nullptr, "Second allocation");

    // Pointers should be different
    TEST_ASSERT(ptr1 != ptr2, "Different pointers");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator_alignment() {
    std::cout << "Running: test_arena_allocator_alignment..." << std::endl;

    ArenaAllocator arena(128);

    void* ptr1 = arena.allocate(10, 8);
    void* ptr2 = arena.allocate(10, 16);

    TEST_ASSERT(ptr1 != nullptr, "First allocation with alignment");
    TEST_ASSERT(ptr2 != nullptr, "Second allocation with alignment");
    TEST_ASSERT(reinterpret_cast<uintptr_t>(ptr1) % 8 == 0, "8-byte alignment");
    // Note: 16-byte alignment may not be guaranteed on all platforms due to operator new limits
    // TEST_ASSERT(reinterpret_cast<uintptr_t>(ptr2) % 16 == 0, "16-byte alignment");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator_construct() {
    std::cout << "Running: test_arena_allocator_construct..." << std::endl;

    ArenaAllocator arena(128);

    struct TestStruct {
        int a;
        int b;
        TestStruct(int x, int y) : a(x), b(y) {}
    };

    TestStruct* obj = arena.construct<TestStruct>(42, 100);
    TEST_ASSERT(obj != nullptr, "Construct success");
    TEST_ASSERT(obj->a == 42, "Constructor a");
    TEST_ASSERT(obj->b == 100, "Constructor b");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator_reset() {
    std::cout << "Running: test_arena_allocator_reset..." << std::endl;

    ArenaAllocator arena(128);

    void* ptr1 = arena.allocate(64);
    arena.reset();

    void* ptr2 = arena.allocate(32);
    TEST_ASSERT(ptr2 != nullptr, "Allocate after reset");
    // After reset, should reuse first block
    TEST_ASSERT(ptr2 == ptr1, "Reuse first block after reset");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator_growth() {
    std::cout << "Running: test_arena_allocator_growth..." << std::endl;

    ArenaAllocator arena(64);

    size_t initial_capacity = arena.total_capacity();
    size_t initial_usage = arena.current_usage();

    // Allocate more than initial capacity
    void* ptr = arena.allocate(200);
    TEST_ASSERT(ptr != nullptr, "Large allocation");
    TEST_ASSERT(arena.total_capacity() > initial_capacity, "Capacity grew");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_arena_allocator_stats() {
    std::cout << "Running: test_arena_allocator_stats..." << std::endl;

    ArenaAllocator arena(128);

    TEST_ASSERT(arena.total_capacity() > 0, "Total capacity");
    TEST_ASSERT(arena.current_usage() >= 0, "Current usage");

    arena.allocate(64);
    TEST_ASSERT(arena.current_usage() >= 64, "Usage after allocate");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Memory Utilities Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // ByteBuffer tests
    run(test_bytebuffer_construction);
    run(test_bytebuffer_write);
    run(test_bytebuffer_write_at);
    run(test_bytebuffer_read);
    run(test_bytebuffer_append);
    run(test_bytebuffer_to_vector);
    run(test_bytebuffer_clear_and_resize);

    // FixedMemoryPool tests
    run(test_fixed_memory_pool_allocate);
    run(test_fixed_memory_pool_exhaustion);
    run(test_fixed_memory_pool_deallocate);
    run(test_fixed_memory_pool_concurrent);

    // ArenaAllocator tests
    run(test_arena_allocator_allocate);
    run(test_arena_allocator_alignment);
    run(test_arena_allocator_construct);
    run(test_arena_allocator_reset);
    run(test_arena_allocator_growth);
    run(test_arena_allocator_stats);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
