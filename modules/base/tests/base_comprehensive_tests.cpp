/**
 * @file base_comprehensive_tests.cpp
 * @brief Comprehensive test suite for apollo::base module
 *
 * Coverage targets:
 * - IdPool: 90%+
 * - String: 95%+
 * - Memory (ByteBuffer, FixedMemoryPool, ArenaAllocator): 90%+
 * - ObjectPool: 85%+
 */

#include "apollo/base/id_pool.hpp"
#include "apollo/base/string.hpp"
#include "apollo/base/memory.hpp"
#include "apollo/base/thread_pool.hpp"
#include "apollo/base/time.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cassert>

// ============================================================================
// Test Framework (Minimal)
// ============================================================================

#define TEST(name) bool test_##name()
#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "FAILED: " #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))
#define ASSERT_STRNE(a, b) ASSERT_TRUE(std::string(a) != std::string(b))

// ============================================================================
// IdPool Tests
// ============================================================================

TEST(id_pool_default_constructor) {
    apollo::base::IdPool pool;
    ASSERT_EQ(pool.capacity(), 0);
    ASSERT_EQ(pool.allocated_count(), 0);
    ASSERT_TRUE(pool.is_empty());
    ASSERT_TRUE(pool.is_full());
    return true;
}

TEST(id_pool_size_constructor) {
    apollo::base::IdPool pool(100);
    ASSERT_EQ(pool.capacity(), 100);
    ASSERT_EQ(pool.allocated_count(), 0);
    ASSERT_TRUE(pool.is_empty());
    ASSERT_FALSE(pool.is_full());
    return true;
}

TEST(id_pool_range_constructor) {
    apollo::base::IdPool pool(100, 199);
    ASSERT_EQ(pool.capacity(), 100);
    ASSERT_EQ(pool.allocated_count(), 0);
    ASSERT_TRUE(pool.is_empty());

    // First ID should be min_id
    uint32_t id = pool.allocate();
    ASSERT_EQ(id, 100);
    ASSERT_FALSE(pool.is_valid(99));   // Below min - invalid
    ASSERT_FALSE(pool.is_valid(100));  // Allocated - not available
    ASSERT_TRUE(pool.is_valid(101));   // Within range and not allocated
    ASSERT_FALSE(pool.is_valid(200));  // Above max - invalid

    pool.release(id);
    ASSERT_FALSE(pool.is_allocated(100));
    return true;
}

TEST(id_pool_allocate_and_release) {
    apollo::base::IdPool pool(100, 199);

    // Allocate all IDs
    for (uint32_t i = 0; i < 100; ++i) {
        uint32_t id = pool.allocate();
        ASSERT_EQ(id, 100 + i);
        ASSERT_TRUE(pool.is_allocated(id));
    }

    ASSERT_TRUE(pool.is_full());
    ASSERT_EQ(pool.allocated_count(), 100);
    ASSERT_EQ(pool.usage_percent(), 100);

    // Release and reallocate
    pool.release(150);
    ASSERT_FALSE(pool.is_allocated(150));
    ASSERT_EQ(pool.allocated_count(), 99);

    uint32_t id = pool.allocate();
    ASSERT_EQ(id, 150);  // Should reuse released ID
    ASSERT_TRUE(pool.is_allocated(150));

    return true;
}

TEST(id_pool_reserve) {
    apollo::base::IdPool pool(100, 199);

    ASSERT_TRUE(pool.reserve(150));
    ASSERT_TRUE(pool.is_allocated(150));
    ASSERT_EQ(pool.allocated_count(), 1);

    // Reserve already allocated ID
    ASSERT_FALSE(pool.reserve(150));

    // Reserve out of range ID
    ASSERT_FALSE(pool.reserve(99));
    ASSERT_FALSE(pool.reserve(200));

    return true;
}

TEST(id_pool_reset) {
    apollo::base::IdPool pool(100, 199);

    pool.allocate();
    pool.allocate();
    ASSERT_EQ(pool.allocated_count(), 2);

    pool.reset();
    ASSERT_EQ(pool.allocated_count(), 0);
    ASSERT_TRUE(pool.is_empty());

    // After reset, should be able to allocate from min_id again
    uint32_t id = pool.allocate();
    ASSERT_EQ(id, 100);

    return true;
}

