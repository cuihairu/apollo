// 契约 wire codec 测试（golden：对随仓契约全量消息做确定性随机值
// encode→decode round-trip；布局事实源 = 生成头 kMsgs，字段型逐一对表）。
//
// 覆盖：
//   1. 全量消息 round-trip（逐字段按型比对，含无字段消息）；
//   2. 信封层：u16 id 剥出 + 载荷跟随；
//   3. 截断负例：任一前缀长度的字节流解码必失败（无字段消息除外）；
//   4. 尾部余字节负例（布局不符）；
//   5. id 不在契约 → encode/decode 双向拒绝；
//   6. 字段数不符 → 编码拒绝；Int32 越界 → 编码拒绝。

#include "apollo/net/wire/contract_wire.hpp"

#include <iostream>
#include <random>
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

// 确定性伪随机（同 seed 同序列——golden 断言不许漂）。
std::mt19937_64& rng() {
    static std::mt19937_64 engine(0x5041C0DE);
    return engine;
}

apollo::net::wire::FieldValue sample_field(apollo::contract::gen::WireType type) {
    using apollo::contract::gen::WireType;
    apollo::net::wire::FieldValue v;
    auto& engine = rng();
    switch (type) {
        case WireType::Bool:
            v.integer = static_cast<std::int64_t>(engine() % 2);
            break;
        case WireType::Int32:
            v.integer = static_cast<std::int32_t>(engine());
            break;
        case WireType::Int64:
            v.integer = static_cast<std::int64_t>(engine());
            break;
        case WireType::Float:
            v.real = static_cast<float>(engine() % 100000) / 64.0f;
            break;
        case WireType::Double:
            v.real = static_cast<double>(engine() % 1000000) / 128.0;
            break;
        case WireType::String:
            for (int i = 0; i < 3; ++i) {
                v.text.push_back(static_cast<char>('a' + engine() % 26));
            }
            break;
        case WireType::Bytes:
            for (int i = 0; i < 5; ++i) {
                v.bytes.push_back(static_cast<std::uint8_t>(engine() % 256));
            }
            break;
        case WireType::Int64List:
            for (int i = 0; i < 3; ++i) {
                v.integers.push_back(static_cast<std::int64_t>(engine()));
            }
            break;
        case WireType::StringList:
            for (int i = 0; i < 2; ++i) {
                v.strings.push_back(std::string("s") + static_cast<char>('a' + engine() % 26));
            }
            break;
    }
    return v;
}

bool fields_equal(const apollo::net::wire::FieldValue& a,
                  const apollo::net::wire::FieldValue& b,
                  apollo::contract::gen::WireType type) {
    using apollo::contract::gen::WireType;
    switch (type) {
        case WireType::Bool:
        case WireType::Int32:
        case WireType::Int64:
            return a.integer == b.integer;
        case WireType::Float: {
            const float fa = static_cast<float>(a.real);
            const float fb = static_cast<float>(b.real);
            return fa == fb;  // float 精度即线形精度，round-trip 后须逐位同
        }
        case WireType::Double:
            return a.real == b.real;
        case WireType::String:
            return a.text == b.text;
        case WireType::Bytes:
            return a.bytes == b.bytes;
        case WireType::Int64List:
            return a.integers == b.integers;
        case WireType::StringList:
            return a.strings == b.strings;
    }
    return false;
}

bool test_full_contract_roundtrip() {
    std::cout << "Running: test_full_contract_roundtrip..." << std::endl;
    std::size_t checked = 0;
    for (const auto& meta : apollo::contract::gen::kMsgs) {
        const auto id = static_cast<std::uint16_t>(meta.id);
        TEST_ASSERT(apollo::net::wire::find_msg(id) == &meta, "find_msg hits the table");

        std::vector<apollo::net::wire::FieldValue> in;
        for (std::size_t i = 0; i < meta.fieldCount; ++i) {
            in.push_back(sample_field(meta.fields[i].type));
        }

        std::vector<std::uint8_t> payload;
        TEST_ASSERT(apollo::net::wire::encode_payload(id, in, payload),
                    std::string("encode ok: ") + meta.name);

        std::uint16_t out_id = 0;
        std::vector<apollo::net::wire::FieldValue> out;
        TEST_ASSERT(apollo::net::wire::decode_envelope(
                        apollo::net::wire::append_envelope(id, payload).data(),
                        2 + payload.size(), out_id, out),
                    std::string("envelope decode ok: ") + meta.name);
        TEST_ASSERT(out_id == id, "envelope id round-trips");
        TEST_ASSERT(out.size() == in.size(), "field count preserved");
        for (std::size_t i = 0; i < meta.fieldCount; ++i) {
            TEST_ASSERT(fields_equal(in[i], out[i], meta.fields[i].type),
                        std::string("field value round-trip: ") + meta.name + "."
                            + meta.fields[i].name);
        }
        ++checked;
    }
    TEST_ASSERT(checked >= 48, "whole shipped contract walked");
    std::cout << "  PASSED (" << checked << " msgs)" << std::endl;
    return true;
}

