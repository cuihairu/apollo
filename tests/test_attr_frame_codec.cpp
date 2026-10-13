// AttrSyncFrame wire codec tests (attribute-sync §7.1).
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/frame_codec.hpp"

using namespace apollo::game::attrsync;

// ── Single delta frame roundtrip ─────────────────────────────────

TEST(FrameCodec, SingleDeltaRoundtrip) {
    AttrSyncFrame frame;
    frame.entity_id = 42;
    frame.base_seq = 5;
    frame.is_snapshot = false;

    AttrDelta d1;
    d1.attr_id = 1;
    d1.value = 100;
    d1.change_n = 1;
    frame.deltas.push_back(d1);

    auto bytes = encode_frame(frame);
    auto decoded = decode_frame(bytes);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->entity_id, 42u);
    EXPECT_EQ(decoded->base_seq, 5u);
    EXPECT_FALSE(decoded->is_snapshot);
    ASSERT_EQ(decoded->deltas.size(), 1u);
    EXPECT_EQ(decoded->deltas[0].attr_id, 1u);
    EXPECT_EQ(decoded->deltas[0].value, 100);
    EXPECT_EQ(decoded->deltas[0].change_n, 1u);
}

// ── Multiple deltas roundtrip ────────────────────────────────────

TEST(FrameCodec, MultipleDeltasRoundtrip) {
    AttrSyncFrame frame;
    frame.entity_id = 7;
    frame.base_seq = 10;

    for (std::uint32_t i = 0; i < 5; ++i) {
        AttrDelta d;
        d.attr_id = i + 1;
        d.value = static_cast<std::int64_t>(i) * 1000 - 2000;  // mix neg/pos
        d.change_n = i + 1;
        frame.deltas.push_back(d);
    }

    auto bytes = encode_frame(frame);
    auto decoded = decode_frame(bytes);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->entity_id, 7u);
    EXPECT_EQ(decoded->base_seq, 10u);
    ASSERT_EQ(decoded->deltas.size(), 5u);
    for (std::uint32_t i = 0; i < 5; ++i) {
        EXPECT_EQ(decoded->deltas[i].attr_id, i + 1);
        EXPECT_EQ(decoded->deltas[i].value, static_cast<std::int64_t>(i) * 1000 - 2000);
        EXPECT_EQ(decoded->deltas[i].change_n, i + 1);
    }
}

// ── Snapshot frame roundtrip ─────────────────────────────────────

TEST(FrameCodec, SnapshotFlagRoundtrip) {
    AttrSyncFrame frame;
    frame.entity_id = 99;
    frame.base_seq = 0;
    frame.is_snapshot = true;

    auto bytes = encode_frame(frame);
    auto decoded = decode_frame(bytes);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->is_snapshot);
    EXPECT_EQ(decoded->entity_id, 99u);
}

// ── Empty deltas ─────────────────────────────────────────────────

TEST(FrameCodec, EmptyDeltasRoundtrip) {
    AttrSyncFrame frame;
    frame.entity_id = 1;
    frame.base_seq = 3;

    auto bytes = encode_frame(frame);
    auto decoded = decode_frame(bytes);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->deltas.empty());
    EXPECT_EQ(decoded->base_seq, 3u);
}

// ── Large values (zigzag + varint multi-byte) ────────────────────

TEST(FrameCodec, LargeValueRoundtrip) {
    AttrSyncFrame frame;
    frame.entity_id = 0xFFFFFFFFFFFFFFFFull;
    frame.base_seq = 0x123456789ull;

    AttrDelta d;
    d.attr_id = 0xFFFFFF;
    d.value = INT64_MAX;
    d.change_n = 0xFFFFFFFFu;
    frame.deltas.push_back(d);

    auto bytes = encode_frame(frame);
    auto decoded = decode_frame(bytes);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->entity_id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(decoded->base_seq, 0x123456789ull);
    ASSERT_EQ(decoded->deltas.size(), 1u);
    EXPECT_EQ(decoded->deltas[0].value, INT64_MAX);
    EXPECT_EQ(decoded->deltas[0].change_n, 0xFFFFFFFFu);
}

// ── Decode failures ──────────────────────────────────────────────

TEST(FrameCodec, DecodeEmptyFails) {
    EXPECT_FALSE(decode_frame({}).has_value());
}

TEST(FrameCodec, DecodeTruncatedFails) {
    AttrSyncFrame frame;
    frame.entity_id = 42;
    AttrDelta d;
    d.attr_id = 1;
    d.value = 100;
    d.change_n = 1;
    frame.deltas.push_back(d);

    auto bytes = encode_frame(frame);
    // Truncate in the middle of the delta payload
    auto truncated = bytes;
    truncated.resize(bytes.size() - 2);
    EXPECT_FALSE(decode_frame(truncated).has_value());
}

TEST(FrameCodec, DecodeGarbageFails) {
    // First varint claims 5 deltas but payload is empty
    std::vector<std::uint8_t> garbage = {0x05, 0x01, 0x02};
    EXPECT_FALSE(decode_frame(garbage).has_value());
}

// ── Batch of frames (per viewer per tick) ────────────────────────

TEST(FrameCodec, BatchRoundtrip) {
    std::vector<AttrSyncFrame> batch;
    for (std::uint64_t e = 1; e <= 3; ++e) {
        AttrSyncFrame frame;
        frame.entity_id = e;
        frame.base_seq = e * 10;
        AttrDelta d;
        d.attr_id = static_cast<std::uint32_t>(e);
        d.value = static_cast<std::int64_t>(e) * 100;
        d.change_n = 1;
        frame.deltas.push_back(d);
        batch.push_back(frame);
    }

    auto bytes = encode_batch(batch);
    auto decoded = decode_batch(bytes);

    ASSERT_TRUE(decoded.has_value());
    ASSERT_EQ(decoded->size(), 3u);
    for (std::size_t i = 0; i < 3; ++i) {
        EXPECT_EQ((*decoded)[i].entity_id, i + 1);
        EXPECT_EQ((*decoded)[i].base_seq, (i + 1) * 10);
        ASSERT_EQ((*decoded)[i].deltas.size(), 1u);
        EXPECT_EQ((*decoded)[i].deltas[0].value, static_cast<std::int64_t>(i + 1) * 100);
    }
}

TEST(FrameCodec, EmptyBatchRoundtrip) {
    auto bytes = encode_batch({});
    auto decoded = decode_batch(bytes);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(decoded->empty());
}
