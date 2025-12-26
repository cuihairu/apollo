/**
 * @file test_channel.cpp
 * @brief Channel 鍏ㄩ潰娴嬭瘯 (杈圭晫鏉′欢 + 鑳屽帇 + 鍏变韩鍐呭瓨瀹夊叏)
 */

#include "apollo/ipc/channel.h"
#include "apollo/ipc/shared_memory_channel.h"
#include "apollo/ipc/async_io.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <string>
#include <cassert>

using namespace apollo::ipc;

//==============================================================================
// 娴嬭瘯杈呭姪
//==============================================================================

namespace test {

#if defined(_WIN32)
    #define COLOR_RESET   ""
    #define COLOR_GREEN   ""
    #define COLOR_RED     ""
    #define COLOR_YELLOW  ""
    #define COLOR_BLUE    ""
#else
    #define COLOR_RESET   "\033[0m"
    #define COLOR_GREEN   "\033[32m"
    #define COLOR_RED     "\033[31m"
    #define COLOR_YELLOW  "\033[33m"
    #define COLOR_BLUE    "\033[34m"
#endif

struct Stats {
    int total = 0, passed = 0, failed = 0;
    void pass() { total++; passed++; }
    void fail() { total++; failed++; }
    void print() const {
        std::cout << "\n" << COLOR_BLUE << "=== Results ===" << COLOR_RESET << "\n";
        std::cout << "  Total: " << total << " | " << COLOR_GREEN << "Passed: " << passed << COLOR_RESET
                  << " | " << COLOR_RED << "Failed: " << failed << COLOR_RESET << "\n";
    }
};

static Stats g_stats;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << COLOR_RED << "  ASSERT FAILED: " << msg << " at " << __FILE__ << ":" << __LINE__ << COLOR_RESET << "\n"; \
            return false; \
        } \
    } while (0)

#define TEST_CASE(name) \
    bool test_##name(); \
    struct Runner_##name { \
        Runner_##name() { \
            std::cout << COLOR_BLUE << "[TEST] " << #name << COLOR_RESET << "\n"; \
            auto start = std::chrono::steady_clock::now(); \
            if (test_##name()) { \
                std::cout << COLOR_GREEN << "  PASSED" << COLOR_RESET; \
                g_stats.pass(); \
            } else { \
                std::cout << COLOR_RED << "  FAILED" << COLOR_RESET; \
                g_stats.fail(); \
            } \
            auto end = std::chrono::steady_clock::now(); \
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count(); \
            if (duration > 0) std::cout << " (" << duration << "ms)"; \
            std::cout << "\n"; \
        } \
    } runner_##name; \
    bool test_##name()

} // namespace test

//==============================================================================
// 鑳屽帇绠＄悊鍣ㄦ祴璇?
//==============================================================================

TEST_CASE(backpressure_init) {
    BackpressureConfig config;
    config.lowWatermark = 1024;
    config.highWatermark = 4096;
    config.bufferSize = 8192;
    config.strategy = BackpressureStrategy::Buffer;

    BackpressureManager bpm(config);

    TEST_ASSERT(bpm.canSend(1024), "Can send under buffer size");
    TEST_ASSERT(!bpm.isHighWatermark(), "Not high initially");
    TEST_ASSERT(bpm.isLowWatermark(), "Low initially");

    return true;
}

TEST_CASE(backpressure_watermark_transitions) {
    BackpressureConfig config;
    config.lowWatermark = 100;
    config.highWatermark = 500;
    config.bufferSize = 1000;

    bool highTriggered = false;
    bool lowTriggered = false;

    config.onHighWatermark = [&] { highTriggered = true; };
    config.onLowWatermark = [&] { lowTriggered = true; };

    BackpressureManager bpm(config);

    // 鍗囧埌楂樻按浣?
    for (int i = 0; i < 5; ++i) {
        bpm.applyBackpressure(150);  // 5 * 150 = 750 > 500
    }
    TEST_ASSERT(bpm.isHighWatermark(), "Should be high watermark");
    TEST_ASSERT(highTriggered, "High callback triggered");

    // 闄嶅埌浣庢按浣?
    bpm.pendingSize_.store(80, std::memory_order_relaxed);
    bpm.updateWatermark(80);
    TEST_ASSERT(bpm.isLowWatermark(), "Should be low watermark");
    TEST_ASSERT(lowTriggered, "Low callback triggered");

    return true;
}

