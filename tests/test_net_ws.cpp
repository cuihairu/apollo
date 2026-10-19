// WebSocket 编解码测试（WsTests；net M1 内核之上的纯编解码件——零 socket
// 零线程，传输与生命周期归宿主泵）。
//
// 覆盖（RFC 6455 已知向量 + 增量语义 + 协议负例）：
//   1. Sec-WebSocket-Accept 已知向量（§1.3 样例 key → s3pPLMBiTxaQ9kYGzzhZRbK+xOo=）；
//   2. 升级请求解析：完整 GET + 头部（method/path/key/version/Upgrade/
//      Connection token）；逐字节/任意切分增量喂入等价；头部超上限判坏；
//   3. 升级校验负例：POST/缺 key/key 非 24 位 base64/版本非 13/缺
//      Upgrade 头/Connection 无 upgrade token → 非法升级请求；
//   4. 101 应答形态（含算出的 Accept、头部终止）与拒绝应答形态；
//   5. 帧编码：无掩码文本帧逐字节对 RFC §5.7 向量；掩码向量解析回
//      "Hello"；服务端出帧不掩码；
//   6. 分片消息拼装（continuation，控制帧可穿插且不被分片）；
//   7. ping → pong 回执、close → peer_closed + close 回执字节；
//   8. 扩展长度路径：ext16（300B）与 ext64（70000B）掩码帧 round-trip；
//   9. 协议负例：RSV 位/客户端无掩码/控制帧分片/保留 opcode/声明超限
//      → fatal（语义：断连，无重同步）；
//  10. 增量喂入等价：任意切分与一次整喂同果；
//  11. 头部同 chunk 粘帧：就绪后残余完整移交帧层（快客户端粘发态）。

#include "apollo/net/ws/frame.hpp"
#include "apollo/net/ws/handshake.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                                      \
            return false;                                                                                \
        }                                                                                                \
    } while (0)