TEST(id_pool_stress) {
    apollo::base::IdPool pool(1000);
    std::vector<uint32_t> allocated;

    // Allocate all
    for (uint32_t i = 0; i < 1000; ++i) {
        uint32_t id = pool.allocate();
        allocated.push_back(id);
    }
    ASSERT_TRUE(pool.is_full());

    // Release every other
    for (size_t i = 0; i < allocated.size(); i += 2) {
        pool.release(allocated[i]);
    }
    ASSERT_EQ(pool.allocated_count(), 500);

    // Allocate again
    for (uint32_t i = 0; i < 500; ++i) {
        uint32_t id = pool.allocate();
        ASSERT_TRUE(pool.is_allocated(id));  // Check if successfully allocated
    }
    ASSERT_TRUE(pool.is_full());

    return true;
}

// ============================================================================
// ObjectPool Tests
// ============================================================================

TEST(object_pool_basic) {
    apollo::base::ObjectPool<std::string> pool(10);

    ASSERT_EQ(pool.capacity(), 10);
    ASSERT_EQ(pool.allocated_count(), 0);

    uint32_t id1 = pool.allocate("hello");
    uint32_t id2 = pool.allocate("world");

    ASSERT_NE(id1, UINT32_MAX);
    ASSERT_NE(id2, UINT32_MAX);
    ASSERT_EQ(pool.allocated_count(), 2);

    auto* str1 = pool.get(id1);
    auto* str2 = pool.get(id2);

    ASSERT_TRUE(str1 != nullptr);
    ASSERT_TRUE(str2 != nullptr);
    ASSERT_EQ(*str1, "hello");
    ASSERT_EQ(*str2, "world");

    return true;
}

TEST(object_pool_release) {
    apollo::base::ObjectPool<int> pool(5);

    uint32_t id = pool.allocate(42);
    ASSERT_TRUE(pool.is_allocated(id));
    ASSERT_EQ(*pool.get(id), 42);

    auto released = pool.release(id);
    ASSERT_TRUE(released != nullptr);
    ASSERT_EQ(*released, 42);
    ASSERT_FALSE(pool.is_allocated(id));

    // Allocate again, should reuse slot
    uint32_t id2 = pool.allocate(99);
    ASSERT_EQ(id, id2);
    ASSERT_EQ(*pool.get(id2), 99);

    return true;
}

TEST(object_pool_full) {
    apollo::base::ObjectPool<int> pool(2);

    ASSERT_NE(pool.allocate(1), UINT32_MAX);
    ASSERT_NE(pool.allocate(2), UINT32_MAX);
    ASSERT_EQ(pool.allocate(3), UINT32_MAX);  // Pool full

    ASSERT_TRUE(pool.allocated_count() == pool.capacity());
    ASSERT_EQ(pool.allocated_count(), 2);

    return true;
}

TEST(object_pool_invalid_access) {
    apollo::base::ObjectPool<int> pool(10);

    ASSERT_EQ(pool.get(999), nullptr);     // Out of range
    ASSERT_EQ(pool.get(UINT32_MAX), nullptr);  // Invalid

    ASSERT_FALSE(pool.is_allocated(999));

    auto released = pool.release(999);
    ASSERT_TRUE(released == nullptr);

    return true;
}

// ============================================================================
// String Tests - Trim Functions
// ============================================================================

TEST(string_trim) {
    using apollo::base::String;

    ASSERT_EQ(String::trim("  hello  "), "hello");
    ASSERT_EQ(String::trim("\t\nworld\n\t"), "world");
    ASSERT_EQ(String::trim("nospace"), "nospace");
    ASSERT_EQ(String::trim("   "), "");
    ASSERT_EQ(String::trim(""), "");

    // In-place trim
    std::string str = "  test  ";
    String::trim_in_place(str);
    ASSERT_EQ(str, "test");

    return true;
}

TEST(string_ltrim) {
    using apollo::base::String;

    ASSERT_EQ(String::ltrim("  hello  "), "hello  ");
    ASSERT_EQ(String::ltrim("\tworld"), "world");
    ASSERT_EQ(String::ltrim("nospace"), "nospace");
    ASSERT_EQ(String::ltrim("   "), "");

    std::string str = "  test";
    String::ltrim_in_place(str);
    ASSERT_EQ(str, "test");

    return true;
}

