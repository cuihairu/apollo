// net M1 内核测试（LinkTests）。
//
// 增量 1 覆盖（帧定界）：
//   1. CRC32C 已知向量（"123456789" → 0xE3069283）与空串向量；
//   2. 编码→解析 round-trip（空载/载荷/control 旗标/多帧连发）；
//   3. 流式喂入：逐字节喂、任意切分喂，与一次整喂等价；
//   4. 坏头重同步：magic 破坏 / 头校验破坏 → 逐字节扫描丢弃至下一好帧，
//      resync_dropped 记账；
//   5. 载荷尾校验破坏 → fatal（链路须断连）；
//   6. len 超上限 → fatal；
//   7. 头校验护 len：改 len 字节必被 hcrc 拦下（重同步而非误读）。

#include "apollo/net/link/frame.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

using namespace apollo::net::link;

bool test_crc32c_vectors() {
    // Castagnoli 标准检查向量。
    const std::string check = "123456789";
    TEST_ASSERT(crc32c(reinterpret_cast<const std::uint8_t*>(check.data()), check.size()) ==
                    0xE3069283u,
                "crc32c check vector");
    TEST_ASSERT(crc32c(nullptr, 0) == 0x00000000u, "crc32c empty vector");
    const std::string check32 = "The quick brown fox jumps over the lazy dog";
    TEST_ASSERT(crc32c(reinterpret_cast<const std::uint8_t*>(check32.data()), check32.size()) ==
                    0x22620404u,
                "crc32c fox vector");
    return true;
}

bool test_roundtrip() {
    std::vector<std::uint8_t> wire;
    const std::vector<std::uint8_t> p1 = {'h', 'e', 'l', 'l', 'o'};
    const std::vector<std::uint8_t> p2;  // 空载荷
    std::vector<std::uint8_t> p3(300);
    for (std::size_t i = 0; i < p3.size(); ++i) {
        p3[i] = static_cast<std::uint8_t>(i * 7);
    }
    append_frame(wire, 1, 0, p1);
    append_frame(wire, 2, kFlagControl, p2);
    append_frame(wire, 3, 0, p3);

    FrameParser parser;
    ParseOutcome out = parser.feed(wire);
    TEST_ASSERT(!out.fatal, "roundtrip not fatal");
    TEST_ASSERT(out.frames.size() == 3, "three frames parsed");
    TEST_ASSERT(out.resync_dropped == 0, "no resync on clean stream");
    TEST_ASSERT(out.frames[0].seq == 1 && out.frames[0].payload == p1 && !is_control(out.frames[0]),
                "frame 1 payload");
    TEST_ASSERT(out.frames[1].seq == 2 && out.frames[1].payload.empty() && is_control(out.frames[1]),
                "frame 2 control empty");
    TEST_ASSERT(out.frames[2].seq == 3 && out.frames[2].payload == p3, "frame 3 large payload");
    TEST_ASSERT(parser.buffered() == 0, "no residue after full parse");
    return true;
}

bool test_stream_split_feed() {
    std::vector<std::uint8_t> wire;
    std::vector<std::uint8_t> payload(1000);
    for (std::size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<std::uint8_t>(i ^ 0x5A);
    }
    append_frame(wire, 7, 0, payload);
    append_frame(wire, 8, kFlagControl, std::vector<std::uint8_t>{1, 2, 3});

    // 逐字节喂与整喂必须等价。
    FrameParser byte_wise;
    std::vector<Frame> collected;
    for (std::uint8_t b : wire) {
        ParseOutcome o = byte_wise.feed(&b, 1);
        TEST_ASSERT(!o.fatal, "byte-wise feed not fatal");
        TEST_ASSERT(o.resync_dropped == 0, "byte-wise no resync");
        for (auto& f : o.frames) {
            collected.push_back(std::move(f));
        }
    }
    TEST_ASSERT(collected.size() == 2, "byte-wise two frames");
    TEST_ASSERT(collected[0].seq == 7 && collected[0].payload.size() == 1000, "frame A");
    TEST_ASSERT(collected[1].seq == 8 && is_control(collected[1]), "frame B");

    // 奇怪切分（7/13/1...）同样等价。
    FrameParser odd;
    std::size_t off = 0;
    std::size_t total_frames = 0;
    const std::size_t steps[] = {7, 13, 1, 500, 64, 3, 4096};
    std::size_t step = 0;
    while (off < wire.size()) {
        const std::size_t n = std::min(steps[step % 7], wire.size() - off);
        ParseOutcome o = odd.feed(wire.data() + off, n);
        TEST_ASSERT(!o.fatal, "odd split not fatal");
        total_frames += o.frames.size();
        off += n;
        ++step;
    }
    TEST_ASSERT(total_frames == 2, "odd split two frames");
    return true;
}