TEST_CASE(backpressure_drop_strategy) {
    BackpressureConfig config;
    config.strategy = BackpressureStrategy::Drop;
    config.bufferSize = 1000;

    BackpressureManager bpm(config);

    // 瓒呰繃 buffer size 搴旇琚涪寮?
    auto result = bpm.applyBackpressure(1500);
    TEST_ASSERT(result == SendResult::Dropped, "Should drop when buffer exceeded");

    return true;
}

TEST_CASE(backpressure_block_strategy) {
    BackpressureConfig config;
    config.strategy = BackpressureStrategy::Block;
    config.blockTimeoutMs = 100;
    config.bufferSize = 1000;

    BackpressureManager bpm(config);

    // 濉弧鍒版帴杩戜笂闄?
    bpm.pendingSize_.store(900, std::memory_order_relaxed);

    // 搴旇闃诲
    auto start = std::chrono::steady_clock::now();
    auto result = bpm.applyBackpressure(200);
    auto end = std::chrono::steady_clock::now();

    // 搴旇绛夊緟涓€娈垫椂闂?
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    TEST_ASSERT(duration >= 50, "Should block for some time");  // 鑷冲皯 50ms

    return true;
}

TEST_CASE(backpressure_fail_strategy) {
    BackpressureConfig config;
    config.strategy = BackpressureStrategy::Fail;
    config.highWatermark = 500;
    config.bufferSize = 1000;

    BackpressureManager bpm(config);

    // 瓒呰繃楂樻按浣嶅簲璇ュけ璐?
    bpm.pendingSize_.store(600, std::memory_order_relaxed);
    auto result = bpm.applyBackpressure(100);
    TEST_ASSERT(result == SendResult::BufferFull, "Should fail when high watermark");

    return true;
}

TEST_CASE(backpressure_concurrent_access) {
    BackpressureConfig config;
    config.strategy = BackpressureStrategy::Block;
    config.bufferSize = 10000;
    config.highWatermark = 5000;
    config.lowWatermark = 1000;

    BackpressureManager bpm(config);

    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};
    std::vector<std::thread> threads;

    // 澶氱嚎绋嬪苟鍙戝彂閫?
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                auto result = bpm.applyBackpressure(100);
                if (result == SendResult::Success) {
                    successCount++;
                } else {
                    failCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT(successCount + failCount == 400, "All operations completed");
    TEST_ASSERT(bpm.getPendingSize() <= 10000, "Size within buffer limit");

    return true;
}

//==============================================================================

//==============================================================================
// 双重水位线测试 - 消息数量 + 字节大小
//==============================================================================

TEST_CASE(backpressure_dual_watermark_init) {
    BackpressureConfig config;
    config.lowMessageCount = 10;
    config.highMessageCount = 50;
    config.maxMessageCount = 100;
    config.lowByteCount = 1024;
    config.highByteCount = 4096;
    config.maxByteCount = 8192;
    config.strategy = BackpressureStrategy::Buffer;

    BackpressureManager bpm(config);

    TEST_ASSERT(bpm.canSend(1024), "Can send under buffer size");
    TEST_ASSERT(!bpm.isHighWatermark(), "Not high initially");
    TEST_ASSERT(bpm.isLowWatermark(), "Low initially");
    TEST_ASSERT(bpm.getPendingCount() == 0, "No pending messages initially");
    TEST_ASSERT(bpm.getPendingSize() == 0, "No pending bytes initially");

    return true;
}

