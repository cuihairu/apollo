/**
 * @file test_id_pool.cpp
 * @brief IdPool 和 ObjectPool 测试
 */

#include "apollo/utils/id_pool.h"
#include <iostream>
#include <cassert>
#include <string>

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
// IdPool 测试用例
//==============================================================================

/**
 * @brief 测试 IdPool 基本分配和释放
 */
bool test_idpool_basic() {
    std::cout << "Running: test_idpool_basic..." << std::endl;

    IdPool pool(100, 199);

    TEST_ASSERT(pool.capacity() == 100, "Capacity correct");
    TEST_ASSERT(pool.isEmpty(), "Initially empty");
    TEST_ASSERT(!pool.isFull(), "Not full initially");

    // 分配10个ID
    for (uint32_t i = 0; i < 10; ++i) {
        uint32_t id = pool.allocate();
        TEST_ASSERT(id == 100 + i, "Sequential allocation");
    }

    TEST_ASSERT(pool.getAllocatedCount() == 10, "Allocated count");
    TEST_ASSERT(pool.getUsage() == 10, "Usage percentage");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 释放和重用
 */
bool test_idpool_release() {
    std::cout << "Running: test_idpool_release..." << std::endl;

    IdPool pool(10);

    uint32_t id1 = pool.allocate();
    uint32_t id2 = pool.allocate();
    uint32_t id3 = pool.allocate();

    TEST_ASSERT(pool.getAllocatedCount() == 3, "Three allocated");

    pool.release(id2);
    TEST_ASSERT(pool.getAllocatedCount() == 2, "Two after release");
    TEST_ASSERT(!pool.isAllocated(id2), "id2 not allocated");

    uint32_t id4 = pool.allocate();
    TEST_ASSERT(id4 == id2, "Reused released id");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 边界条件
 */
bool test_idpool_boundaries() {
    std::cout << "Running: test_idpool_boundaries..." << std::endl;

    IdPool pool(5, 9);  // 容量为5

    TEST_ASSERT(pool.capacity() == 5, "Capacity");

    // 分配所有ID
    for (uint32_t i = 0; i < 5; ++i) {
        uint32_t id = pool.allocate();
        TEST_ASSERT(id != UINT32_MAX, "Allocation successful");
    }

    TEST_ASSERT(pool.isFull(), "Pool is full");

    // 池满时分配应失败
    uint32_t id = pool.allocate();
    TEST_ASSERT(id == UINT32_MAX, "Allocation fails when full");

    // 释放一个后再分配
    pool.release(5);
    id = pool.allocate();
    TEST_ASSERT(id != UINT32_MAX, "Allocation after release");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 验证函数
 */
bool test_idpool_validation() {
    std::cout << "Running: test_idpool_validation..." << std::endl;

    IdPool pool(100, 199);

    uint32_t id = pool.allocate();

    TEST_ASSERT(pool.isAllocated(id), "Allocated ID is valid");
    TEST_ASSERT(pool.isValid(id), "isValid returns true");
    TEST_ASSERT(!pool.isAllocated(999), "Out of range ID not allocated");
    TEST_ASSERT(!pool.isValid(999), "Out of range ID not valid");
    TEST_ASSERT(!pool.isAllocated(150), "Unallocated ID");
    TEST_ASSERT(!pool.isValid(150), "Unallocated ID not valid");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 预分配
 */
bool test_idpool_reserve() {
    std::cout << "Running: test_idpool_reserve..." << std::endl;

    IdPool pool(100, 199);

    TEST_ASSERT(pool.reserve(150), "Reserve ID");
    TEST_ASSERT(pool.isAllocated(150), "Reserved ID is allocated");
    TEST_ASSERT(!pool.reserve(150), "Cannot reserve already allocated");

    uint32_t id = pool.allocate();
    TEST_ASSERT(id != 150, "Allocated ID is not the reserved one");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 重置
 */
bool test_idpool_reset() {
    std::cout << "Running: test_idpool_reset..." << std::endl;

    IdPool pool(100, 199);

    for (int i = 0; i < 50; ++i) {
        pool.allocate();
    }

    TEST_ASSERT(pool.getAllocatedCount() == 50, "50 allocated");

    pool.reset();

    TEST_ASSERT(pool.isEmpty(), "Empty after reset");
    TEST_ASSERT(!pool.isFull(), "Not full after reset");
    TEST_ASSERT(pool.getAllocatedCount() == 0, "Zero allocated");

    // 可以重新分配
    uint32_t id = pool.allocate();
    TEST_ASSERT(id != UINT32_MAX, "Can allocate after reset");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 大小构造函数
 */
bool test_idpool_size_constructor() {
    std::cout << "Running: test_idpool_size_constructor..." << std::endl;

    IdPool pool(100);  // 0-99

    TEST_ASSERT(pool.capacity() == 100, "Capacity");
    TEST_ASSERT(pool.allocate() == 0, "First ID is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 IdPool 空池
 */
bool test_idpool_empty() {
    std::cout << "Running: test_idpool_empty..." << std::endl;

    IdPool pool(0);

    TEST_ASSERT(pool.capacity() == 1, "Zero size becomes 1");
    TEST_ASSERT(pool.isEmpty(), "Initially empty");

    uint32_t id = pool.allocate();
    TEST_ASSERT(id == 0, "Can allocate from zero-initialized pool");
    TEST_ASSERT(pool.isFull(), "Full after one allocation");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ObjectPool 测试用例
//==============================================================================

/**
 * @brief 测试 ObjectPool 基本功能
 */
bool test_objectpool_basic() {
    std::cout << "Running: test_objectpool_basic..." << std::endl;

    ObjectPool<std::string> pool(10);

    TEST_ASSERT(pool.capacity() == 10, "Capacity");
    TEST_ASSERT(pool.isEmpty(), "Initially empty");
    TEST_ASSERT(!pool.isFull(), "Not full");

    uint32_t id1 = pool.allocate(std::string("Hello"));
    uint32_t id2 = pool.allocate(std::string("World"));

    TEST_ASSERT(id1 == 0, "First ID is 0");
    TEST_ASSERT(id2 == 1, "Second ID is 1");
    TEST_ASSERT(pool.getAllocatedCount() == 2, "Two allocated");

    std::string* obj1 = pool.get(id1);
    TEST_ASSERT(obj1 != nullptr, "Get allocated object");
    TEST_ASSERT(*obj1 == "Hello", "Object value correct");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 ObjectPool 释放
 */
bool test_objectpool_release() {
    std::cout << "Running: test_objectpool_release..." << std::endl;

    ObjectPool<int> pool(5);

    uint32_t id1 = pool.allocate(100);
    uint32_t id2 = pool.allocate(200);
    uint32_t id3 = pool.allocate(300);

    auto obj = pool.release(id2);
    TEST_ASSERT(obj != nullptr, "Release returns object");
    TEST_ASSERT(*obj == 200, "Released object value");
    TEST_ASSERT(!pool.isAllocated(id2), "Released ID not allocated");

    // 重新分配
    uint32_t id4 = pool.allocate(400);
    TEST_ASSERT(id4 == id2, "Reused released slot");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 ObjectPool 满和空
 */
bool test_objectpool_full_empty() {
    std::cout << "Running: test_objectpool_full_empty..." << std::endl;

    ObjectPool<int> pool(3);

    TEST_ASSERT(pool.isEmpty(), "Initially empty");

    pool.allocate(1);
    pool.allocate(2);
    pool.allocate(3);

    TEST_ASSERT(pool.isFull(), "Full after 3 allocations");
    TEST_ASSERT(pool.getAllocatedCount() == 3, "3 allocated");

    uint32_t id = pool.allocate(4);
    TEST_ASSERT(id == UINT32_MAX, "Cannot allocate when full");

    pool.release(0);
    TEST_ASSERT(!pool.isFull(), "Not full after release");
    TEST_ASSERT(!pool.isEmpty(), "Not empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 ObjectPool 获取未分配对象
 */
bool test_objectpool_get_unallocated() {
    std::cout << "Running: test_objectpool_get_unallocated..." << std::endl;

    ObjectPool<int> pool(10);

    int* obj = pool.get(5);
    TEST_ASSERT(obj == nullptr, "Get unallocated returns nullptr");

    uint32_t id = pool.allocate(42);
    obj = pool.get(id);
    TEST_ASSERT(obj != nullptr, "Get allocated returns object");
    TEST_ASSERT(*obj == 42, "Object value correct");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// 测试运行器
//==============================================================================

int main() {
    std::cout << "====================================" << std::endl;
    std::cout << "=== Apollo IdPool Tests ===" << std::endl;
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

    // IdPool 测试
    std::cout << "--- IdPool Tests ---" << std::endl;
    run("test_idpool_basic", test_idpool_basic);
    run("test_idpool_release", test_idpool_release);
    run("test_idpool_boundaries", test_idpool_boundaries);
    run("test_idpool_validation", test_idpool_validation);
    run("test_idpool_reserve", test_idpool_reserve);
    run("test_idpool_reset", test_idpool_reset);
    run("test_idpool_size_constructor", test_idpool_size_constructor);
    run("test_idpool_empty", test_idpool_empty);

    // ObjectPool 测试
    std::cout << std::endl << "--- ObjectPool Tests ---" << std::endl;
    run("test_objectpool_basic", test_objectpool_basic);
    run("test_objectpool_release", test_objectpool_release);
    run("test_objectpool_full_empty", test_objectpool_full_empty);
    run("test_objectpool_get_unallocated", test_objectpool_get_unallocated);

    std::cout << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    std::cout << "====================================" << std::endl;

    return (passed == total) ? 0 : 1;
}
