/**
 * @file test_buffer.cpp
 * @brief NetBuffer 单元测试
 */

#include "apollo/net/adapters/native_adapter.h"
#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>

using namespace apollo::net::adapters;

// 测试辅助宏
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// ========== 测试用例 ==========

/**
 * @brief 测试缓冲区初始化
 */
bool test_buffer_init() {
    NetBuffer buffer;
    TEST_ASSERT(buffer.capacity() == NetBuffer::DEFAULT_SIZE, "Default size");
    TEST_ASSERT(buffer.readableBytes() == 0, "Initial readable bytes");
    TEST_ASSERT(buffer.writableBytes() == NetBuffer::DEFAULT_SIZE - 1, "Initial writable bytes");
    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试基本读写操作
 */
bool test_buffer_read_write() {
    NetBuffer buffer;
    const char* data = "Hello, World!";
    size_t length = strlen(data);

    // 写入数据
    buffer.write(data, length);
    TEST_ASSERT(buffer.readableBytes() == length, "Readable after write");

    // 读取数据
    char readBuf[128];
    size_t readLen = buffer.read(readBuf, length);
    TEST_ASSERT(readLen == length, "Read length");
    TEST_ASSERT(memcmp(readBuf, data, length) == 0, "Read content");
    TEST_ASSERT(buffer.readableBytes() == 0, "Readable after read");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试环形缓冲区特性
 */
bool test_buffer_wrap_around() {
    NetBuffer buffer(32); // 小缓冲区

    char readBuf[32];

    // 第一次写入并读取
    buffer.write("AAAA", 4);
    buffer.read(readBuf, 4);

    // 第二次写入，可能绕回
    buffer.write("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", 30);

    TEST_ASSERT(buffer.readableBytes() == 30, "Readable after wrap");

    buffer.read(readBuf, 30);
    TEST_ASSERT(memcmp(readBuf, "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", 30) == 0, "Wrap content");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 peek 操作
 */
bool test_buffer_peek() {
    NetBuffer buffer;
    const char* data = "Peek Test";

    buffer.write(data, 9);

    char peekBuf[16];
    size_t peekLen = buffer.peek(peekBuf, 9);
    TEST_ASSERT(peekLen == 9, "Peek length");
    TEST_ASSERT(memcmp(peekBuf, data, 9) == 0, "Peek content");
    TEST_ASSERT(buffer.readableBytes() == 9, "Readable after peek"); // peek 不移动指针

    // 清空
    buffer.skip(9);
    TEST_ASSERT(buffer.readableBytes() == 0, "Readable after skip");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 skip 操作
 */
bool test_buffer_skip() {
    NetBuffer buffer;
    buffer.write("0123456789", 10);

    buffer.skip(5);
    TEST_ASSERT(buffer.readableBytes() == 5, "Readable after skip");

    char readBuf[16];
    buffer.read(readBuf, 5);
    TEST_ASSERT(memcmp(readBuf, "56789", 5) == 0, "Content after skip");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试缓冲区扩容
 */
bool test_buffer_expand() {
    NetBuffer buffer(16);

    // 写入超过初始大小
    std::string largeData(100, 'X');
    buffer.write(largeData.data(), 100);

    TEST_ASSERT(buffer.readableBytes() == 100, "Readable after expand");
    TEST_ASSERT(buffer.capacity() >= 100, "Capacity after expand");

    // 验证内容
    std::vector<char> readBuf(100);
    buffer.read(readBuf.data(), 100);
    TEST_ASSERT(memcmp(readBuf.data(), largeData.data(), 100) == 0, "Expanded content");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试最大大小限制
 */
bool test_buffer_max_size() {
    NetBuffer buffer;

    // 尝试写入超过最大限制的数据
    std::string largeData(NetBuffer::MAX_SIZE * 2, 'X');
    buffer.write(largeData.data(), largeData.size());

    // 应该被限制在最大大小附近
    size_t readable = buffer.readableBytes();
    TEST_ASSERT(readable <= NetBuffer::MAX_SIZE, "Max size limit");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试 reset 操作
 */
bool test_buffer_reset() {
    NetBuffer buffer;

    buffer.write("data", 4);
    buffer.skip(2); // 移动读指针
    buffer.reset();

    TEST_ASSERT(buffer.readableBytes() == 0, "Readable after reset");
    TEST_ASSERT(buffer.writableBytes() == buffer.capacity() - 1, "Writable after reset");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试多次写入和读取
 */
bool test_buffer_multiple_operations() {
    NetBuffer buffer;

    // 多次写入
    buffer.write("Hello", 5);
    buffer.write(" ", 1);
    buffer.write("World", 5);

    TEST_ASSERT(buffer.readableBytes() == 11, "Total readable");

    // 多次读取
    char buf[16];
    buffer.read(buf, 5);
    TEST_ASSERT(memcmp(buf, "Hello", 5) == 0, "First read");

    buffer.read(buf, 1);
    TEST_ASSERT(memcmp(buf, " ", 1) == 0, "Second read");

    buffer.read(buf, 5);
    TEST_ASSERT(memcmp(buf, "World", 5) == 0, "Third read");

    TEST_ASSERT(buffer.readableBytes() == 0, "All read");

    std::cout << "  PASSED" << std::endl;
    return true;
}

/**
 * @brief 测试边界条件
 */
bool test_buffer_edge_cases() {
    NetBuffer buffer;

    // 空读取
    char buf[16];
    size_t readLen = buffer.read(buf, 16);
    TEST_ASSERT(readLen == 0, "Read from empty");

    // 写入空数据
    buffer.write("", 0);
    TEST_ASSERT(buffer.readableBytes() == 0, "Write empty");

    // 单字节
    buffer.write("X", 1);
    TEST_ASSERT(buffer.readableBytes() == 1, "Single byte write");
    buffer.read(buf, 1);
    TEST_ASSERT(buf[0] == 'X', "Single byte content");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ========== 测试运行器 ==========

int main() {
    std::cout << "=== NetBuffer Unit Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int total = 0;

    auto run = [&](const char* name, bool (*test)()) {
        std::cout << "Running: " << name << "...";
        total++;
        if (test()) passed++;
    };

    // 运行所有测试
    run("test_buffer_init", test_buffer_init);
    run("test_buffer_read_write", test_buffer_read_write);
    run("test_buffer_wrap_around", test_buffer_wrap_around);
    run("test_buffer_peek", test_buffer_peek);
    run("test_buffer_skip", test_buffer_skip);
    run("test_buffer_expand", test_buffer_expand);
    run("test_buffer_max_size", test_buffer_max_size);
    run("test_buffer_reset", test_buffer_reset);
    run("test_buffer_multiple_operations", test_buffer_multiple_operations);
    run("test_buffer_edge_cases", test_buffer_edge_cases);

    std::cout << std::endl;
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;

    return (passed == total) ? 0 : 1;
}

int test_buffer_main() {
    return main();
}