namespace {

namespace ws = apollo::net::ws;

// ---- 1. Accept 已知向量 ----

bool test_accept_key_vector() {
    const std::string accept = ws::compute_accept_key("dGhlIHNhbXBsZSBub25jZQ==");
    TEST_ASSERT(accept == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "RFC 6455 §1.3 已知向量");
    TEST_ASSERT(ws::compute_accept_key("").empty(), "空 key 不产出假应答");
    return true;
}

// ---- 2. 升级请求解析 ----

const char* kGoodUpgrade =
    "GET /game HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "Upgrade: websocket\r\n"
    "Connection: keep-alive, Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";

bool test_handshake_parse() {
    ws::HandshakeParser parser;
    const auto* p = reinterpret_cast<const std::uint8_t*>(kGoodUpgrade);
    const auto state = parser.feed(p, std::strlen(kGoodUpgrade));
    TEST_ASSERT(state == ws::HandshakeParser::State::kReady, "完整请求一次整喂即就绪");
    const auto& req = parser.request();
    TEST_ASSERT(req.method == "GET", "method 提取");
    TEST_ASSERT(req.path == "/game", "path 提取");
    TEST_ASSERT(req.key == "dGhlIHNhbXBsZSBub25jZQ==", "key 提取");
    TEST_ASSERT(req.version == 13, "version 提取");
    TEST_ASSERT(req.upgrade_websocket, "Upgrade: websocket 命中（大小写不敏感）");
    TEST_ASSERT(req.connection_upgrade, "Connection 含 upgrade token");

    // 增量：逐字节喂到倒数第二字节仍 NeedMore，最后一字节才 Ready。
    ws::HandshakeParser inc;
    const std::size_t len = std::strlen(kGoodUpgrade);
    for (std::size_t i = 0; i + 1 < len; ++i) {
        const auto st = inc.feed(p + i, 1);
        TEST_ASSERT(st == ws::HandshakeParser::State::kNeedMore,
                    "未收完头部一律 NeedMore");
    }
    TEST_ASSERT(inc.feed(p + len - 1, 1) == ws::HandshakeParser::State::kReady,
                "末字节就绪");
    TEST_ASSERT(inc.request().path == "/game", "增量与整喂同果");

    // 头部超上限（8 KiB 无终止）→ 判坏。
    ws::HandshakeParser cap;
    std::string big(9000, 'x');
    const auto* b = reinterpret_cast<const std::uint8_t*>(big.data());
    TEST_ASSERT(cap.feed(b, big.size()) == ws::HandshakeParser::State::kBad,
                "头部超上限判坏");

    // 大小写不敏感：头名与 token 值都换大小写。
    const char* cased =
        "GET /g HTTP/1.1\r\n"
        "UPGRADE: WebSocket\r\n"
        "CONNECTION: UPGRADE\r\n"
        "SEC-WEBSOCKET-KEY: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "SEC-WEBSOCKET-VERSION: 13\r\n\r\n";
    ws::HandshakeParser mixed;
    const auto* m = reinterpret_cast<const std::uint8_t*>(cased);
    TEST_ASSERT(mixed.feed(m, std::strlen(cased)) == ws::HandshakeParser::State::kReady,
                "大小写混合头可解析");
    TEST_ASSERT(mixed.request().upgrade_websocket && mixed.request().connection_upgrade,
                "大小写不敏感命中");
    return true;
}

// ---- 3. 升级校验负例 ----

bool test_handshake_validation() {
    auto validate = [](const std::string& raw) {
        ws::HandshakeParser parser;
        const auto* p = reinterpret_cast<const std::uint8_t*>(raw.data());
        const auto state = parser.feed(p, raw.size());
        if (state != ws::HandshakeParser::State::kReady) {
            return false;  // 解析层已判坏
        }
        return ws::is_valid_upgrade(parser.request());
    };

    TEST_ASSERT(validate(kGoodUpgrade), "合法请求通过校验");
    TEST_ASSERT(!validate(std::string("POST /game HTTP/1.1\r\n") +
                          "Host: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                          "Sec-WebSocket-Version: 13\r\n\r\n"),
                "POST 拒绝");
    TEST_ASSERT(!validate(std::string("GET /game HTTP/1.1\r\n") +
                          "Host: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                          "Sec-WebSocket-Version: 13\r\n\r\n"),
                "缺 Sec-WebSocket-Key 拒绝");
    TEST_ASSERT(!validate(std::string("GET /game HTTP/1.1\r\n") +
                          "Host: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                          "Sec-WebSocket-Key: tooshort\r\n" +
                          "Sec-WebSocket-Version: 13\r\n\r\n"),
                "key 非 24 位 base64 拒绝");
    TEST_ASSERT(!validate(std::string("GET /game HTTP/1.1\r\n") +
                          "Host: x\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                          "Sec-WebSocket-Version: 8\r\n\r\n"),
                "版本非 13 拒绝");
    TEST_ASSERT(!validate(std::string("GET /game HTTP/1.1\r\n") +
                          "Host: x\r\nConnection: Upgrade\r\n" +
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                          "Sec-WebSocket-Version: 13\r\n\r\n"),
                "缺 Upgrade 头拒绝");
    TEST_ASSERT(!validate(std::string("GET /game HTTP/1.1\r\n") +
                          "Host: x\r\nUpgrade: websocket\r\nConnection: keep-alive\r\n" +
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                          "Sec-WebSocket-Version: 13\r\n\r\n"),
                "Connection 无 upgrade token 拒绝");
    TEST_ASSERT(!validate("garbage\r\n\r\n"), "非 HTTP 字节判坏");
    return true;
}

// ---- 4. 应答形态 ----

bool test_responses() {
    const auto ok = ws::make_accept_response("dGhlIHNhbXBsZSBub25jZQ==");
    const std::string text(ok.begin(), ok.end());
    TEST_ASSERT(text.rfind("HTTP/1.1 101", 0) == 0, "101 应答首行");
    TEST_ASSERT(text.find("s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos,
                "应答含 Accept");
    TEST_ASSERT(text.find("Upgrade: websocket") != std::string::npos,
                "应答含 Upgrade 头");
    TEST_ASSERT(text.substr(text.size() - 4) == "\r\n\r\n", "应答以头部终止收尾");

    const auto reject = ws::make_reject_response("bad version");
    const std::string rtext(reject.begin(), reject.end());
    TEST_ASSERT(rtext.rfind("HTTP/1.1 400", 0) == 0, "拒绝应答 400 首行");
    TEST_ASSERT(rtext.find("bad version") != std::string::npos, "拒绝原因可见");
    return true;
}

// ---- 测试侧掩码编码器（客户端→服务端帧必须掩码）----

void append_masked_frame(std::vector<std::uint8_t>& out, ws::OpCode opcode, bool fin,
                         const std::uint8_t* payload, std::size_t size, std::uint8_t mask_key) {
    const std::uint8_t first = (fin ? 0x80 : 0x00) | static_cast<std::uint8_t>(opcode);
    out.push_back(first);
    const std::uint8_t mask_bit = 0x80;
    if (size < 126) {
        out.push_back(mask_bit | static_cast<std::uint8_t>(size));
    } else if (size <= 0xFFFF) {
        out.push_back(mask_bit | 126);
        out.push_back(static_cast<std::uint8_t>(size >> 8));
        out.push_back(static_cast<std::uint8_t>(size & 0xFF));
    } else {
        out.push_back(mask_bit | 127);
        for (int shift = 56; shift >= 0; shift -= 8) {
            out.push_back(static_cast<std::uint8_t>((size >> shift) & 0xFF));
        }
    }
    out.push_back(mask_key);
    out.push_back(mask_key);
    out.push_back(mask_key);
    out.push_back(mask_key);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(payload[i] ^ mask_key);
    }
}

// ---- 5. 帧编码与掩码解析 ----

bool test_frame_codec_vectors() {
    // RFC §5.7：无掩码单帧文本 "Hello" = 81 05 48 65 6c 6c 6f（服务端出帧）。
    std::vector<std::uint8_t> out;
    const std::uint8_t hello[] = "Hello";
    ws::append_frame(out, ws::OpCode::kText, hello, 5);
    const std::uint8_t expected[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
    TEST_ASSERT(out.size() == sizeof expected && std::memcmp(out.data(), expected, sizeof expected) == 0,
                "无掩码文本帧逐字节对 RFC 向量");

    // RFC §5.7：掩码文本 "Hello" = 81 85 37 fa 21 3d 7f 9f 4d 51 58。
    const std::uint8_t masked[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d,
                                   0x7f, 0x9f, 0x4d, 0x51, 0x58};
    ws::FrameParser parser;
    const auto outcome = parser.feed(masked, sizeof masked);
    TEST_ASSERT(!outcome.fatal, "掩码帧不 fatal");
    TEST_ASSERT(outcome.messages.size() == 1, "产出一条消息");
    TEST_ASSERT(outcome.messages[0].opcode == ws::OpCode::kText, "opcode 文本");
    TEST_ASSERT(outcome.messages[0].payload.size() == 5 &&
                    std::memcmp(outcome.messages[0].payload.data(), "Hello", 5) == 0,
                "掩码解出 Hello");
    return true;
}

// ---- 6. 分片拼装 ----

bool test_fragmentation() {
    // RFC §5.7 分片形态：text 分片 + 穿插 ping + continuation 收尾。
    std::vector<std::uint8_t> raw;
    const std::uint8_t part1[] = "Hel";
    const std::uint8_t part2[] = "lo";
    const std::uint8_t pingp[] = "!";
    append_masked_frame(raw, ws::OpCode::kText, false, part1, 3, 0x37);
    append_masked_frame(raw, ws::OpCode::kPing, true, pingp, 1, 0x21);
    append_masked_frame(raw, ws::OpCode::kContinuation, true, part2, 2, 0x5a);

    ws::FrameParser parser;
    const auto outcome = parser.feed(raw);
    TEST_ASSERT(!outcome.fatal, "穿插控制帧的分片流合法");
    TEST_ASSERT(outcome.messages.size() == 1, "分片拼成一条业务消息");
    TEST_ASSERT(outcome.messages[0].opcode == ws::OpCode::kText, "消息 opcode 取首片");
    TEST_ASSERT(outcome.messages[0].payload.size() == 5 &&
                    std::memcmp(outcome.messages[0].payload.data(), "Hello", 5) == 0,
                "分片拼装字节正确");
    TEST_ASSERT(!outcome.pending_control.empty(), "ping 已有 pong 回执排队");

    // 新数据帧前无未完分片约束：continuation 无前置分片 → fatal。
    ws::FrameParser stray;
    std::vector<std::uint8_t> cont;
    append_masked_frame(cont, ws::OpCode::kContinuation, true, part2, 2, 0x5a);
    TEST_ASSERT(stray.feed(cont).fatal, "孤立 continuation fatal");

    // 数据帧后期待 continuation，来新 text 片 → fatal。
    ws::FrameParser badseq;
    std::vector<std::uint8_t> seq;
    append_masked_frame(seq, ws::OpCode::kText, false, part1, 3, 0x37);
    TEST_ASSERT(!badseq.feed(seq).fatal, "首片合法");
    std::vector<std::uint8_t> seq2;
    append_masked_frame(seq2, ws::OpCode::kText, true, part2, 2, 0x5a);
    TEST_ASSERT(badseq.feed(seq2).fatal, "分片中途换新 text 帧 fatal");
    return true;
}

// ---- 7. ping/pong 与 close ----

bool test_control_frames() {
    ws::FrameParser parser;
    std::vector<std::uint8_t> raw;
    const std::uint8_t pingp[] = "Hello";
    append_masked_frame(raw, ws::OpCode::kPing, true, pingp, 5, 0x37);
    auto outcome = parser.feed(raw);
    TEST_ASSERT(!outcome.fatal, "ping 合法");
    TEST_ASSERT(outcome.messages.empty(), "控制帧不进业务消息");
    TEST_ASSERT(!outcome.peer_closed, "ping 不是关闭");
    TEST_ASSERT(!outcome.pending_control.empty(), "ping 排队 pong 回执");
    // pong 回执形态：0x8a 0x05 'H''e''l''l''o'（无掩码、原载荷回）。
    const std::uint8_t want_pong[] = {0x8a, 0x05, 'H', 'e', 'l', 'l', 'o'};
    TEST_ASSERT(outcome.pending_control.size() == sizeof want_pong &&
                    std::memcmp(outcome.pending_control.data(), want_pong,
                                sizeof want_pong) == 0,
                "pong 回执逐字节正确");

    // pong（对端应我方 ping）：静默受理。
    ws::FrameParser pong_parser;
    std::vector<std::uint8_t> pong;
    append_masked_frame(pong, ws::OpCode::kPong, true, pingp, 5, 0x5a);
    auto pong_outcome = pong_parser.feed(pong);
    TEST_ASSERT(!pong_outcome.fatal && pong_outcome.messages.empty() &&
                    pong_outcome.pending_control.empty(),
                "pong 静默受理");

    // close：peer_closed + close 回执（echo code）。
    ws::FrameParser closer;
    std::vector<std::uint8_t> close;
    const std::uint8_t close_payload[] = {0x03, 0xe8};  // 1000
    append_masked_frame(close, ws::OpCode::kClose, true, close_payload, 2, 0x21);
    auto close_outcome = closer.feed(close);
    TEST_ASSERT(!close_outcome.fatal, "close 帧合法");
    TEST_ASSERT(close_outcome.peer_closed, "close 置 peer_closed");
    TEST_ASSERT(close_outcome.messages.empty(), "close 不进业务消息");
    const std::uint8_t want_close[] = {0x88, 0x02, 0x03, 0xe8};
    TEST_ASSERT(close_outcome.pending_control.size() == sizeof want_close &&
                    std::memcmp(close_outcome.pending_control.data(), want_close,
                                sizeof want_close) == 0,
                "close 回执 echo code 1000");
    return true;
}

// ---- 8. 扩展长度 ----

bool test_extended_lengths() {
    // ext16 路径：300 字节。
    std::vector<std::uint8_t> payload300(300);
    for (std::size_t i = 0; i < payload300.size(); ++i) {
        payload300[i] = static_cast<std::uint8_t>(i * 7);
    }
    std::vector<std::uint8_t> raw16;
    append_masked_frame(raw16, ws::OpCode::kBinary, true, payload300.data(),
                        payload300.size(), 0x42);
    ws::FrameParser p16;
    auto o16 = p16.feed(raw16);
    TEST_ASSERT(!o16.fatal && o16.messages.size() == 1, "ext16 帧产出一条消息");
    TEST_ASSERT(o16.messages[0].payload == payload300, "ext16 载荷一致");

    // ext64 路径：70000 字节。
    std::vector<std::uint8_t> payload70k(70000);
    for (std::size_t i = 0; i < payload70k.size(); ++i) {
        payload70k[i] = static_cast<std::uint8_t>(i * 13);
    }
    std::vector<std::uint8_t> raw64;
    append_masked_frame(raw64, ws::OpCode::kBinary, true, payload70k.data(),
                        payload70k.size(), 0x99);
    ws::FrameParser p64;
    auto o64 = p64.feed(raw64);
    TEST_ASSERT(!o64.fatal && o64.messages.size() == 1, "ext64 帧产出一条消息");
    TEST_ASSERT(o64.messages[0].payload == payload70k, "ext64 载荷一致");
    return true;
}

// ---- 9. 协议负例 ----

bool test_protocol_errors() {
    // RSV 位。
    {
        ws::FrameParser parser;
        std::vector<std::uint8_t> raw = {0xC1, 0x80, 0x00, 0x00, 0x00, 0x00};
        TEST_ASSERT(parser.feed(raw).fatal, "RSV 位 fatal");
    }
    // 客户端→服务端无掩码。
    {
        ws::FrameParser parser;
        std::vector<std::uint8_t> raw = {0x81, 0x01, 'x'};
        TEST_ASSERT(parser.feed(raw).fatal, "客户端帧无掩码 fatal");
    }
    // 控制帧分片（FIN=0 的 ping）。
    {
        ws::FrameParser parser;
        std::vector<std::uint8_t> raw;
        const std::uint8_t one[] = "x";
        append_masked_frame(raw, ws::OpCode::kPing, false, one, 1, 0x33);
        TEST_ASSERT(parser.feed(raw).fatal, "控制帧分片 fatal");
    }
    // 保留 opcode（3）。
    {
        ws::FrameParser parser;
        std::vector<std::uint8_t> raw;
        const std::uint8_t one[] = "x";
        append_masked_frame(raw, static_cast<ws::OpCode>(3), true, one, 1, 0x33);
        TEST_ASSERT(parser.feed(raw).fatal, "保留 opcode fatal");
    }
    // 声明长度超上限。
    {
        ws::FrameParser parser(1024);
        std::vector<std::uint8_t> raw = {0x82, 0xFF, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x11, 0x11};
        TEST_ASSERT(parser.feed(raw).fatal, "声明超限 fatal");
    }
    // 控制帧载荷超 125。
    {
        ws::FrameParser parser;
        std::vector<std::uint8_t> raw;
        std::vector<std::uint8_t> big(126, 'p');
        append_masked_frame(raw, ws::OpCode::kPing, true, big.data(), big.size(), 0x33);
        TEST_ASSERT(parser.feed(raw).fatal, "控制帧载荷超 125 fatal");
    }
    return true;
}

// ---- 10. 增量喂入等价 ----

bool test_incremental_equivalence() {
    std::vector<std::uint8_t> stream;
    std::vector<std::uint8_t> part1(40);
    std::vector<std::uint8_t> part2(200);
    for (std::size_t i = 0; i < part1.size(); ++i) {
        part1[i] = static_cast<std::uint8_t>(i);
    }
    for (std::size_t i = 0; i < part2.size(); ++i) {
        part2[i] = static_cast<std::uint8_t>(255 - i);
    }
    append_masked_frame(stream, ws::OpCode::kBinary, false, part1.data(), part1.size(), 0x11);
    const std::uint8_t pingp[] = "ka";
    append_masked_frame(stream, ws::OpCode::kPing, true, pingp, 2, 0x22);
    append_masked_frame(stream, ws::OpCode::kContinuation, true, part2.data(),
                        part2.size(), 0x33);

    ws::FrameParser whole;
    const auto ref = whole.feed(stream);
    TEST_ASSERT(!ref.fatal && ref.messages.size() == 1, "整喂基准成立");
    const auto ref_msg = ref.messages[0].payload;
    const auto ref_ctrl = ref.pending_control;

    // 任意切分：每 7 字节一喂。
    ws::FrameParser chopped;
    ws::FrameParser::Outcome got{};
    for (std::size_t i = 0; i < stream.size(); i += 7) {
        const std::size_t n = std::min<std::size_t>(7, stream.size() - i);
        auto piece = chopped.feed(stream.data() + i, n);
        TEST_ASSERT(!piece.fatal, "切分喂入不 fatal");
        for (auto& m : piece.messages) {
            got.messages.push_back(std::move(m));
        }
        got.pending_control.insert(got.pending_control.end(),
                                   piece.pending_control.begin(),
                                   piece.pending_control.end());
    }
    TEST_ASSERT(got.messages.size() == 1, "切分喂入产出同一条消息");
    TEST_ASSERT(got.messages[0].payload == ref_msg, "切分喂入载荷一致");
    TEST_ASSERT(got.pending_control == ref_ctrl, "切分喂入回执一致");
    return true;
}

// ---- 11. 头部同 chunk 粘帧残余 ----

bool test_handshake_remainder() {
    // 升级请求与首帧粘在一个 chunk：头部就绪后残余必须完整移交帧层，
    // 不得丢弃（快客户端一并把帧发上来的实态）。
    std::vector<std::uint8_t> stream(kGoodUpgrade, kGoodUpgrade + std::strlen(kGoodUpgrade));
    const std::uint8_t hello[] = "hello";
    append_masked_frame(stream, ws::OpCode::kBinary, true, hello, 5, 0x5a);

    ws::HandshakeParser parser;
    const auto state = parser.feed(stream);
    TEST_ASSERT(state == ws::HandshakeParser::State::kReady, "粘帧 chunk 头部仍就绪");
    const auto remainder = parser.take_remainder();
    TEST_ASSERT(remainder.size() == 11, "残余 = 首帧 11 字节（2 头 + 4 掩码 + 5 载荷）");
    TEST_ASSERT(parser.take_remainder().empty(), "取走即清");

    // 后续 chunk 继续入残余（掩码空 binary 帧：2 头 + 4 掩码，零载荷）。
    const std::uint8_t tail[] = {0x82, 0x80, 0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT(parser.feed(tail, sizeof tail) == ws::HandshakeParser::State::kReady,
                "头部就绪后再喂不判坏");
    const auto more = parser.take_remainder();
    TEST_ASSERT(more.size() == 6 && more[0] == 0x82, "后续字节续入残余");

    // 残余直喂帧层即可解出消息（粘的字节完整）。
    ws::FrameParser frames;
    const auto out1 = frames.feed(remainder);
    const auto out2 = frames.feed(more);
    TEST_ASSERT(!out1.fatal && out1.messages.size() == 1, "残余首帧完整可解");
    TEST_ASSERT(out1.messages[0].opcode == ws::OpCode::kBinary &&
                    std::string(out1.messages[0].payload.begin(),
                                out1.messages[0].payload.end()) == "hello",
                "载荷解掩码正确");
    TEST_ASSERT(!out2.fatal && out2.messages.size() == 1 && out2.messages[0].payload.empty(),
                "残余后续帧完整可解");
    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"accept_key_vector", test_accept_key_vector},
        {"handshake_parse", test_handshake_parse},
        {"handshake_validation", test_handshake_validation},
        {"responses", test_responses},
        {"frame_codec_vectors", test_frame_codec_vectors},
        {"fragmentation", test_fragmentation},
        {"control_frames", test_control_frames},
        {"extended_lengths", test_extended_lengths},
        {"protocol_errors", test_protocol_errors},
        {"incremental_equivalence", test_incremental_equivalence},
        {"handshake_remainder", test_handshake_remainder},
    };

    int failed = 0;
    for (const auto& case_entry : cases) {
        if (!case_entry.fn()) {
            std::cerr << "FAIL: case " << case_entry.name << std::endl;
            ++failed;
        }
    }
    if (failed == 0) {
        std::cout << "All tests passed" << std::endl;
        return 0;
    }
    std::cerr << failed << " test(s) failed" << std::endl;
    return 1;
}
