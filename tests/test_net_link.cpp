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
#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/link/tcp_transport.hpp"
#include "apollo/net/link/watermark.hpp"

#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
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

// ---- 增量 4：InterServerLink（握手 / 分发 / 可靠重发 / 背压 / 断连可检）----

using LinkClock = std::uint64_t;  // 单调毫秒（注入）

struct SinkRecorder {
    int ups = 0;
    int downs = 0;
    LinkDownReason last_reason = LinkDownReason::kLocalClose;
    int restarts = 0;
    std::uint64_t restart_old = 0;
    std::uint64_t restart_new = 0;

    LinkEventSink sink() {
        LinkEventSink s;
        s.on_up = [this] { ++ups; };
        s.on_down = [this](LinkDownReason r) {
            ++downs;
            last_reason = r;
        };
        s.on_peer_restart = [this](std::uint64_t o, std::uint64_t n) {
            ++restarts;
            restart_old = o;
            restart_new = n;
        };
        return s;
    }
};

struct RxLog {
    std::vector<std::string> got;

    Dispatcher::Handler handler() {
        return [this](std::uint16_t id, const std::uint8_t* data, std::size_t size) {
            got.emplace_back(std::to_string(id) + ":" +
                             std::string(reinterpret_cast<const char*>(data), size));
        };
    }
};

LinkConfig make_link_config(const char* name, std::uint64_t route_version,
                            std::uint64_t epoch = 1) {
    LinkConfig c;
    c.self_name = name;
    c.route_version = route_version;
    c.authority_epoch = epoch;
    return c;
}

RouteTable make_dispatch_table() {
    RouteTable t;
    t.add({901, "test.echo", "P2P", "control", "internal", "native"});
    t.add({5, "test.pos", "C2S", "movement", "client", "native"});
    return t;
}

// 泵四拍：一拍送出、一拍处理回执、一拍收 ack，冗余一拍兜底。
void pump_rounds(InterServerLink& a, InterServerLink& b, LinkClock now) {
    for (int i = 0; i < 4; ++i) {
        const LinkClock t = now + static_cast<LinkClock>(i);
        a.pump(t);
        b.pump(t);
    }
}

bool up_both(InterServerLink& a, InterServerLink& b, LinkClock now) {
    pump_rounds(a, b, now);
    return a.state() == LinkState::kUp && b.state() == LinkState::kUp;
}

std::vector<std::uint8_t> tagged_payload(std::uint32_t index) {
    std::vector<std::uint8_t> p(12, 0xAB);
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>((index >> (8 * i)) & 0xFF);
    }
    return p;
}

bool test_link_handshake_dispatch() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 0x1122334455667788ull));
    InterServerLink b(make_link_config("game", 0x1122334455667788ull));
    SinkRecorder sa, sb;
    a.set_sink(sa.sink());
    b.set_sink(sb.sink());
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    b.dispatcher().allow_domain("internal");
    RxLog rx;
    b.dispatcher().set_handler(901, rx.handler());

    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "handshake completes both sides");
    TEST_ASSERT(sa.ups == 1 && sb.ups == 1, "on_up fired once each");
    TEST_ASSERT(a.peer_epoch() == 1 && b.peer_epoch() == 1, "epoch exchanged");

    const std::vector<std::uint8_t> body = {'h', 'i'};
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted,
                "reliable send accepted");
    pump_rounds(a, b, 2000);
    TEST_ASSERT(rx.got.size() == 1 && rx.got[0] == "901:hi", "dispatched once with body");
    TEST_ASSERT(b.stats().dispatched == 1, "dispatch mirror counted");
    TEST_ASSERT(a.stats().reliable_acked == 1 && a.reliable_pending() == 0,
                "ack round trip drained queue");

    // 域闸（§11.3）：client 域 id 未过 internal 白名单 → 拒收计数、不投 handler，
    // 但 reliable 语义照常回执（链路层送达与路由拒收是两件事）。
    TEST_ASSERT(a.send(5, body, SendClass::kReliable) == SendCode::kAccepted,
                "client-id send accepted");
    pump_rounds(a, b, 3000);
    TEST_ASSERT(rx.got.size() == 1, "forbidden domain not delivered");
    TEST_ASSERT(b.stats().dispatch_rejected == 1, "domain forbidden counted");
    TEST_ASSERT(a.stats().reliable_acked == 2, "rejected frame still acked");
    return true;
}

