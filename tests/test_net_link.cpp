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

#include "apollo/net/link/dispatcher.hpp"
#include "apollo/net/link/frame.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/watermark.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
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

// ---- 增量 2：契约路由表 + 按 id 分发 ----

using namespace apollo::net::link;

RouteEntry make_entry(std::uint16_t id, const char* domain) {
    RouteEntry e;
    e.id = id;
    e.name = "msg_" + std::to_string(id);
    e.dir = "P2P";
    e.channel = "events";
    e.domain = domain;
    e.binding = "native";
    return e;
}

bool test_route_table_basics() {
    RouteTable t;
    TEST_ASSERT(t.add(make_entry(10, "client")), "add 10");
    TEST_ASSERT(t.add(make_entry(900, "internal")), "add 900");
    TEST_ASSERT(!t.add(make_entry(10, "client")), "dup id rejected");
    TEST_ASSERT(t.size() == 2, "table size");

    const RouteEntry* e10 = t.find(10);
    TEST_ASSERT(e10 != nullptr && e10->domain == "client", "find 10");
    const RouteEntry* e900 = t.find(900);
    TEST_ASSERT(e900 != nullptr && e900->domain == "internal", "find 900");
    TEST_ASSERT(t.find(999) == nullptr, "miss returns null");
    return true;
}

bool test_route_version_from_hash() {
    const std::string h1 = "132a03c831fc18630207e5fb22272df7f97275c0abf85b32b22f75955f44a874";
    const std::string h2 = "c7b4dac8bdfa7b782f910ec2c4b5f452c1f47bcf9566d8dd4caca83d4658eaff";
    const std::uint64_t v1 = route_version_from_hash(h1);
    const std::uint64_t v2 = route_version_from_hash(h2);
    TEST_ASSERT(v1 == route_version_from_hash(h1), "deterministic");
    TEST_ASSERT(v1 != v2, "different hashes differ");
    // 首 16 hex = 132a03c831fc1863 → 大端 u64。
    TEST_ASSERT(v1 == 0x132a03c831fc1863ull, "big-endian prefix value");
    TEST_ASSERT(route_version_from_hash("") == 0, "empty hash safe");
    TEST_ASSERT(route_version_from_hash("zzzz") == 0, "non-hex safe");
    return true;
}

bool test_dispatcher_gates() {
    RouteTable t;
    t.add(make_entry(900, "internal"));
    t.add(make_entry(901, "internal"));
    t.add(make_entry(10, "client"));

    Dispatcher d;
    d.set_route_table(&t);

    std::uint16_t got_id = 0;
    std::size_t got_size = 100;
    int hits = 0;
    d.set_handler(900, [&](std::uint16_t id, const std::uint8_t* data, std::size_t size) {
        got_id = id;
        got_size = size;
        ++hits;
    });

    // 无白名单 = 机制默认开放：命中。
    const std::uint8_t body[] = {1, 2, 3};
    TEST_ASSERT(d.dispatch(900, body, sizeof(body)) == DispatchResult::kDispatched,
                "dispatch hit without filter");
    TEST_ASSERT(hits == 1 && got_id == 900 && got_size == 3, "handler args");

    // 域白名单收口后：internal 过闸，client 拒。
    d.allow_domain("internal");
    TEST_ASSERT(d.dispatch(900, nullptr, 0) == DispatchResult::kDispatched, "internal passes");
    TEST_ASSERT(d.dispatch(10, nullptr, 0) == DispatchResult::kDomainForbidden,
                "client forbidden");
    TEST_ASSERT(d.dispatch(777, nullptr, 0) == DispatchResult::kUnknownId, "unknown id");
    TEST_ASSERT(d.dispatch(901, nullptr, 0) == DispatchResult::kNoHandler, "table hit no handler");

    const auto& st = d.stats();
    TEST_ASSERT(st.dispatched == 2 && st.domain_forbidden == 1 && st.unknown_id == 1 &&
                    st.no_handler == 1,
                "stats counted");
    return true;
}

bool test_dispatcher_no_table_rejects() {
    Dispatcher d;
    TEST_ASSERT(d.dispatch(1, nullptr, 0) == DispatchResult::kUnknownId,
                "no table = reject, not crash");
    return true;
}

bool test_load_golden_route_json() {
#ifndef APOLLO_CONTRACT_ROUTE_JSON
    TEST_ASSERT(false, "golden path not defined");
#else
    std::ifstream in(APOLLO_CONTRACT_ROUTE_JSON);
    TEST_ASSERT(in.good(), "golden route file opens");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    TEST_ASSERT(!text.empty(), "golden route file non-empty");

    RouteTable t;
    TEST_ASSERT(load_route_table_json(text, t), "golden parses");
    TEST_ASSERT(t.size() >= 20, "route count sane");
    TEST_ASSERT(!t.schema_hash().empty() && t.schema_hash().size() == 64, "schema hash present");
    TEST_ASSERT(!t.internal_hash().empty(), "internal hash present");

    // 契约锚点：internal 域 900 session_up 在表（P1-2 目录事件族）。
    const RouteEntry* e = t.find(900);
    TEST_ASSERT(e != nullptr && e->name == "session_up" && e->domain == "internal",
                "session_up internal anchored");
    // client 域锚点：10 move。
    const RouteEntry* m = t.find(10);
    TEST_ASSERT(m != nullptr && m->name == "move" && m->domain == "client", "move client anchored");

    // 坏结构整表拒。
    RouteTable bad;
    TEST_ASSERT(!load_route_table_json("{\"routes\": []}", bad), "empty routes rejected");
    TEST_ASSERT(!load_route_table_json("not json", bad), "garbage rejected");
    TEST_ASSERT(!load_route_table_json(
                    "{\"schema_hash\":\"a\",\"client_hash\":\"b\",\"internal_hash\":\"c\","
                    "\"routes\":[{\"id\":1},{\"id\":1}]}",
                    bad),
                "dup id in artifact rejected");
    return true;
#endif
}

