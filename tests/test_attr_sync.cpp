// Attribute-sync per-viewer delta pipeline tests.
// Covers ChangeHistory, ViewerAttrState, AttrDelta encoding, delta computation.
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/change_history.hpp"
#include "apollo/game/attrsync/viewer_attr_state.hpp"
#include "apollo/game/attrsync/attr_delta.hpp"
#include "apollo/game/attrsync/delta_builder.hpp"

using namespace apollo::game::attrsync;

// ── ChangeHistory ────────────────────────────────────────────────

TEST(ChangeHistory, AppendIncrementsSeq) {
    ChangeHistory hist(16);
    hist.append(1, 100);
    hist.append(2, 200);
    EXPECT_EQ(hist.world_seq(), 2u);
}

TEST(ChangeHistory, AppendStoresValue) {
    ChangeHistory hist(16);
    hist.append(1, 100);
    auto entries = hist.entries_since(0);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].attr_id, 1u);
    EXPECT_EQ(entries[0].value, 100);
    EXPECT_EQ(entries[0].seq, 1u);
}

TEST(ChangeHistory, RingOverwrite) {
    ChangeHistory hist(4);
    hist.append(1, 10);
    hist.append(2, 20);
    hist.append(3, 30);
    hist.append(4, 40);
    hist.append(5, 50);  // overwrites seq=1
    EXPECT_EQ(hist.world_seq(), 5u);
    // entries_since(0) should return all 5 (ring holds last 4 + current)
    auto entries = hist.entries_since(0);
    EXPECT_EQ(entries.size(), 4u);  // ring capacity = 4
}

TEST(ChangeHistory, EntriesSince) {
    ChangeHistory hist(16);
    hist.append(1, 100);  // seq=1
    hist.append(2, 200);  // seq=2
    hist.append(3, 300);  // seq=3
    auto entries = hist.entries_since(1);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].seq, 2u);
    EXPECT_EQ(entries[1].seq, 3u);
}

TEST(ChangeHistory, EntriesSinceEmpty) {
    ChangeHistory hist(16);
    hist.append(1, 100);
    auto entries = hist.entries_since(1);
    EXPECT_TRUE(entries.empty());
}

TEST(ChangeHistory, LastWriteWins) {
    ChangeHistory hist(16);
    hist.append(1, 100);  // seq=1
    hist.append(1, 200);  // seq=2 (same attr)
    auto entries = hist.entries_since(0);
    ASSERT_EQ(entries.size(), 2u);
    // Both entries present; delta builder handles last-write-wins
    EXPECT_EQ(entries[0].value, 100);
    EXPECT_EQ(entries[1].value, 200);
}

// ── ViewerAttrState ───────────────────────────────────────────────

TEST(ViewerAttrState, SetGetAckedSeq) {
    ViewerAttrState state;
    state.set_acked(42, 10);
    EXPECT_EQ(state.acked_seq(42), 10u);
}

TEST(ViewerAttrState, DefaultZero) {
    ViewerAttrState state;
    EXPECT_EQ(state.acked_seq(99), 0u);
}

TEST(ViewerAttrState, MultipleEntities) {
    ViewerAttrState state;
    state.set_acked(1, 100);
    state.set_acked(2, 200);
    EXPECT_EQ(state.acked_seq(1), 100u);
    EXPECT_EQ(state.acked_seq(2), 200u);
}

// ── AttrDelta encoding ────────────────────────────────────────────

TEST(AttrDelta, EncodeDecodeRoundtrip) {
    AttrDelta delta;
    delta.attr_id = 42;
    delta.value = 12345;
    delta.change_n = 3;

    auto encoded = delta.encode();
    auto decoded = AttrDelta::decode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->attr_id, 42u);
    EXPECT_EQ(decoded->value, 12345);
    EXPECT_EQ(decoded->change_n, 3u);
}

TEST(AttrDelta, EncodeDecodeNegativeValue) {
    AttrDelta delta;
    delta.attr_id = 1;
    delta.value = -100;
    delta.change_n = 1;

    auto encoded = delta.encode();
    auto decoded = AttrDelta::decode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->value, -100);
}

TEST(AttrDelta, EncodeDecodeZero) {
    AttrDelta delta;
    delta.attr_id = 0;
    delta.value = 0;
    delta.change_n = 0;

    auto encoded = delta.encode();
    auto decoded = AttrDelta::decode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->attr_id, 0u);
    EXPECT_EQ(decoded->value, 0);
    EXPECT_EQ(decoded->change_n, 0u);
}

TEST(AttrDelta, DecodeEmptyFails) {
    auto decoded = AttrDelta::decode({});
    EXPECT_FALSE(decoded.has_value());
}

// ── DeltaBuilder ──────────────────────────────────────────────────

TEST(DeltaBuilder, BuildDeltaFromHistory) {
    ChangeHistory hist(16);
    hist.append(1, 100);  // seq=1
    hist.append(2, 200);  // seq=2
    hist.append(1, 150);  // seq=3 (overwrite attr 1)

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer has seen seq=1

    DeltaBuilder builder;
    auto frame = builder.build(42, hist, viewer);

    EXPECT_EQ(frame.entity_id, 42u);
    EXPECT_EQ(frame.base_seq, 1u);
    // Should contain seq 2 and 3 (last-write-wins: attr 1 appears once with value 150)
    EXPECT_FALSE(frame.deltas.empty());
}

TEST(DeltaBuilder, EmptyDeltaWhenCaughtUp) {
    ChangeHistory hist(16);
    hist.append(1, 100);  // seq=1

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer caught up

    DeltaBuilder builder;
    auto frame = builder.build(42, hist, viewer);

    EXPECT_TRUE(frame.deltas.empty());
}

TEST(DeltaBuilder, SnapshotWhenHistoryExhausted) {
    ChangeHistory hist(3);  // small ring
    hist.append(1, 10);     // seq=1
    hist.append(2, 20);     // seq=2
    hist.append(3, 30);     // seq=3
    hist.append(4, 40);     // seq=4 (overwrites seq=1)
    hist.append(5, 50);     // seq=5 (overwrites seq=2)

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer stuck at seq=1, ring holds seq 3-5 only

    DeltaBuilder builder;
    auto frame = builder.build(42, hist, viewer);

    // world_seq(5) - acked_seq(1) = 4 > capacity(3) → snapshot
    EXPECT_TRUE(frame.is_snapshot);
}

TEST(DeltaBuilder, SnapshotOnEnter) {
    ChangeHistory hist(16);
    hist.append(1, 100);
    hist.append(2, 200);

    ViewerAttrState viewer;
    // viewer at seq=0 (never seen this entity)

    DeltaBuilder builder;
    auto frame = builder.build(42, hist, viewer);

    // First view should be snapshot
    EXPECT_TRUE(frame.is_snapshot);
}