bool test_link_burst_no_loss_no_dup() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    InterServerLink b(make_link_config("game", 7));
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    b.dispatcher().allow_domain("internal");
    RxLog rx;
    b.dispatcher().set_handler(901, rx.handler());
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");

    constexpr int kCount = 200;
    for (std::uint32_t i = 0; i < kCount; ++i) {
        TEST_ASSERT(a.send(901, tagged_payload(i), SendClass::kReliable) == SendCode::kAccepted,
                    "burst send accepted");
    }
    pump_rounds(a, b, 2000);
    TEST_ASSERT(rx.got.size() == kCount, "all frames delivered");
    TEST_ASSERT(std::set<std::string>(rx.got.begin(), rx.got.end()).size() == kCount,
                "no duplicates");
    TEST_ASSERT(b.stats().dup_dropped == 0, "no dup on clean link");
    TEST_ASSERT(a.stats().reliable_acked == kCount, "all acked");
    TEST_ASSERT(a.reliable_pending() == 0 && a.queued_bytes() == 0, "queues empty");
    return true;
}

bool test_link_disconnect_detect() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    InterServerLink b(make_link_config("game", 7));
    SinkRecorder sa, sb;
    a.set_sink(sa.sink());
    b.set_sink(sb.sink());
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");

    b.close();
    TEST_ASSERT(sb.downs == 1, "closer sees down immediately");
    TEST_ASSERT(sb.last_reason == LinkDownReason::kLocalClose, "closer sees local close");
    TEST_ASSERT(sa.downs == 0, "a not notified before pumping");
    TEST_ASSERT(!a.pump(1200), "a pump reports down after peer close");
    TEST_ASSERT(sa.downs == 1 && sa.last_reason == LinkDownReason::kPeerClosed,
                "peer close detected");
    TEST_ASSERT(a.state() == LinkState::kDown, "state down");

    // 断连期间的发送语义（§5.7）：Droppable 即拒，Reliable 照常入队待重发。
    const std::vector<std::uint8_t> body = {'x'};
    TEST_ASSERT(a.send(901, body, SendClass::kDroppable) == SendCode::kBackPressured,
                "droppable rejected while down");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted,
                "reliable queued while down");
    TEST_ASSERT(a.reliable_pending() == 1, "reliable pending for redelivery");
    return true;
}

bool test_link_redelivery_exactly_once() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    InterServerLink b(make_link_config("game", 7));
    SinkRecorder sa, sb;
    a.set_sink(sa.sink());
    b.set_sink(sb.sink());
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    b.dispatcher().allow_domain("internal");
    RxLog rx;
    b.dispatcher().set_handler(901, rx.handler());
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");

    // m1 已送达 B 但 ack 在途中死于断连：A 侧保持未确认。
    const std::vector<std::uint8_t> body = {'m', '1'};
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "m1 accepted");
    TEST_ASSERT(a.pump(2000), "flush m1");
    TEST_ASSERT(b.pump(2001), "b consumes m1");
    TEST_ASSERT(rx.got.size() == 1, "m1 dispatched");
    a.close();  // A 不读 ack 即断——ack 随旧传输湮灭
    TEST_ASSERT(a.reliable_pending() == 1, "m1 still unacked");

    // 同 epoch 重连（Link 对象不换）：A 重发未确认帧，B 按去重水位判重。
    auto [ep2_a, ep2_b] = make_loopback_pair();
    a.attach(std::move(ep2_a), 3000);
    b.attach(std::move(ep2_b), 3000);
    pump_rounds(a, b, 3100);
    TEST_ASSERT(a.state() == LinkState::kUp && b.state() == LinkState::kUp, "reconnected");
    TEST_ASSERT(sa.restarts == 0 && sb.restarts == 0, "same epoch is not a restart");
    TEST_ASSERT(rx.got.size() == 1, "redelivered frame not dispatched twice");
    TEST_ASSERT(b.stats().dup_dropped == 1, "dup detected once");
    TEST_ASSERT(a.stats().reliable_acked == 1 && a.reliable_pending() == 0,
                "m1 exactly once end to end");
    return true;
}