// ---- 增量 3：背压水位 ----

bool test_watermark_levels() {
    WatermarkConfig cfg;
    cfg.capacity_bytes = 1000;  // 小容量好算：1‰ = 1B
    const Watermark wm(cfg);

    TEST_ASSERT(wm.level(0) == WatermarkLevel::kOk, "empty ok");
    TEST_ASSERT(wm.level(499) == WatermarkLevel::kOk, "499 ok");
    TEST_ASSERT(wm.level(500) == WatermarkLevel::kSoft, "500 soft boundary inclusive");
    TEST_ASSERT(wm.level(749) == WatermarkLevel::kSoft, "749 soft");
    TEST_ASSERT(wm.level(750) == WatermarkLevel::kHard, "750 hard boundary inclusive");
    TEST_ASSERT(wm.level(949) == WatermarkLevel::kHard, "949 hard");
    TEST_ASSERT(wm.level(950) == WatermarkLevel::kCut, "950 cut boundary inclusive");
    TEST_ASSERT(wm.level(1000) == WatermarkLevel::kCut, "full cut");
    TEST_ASSERT(wm.level(1001) == WatermarkLevel::kCut, "overfull cut");
    return true;
}

bool test_watermark_admit_matrix() {
    WatermarkConfig cfg;
    cfg.capacity_bytes = 1000;
    const Watermark wm(cfg);

    // Reliable：cut 才拒。
    TEST_ASSERT(wm.admit(WatermarkLevel::kOk, SendClass::kReliable, 10) ==
                SendCode::kAccepted, "reliable ok");
    TEST_ASSERT(wm.admit(WatermarkLevel::kHard, SendClass::kReliable, 10) ==
                SendCode::kAccepted, "reliable hard accepted");
    TEST_ASSERT(wm.admit(WatermarkLevel::kCut, SendClass::kReliable, 10) ==
                SendCode::kBackPressured, "reliable cut rejected");

    // Droppable：soft 起拒新。
    TEST_ASSERT(wm.admit(WatermarkLevel::kOk, SendClass::kDroppable, 10) ==
                SendCode::kAccepted, "droppable ok");
    TEST_ASSERT(wm.admit(WatermarkLevel::kSoft, SendClass::kDroppable, 10) ==
                SendCode::kBackPressured, "droppable soft rejected");
    TEST_ASSERT(wm.admit(WatermarkLevel::kCut, SendClass::kDroppable, 10) ==
                SendCode::kBackPressured, "droppable cut rejected");

    // Control：主闸永远放行（独立预算在 admit_control）。
    TEST_ASSERT(wm.admit(WatermarkLevel::kCut, SendClass::kControl, 10) ==
                SendCode::kAccepted, "control bypasses main gate");

    // Control 独立预算：预算内放行、超预算拒（主队列满不满无关）。
    TEST_ASSERT(wm.admit_control(0, 0, 100) == SendCode::kAccepted, "control reserve ok");
    TEST_ASSERT(wm.admit_control(0, 15 * 1024 + 1, 1024) == SendCode::kBackPressured,
                "control reserve exhausted");
    TEST_ASSERT(wm.admit_control(1000, 0, 16) == SendCode::kAccepted,
                "control ok even at main cut");
    return true;
}

bool test_cut_sustainer() {
    CutSustainer s;
    TEST_ASSERT(!s.sustained_for(1000, 500), "not cutting");

    s.update(true, 1000);  // 进入 cut @1000ms
    TEST_ASSERT(!s.sustained_for(1499, 500), "1499ms not sustained");
    TEST_ASSERT(s.sustained_for(1500, 500), "1500ms sustained");

    s.update(false, 1600);  // 退出 cut 复位
    TEST_ASSERT(!s.sustained_for(5000, 500), "reset after leaving cut");

    s.update(true, 2000);
    s.update(true, 2100);  // 重复 update 不改起点
    TEST_ASSERT(s.sustained_for(2500, 500), "sustain anchored at first cut");
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
        {"route_table_basics", test_route_table_basics},
        {"route_version_from_hash", test_route_version_from_hash},
        {"dispatcher_gates", test_dispatcher_gates},
        {"dispatcher_no_table_rejects", test_dispatcher_no_table_rejects},
        {"load_golden_route_json", test_load_golden_route_json},
        {"watermark_levels", test_watermark_levels},
        {"watermark_admit_matrix", test_watermark_admit_matrix},
        {"cut_sustainer", test_cut_sustainer},
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
