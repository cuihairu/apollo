// SharedMemoryRingBuffer 回归（audit P0#4 修复 2026-11-12）。
// ①allocate+memcpy+commit 非原子 → 并发写撕裂（write() 持 writeMutex_ 串行化，
//   FlatBufferChannel::send 改走 write()）；②open() 不校验容量 → 读写两端按
//   各自 size 寻址越界；③回绕边界差一（readPos==totalSize 恰好接界被误拒）。
#include "apollo/ipc/shared_memory_channel.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace {

using apollo::ipc::SharedMemoryRingBuffer;

#define TEST_ASSERT(cond, msg) \
    do { if (!(cond)) { std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; return false; } } while (0)

// write()/read()/consume() 基础回路 + type 传播 + 超容量拒绝
bool test_write_read_roundtrip() {
    SharedMemoryRingBuffer buf("/apollo_p04_roundtrip", 4096);
    TEST_ASSERT(buf.create(), "create");
    TEST_ASSERT(buf.isValid(), "valid");

    const char payload[] = "apollo-shm-payload-0123456789";
    TEST_ASSERT(buf.write(payload, sizeof(payload), 42), "write");
    TEST_ASSERT(buf.count() == 1, "count 1");
    TEST_ASSERT(!buf.isEmpty(), "not empty");

    SharedMemoryRingBuffer::ReadResult r{};
    TEST_ASSERT(buf.read(r), "read");
    TEST_ASSERT(r.size == sizeof(payload), "size matches");
    TEST_ASSERT(r.type == 42, "type matches");
    TEST_ASSERT(r.timestamp != 0, "timestamp set");
    TEST_ASSERT(std::memcmp(r.data, payload, sizeof(payload)) == 0, "payload intact");

    buf.consume();
    TEST_ASSERT(buf.isEmpty(), "empty after consume");
    TEST_ASSERT(buf.count() == 0, "count 0 after consume");

    // 超容量：拒绝且状态不变
    std::vector<char> big(8192, 'x');
    TEST_ASSERT(!buf.write(big.data(), big.size(), 1), "oversize write rejected");
    TEST_ASSERT(buf.isEmpty(), "state unchanged after reject");
    return true;
}

// audit P0#4②：同名通道容量不一致必须拒绝打开（否则两侧按各自 size 寻址 →
// 越界写 / SIGBUS）。修复前 open() 只查 magic 不查容量。
bool test_open_capacity_mismatch_rejected() {
    SharedMemoryRingBuffer server("/apollo_p04_capmis", 4096);
    TEST_ASSERT(server.create(), "server create 4096");

    SharedMemoryRingBuffer clientWrong("/apollo_p04_capmis", 8192);
    TEST_ASSERT(!clientWrong.open(), "open with wrong capacity rejected");
    TEST_ASSERT(!clientWrong.isValid(), "wrong-capacity open leaves invalid");

    SharedMemoryRingBuffer clientRight("/apollo_p04_capmis", 4096);
    TEST_ASSERT(clientRight.open(), "open with matching capacity ok");
    return true;
}

// audit P0#4③：回绕边界差一——readPos == totalSize（恰好接界）必须允许回绕
// 写入。4096 容量，两条 16+2040=2056 的消息：写 A 消费后 readPos=writePos=
// 2056，写 B 触发回绕且 totalSize(2056)==readPos(2056)，原 `<=` 判定误拒。
bool test_wrap_boundary_exact_fit() {
    constexpr size_t kCap = 4096;
    constexpr size_t kMsg = 2040;  // 16 + 2040 = 2056
    static_assert(sizeof(SharedMemoryRingBuffer::MessageBlock) == 16,
                  "boundary arithmetic assumes 16-byte MessageBlock");
    SharedMemoryRingBuffer buf("/apollo_p04_wrap", kCap);
    TEST_ASSERT(buf.create(), "create");

    std::vector<char> a(kMsg, 'A'), b(kMsg, 'B');
    TEST_ASSERT(buf.write(a.data(), a.size(), 1), "write A");
    {
        SharedMemoryRingBuffer::ReadResult r{};
        TEST_ASSERT(buf.read(r), "read A");
        TEST_ASSERT(r.size == kMsg && r.type == 1, "A header");
        TEST_ASSERT(std::memcmp(r.data, a.data(), a.size()) == 0, "A payload");
        buf.consume();
    }
    TEST_ASSERT(buf.isEmpty(), "empty before wrap write");

    TEST_ASSERT(buf.write(b.data(), b.size(), 2),
                "write B wraps at exact boundary (old <= check rejected)");
    {
        SharedMemoryRingBuffer::ReadResult r{};
        TEST_ASSERT(buf.read(r), "read B after wrap");
        TEST_ASSERT(r.size == kMsg && r.type == 2, "B header");
        TEST_ASSERT(std::memcmp(r.data, b.data(), b.size()) == 0, "B payload");
        buf.consume();
    }
    TEST_ASSERT(buf.isEmpty() && buf.count() == 0, "drained after wrap");
    return true;
}