TEST(string_rtrim) {
    using apollo::base::String;

    ASSERT_EQ(String::rtrim("  hello  "), "  hello");
    ASSERT_EQ(String::rtrim("world\t"), "world");
    ASSERT_EQ(String::rtrim("nospace"), "nospace");
    ASSERT_EQ(String::rtrim("   "), "");

    std::string str = "test  ";
    String::rtrim_in_place(str);
    ASSERT_EQ(str, "test");

    return true;
}

// ============================================================================
// String Tests - Case Conversion
// ============================================================================

TEST(string_case_conversion) {
    using apollo::base::String;

    ASSERT_EQ(String::to_lower("HELLO World"), "hello world");
    ASSERT_EQ(String::to_lower("123ABC"), "123abc");
    ASSERT_EQ(String::to_lower(""), "");

    ASSERT_EQ(String::to_upper("hello World"), "HELLO WORLD");
    ASSERT_EQ(String::to_upper("abc123"), "ABC123");
    ASSERT_EQ(String::to_upper(""), "");

    return true;
}

// ============================================================================
// String Tests - Comparison
// ============================================================================

TEST(string_iequals) {
    using apollo::base::String;

    ASSERT_TRUE(String::iequals("hello", "HELLO"));
    ASSERT_TRUE(String::iequals("Hello World", "HELLO world"));
    ASSERT_FALSE(String::iequals("hello", "world"));
    ASSERT_FALSE(String::iequals("hello", "helloo"));
    ASSERT_TRUE(String::iequals("", ""));

    return true;
}

TEST(string_starts_ends_with) {
    using apollo::base::String;

    ASSERT_TRUE(String::starts_with("hello world", "hello"));
    ASSERT_TRUE(String::starts_with("hello", ""));
    ASSERT_FALSE(String::starts_with("hello world", "world"));
    ASSERT_FALSE(String::starts_with("hello", "hello world"));

    ASSERT_TRUE(String::ends_with("hello world", "world"));
    ASSERT_TRUE(String::ends_with("hello", ""));
    ASSERT_FALSE(String::ends_with("hello world", "hello"));
    ASSERT_FALSE(String::ends_with("hello", "hello world"));

    // Case-insensitive versions
    ASSERT_TRUE(String::istarts_with("Hello World", "hello"));
    ASSERT_TRUE(String::istarts_with("HELLO", ""));
    ASSERT_FALSE(String::istarts_with("hello", "world"));

    ASSERT_TRUE(String::iends_with("Hello World", "WORLD"));
    ASSERT_TRUE(String::iends_with("HELLO", ""));
    ASSERT_TRUE(String::iends_with("hello", "HELLO"));  // Case-insensitive

    return true;
}

// ============================================================================
// String Tests - Split and Join
// ============================================================================

TEST(string_split_char) {
    using apollo::base::String;

    auto parts = String::split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3);
    ASSERT_EQ(parts[0], "a");
    ASSERT_EQ(parts[1], "b");
    ASSERT_EQ(parts[2], "c");

    // Empty string
    parts = String::split("", ',');
    ASSERT_EQ(parts.size(), 1);
    ASSERT_EQ(parts[0], "");

    // Consecutive delimiters
    parts = String::split("a,,b", ',');
    ASSERT_EQ(parts.size(), 3);
    ASSERT_EQ(parts[0], "a");
    ASSERT_EQ(parts[1], "");
    ASSERT_EQ(parts[2], "b");

    return true;
}

TEST(string_split_string) {
    using apollo::base::String;

    auto parts = String::split("a::b::c", "::");
    ASSERT_EQ(parts.size(), 3);
    ASSERT_EQ(parts[0], "a");
    ASSERT_EQ(parts[1], "b");
    ASSERT_EQ(parts[2], "c");

    // Empty delimiter
    parts = String::split("abc", "");
    ASSERT_EQ(parts.size(), 1);
    ASSERT_EQ(parts[0], "abc");

    return true;
}

TEST(string_join) {
    using apollo::base::String;

    std::vector<std::string> strs = {"a", "b", "c"};
    ASSERT_EQ(String::join(strs, ","), "a,b,c");
    ASSERT_EQ(String::join(strs, "::"), "a::b::c");

    // Empty vector
    strs.clear();
    ASSERT_EQ(String::join(strs, ","), "");

    // Single element
    strs = {"single"};
    ASSERT_EQ(String::join(strs, ","), "single");

    return true;
}