bool test_truncation_rejected() {
    std::cout << "Running: test_truncation_rejected..." << std::endl;
    // 取带字段的代表性消息（cell_skill_cast 十一字段，含 float/int32/int64）。
    const auto id = static_cast<std::uint16_t>(apollo::contract::gen::MsgId::cell_skill_cast);
    const auto* meta = apollo::net::wire::find_msg(id);
    TEST_ASSERT(meta != nullptr && meta->fieldCount == 11, "skill cast has 11 fields");

    std::vector<apollo::net::wire::FieldValue> in;
    for (std::size_t i = 0; i < meta->fieldCount; ++i) {
        in.push_back(sample_field(meta->fields[i].type));
    }
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(apollo::net::wire::encode_payload(id, in, payload), "encode ok");
    TEST_ASSERT(!payload.empty(), "payload non-empty");

    const auto envelope = apollo::net::wire::append_envelope(id, payload);
    for (std::size_t cut = 2; cut < envelope.size(); ++cut) {
        std::uint16_t out_id = 0;
        std::vector<apollo::net::wire::FieldValue> out;
        TEST_ASSERT(!apollo::net::wire::decode_envelope(envelope.data(), cut, out_id, out),
                    "truncated decode rejected");
    }
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_layout_mismatch_rejected() {
    std::cout << "Running: test_layout_mismatch_rejected..." << std::endl;
    const auto id = static_cast<std::uint16_t>(apollo::contract::gen::MsgId::cell_intent_ack);
    const auto* meta = apollo::net::wire::find_msg(id);
    TEST_ASSERT(meta != nullptr && meta->fieldCount == 3, "ack has 3 fields");

    // 正确编码后追加尾字节：布局不符必拒。
    std::vector<apollo::net::wire::FieldValue> in(3);
    in[0].integer = 7;
    in[1].integer = 2101;
    in[2].text = "backpressure";
    std::vector<std::uint8_t> payload;
    TEST_ASSERT(apollo::net::wire::encode_payload(id, in, payload), "encode ok");
    auto envelope = apollo::net::wire::append_envelope(id, payload);
    envelope.push_back(0x00);
    std::uint16_t out_id = 0;
    std::vector<apollo::net::wire::FieldValue> out;
    TEST_ASSERT(!apollo::net::wire::decode_envelope(envelope.data(), envelope.size(), out_id, out),
                "trailing bytes rejected");

    // id 不在契约。
    TEST_ASSERT(apollo::net::wire::find_msg(0xDEAD) == nullptr, "unknown id absent");
    TEST_ASSERT(!apollo::net::wire::decode_envelope(envelope.data(), 2, out_id, out),
                "unknown id decode rejected");
    std::vector<std::uint8_t> junk;
    TEST_ASSERT(!apollo::net::wire::encode_payload(0xDEAD, in, junk),
                "unknown id encode rejected");

    // 字段数不符。
    std::vector<apollo::net::wire::FieldValue> too_few(2);
    TEST_ASSERT(!apollo::net::wire::encode_payload(id, too_few, junk),
                "field count mismatch rejected");

    // Int32 越界（entity_type 型 Int32；塞超 int32 值域的值）。
    const auto move_id = static_cast<std::uint16_t>(apollo::contract::gen::MsgId::cell_create_entity);
    const auto* move_meta = apollo::net::wire::find_msg(move_id);
    std::vector<apollo::net::wire::FieldValue> over(move_meta->fieldCount);
    for (std::size_t i = 0; i < over.size(); ++i) {
        over[i] = sample_field(move_meta->fields[i].type);
    }
    for (std::size_t i = 0; i < move_meta->fieldCount; ++i) {
        if (move_meta->fields[i].type == apollo::contract::gen::WireType::Int32) {
            over[i].integer = 1LL << 40;
        }
    }
    TEST_ASSERT(!apollo::net::wire::encode_payload(move_id, over, junk),
                "int32 overflow rejected");
    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_empty_payload_msg() {
    std::cout << "Running: test_empty_payload_msg..." << std::endl;
    const auto id = static_cast<std::uint16_t>(apollo::contract::gen::MsgId::party_create);
    const auto* meta = apollo::net::wire::find_msg(id);
    TEST_ASSERT(meta != nullptr && meta->fieldCount == 0, "party_create has no fields");

    std::vector<std::uint8_t> payload{{0xAB, 0xCD}};  // 预置非空：encode 须清空
    std::vector<apollo::net::wire::FieldValue> in;
    TEST_ASSERT(apollo::net::wire::encode_payload(id, in, payload), "encode ok");
    TEST_ASSERT(payload.empty(), "no-field payload is empty");

    std::uint16_t out_id = 0;
    std::vector<apollo::net::wire::FieldValue> out;
    TEST_ASSERT(apollo::net::wire::decode_envelope(
                    apollo::net::wire::append_envelope(id, payload).data(), 2, out_id, out),
                "empty envelope decodes");
    TEST_ASSERT(out_id == id && out.empty(), "empty fields preserved");
    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

int main() {
    int failed = 0;
    if (!test_full_contract_roundtrip()) ++failed;
    if (!test_truncation_rejected()) ++failed;
    if (!test_layout_mismatch_rejected()) ++failed;
    if (!test_empty_payload_msg()) ++failed;
    if (failed == 0) {
        std::cout << "ALL WIRE TESTS PASSED" << std::endl;
        return 0;
    }
    std::cerr << failed << " wire test(s) failed" << std::endl;
    return 1;
}