TEST_CASE(backpressure_dual_watermark_count_triggers) {
    BackpressureConfig config;
    config.lowMessageCount = 5;
    config.highMessageCount = 20;
    config.maxMessageCount = 50;
    config.lowByteCount = 100000;
    config.highByteCount = 200000;
    config.maxByteCount = 500000;

    bool highTriggered = false;
    config.onHighWatermark = [&] { highTriggered = true; };

    BackpressureManager bpm(config);

    for (int i = 0; i < 25; ++i) {
        bpm.addMessage(100);
    }
    TEST_ASSERT(bpm.isHighWatermark(), "High watermark by message count");
    TEST_ASSERT(highTriggered, "High callback triggered");
    TEST_ASSERT(bpm.getPendingCount() == 25, "25 messages pending");

    return true;
}

TEST_CASE(backpressure_dual_watermark_byte_triggers) {
    BackpressureConfig config;
    config.lowMessageCount = 1000;
    config.highMessageCount = 2000;
    config.maxMessageCount = 5000;
    config.lowByteCount = 500;
    config.highByteCount = 2000;
    config.maxByteCount = 5000;

    bool highTriggered = false;
    config.onHighWatermark = [&] { highTriggered = true; };

    BackpressureManager bpm(config);

    bpm.addMessage(3000);
    TEST_ASSERT(bpm.isHighWatermark(), "High watermark by byte count");
    TEST_ASSERT(highTriggered, "High callback triggered");
    TEST_ASSERT(bpm.getPendingCount() == 1, "1 message pending");

    return true;
}

TEST_CASE(backpressure_dual_watermark_both_low) {
    BackpressureConfig config;
    config.lowMessageCount = 5;
    config.highMessageCount = 20;
    config.maxMessageCount = 50;
    config.lowByteCount = 500;
    config.highByteCount = 2000;
    config.maxByteCount = 5000;

    BackpressureManager bpm(config);

    for (int i = 0; i < 10; ++i) {
        bpm.addMessage(300);
    }
    TEST_ASSERT(!bpm.isLowWatermark(), "Not low watermark (both above low)");

    for (int i = 0; i < 8; ++i) {
        bpm.removeMessage(300);
    }
    TEST_ASSERT(!bpm.isLowWatermark(), "Not low (bytes still above low)");

    bpm.removeMessage(150);
    TEST_ASSERT(bpm.isLowWatermark(), "Low watermark (both below low)");

    return true;
}

TEST_CASE(backpressure_dual_watermark_either_high) {
    BackpressureConfig config;
    config.lowMessageCount = 5;
    config.highMessageCount = 20;
    config.maxMessageCount = 50;
    config.lowByteCount = 500;
    config.highByteCount = 2000;
    config.maxByteCount = 5000;

    BackpressureManager bpm(config);

    for (int i = 0; i < 25; ++i) {
        bpm.addMessage(50);
    }
    TEST_ASSERT(bpm.isHighWatermark(), "High by message count");

    bpm.reset();

    bpm.addMessage(3000);
    TEST_ASSERT(bpm.isHighWatermark(), "High by byte count");

    return true;
}

TEST_CASE(backpressure_dual_watermark_tracking) {
    BackpressureConfig config;
    config.maxMessageCount = 100;
    config.maxByteCount = 10000;

    BackpressureManager bpm(config);

    TEST_ASSERT(bpm.getPendingCount() == 0, "Initial count 0");
    TEST_ASSERT(bpm.getPendingSize() == 0, "Initial size 0");

    bpm.addMessage(100);
    TEST_ASSERT(bpm.getPendingCount() == 1, "Count 1");
    TEST_ASSERT(bpm.getPendingSize() == 100, "Size 100");

    bpm.addMessage(200);
    TEST_ASSERT(bpm.getPendingCount() == 2, "Count 2");
    TEST_ASSERT(bpm.getPendingSize() == 300, "Size 300");

    bpm.removeMessage(100);
    TEST_ASSERT(bpm.getPendingCount() == 1, "Count 1 after remove");
    TEST_ASSERT(bpm.getPendingSize() == 200, "Size 200 after remove");

    return true;
}