bool test_resync_on_bad_magic() {
    std::vector<std::uint8_t> wire;
    const std::vector<std::uint8_t> p = {9, 8, 7};
    append_frame(wire, 5, 0, p);
    // 头部插入 5 字节垃圾 + 半个魔数。
    const std::uint8_t garbage[] = {0x00, 0xFF, 0x50, 0x41, 0x01};
    FrameParser parser;
    ParseOutcome o1 = parser.feed(garbage, sizeof(garbage));
    TEST_ASSERT(!o1.fatal && o1.frames.empty(), "garbage only: no frames no fatal");
    ParseOutcome o2 = parser.feed(wire);
    TEST_ASSERT(!o2.fatal, "resync feed not fatal");
    TEST_ASSERT(o2.frames.size() == 1 && o2.frames[0].seq == 5, "good frame recovered");
    TEST_ASSERT(o2.resync_dropped == 5, "resync drop count");
    return true;
}

bool test_resync_on_header_crc_corrupt() {
    std::vector<std::uint8_t> wire;
    append_frame(wire, 11, 0, std::vector<std::uint8_t>{'x'});
    // 破坏头内 len 字节（offset 8）——hcrc 必拦，走重同步不误读。
    wire[8] ^= 0xFF;
    std::vector<std::uint8_t> wire2;
    append_frame(wire2, 12, 0, std::vector<std::uint8_t>{'y'});
    wire.insert(wire.end(), wire2.begin(), wire2.end());

    FrameParser parser;
    ParseOutcome out = parser.feed(wire);
    TEST_ASSERT(!out.fatal, "header corruption resyncs, not fatal");
    TEST_ASSERT(out.frames.size() == 1 && out.frames[0].seq == 12, "second frame recovered");
    // 帧 1 全长（16B 头 + 1B 载荷 + 4B 尾校验）被逐字节扫描丢弃。
    TEST_ASSERT(out.resync_dropped == kFrameHeaderSize + 1 + kFrameTailSize,
                "first frame dropped byte-wise");
    return true;
}

bool test_payload_crc_fatal() {
    std::vector<std::uint8_t> wire;
    append_frame(wire, 21, 0, std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7, 8});
    wire[kFrameHeaderSize] ^= 0x01;  // 破坏载荷首字节（尾校验必不过）

    FrameParser parser;
    ParseOutcome out = parser.feed(wire);
    TEST_ASSERT(out.fatal, "payload corruption is fatal");
    TEST_ASSERT(out.frames.empty(), "no frames on fatal");
    return true;
}

bool test_oversize_len_fatal() {
    std::vector<std::uint8_t> wire;
    append_frame(wire, 31, 0, std::vector<std::uint8_t>{1});
    wire[8] = 0xFF;  // len 低字节
    wire[9] = 0xFF;
    wire[10] = 0x10;  // len ≈ 0x10FFFF > 1 MiB 上限
    // 重算 hcrc 使头校验通过（直达 len 检查）。
    const std::uint32_t hc = crc32c(wire.data(), 14);
    wire[14] = static_cast<std::uint8_t>(hc & 0xFF);
    wire[15] = static_cast<std::uint8_t>((hc >> 8) & 0xFF);

    FrameParser parser;
    ParseOutcome out = parser.feed(wire);
    TEST_ASSERT(out.fatal, "oversize len is fatal");
    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"crc32c_vectors", test_crc32c_vectors},
        {"roundtrip", test_roundtrip},
        {"stream_split_feed", test_stream_split_feed},
        {"resync_on_bad_magic", test_resync_on_bad_magic},
        {"resync_on_header_crc_corrupt", test_resync_on_header_crc_corrupt},
        {"payload_crc_fatal", test_payload_crc_fatal},
        {"oversize_len_fatal", test_oversize_len_fatal},
    };
    int failed = 0;
    for (const auto& c : cases) {
        if (!c.fn()) {
            std::cerr << "CASE FAILED: " << c.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "LinkTests: all " << (sizeof(cases) / sizeof(cases[0])) << " cases passed"
                  << std::endl;
        return 0;
    }
    std::cout << "LinkTests: " << failed << " case(s) failed" << std::endl;
    return 1;
}