bool test_link_peer_restart_epoch() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    auto b = std::make_unique<InterServerLink>(make_link_config("game", 7, 1));
    SinkRecorder sa;
    a.set_sink(sa.sink());
    a.dispatcher().set_route_table(&table);
    b->dispatcher().set_route_table(&table);
    b->dispatcher().allow_domain("internal");
    RxLog rx;
    b->dispatcher().set_handler(901, rx.handler());
    a.attach(std::move(ep_a), 1000);
    b->attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, *b, 1100), "up");
    const std::vector<std::uint8_t> body = {'m', '1'};
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "m1 accepted");
    pump_rounds(a, *b, 2000);
    TEST_ASSERT(rx.got.size() == 1, "m1 dispatched");

    // 对端重启：进程换代（epoch 2），Link 对象重建。A 断连期间排队的
    // reliable 在重启检出时清队（§5.7：老一代永不再 ack，上层裁决重发）。
    b->close();
    TEST_ASSERT(a.pump(3000) == false, "a sees peer close");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted,
                "m2 queued while down");
    TEST_ASSERT(a.reliable_pending() == 1, "m2 pending");

    auto [ep2_a, ep2_b] = make_loopback_pair();
    auto b2 = std::make_unique<InterServerLink>(make_link_config("game", 7, 2));
    RxLog rx2;
    b2->dispatcher().set_route_table(&table);
    b2->dispatcher().allow_domain("internal");
    b2->dispatcher().set_handler(901, rx2.handler());
    a.attach(std::move(ep2_a), 4000);
    b2->attach(std::move(ep2_b), 4000);
    pump_rounds(a, *b2, 4100);
    TEST_ASSERT(a.state() == LinkState::kUp, "a back up");
    TEST_ASSERT(sa.restarts == 1, "peer restart event fired");
    TEST_ASSERT(sa.restart_old == 1 && sa.restart_new == 2, "epoch pair reported");
    TEST_ASSERT(a.stats().reliable_cleared == 1 && a.reliable_pending() == 0,
                "stale generation queue cleared");
    TEST_ASSERT(rx2.got.empty(), "cleared frame never reached new generation");

    // 新一代去重水位已重置：新帧按序正常恰一次。
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "m3 accepted");
    pump_rounds(a, *b2, 5000);
    TEST_ASSERT(rx2.got.size() == 1, "m3 delivered once to new generation");
    TEST_ASSERT(b2->stats().dup_dropped == 0, "no false dup after restart");
    return true;
}

bool test_link_send_gates() {
    LinkConfig c = make_link_config("auth", 7);
    c.watermark.capacity_bytes = 1000;  // soft 500 / hard 750 / cut 950（千分比缺省）
    InterServerLink a(c);
    const std::vector<std::uint8_t> body(300, 'z');

    // 断连（kIdle）：Droppable 即拒；Reliable 入队（in-flight 表语义）。
    TEST_ASSERT(a.send(901, body, SendClass::kDroppable) == SendCode::kBackPressured,
                "droppable rejected while idle");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "r1 accepted");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "r2 accepted");
    TEST_ASSERT(a.queued_bytes() == 644, "two frames on queue");
    TEST_ASSERT(a.send(901, body, SendClass::kDroppable) == SendCode::kBackPressured,
                "droppable rejected at soft");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted,
                "reliable accepted at soft");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kBackPressured,
                "reliable frozen at cut");
    TEST_ASSERT(a.stats().send_rejected == 3, "rejections counted");

    // 连上并腾空后闸门复位。
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink b(make_link_config("game", 7));
    b.dispatcher().set_route_table(&table);
    b.dispatcher().allow_domain("internal");
    a.dispatcher().set_route_table(&table);
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    pump_rounds(a, b, 1100);
    TEST_ASSERT(a.stats().reliable_acked == 3, "queued frames delivered and acked");
    TEST_ASSERT(a.queued_bytes() == 0, "queue drained");
    TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted,
                "gate reopens after drain");
    return true;
}