// TokenBucket 娴嬭瘯
//==============================================================================

TEST_CASE(tokenbucket_basic) {
    TokenBucket bucket(1000, 100);  // 1000 tokens/sec, burst 100

    // 鍒濆搴旇鏈?burst 鏁伴噺
    TEST_ASSERT(bucket.available() == 100, "Initial burst");

    // 娑堣垂 50 tokens
    TEST_ASSERT(bucket.tryConsume(50), "Consume 50");
    TEST_ASSERT(bucket.available() == 50, "50 remaining");

    return true;
}

TEST_CASE(tokenbucket_rate_limiting) {
    TokenBucket bucket(100, 100);  // 100 tokens/sec, burst 100

    // 娑堣垂鍏ㄩ儴
    TEST_ASSERT(bucket.tryConsume(100), "Consume all");
    TEST_ASSERT(!bucket.tryConsume(1), "Should be empty");

    // 绛夊緟琛ュ厖
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bucket.refill();

    // 搴旇鏈夋柊鐨?tokens
    size_t available = bucket.available();
    TEST_ASSERT(available > 0, "Tokens refilled");
    TEST_ASSERT(available <= 100, "Not exceed burst");

    return true;
}

TEST_CASE(tokenbucket_concurrent) {
    TokenBucket bucket(10000, 1000);
    std::atomic<int> consumed{0};
    std::atomic<int> blocked{0};

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                if (bucket.tryConsume(10)) {
                    consumed++;
                } else {
                    blocked++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT(consumed + blocked == 1000, "All attempts made");
    TEST_ASSERT(consumed <= 100, "Cannot exceed burst");

    return true;
}

//==============================================================================
// 鍏变韩鍐呭瓨鐜舰缂撳啿鍖烘祴璇?
//==============================================================================

TEST_CASE(shm_ringbuffer_create) {
    SharedMemoryRingBuffer buffer("test_create", 4096);

    TEST_ASSERT(buffer.create(), "Create shared memory");
    TEST_ASSERT(buffer.isValid(), "Valid after create");
    TEST_ASSERT(buffer.capacity() == 4096, "Capacity correct");

    buffer.close();
    return true;
}

TEST_CASE(shm_ringbuffer_open) {
    SharedMemoryRingBuffer buffer1("test_open", 4096);
    buffer1.create();

    SharedMemoryRingBuffer buffer2("test_open", 4096);
    TEST_ASSERT(buffer2.open(), "Open existing shared memory");
    TEST_ASSERT(buffer2.isValid(), "Valid after open");

    buffer1.close();
    buffer2.close();
    return true;
}

TEST_CASE(shm_ringbuffer_write_read) {
    SharedMemoryRingBuffer buffer("test_wr", 4096);
    buffer.create();

    const char* testData = "Hello, Shared Memory!";
    size_t size = strlen(testData);

    void* ptr = buffer.allocate(size, 1);
    TEST_ASSERT(ptr != nullptr, "Allocate succeeded");

    std::memcpy(ptr, testData, size);
    buffer.commit(size);

    SharedMemoryRingBuffer::ReadResult result;
    TEST_ASSERT(buffer.read(result), "Read succeeded");
    TEST_ASSERT(result.size == size, "Size matches");
    TEST_ASSERT(std::memcmp(result.data, testData, size) == 0, "Data matches");

    buffer.close();
    return true;
}

TEST_CASE(shm_ringbuffer_wraparound) {
    SharedMemoryRingBuffer buffer("test_wrap", 256);
    buffer.create();

    // 鍐欏埌鎺ヨ繎鏈熬
    size_t writePos = buffer.header_->writePos.load();
    size_t capacity = buffer.capacity();

    buffer.header_->writePos.store(capacity - 10);

    // 搴旇鑳藉鍥炵粫
    void* ptr = buffer.allocate(50, 1);
    TEST_ASSERT(ptr != nullptr, "Allocate with wraparound");

    buffer.close();
    return true;
}

TEST_CASE(shm_ringbuffer_full_empty) {
    SharedMemoryRingBuffer buffer("test_full", 512);
    buffer.create();

    TEST_ASSERT(buffer.isEmpty(), "Initially empty");
    TEST_ASSERT(!buffer.isFull(), "Not full initially");

    // 濉弧
    void* ptr = buffer.allocate(512, 1);
    TEST_ASSERT(ptr != nullptr, "Allocate full capacity");
    buffer.commit(512);

    TEST_ASSERT(!buffer.isEmpty(), "Not empty after write");
    TEST_ASSERT(buffer.isFull(), "Full after fill");

    buffer.close();
    return true;
}

TEST_CASE(shm_ringbuffer_concurrent) {
    SharedMemoryRingBuffer buffer("test_concurrent", 8192);
    buffer.create();

    std::atomic<int> writeCount{0};
    std::atomic<int> readCount{0};
    std::atomic<bool> running{true};

    std::thread writer([&]() {
        for (int i = 0; i < 1000; ++i) {
            void* ptr = buffer.allocate(64, 1);
            if (ptr) {
                std::memset(ptr, i % 256, 64);
                buffer.commit(64);
                writeCount++;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    std::thread reader([&]() {
        SharedMemoryRingBuffer::ReadResult result;
        while (running.load() || buffer.available() > 0) {
            if (buffer.read(result)) {
                readCount++;
                buffer.consume();
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    writer.join();
    reader.join();

    buffer.close();

    std::cout << "    Wrote: " << writeCount << ", Read: " << readCount << "\n";
    TEST_ASSERT(writeCount == readCount, "All writes read");

    return true;
}

TEST_CASE(shm_ringbuffer_magic_validation) {
    SharedMemoryRingBuffer buffer("test_magic", 4096);
    buffer.create();

    // 楠岃瘉榄旀暟
    TEST_ASSERT(buffer.header_->magic == 0x41504C53, "Magic number correct");

    buffer.close();
    return true;
}

TEST_CASE(shm_ringbuffer_multiple_instances) {
    const char* name = "test_multi";
    const size_t size = 4096;

    // 鍒涘缓澶氫釜瀹炰緥
    std::vector<std::unique_ptr<SharedMemoryRingBuffer>> buffers;

    for (int i = 0; i < 5; ++i) {
        auto buf = std::make_unique<SharedMemoryRingBuffer>(name, size);
        if (i == 0) {
            TEST_ASSERT(buf->create(), "First create");
        } else {
            TEST_ASSERT(buf->open(), "Others open");
        }
        TEST_ASSERT(buf->isValid(), "Valid");
        buffers.push_back(std::move(buf));
    }

    // 娓呯悊
    for (auto& buf : buffers) {
        buf->close();
    }

    return true;
}

//==============================================================================
// FlatBuffers + 鍏变韩鍐呭瓨娴嬭瘯
//==============================================================================

#ifdef APOLLO_HAS_FLATBUFFERS

TEST_CASE(flatbuffer_channel_basic) {
    FlatBufferChannel channel("fb_test", 4096);
    TEST_ASSERT(channel.createServer(), "Create server");

    TEST_ASSERT(channel.isConnected(), "Connected");
    channel.disconnect();

    return true;
}

TEST_CASE(flatbuffer_channel_send_receive) {
    FlatBufferChannel channel("fb_send_recv", 8192);
    channel.createServer();

    const char* testData = "FlatBuffer test data";
    size_t size = strlen(testData);

    TEST_ASSERT(channel.send(testData, size, 1), "Send succeeded");

    FlatBufferChannel::Message msg;
    TEST_ASSERT(channel.receive(msg), "Receive succeeded");
    TEST_ASSERT(msg.size == size, "Size matches");

    std::string received(static_cast<const char*>(msg.data), msg.size);
    TEST_ASSERT(received == testData, "Content matches");

    channel.disconnect();
    return true;
}

TEST_CASE(flatbuffer_channel_zero_copy) {
    FlatBufferChannel channel("fb_zerocopy", 4096);
    channel.createServer();

    // 鍙戦€?
    std::vector<uint8_t> data(1000, 0xAB);
    TEST_ASSERT(channel.send(data.data(), data.size(), 1), "Send");

    // 鎺ユ敹 - 闆舵嫹璐濊闂?
    FlatBufferChannel::Message msg;
    TEST_ASSERT(channel.receive(msg), "Receive");

    // 楠岃瘉鐩存帴璁块棶鍏变韩鍐呭瓨
    const uint8_t* ptr = static_cast<const uint8_t*>(msg.data);
    TEST_ASSERT(ptr[0] == 0xAB, "First byte correct");
    TEST_ASSERT(ptr[999] == 0xAB, "Last byte correct");

    // 楠岃瘉娌℃湁鎷疯礉 (妫€鏌ュ湴鍧€鏄惁鍦ㄥ悎鐞嗚寖鍥?
    auto* rawBuf = reinterpret_cast<const uint8_t*>(channel.ringBuffer_->data());
    auto offset = ptr - rawBuf;
    TEST_ASSERT(offset >= 0 && offset < 4096, "Pointer within shared memory");

    channel.consume();
    channel.disconnect();
    return true;
}

#endif // APOLLO_HAS_FLATBUFFERS

//==============================================================================
// 杈圭晫鏉′欢娴嬭瘯
//==============================================================================

TEST_CASE(edgecase_empty_channel) {
    SharedMemoryRingBuffer buffer("test_empty", 1024);
    buffer.create();

    SharedMemoryRingBuffer::ReadResult result;
    TEST_ASSERT(!buffer.read(result), "Read on empty returns false");
    TEST_ASSERT(buffer.available() == 0, "Available is 0");

    buffer.close();
    return true;
}

TEST_CASE(edgecase_zero_size_message) {
    SharedMemoryRingBuffer buffer("test_zero", 1024);
    buffer.create();

    void* ptr = buffer.allocate(0, 1);
    TEST_ASSERT(ptr != nullptr, "Allocate 0 bytes");
    buffer.commit(0);

    TEST_ASSERT(buffer.isEmpty(), "Still empty after 0-byte write");

    buffer.close();
    return true;
}

TEST_CASE(edgecase_max_size_message) {
    SharedMemoryRingBuffer buffer("test_max", 1024);
    buffer.create();

    // 鍒嗛厤鍑犱箮鍏ㄩ儴绌洪棿
    void* ptr = buffer.allocate(1000, 1);
    TEST_ASSERT(ptr != nullptr, "Allocate large message");
    buffer.commit(1000);

    TEST_ASSERT(!buffer.isEmpty(), "Not empty");

    buffer.close();
    return true;
}

TEST_CASE(edgecase_exceed_capacity) {
    SharedMemoryRingBuffer buffer("test_exceed", 512);
    buffer.create();

    // 灏濊瘯鍒嗛厤瓒呰繃瀹归噺鐨勬秷鎭?
    void* ptr = buffer.allocate(1000, 1);
    TEST_ASSERT(ptr == nullptr, "Fail to allocate over capacity");

    buffer.close();
    return true;
}

TEST_CASE(edgecase_rapid_write_read) {
    SharedMemoryRingBuffer buffer("test_rapid", 4096);
    buffer.create();

    std::atomic<int> ops{0};

    // 蹇€熻繛缁搷浣?
    for (int i = 0; i < 1000; ++i) {
        void* ptr = buffer.allocate(16, 1);
        if (ptr) {
            buffer.commit(16);
            SharedMemoryRingBuffer::ReadResult result;
            if (buffer.read(result)) {
                buffer.consume();
                ops++;
            }
        }
    }

    TEST_ASSERT(ops > 0, "Some operations succeeded");

    buffer.close();
    return true;
}

TEST_CASE(edgecase_corrupted_magic) {
    // 涓嶅垱寤猴紝鐩存帴灏濊瘯鎵撳紑涓嶅瓨鍦ㄧ殑鍏变韩鍐呭瓨
    SharedMemoryRingBuffer buffer("test_corrupt", 4096);
    TEST_ASSERT(!buffer.open(), "Fail to open non-existent");
    TEST_ASSERT(!buffer.isValid(), "Invalid after failed open");

    return true;
}

//==============================================================================
// 鎬ц兘娴嬭瘯
//==============================================================================

TEST_CASE(performance_shared_memory_throughput) {
    SharedMemoryRingBuffer buffer("test_perf", 1024 * 1024);  // 1MB
    buffer.create();

    const size_t msgSize = 1024;
    const int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        void* ptr = buffer.allocate(msgSize, 1);
        if (ptr) {
            buffer.commit(msgSize);
        }
    }

    auto mid = std::chrono::high_resolution_clock::now();

    int readCount = 0;
    SharedMemoryRingBuffer::ReadResult result;
    while (readCount < iterations) {
        if (buffer.read(result)) {
            buffer.consume();
            readCount++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto writeTime = std::chrono::duration_cast<std::chrono::microseconds>(mid - start).count();
    auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    double throughputMBps = (iterations * msgSize / 1024.0 / 1024.0) / (totalTime / 1000000.0);

    std::cout << "    Write: " << writeTime << "渭s, Total: " << totalTime
              << "渭s, Throughput: " << throughputMBps << " MB/s\n";

    TEST_ASSERT(readCount == iterations, "All messages read");

    buffer.close();
    return true;
}

TEST_CASE(perference_multiprocess_safe) {
    // 杩欎釜娴嬭瘯楠岃瘉 fork 鍚庣殑鍏变韩鍐呭瓨瀹夊叏
    // 鍦ㄥ疄闄呬娇鐢ㄤ腑锛屽杩涚▼鍏变韩鍐呭瓨闇€瑕佷粩缁嗘祴璇?

    SharedMemoryRingBuffer buffer("test_multiproc", 4096);
    buffer.create();

    // 鍐欏叆涓€浜涙暟鎹?
    const char* testStr = "Multi-process test";
    size_t len = strlen(testStr);

    void* ptr = buffer.allocate(len, 1);
    std::memcpy(ptr, testStr, len);
    buffer.commit(len);

    // 鍦ㄥ瓙杩涚▼涓鍙?
    #ifdef __linux__
        pid_t pid = fork();
        if (pid == 0) {
            // 瀛愯繘绋?
            SharedMemoryRingBuffer childBuffer("test_multiproc", 4096);
            if (childBuffer.open()) {
                SharedMemoryRingBuffer::ReadResult result;
                if (childBuffer.read(result)) {
                    std::string received(static_cast<const char*>(result.data), result.size);
                    TEST_ASSERT(received == testStr, "Child reads correct data");
                }
                childBuffer.close();
            }
            _exit(0);
        } else {
            // 鐖惰繘绋?
            int status;
            waitpid(pid, &status, 0);
            TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "Child succeeded");
        }
    #else
        std::cout << "    Skipped on non-Unix platform\n";
    #endif

    buffer.close();
    return true;
}

} // namespace test

//==============================================================================
// 涓荤▼搴?
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << test::COLOR_BLUE << "========================================" << test::COLOR_RESET << "\n";
    std::cout << test::COLOR_BLUE << "=== Apollo Channel Tests ===" << test::COLOR_RESET << "\n";
    std::cout << test::COLOR_BLUE << "========================================" << test::COLOR_RESET << "\n\n";

    std::cout << "Testing Shared Memory Safety, Backpressure, and Edge Cases\n\n";

    // 娴嬭瘯浼氶€氳繃闈欐€佹瀯閫犺嚜鍔ㄨ繍琛?

    std::cout << "\n";
    test::g_stats.print();

    return (test::g_stats.failed == 0) ? 0 : 1;
}