// ============================================================================
// String Tests - Replace
// ============================================================================

TEST(string_replace) {
    using apollo::base::String;

    ASSERT_EQ(String::replace("hello world", "world", "there"), "hello there");
    ASSERT_EQ(String::replace("aaa", "a", "b"), "bbb");
    ASSERT_EQ(String::replace("hello", "x", "y"), "hello");  // No match

    // Empty from string
    ASSERT_EQ(String::replace("hello", "", "X"), "hello");

    // In-place replace
    std::string str = "hello world";
    String::replace_in_place(str, "world", "there");
    ASSERT_EQ(str, "hello there");

    return true;
}

// ============================================================================
// String Tests - Format and Conversion
// ============================================================================

TEST(string_format) {
    using apollo::base::String;

    ASSERT_EQ(String::format("%s %d", "test", 42), "test 42");
    ASSERT_EQ(String::format("%.2f", 3.14159), "3.14");
    ASSERT_EQ(String::format(""), "");

    return true;
}

TEST(string_empty_blank) {
    using apollo::base::String;

    ASSERT_TRUE(String::is_empty(""));
    ASSERT_FALSE(String::is_empty("hello"));

    ASSERT_TRUE(String::is_blank(""));
    ASSERT_TRUE(String::is_blank("   "));
    ASSERT_TRUE(String::is_blank("\t\n"));
    ASSERT_FALSE(String::is_blank("hello"));
    ASSERT_FALSE(String::is_blank(" hello "));

    return true;
}

TEST(string_to_numeric) {
    using apollo::base::String;

    ASSERT_EQ(String::to_int32("123"), 123);
    ASSERT_EQ(String::to_int32("-456"), -456);
    ASSERT_EQ(String::to_int32("invalid", 99), 99);  // Default value

    ASSERT_EQ(String::to_int64("12345678901234"), 12345678901234LL);
    ASSERT_EQ(String::to_int64("invalid", 99), 99);

    ASSERT_GT(String::to_double("3.14"), 3.1);
    ASSERT_LT(String::to_double("3.14"), 3.2);
    ASSERT_EQ(String::to_double("invalid", 9.9), 9.9);

    return true;
}

TEST(string_to_bool) {
    using apollo::base::String;

    // True values
    ASSERT_TRUE(String::to_bool("true"));
    ASSERT_TRUE(String::to_bool("TRUE"));
    ASSERT_TRUE(String::to_bool("1"));
    ASSERT_TRUE(String::to_bool("yes"));
    ASSERT_TRUE(String::to_bool("YES"));
    ASSERT_TRUE(String::to_bool("on"));
    ASSERT_TRUE(String::to_bool("ON"));

    // False values
    ASSERT_FALSE(String::to_bool("false"));
    ASSERT_FALSE(String::to_bool("FALSE"));
    ASSERT_FALSE(String::to_bool("0"));
    ASSERT_FALSE(String::to_bool("no"));
    ASSERT_FALSE(String::to_bool("off"));

    // Default value
    ASSERT_TRUE(String::to_bool("invalid", true));
    ASSERT_FALSE(String::to_bool("invalid", false));

    return true;
}

TEST(string_format_bytes) {
    using apollo::base::String;

    ASSERT_EQ(String::format_bytes(0), "0.00 B");
    ASSERT_EQ(String::format_bytes(1024), "1.00 KB");
    ASSERT_EQ(String::format_bytes(1536), "1.50 KB");
    ASSERT_EQ(String::format_bytes(1048576), "1.00 MB");
    ASSERT_EQ(String::format_bytes(1073741824), "1.00 GB");
    ASSERT_EQ(String::format_bytes(1099511627776ULL), "1.00 TB");

    return true;
}

TEST(string_hash) {
    using apollo::base::String;

    size_t h1 = String::hash("hello");
    size_t h2 = String::hash("hello");
    size_t h3 = String::hash("world");

    ASSERT_EQ(h1, h2);  // Same string, same hash
    ASSERT_NE(h1, h3);  // Different strings, different hashes

    return true;
}

// ============================================================================
// Memory Tests - ByteBuffer
// ============================================================================