bool test_link_cut_sustained_down() {
    LinkConfig c = make_link_config("auth", 7);
    c.watermark.capacity_bytes = 1000;
    c.cut_close_ms = 5000;
    c.heartbeat_timeout_ms = 60000;  // 隔离心跳，单测 cut 断连
    InterServerLink a(c);
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink b(make_link_config("game", 7));
    SinkRecorder sa;
    a.set_sink(sa.sink());
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");

    // 灌到 cut 并停泵对端（ack 永不到来）：占用持续 ≥95%。
    const std::vector<std::uint8_t> body(300, 'z');
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT(a.send(901, body, SendClass::kReliable) == SendCode::kAccepted, "fill");
    }
    TEST_ASSERT(a.pump(2000), "cut anchored");
    TEST_ASSERT(sa.downs == 0, "cut alone is not down");
    TEST_ASSERT(a.pump(4000), "sustained 2s alive");
    TEST_ASSERT(a.pump(6999), "sustained 4.999s alive");
    TEST_ASSERT(!a.pump(7000), "cut 5s -> down");
    TEST_ASSERT(sa.last_reason == LinkDownReason::kBackpressureCut, "backpressure cut reason");
    return true;
}

bool test_link_heartbeat_timeout() {
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    InterServerLink b(make_link_config("game", 7));
    SinkRecorder sa;
    a.set_sink(sa.sink());
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");
    TEST_ASSERT(a.pump(10000), "14s silent still alive");
    TEST_ASSERT(!a.pump(20000), "silent past timeout -> down");
    TEST_ASSERT(sa.last_reason == LinkDownReason::kHeartbeatTimeout, "heartbeat reason");
    return true;
}

bool test_link_route_version_mismatch() {
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 0xAAAA000000000000ull));
    InterServerLink b(make_link_config("game", 0xBBBB000000000000ull));
    SinkRecorder sa, sb;
    a.set_sink(sa.sink());
    b.set_sink(sb.sink());
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    pump_rounds(a, b, 1100);
    TEST_ASSERT(a.state() == LinkState::kDown && b.state() == LinkState::kDown,
                "mismatch kills both");
    // 失配由读到外来 Hello 的一侧检出；另一侧只见随之而来的对端关闭。
    TEST_ASSERT(sb.last_reason == LinkDownReason::kRouteVersionMismatch, "reader detects");
    TEST_ASSERT(sa.last_reason == LinkDownReason::kPeerClosed, "other side sees close");
    return true;
}

bool test_link_droppable_cleared_on_down() {
    RouteTable table = make_dispatch_table();
    auto [ep_a, ep_b] = make_loopback_pair();
    InterServerLink a(make_link_config("auth", 7));
    InterServerLink b(make_link_config("game", 7));
    SinkRecorder sa;
    a.set_sink(sa.sink());
    a.dispatcher().set_route_table(&table);
    b.dispatcher().set_route_table(&table);
    a.attach(std::move(ep_a), 1000);
    b.attach(std::move(ep_b), 1000);
    TEST_ASSERT(up_both(a, b, 1100), "up");

    // 排队未刷的 droppable 在断连即弃（§5.7 OneWay 默认丢弃），计数可查。
    const std::vector<std::uint8_t> body = {'d'};
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT(a.send(901, body, SendClass::kDroppable) == SendCode::kAccepted,
                    "droppable queued");
    }
    a.close();
    TEST_ASSERT(sa.last_reason == LinkDownReason::kLocalClose, "local close");
    TEST_ASSERT(a.stats().droppable_dropped == 3, "queued droppable discarded on down");
    TEST_ASSERT(a.queued_bytes() == 0, "droppable queue cleared");
    return true;
}

// ---- 增量 5：TCP 底座（TcpTransport / TcpConnector / TcpListener）----

#if !defined(_WIN32)