// audit P0#4①核心：多写者并发 write()。修复前 allocate/commit 各自 relaxed 读
// writePos → 多写者取到同一位置重叠 memcpy、撕裂发布；修复后全程持
// writeMutex_ 串行化，每条消息完整（size 与填充模式一一对应）。单读者为通道
// 单消费者约定。
bool test_multi_writer_no_tear() {
    constexpr size_t kCap = 128 * 1024;
    constexpr int kWriters = 4;
    constexpr int kMsgsPerWriter = 400;
    const size_t sizes[kWriters] = {128, 192, 256, 320};  // 每写者独立 size+填充模式
    const char fills[kWriters] = {0x11, 0x22, 0x33, 0x44};

    SharedMemoryRingBuffer buf("/apollo_p04_multi", kCap);
    TEST_ASSERT(buf.create(), "create");

    std::atomic<int> consumed{0};
    std::atomic<bool> stopReader{false};
    std::atomic<bool> readerOk{true};

    std::thread reader([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        const int expected = kWriters * kMsgsPerWriter;
        while (consumed.load() < expected) {
            if (std::chrono::steady_clock::now() > deadline) {
                std::cerr << "reader deadline: consumed=" << consumed.load()
                          << "/" << expected << std::endl;
                readerOk = false; break;
            }
            SharedMemoryRingBuffer::ReadResult r{};
            if (!buf.read(r)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            bool ok = false;
            for (int w = 0; w < kWriters; ++w) {
                if (r.size == sizes[w]) {
                    const auto* p = static_cast<const char*>(r.data);
                    ok = true;
                    for (size_t i = 0; i < r.size; ++i) {
                        if (p[i] != fills[w]) { ok = false; break; }
                    }
                    break;
                }
            }
            if (!ok) {  // 撕裂/重叠 → size-模式失配
                const auto* p = static_cast<const unsigned char*>(r.data);
                std::cerr << "tear: size=" << r.size << " type=" << r.type
                          << " byte0=" << (r.size ? (int)p[0] : -1)
                          << " byte1=" << (r.size > 1 ? (int)p[1] : -1)
                          << " consumed=" << consumed.load() << std::endl;
                readerOk = false; break;
            }
            buf.consume();
            consumed.fetch_add(1);
        }
        stopReader = true;
    });

    std::vector<std::thread> writers;
    for (int w = 0; w < kWriters; ++w) {
        writers.emplace_back([&, w] {
            std::vector<char> msg(sizes[w], fills[w]);
            for (int i = 0; i < kMsgsPerWriter && readerOk.load(); ++i) {
                while (!buf.write(msg.data(), msg.size(), static_cast<uint32_t>(w + 1))) {
                    if (!readerOk.load() || stopReader.load()) return;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }
    for (auto& t : writers) t.join();
    reader.join();

    TEST_ASSERT(readerOk.load(), "reader validated all messages intact (tear/overlap would fail)");
    TEST_ASSERT(consumed.load() == kWriters * kMsgsPerWriter, "all messages delivered");
    TEST_ASSERT(buf.isEmpty() && buf.count() == 0, "buffer drained");
    return true;
}

// audit P0#4③ 同源（并发压测实测复现）：消息恰好写满到 capacity 时写端
// commit 与读端 consume 的回绕约定必须一致（都回 0）。旧 commit 置
// totalSize、读端置 0：写端下一消息落在 totalSize，读端却从 0 重读上一圈
// 陈旧消息并冲过写端前沿，撕裂后续消息。
bool test_exact_fill_wrap_consistency() {
    // 场景1：单消息恰好占满整圈（4096 = 16 + 4080），写读两轮
    {
        SharedMemoryRingBuffer buf("/apollo_p04_exact1", 4096);
        TEST_ASSERT(buf.create(), "create");
        std::vector<char> a(4080, 'a'), b(4080, 'b');
        TEST_ASSERT(buf.write(a.data(), a.size(), 1), "write full-ring A");
        SharedMemoryRingBuffer::ReadResult r{};
        TEST_ASSERT(buf.read(r), "read A");
        TEST_ASSERT(r.size == a.size() && std::memcmp(r.data, a.data(), a.size()) == 0,
                    "A intact");
        buf.consume();
        TEST_ASSERT(buf.isEmpty(), "empty after A");
        TEST_ASSERT(buf.write(b.data(), b.size(), 2), "write B after exact fill");
        TEST_ASSERT(buf.read(r), "read B");
        TEST_ASSERT(r.size == b.size() && r.type == 2 &&
                    std::memcmp(r.data, b.data(), b.size()) == 0, "B intact");
        buf.consume();
        TEST_ASSERT(buf.isEmpty(), "empty after B");
    }
    // 场景2：尾部消息恰好顶满 capacity（2016+2080=4096），随后写读 C。
    // 约定分裂时 C 落在 2080 而读端在 0，读到 A 的陈旧字节。
    {
        SharedMemoryRingBuffer buf("/apollo_p04_exact2", 4096);
        TEST_ASSERT(buf.create(), "create");
        std::vector<char> a(2000, 'a'), b(2064, 'b'), c(2000, 'c');
        TEST_ASSERT(buf.write(a.data(), a.size(), 1), "write A");
        {
            SharedMemoryRingBuffer::ReadResult r{};
            TEST_ASSERT(buf.read(r), "read A");
            TEST_ASSERT(std::memcmp(r.data, a.data(), a.size()) == 0, "A intact");
            buf.consume();
        }
        TEST_ASSERT(buf.write(b.data(), b.size(), 2), "write B exact-fills tail");
        {
            SharedMemoryRingBuffer::ReadResult r{};
            TEST_ASSERT(buf.read(r), "read B");
            TEST_ASSERT(r.type == 2 && std::memcmp(r.data, b.data(), b.size()) == 0,
                        "B intact");
            buf.consume();
        }
        TEST_ASSERT(buf.isEmpty(), "empty after B");
        TEST_ASSERT(buf.write(c.data(), c.size(), 3), "write C after tail exact fill");
        SharedMemoryRingBuffer::ReadResult r{};
        TEST_ASSERT(buf.read(r), "read C");
        TEST_ASSERT(r.type == 3 && std::memcmp(r.data, c.data(), c.size()) == 0,
                    "C intact (wrap convention split would return stale A bytes)");
        buf.consume();
        TEST_ASSERT(buf.isEmpty() && buf.count() == 0, "drained");
    }
    return true;
}

} // namespace

int main() {
    int passed = 0, total = 0;
    auto run = [&](const char* name, bool (*fn)()) {
        std::cout << "== " << name << " ==" << std::endl;
        ++total;
        if (fn()) { ++passed; std::cout << "  PASSED" << std::endl; }
    };
    run("test_write_read_roundtrip", test_write_read_roundtrip);
    run("test_open_capacity_mismatch_rejected", test_open_capacity_mismatch_rejected);
    run("test_wrap_boundary_exact_fit", test_wrap_boundary_exact_fit);
    run("test_exact_fill_wrap_consistency", test_exact_fill_wrap_consistency);
    run("test_multi_writer_no_tear", test_multi_writer_no_tear);
    std::cout << "=== Results: " << passed << "/" << total << " passed ===" << std::endl;
    return (passed == total) ? 0 : 1;
}