TEST(bytebuffer_basic) {
    apollo::base::ByteBuffer buf(1024);

    ASSERT_EQ(buf.capacity(), 1024);
    ASSERT_EQ(buf.size(), 0);
    ASSERT_TRUE(buf.empty());

    const uint8_t* data = buf.data();
    ASSERT_TRUE(data != nullptr);

    return true;
}

TEST(bytebuffer_write) {
    apollo::base::ByteBuffer buf(100);

    const char* msg = "hello world";
    ASSERT_TRUE(buf.write(msg, 11));
    ASSERT_EQ(buf.size(), 11);

    // Write more
    ASSERT_TRUE(buf.write(msg, 11));
    ASSERT_EQ(buf.size(), 22);

    // Verify data
    ASSERT_EQ(std::memcmp(buf.data(), "hello worldhello world", 22), 0);

    return true;
}

TEST(bytebuffer_write_at) {
    apollo::base::ByteBuffer buf(100);

    const char* msg = "hello";
    buf.write(msg, 5);

    ASSERT_TRUE(buf.write_at(2, "x", 1));
    ASSERT_EQ(buf.size(), 5);  // Size shouldn't increase when writing within bounds
    ASSERT_EQ(std::memcmp(buf.data(), "hexlo", 5), 0);

    return true;
}

TEST(bytebuffer_clear) {
    apollo::base::ByteBuffer buf(100);

    buf.write("test", 4);
    ASSERT_EQ(buf.size(), 4);

    buf.clear();
    ASSERT_EQ(buf.size(), 0);
    ASSERT_TRUE(buf.empty());

    return true;
}

TEST(bytebuffer_append) {
    apollo::base::ByteBuffer buf1(100);
    apollo::base::ByteBuffer buf2(100);

    buf1.write("hello ", 6);
    buf2.write("world", 5);

    ASSERT_TRUE(buf1.append(buf2));
    ASSERT_EQ(buf1.size(), 11);
    ASSERT_EQ(std::memcmp(buf1.data(), "hello world", 11), 0);

    return true;
}

TEST(bytebuffer_to_vector) {
    apollo::base::ByteBuffer buf(100);

    buf.write("test", 4);
    auto vec = buf.to_vector();

    ASSERT_EQ(vec.size(), 4);
    ASSERT_EQ(vec[0], 't');
    ASSERT_EQ(vec[3], 't');

    return true;
}

// ============================================================================
// Memory Tests - FixedMemoryPool
// ============================================================================

TEST(fixed_memory_pool_basic) {
    apollo::base::FixedMemoryPool<64, 10> pool;

    ASSERT_EQ(pool.capacity(), 10);
    ASSERT_EQ(pool.allocated_count(), 0);
    ASSERT_EQ(pool.block_size(), 64);

    // Allocate
    void* ptr1 = pool.allocate();
    ASSERT_TRUE(ptr1 != nullptr);
    ASSERT_EQ(pool.allocated_count(), 1);

    void* ptr2 = pool.allocate();
    ASSERT_TRUE(ptr2 != nullptr);
    ASSERT_NE(ptr1, ptr2);  // Different pointers

    return true;
}

TEST(fixed_memory_pool_deallocate) {
    apollo::base::FixedMemoryPool<64, 10> pool;

    void* ptr1 = pool.allocate();
    ASSERT_EQ(pool.allocated_count(), 1);

    pool.deallocate(ptr1);
    ASSERT_EQ(pool.allocated_count(), 0);

    // Reallocate should get the same memory
    void* ptr2 = pool.allocate();
    ASSERT_EQ(ptr1, ptr2);

    return true;
}

TEST(fixed_memory_pool_exhausted) {
    apollo::base::FixedMemoryPool<32, 2> pool;

    void* ptr1 = pool.allocate();
    void* ptr2 = pool.allocate();

    ASSERT_TRUE(ptr1 != nullptr);
    ASSERT_TRUE(ptr2 != nullptr);

    // Pool exhausted
    void* ptr3 = pool.allocate();
    ASSERT_TRUE(ptr3 == nullptr);

    return true;
}

TEST(fixed_memory_pool_null_deallocate) {
    apollo::base::FixedMemoryPool<64, 10> pool;

    // Deallocate null should be safe
    pool.deallocate(nullptr);
    ASSERT_EQ(pool.allocated_count(), 0);

    return true;
}