bool test_tcp_raw_echo() {
    TcpListener listener;
    TEST_ASSERT(listener.start("127.0.0.1", 0), "listener binds ephemeral port");
    TEST_ASSERT(listener.port() != 0, "bound port observed");

    TcpConnector connector;
    TEST_ASSERT(connector.start("127.0.0.1", listener.port(), 3000, 1000), "connect started");
    TcpConnector::PumpResult r = TcpConnector::PumpResult::kInProgress;
    for (int i = 0; i < 100 && r == TcpConnector::PumpResult::kInProgress; ++i) {
        r = connector.pump(1000 + static_cast<std::uint64_t>(i) * 10);
    }
    TEST_ASSERT(r == TcpConnector::PumpResult::kConnected, "loopback connects");
    auto client = connector.take_transport();
    TEST_ASSERT(client != nullptr, "transport handed out");
    TEST_ASSERT(connector.state() == ConnectState::kIdle, "connector released fd");

    auto server = listener.accept();
    TEST_ASSERT(server != nullptr, "accept yields transport");

    const std::vector<std::uint8_t> payload = {'p', 'i', 'n', 'g'};
    TEST_ASSERT(client->write(payload.data(), payload.size()) ==
                static_cast<int>(payload.size()), "client write");
    std::uint8_t buf[16] = {};
    int n = 0;
    for (int i = 0; i < 100 && n == 0; ++i) {
        n = server->read(buf, sizeof buf);
    }
    TEST_ASSERT(n == 4 && std::memcmp(buf, "ping", 4) == 0, "server reads payload");

    TEST_ASSERT(server->write(payload.data(), payload.size()) == 4, "server write");
    n = 0;
    for (int i = 0; i < 100 && n == 0; ++i) {
        n = client->read(buf, sizeof buf);
    }
    TEST_ASSERT(n == 4 && std::memcmp(buf, "ping", 4) == 0, "client reads echo");

    // EOF 语义：对端 close 后读侧见 -1。
    server->close();
    n = 0;
    for (int i = 0; i < 100 && n == 0; ++i) {
        n = client->read(buf, sizeof buf);
    }
    TEST_ASSERT(n == -1, "peer close surfaces as EOF");
    return true;
}

bool test_tcp_connector_refused() {
    // 占一个端口再关掉：本机无人监听的端口（端口重用窗口内）。
    std::uint16_t dead_port = 0;
    {
        TcpListener probe;
        TEST_ASSERT(probe.start("127.0.0.1", 0), "probe binds");
        dead_port = probe.port();
    }
    TcpConnector connector;
    TEST_ASSERT(connector.start("127.0.0.1", dead_port, 3000, 1000), "connect started");
    TcpConnector::PumpResult r = TcpConnector::PumpResult::kInProgress;
    for (int i = 0; i < 100 && r == TcpConnector::PumpResult::kInProgress; ++i) {
        r = connector.pump(1000 + static_cast<std::uint64_t>(i) * 10);
    }
    TEST_ASSERT(r == TcpConnector::PumpResult::kFailed, "refused connection fails");
    TEST_ASSERT(connector.failure() == ConnectFailure::kNoSuchPort, "refused -> no such port");
    TEST_ASSERT(connector.take_transport() == nullptr, "no transport on failure");
    return true;
}

bool test_tcp_connector_timeout() {
    // RFC1918 非路由地址：SYN 无响应，触发一次性超时（沙箱直接不可达时
    // 退化为立即 kNoSuchPort——同样收口为类型化失败，两态都算通过）。
    TcpConnector connector;
    TEST_ASSERT(connector.start("10.255.255.1", 81, 100, 1000), "connect started");
    TcpConnector::PumpResult r = TcpConnector::PumpResult::kInProgress;
    for (int i = 0; i < 100 && r == TcpConnector::PumpResult::kInProgress; ++i) {
        r = connector.pump(1000 + static_cast<std::uint64_t>(i) * 10);
    }
    TEST_ASSERT(r == TcpConnector::PumpResult::kFailed, "unreachable target fails");
    TEST_ASSERT(connector.failure() == ConnectFailure::kTimedOut ||
                    connector.failure() == ConnectFailure::kNoSuchPort,
                "typed failure reason");
    return true;
}

bool test_tcp_connector_bad_host() {
    TcpConnector connector;
    // 空主机名：getaddrinfo 按契约必败——不依赖解析器行为（沙箱 fake-ip
    // 通配解析会把任意假域名映射到 198.18.0.0/15，域名类负例不可测）。
    TEST_ASSERT(!connector.start("", 80, 1000, 1000),
                "unresolvable host fails at start");
    TEST_ASSERT(connector.state() == ConnectState::kFailed, "state failed");
    TEST_ASSERT(connector.failure() == ConnectFailure::kSystemError, "dns -> system error");
    TEST_ASSERT(connector.pump(2000) == TcpConnector::PumpResult::kFailed, "stays failed");
    return true;
}

bool test_tcp_link_pair_e2e() {
    // 全链路：listener + connector 产出 TCP 传输 → InterServerLink 握手 →
    // 可靠业务帧恰一次（真实 socket，非 loopback 缝）。
    RouteTable table = make_dispatch_table();
    TcpListener listener;
    TEST_ASSERT(listener.start("127.0.0.1", 0), "listener up");

    InterServerLink b(make_link_config("game", 7));
    b.dispatcher().set_route_table(&table);
    b.dispatcher().allow_domain("internal");
    RxLog rx;
    b.dispatcher().set_handler(901, rx.handler());

    InterServerLink a(make_link_config("auth", 7));
    a.dispatcher().set_route_table(&table);

    TcpConnector connector;
    TEST_ASSERT(connector.start("127.0.0.1", listener.port(), 3000, 1000), "connect started");
    // 交替泵：连接器、监听器、两侧链路共用一个宿主泵节拍。
    for (int i = 0; i < 200 && a.state() == LinkState::kIdle; ++i) {
        const std::uint64_t t = 1000 + static_cast<std::uint64_t>(i) * 5;
        connector.pump(t);
        if (auto t_a = connector.take_transport()) {
            a.attach(std::move(t_a), t);
        }
    }
    for (int i = 0; i < 200 && b.state() == LinkState::kIdle; ++i) {
        const std::uint64_t t = 1500 + static_cast<std::uint64_t>(i) * 5;
        if (auto t_b = listener.accept()) {
            b.attach(std::move(t_b), t);
        }
    }
    for (int i = 0; i < 200 && (a.state() != LinkState::kUp || b.state() != LinkState::kUp); ++i) {
        const std::uint64_t t = 2000 + static_cast<std::uint64_t>(i) * 5;
        a.pump(t);
        b.pump(t);
    }
    TEST_ASSERT(a.state() == LinkState::kUp && b.state() == LinkState::kUp, "up over TCP");

    // 正向一帧：恰一次到 B。
    TEST_ASSERT(a.send(901, std::vector<std::uint8_t>{'t', 'c', 'p'}, SendClass::kReliable) ==
                SendCode::kAccepted, "forward send accepted");
    for (int i = 0; i < 200 && rx.got.empty(); ++i) {
        const std::uint64_t t = 2500 + static_cast<std::uint64_t>(i) * 5;
        a.pump(t);
        b.pump(t);
    }
    TEST_ASSERT(rx.got.size() == 1 && rx.got[0] == "901:tcp", "reliable frame delivered");

    // 反向一帧 + 双向 ack 出队收尾。
    TEST_ASSERT(b.send(901, std::vector<std::uint8_t>{'o', 'k'}, SendClass::kReliable) ==
                SendCode::kAccepted, "reverse send accepted");
    for (int i = 0; i < 50; ++i) {
        const std::uint64_t t = 3000 + static_cast<std::uint64_t>(i) * 5;
        a.pump(t);
        b.pump(t);
        if (b.stats().reliable_acked == 1) {
            break;
        }
    }
    TEST_ASSERT(b.stats().reliable_acked == 1, "reverse ack round trip");
    TEST_ASSERT(a.stats().reliable_acked == 1, "forward ack round trip");
    return true;
}

#endif  // !_WIN32

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
        {"link_handshake_dispatch", test_link_handshake_dispatch},
        {"link_burst_no_loss_no_dup", test_link_burst_no_loss_no_dup},
        {"link_disconnect_detect", test_link_disconnect_detect},
        {"link_redelivery_exactly_once", test_link_redelivery_exactly_once},
        {"link_peer_restart_epoch", test_link_peer_restart_epoch},
        {"link_send_gates", test_link_send_gates},
        {"link_cut_sustained_down", test_link_cut_sustained_down},
        {"link_heartbeat_timeout", test_link_heartbeat_timeout},
        {"link_route_version_mismatch", test_link_route_version_mismatch},
        {"link_droppable_cleared_on_down", test_link_droppable_cleared_on_down},
#if !defined(_WIN32)
        {"tcp_raw_echo", test_tcp_raw_echo},
        {"tcp_connector_refused", test_tcp_connector_refused},
        {"tcp_connector_timeout", test_tcp_connector_timeout},
        {"tcp_connector_bad_host", test_tcp_connector_bad_host},
        {"tcp_link_pair_e2e", test_tcp_link_pair_e2e},
#endif
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