// ============================================================================
// Memory Tests - ArenaAllocator
// ============================================================================

TEST(arena_allocator_basic) {
    apollo::base::ArenaAllocator arena(1024);

    ASSERT_GT(arena.total_capacity(), 0);
    ASSERT_EQ(arena.current_usage(), 0);

    // Allocate some memory
    void* ptr1 = arena.allocate(100);
    ASSERT_TRUE(ptr1 != nullptr);

    size_t usage = arena.current_usage();
    ASSERT_GT(usage, 0);

    return true;
}

TEST(arena_allocator_alignment) {
    apollo::base::ArenaAllocator arena(1024);

    // Allocate with different alignments
    void* ptr1 = arena.allocate(10, 8);
    void* ptr2 = arena.allocate(10, 16);
    void* ptr3 = arena.allocate(10, 32);

    ASSERT_TRUE(ptr1 != nullptr);
    ASSERT_TRUE(ptr2 != nullptr);
    ASSERT_TRUE(ptr3 != nullptr);

    return true;
}

TEST(arena_allocator_construct) {
    apollo::base::ArenaAllocator arena(1024);

    struct TestStruct {
        int a;
        double b;
        std::string c;

        TestStruct(int x, double y, const std::string& z)
            : a(x), b(y), c(z) {}
    };

    auto* obj = arena.construct<TestStruct>(42, 3.14, "hello");
    ASSERT_TRUE(obj != nullptr);
    ASSERT_EQ(obj->a, 42);
    ASSERT_GT(obj->b, 3.1);
    ASSERT_LT(obj->b, 3.2);
    ASSERT_EQ(obj->c, "hello");

    return true;
}

TEST(arena_allocator_reset) {
    apollo::base::ArenaAllocator arena(1024);

    arena.allocate(100);
    arena.allocate(200);
    size_t usage = arena.current_usage();
    ASSERT_GT(usage, 0);

    arena.reset();
    ASSERT_EQ(arena.current_usage(), 0);

    // Can allocate again after reset
    void* ptr = arena.allocate(50);
    ASSERT_TRUE(ptr != nullptr);

    return true;
}

TEST(arena_allocator_large_allocation) {
    apollo::base::ArenaAllocator arena(100);  // Small initial capacity

    // Request larger allocation than current block
    void* ptr = arena.allocate(1000);
    ASSERT_TRUE(ptr != nullptr);

    // Should have allocated a new block
    ASSERT_GT(arena.total_capacity(), 1000);

    return true;
}

// ============================================================================
// ThreadPool Tests
// ============================================================================

TEST(thread_pool_submit_with_result) {
    apollo::base::ThreadPool pool(2);

    auto future = pool.submit([](int a, int b) { return a + b; }, 10, 32);
    ASSERT_EQ(future.get(), 42);

    return true;
}

TEST(thread_pool_enqueue_void) {
    apollo::base::ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        pool.enqueue([&counter]() { counter++; });
    }

    pool.wait_for_all();
    ASSERT_EQ(counter, 10);

    return true;
}

TEST(thread_pool_multiple_wait) {
    apollo::base::ThreadPool pool(2);

    pool.enqueue([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
    pool.enqueue([]() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });

    pool.wait_for_all();
    ASSERT_EQ(pool.pending_task_count(), 0);

    return true;
}

// ============================================================================
// Time Tests
// ============================================================================

TEST(time_now) {
    auto now = apollo::base::Time::now();
    ASSERT_GT(now, 0);

    // Second call should return later time
    auto now2 = apollo::base::Time::now();
    ASSERT_GE(now2, now);

    return true;
}

TEST(time_format_now) {
    auto formatted = apollo::base::Time::format_now();
    ASSERT_GE(formatted.size(), 19);  // "YYYY-MM-DD HH:MM:SS"

    // Should contain common date/time separators
    ASSERT_TRUE(formatted.find('-') != std::string::npos);
    ASSERT_TRUE(formatted.find(':') != std::string::npos);

    return true;
}

TEST(time_timer) {
    apollo::base::Timer timer;

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    double elapsed = timer.elapsed_millis();
    ASSERT_GE(elapsed, 45.0);  // Allow some tolerance
    ASSERT_LE(elapsed, 100.0);  // Should not be too slow

    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Apollo Base Module Comprehensive Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    #define RUN_TEST(name) \
        do { \
            std::cout << "Running test_" #name "... "; \
            if (test_##name()) { \
                std::cout << "PASSED" << std::endl; \
                ++passed; \
            } else { \
                std::cout << "FAILED" << std::endl; \
                ++failed; \
            } \
        } while(0)

    // IdPool Tests
    std::cout << "--- IdPool Tests ---" << std::endl;
    RUN_TEST(id_pool_default_constructor);
    RUN_TEST(id_pool_size_constructor);
    RUN_TEST(id_pool_range_constructor);
    RUN_TEST(id_pool_allocate_and_release);
    RUN_TEST(id_pool_reserve);
    RUN_TEST(id_pool_reset);
    RUN_TEST(id_pool_stress);

    // ObjectPool Tests
    std::cout << "\n--- ObjectPool Tests ---" << std::endl;
    RUN_TEST(object_pool_basic);
    RUN_TEST(object_pool_release);
    RUN_TEST(object_pool_full);
    RUN_TEST(object_pool_invalid_access);

    // String Tests - Trim
    std::cout << "\n--- String Tests - Trim ---" << std::endl;
    RUN_TEST(string_trim);
    RUN_TEST(string_ltrim);
    RUN_TEST(string_rtrim);

    // String Tests - Case
    std::cout << "\n--- String Tests - Case ---" << std::endl;
    RUN_TEST(string_case_conversion);

    // String Tests - Comparison
    std::cout << "\n--- String Tests - Comparison ---" << std::endl;
    RUN_TEST(string_iequals);
    RUN_TEST(string_starts_ends_with);

    // String Tests - Split/Join
    std::cout << "\n--- String Tests - Split/Join ---" << std::endl;
    RUN_TEST(string_split_char);
    RUN_TEST(string_split_string);
    RUN_TEST(string_join);

    // String Tests - Replace
    std::cout << "\n--- String Tests - Replace ---" << std::endl;
    RUN_TEST(string_replace);

    // String Tests - Format/Conversion
    std::cout << "\n--- String Tests - Format/Conversion ---" << std::endl;
    RUN_TEST(string_format);
    RUN_TEST(string_empty_blank);
    RUN_TEST(string_to_numeric);
    RUN_TEST(string_to_bool);
    RUN_TEST(string_format_bytes);
    RUN_TEST(string_hash);

    // Memory Tests - ByteBuffer
    std::cout << "\n--- ByteBuffer Tests ---" << std::endl;
    RUN_TEST(bytebuffer_basic);
    RUN_TEST(bytebuffer_write);
    RUN_TEST(bytebuffer_write_at);
    RUN_TEST(bytebuffer_clear);
    RUN_TEST(bytebuffer_append);
    RUN_TEST(bytebuffer_to_vector);

    // Memory Tests - FixedMemoryPool
    std::cout << "\n--- FixedMemoryPool Tests ---" << std::endl;
    RUN_TEST(fixed_memory_pool_basic);
    RUN_TEST(fixed_memory_pool_deallocate);
    RUN_TEST(fixed_memory_pool_exhausted);
    RUN_TEST(fixed_memory_pool_null_deallocate);

    // Memory Tests - ArenaAllocator
    std::cout << "\n--- ArenaAllocator Tests ---" << std::endl;
    RUN_TEST(arena_allocator_basic);
    RUN_TEST(arena_allocator_alignment);
    RUN_TEST(arena_allocator_construct);
    RUN_TEST(arena_allocator_reset);
    RUN_TEST(arena_allocator_large_allocation);

    // ThreadPool Tests
    std::cout << "\n--- ThreadPool Tests ---" << std::endl;
    RUN_TEST(thread_pool_submit_with_result);
    RUN_TEST(thread_pool_enqueue_void);
    RUN_TEST(thread_pool_multiple_wait);

    // Time Tests
    std::cout << "\n--- Time Tests ---" << std::endl;
    RUN_TEST(time_now);
    RUN_TEST(time_format_now);
    RUN_TEST(time_timer);

    #undef RUN_TEST

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed > 0) {
        std::cerr << "\nSome tests FAILED!" << std::endl;
        return 1;
    }

    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
