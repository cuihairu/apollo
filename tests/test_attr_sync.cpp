// Attribute-sync per-viewer delta pipeline tests.
// Covers ChangeHistory, ViewerAttrState, AttrDelta encoding, delta computation.
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_domain.hpp"
#include "apollo/game/attrsync/change_history.hpp"
#include "apollo/game/attrsync/viewer_attr_state.hpp"
#include "apollo/game/attrsync/attr_delta.hpp"
#include "apollo/game/attrsync/delta_builder.hpp"
#include "apollo/game/attrsync/entity_attrs.hpp"

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
    EntityAttrs attrs(42);
    attrs.set(1, 100);  // seq=1
    attrs.set(2, 200);  // seq=2
    attrs.set(1, 150);  // seq=3 (overwrite attr 1)

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer has seen seq=1

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_EQ(frame.entity_id, 42u);
    EXPECT_EQ(frame.base_seq, 1u);
    // Should contain seq 2 and 3 (last-write-wins: attr 1 appears once with value 150)
    EXPECT_FALSE(frame.deltas.empty());
}

TEST(DeltaBuilder, EmptyDeltaWhenCaughtUp) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);  // seq=1

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer caught up

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.deltas.empty());
}

TEST(DeltaBuilder, SnapshotWhenHistoryExhausted) {
    EntityAttrs attrs(42, /*history_capacity=*/3);
    attrs.set(1, 10);   // seq=1
    attrs.set(2, 20);   // seq=2
    attrs.set(3, 30);   // seq=3
    attrs.set(4, 40);   // seq=4 (overwrites seq=1)
    attrs.set(5, 50);   // seq=5 (overwrites seq=2)

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // viewer stuck at seq=1, ring holds seq 3-5 only

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    // world_seq(5) - acked_seq(1) = 4 > capacity(3) → snapshot
    EXPECT_TRUE(frame.is_snapshot);
}

TEST(DeltaBuilder, SnapshotOnEnter) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);
    attrs.set(2, 200);

    ViewerAttrState viewer;
    // viewer at seq=0 (never seen this entity)

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    // First view should be snapshot
    EXPECT_TRUE(frame.is_snapshot);
}

// ── DeltaBuilder 快照负载组装（§3.3）────────────────────────────────

TEST(DeltaBuilder, SnapshotCarriesAllValues) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);
    attrs.set(2, 200);
    attrs.set(3, 300);

    ViewerAttrState viewer;
    // viewer 从未见过（acked=0，world_seq=3 > 0 → 快照路径）

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    // 快照负载 = 全量当前值（无域裁剪，域投影留契约批）
    ASSERT_EQ(frame.deltas.size(), 3u);
    // 按 attr_id 排序后逐个核对
    std::sort(frame.deltas.begin(), frame.deltas.end(),
              [](const AttrDelta& a, const AttrDelta& b) {
                  return a.attr_id < b.attr_id;
              });
    EXPECT_EQ(frame.deltas[0].attr_id, 1u);
    EXPECT_EQ(frame.deltas[0].value, 100);
    EXPECT_EQ(frame.deltas[1].attr_id, 2u);
    EXPECT_EQ(frame.deltas[1].value, 200);
    EXPECT_EQ(frame.deltas[2].attr_id, 3u);
    EXPECT_EQ(frame.deltas[2].value, 300);
}

TEST(DeltaBuilder, SnapshotAfterHistoryExhaustionCarriesValues) {
    EntityAttrs attrs(42, /*history_capacity=*/3);
    attrs.set(1, 10);   // seq=1（被环形挤出）
    attrs.set(2, 20);   // seq=2（被环形挤出）
    attrs.set(3, 30);   // seq=3
    attrs.set(4, 40);   // seq=4
    attrs.set(5, 50);   // seq=5

    ViewerAttrState viewer;
    viewer.set_acked(42, 1);  // 进度落后 → 历史覆盖不了 → 快照

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    // 负载含全部当前值（含被挤出的 attr 1/2 的最新值）
    ASSERT_EQ(frame.deltas.size(), 5u);
    std::sort(frame.deltas.begin(), frame.deltas.end(),
              [](const AttrDelta& a, const AttrDelta& b) {
                  return a.attr_id < b.attr_id;
              });
    EXPECT_EQ(frame.deltas[0].value, 10);  // attr 1 仍是 10（最后一次写）
    EXPECT_EQ(frame.deltas[1].value, 20);
    EXPECT_EQ(frame.deltas[2].value, 30);
    EXPECT_EQ(frame.deltas[3].value, 40);
    EXPECT_EQ(frame.deltas[4].value, 50);
}

TEST(DeltaBuilder, SnapshotLatestValueWins) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);
    attrs.set(1, 250);  // 同 attr 重复写 → 快照取最终值

    ViewerAttrState viewer;

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].attr_id, 1u);
    EXPECT_EQ(frame.deltas[0].value, 250);
}

TEST(DeltaBuilder, SnapshotThenDelta) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);

    ViewerAttrState viewer;  // acked=0 → 快照
    DeltaBuilder builder;
    auto snap = builder.build(42, 999, attrs, viewer);
    EXPECT_TRUE(snap.is_snapshot);
    ASSERT_EQ(snap.deltas.size(), 1u);

    // 快照即推进（对方进度推到当前 seq）
    viewer.set_acked(42, attrs.history().world_seq());
    attrs.clear_dirty();

    // 后续变更走 delta
    attrs.set(2, 200);
    auto delta = builder.build(42, 999, attrs, viewer);
    EXPECT_FALSE(delta.is_snapshot);
    ASSERT_EQ(delta.deltas.size(), 1u);
    EXPECT_EQ(delta.deltas[0].attr_id, 2u);
    EXPECT_EQ(delta.deltas[0].value, 200);
}

// ── DeltaBuilder 域投影（§2.2）────────────────────────────────────

TEST(DeltaBuilder, SelfDomainHiddenFromOtherViewer) {
    EntityAttrs attrs(42);
    attrs.set(9, 777);
    attrs.set_domain(9, AttrDomain::kSelf);  // 仅 owner 可见

    ViewerAttrState viewer;  // viewer 999 ≠ entity 42
    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    EXPECT_TRUE(frame.deltas.empty());  // 全部被域裁剪
    EXPECT_EQ(frame.base_seq, attrs.history().world_seq());  // 进度照推
}

TEST(DeltaBuilder, SelfDomainVisibleToOwner) {
    EntityAttrs attrs(42);
    attrs.set(9, 777);
    attrs.set_domain(9, AttrDomain::kSelf);

    ViewerAttrState viewer;
    DeltaBuilder builder;
    auto frame = builder.build(42, 42, attrs, viewer);  // owner 自见

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].attr_id, 9u);
    EXPECT_EQ(frame.deltas[0].value, 777);
}

TEST(DeltaBuilder, PublicDomainVisibleByDefault) {
    // 未声明域 = kPublic：任何 tracked viewer 可见
    EntityAttrs attrs(42);
    attrs.set(1, 100);

    ViewerAttrState viewer;
    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
}

TEST(DeltaBuilder, WorldDomainVisibleLikePublic) {
    EntityAttrs attrs(42);
    attrs.set(3, 300);
    attrs.set_domain(3, AttrDomain::kWorld);

    ViewerAttrState viewer;
    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].value, 300);
}

TEST(DeltaBuilder, TeamDomainHiddenWithoutGate) {
    EntityAttrs attrs(42);
    attrs.set(5, 500);
    attrs.set_domain(5, AttrDomain::kTeam);

    ViewerAttrState viewer;
    DeltaBuilder builder;  // 无 group gate → 队/会域默认不可见
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    EXPECT_TRUE(frame.deltas.empty());
}

TEST(DeltaBuilder, TeamDomainVisibleThroughGate) {
    EntityAttrs attrs(42);
    attrs.set(5, 500);
    attrs.set_domain(5, AttrDomain::kTeam);

    ViewerAttrState viewer;
    DeltaBuilder builder;
    builder.set_group_gate([](std::uint64_t /*viewer*/, std::uint64_t /*entity*/,
                              AttrDomain domain) {
        return domain == AttrDomain::kTeam;  // 判同队
    });
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].attr_id, 5u);
    EXPECT_EQ(frame.deltas[0].value, 500);
}

TEST(DeltaBuilder, DeltaPathRespectsDomain) {
    EntityAttrs attrs(42);
    attrs.set(1, 100);

    ViewerAttrState viewer;  // viewer 999
    DeltaBuilder builder;
    auto snap = builder.build(42, 999, attrs, viewer);
    ASSERT_TRUE(snap.is_snapshot);
    viewer.set_acked(42, attrs.history().world_seq());

    attrs.set_domain(9, AttrDomain::kSelf);
    attrs.set(2, 200);  // public
    attrs.set(9, 777);  // self-only

    auto delta = builder.build(42, 999, attrs, viewer);
    EXPECT_FALSE(delta.is_snapshot);
    ASSERT_EQ(delta.deltas.size(), 1u);  // 域裁剪后只剩 attr 2
    EXPECT_EQ(delta.deltas[0].attr_id, 2u);

    // owner 视角同一批变更 → 含 attr 9
    ViewerAttrState owner;
    owner.set_acked(42, snap.base_seq);
    auto owner_delta = builder.build(42, 42, attrs, owner);
    EXPECT_FALSE(owner_delta.is_snapshot);
    ASSERT_EQ(owner_delta.deltas.size(), 2u);
}

TEST(DeltaBuilder, ExhaustedHistorySnapshotAlsoFiltered) {
    EntityAttrs attrs(42, /*history_capacity=*/2);
    attrs.set(1, 10);
    attrs.set(9, 90);
    attrs.set(2, 20);
    attrs.set(3, 30);
    attrs.set_domain(9, AttrDomain::kSelf);

    ViewerAttrState viewer;  // acked=0 → 快照（历史同时覆盖不了）
    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);

    EXPECT_TRUE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 3u);  // attr 1/2/3，无 self 域 attr 9
    std::sort(frame.deltas.begin(), frame.deltas.end(),
              [](const AttrDelta& a, const AttrDelta& b) {
                  return a.attr_id < b.attr_id;
              });
    EXPECT_EQ(frame.deltas[0].attr_id, 1u);
    EXPECT_EQ(frame.deltas[1].attr_id, 2u);
    EXPECT_EQ(frame.deltas[2].attr_id, 3u);
}

// ── slice 复合类型（attribute-sync §2.2 机制层；§3.4 保序例外） ─────────
// 组合 id 纪律：slot 0 = 容器长度格（(0<<16)|base = base 本体），数据 slot
// 1..max；线上 attr_id 高 16 位为 slot（§7.1）。声明面（slice_count/slice_
// type）归契约批——本组只锚机制：粒度、校验、收缩终态、先扩容后写的线序。

TEST(SliceContainer, LenWriteReadAndHistoryEntry) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 16);
    ASSERT_TRUE(attrs.is_slice(7));
    EXPECT_EQ(attrs.slice_max_slots(7), 16u);
    EXPECT_EQ(attrs.slice_len(7), 0u);  // 声明未设长 = 0

    attrs.set_slice_len(7, 3);
    EXPECT_EQ(attrs.slice_len(7), 3u);
    EXPECT_EQ(attrs.get(7), 3);  // 长度格 = base 本体（快照/读面自然携带）

    // 长度变更是同步 op：入 history（组合 id = base，值 = 新长）
    auto entries = attrs.history().entries_since(0);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].attr_id, 7u);
    EXPECT_EQ(entries[0].value, 3);
}

TEST(SliceContainer, LenAboveMaxRejected) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 4);
    attrs.set_slice_len(7, 5);  // > max 拒绝
    EXPECT_EQ(attrs.slice_len(7), 0u);
    EXPECT_TRUE(attrs.history().entries_since(0).empty());
}

TEST(SliceContainer, DirectBaseWriteRejected) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 4);
    attrs.set(7, 999);  // base 直写绕过收缩联动纪律 → 拒绝
    EXPECT_EQ(attrs.get(7), 0);
    EXPECT_TRUE(attrs.history().entries_since(0).empty());
    EXPECT_EQ(attrs.dirty_count(), 0u);

    attrs.set_slice_len(7, 1);  // 正门写入仍可用
    EXPECT_EQ(attrs.get(7), 1);
}

TEST(SliceContainer, SlotGranularWriteAndDirty) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 2);
    attrs.clear_dirty();

    attrs.set_slot(7, 2, 42);
    EXPECT_EQ(attrs.get_slot(7, 2), 42);
    EXPECT_EQ(attrs.get_slot(7, 1), 0);  // 他格不动（slot 粒度同步单元）

    // 组合 id 入 history：高 16 位 slot、低 16 位 base（§7.1 wire 形态）。
    // set_slice_len 本身也入 history（长度 = 同步属性）——entries[0] 是
    // 长度格（base 本体），entries[1] 才是 slot 写。
    const std::uint32_t slot2 = EntityAttrs::slice_slot_id(7, 2);
    auto entries = attrs.history().entries_since(0);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].attr_id, 7u);
    EXPECT_EQ(entries[0].value, 2);
    EXPECT_EQ(entries[1].attr_id, slot2);
    EXPECT_EQ(entries[1].value, 42);
    EXPECT_TRUE(attrs.is_dirty(slot2));
    EXPECT_FALSE(attrs.is_dirty(EntityAttrs::slice_slot_id(7, 1)));
}

TEST(SliceContainer, SlotWriteValidation) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 2);

    attrs.set_slot(7, 3, 99);   // 越界（slot > len）拒绝
    attrs.set_slot(7, 0, 99);   // slot 0 = 长度格，正门是 set_slice_len
    attrs.set_slot(8, 1, 99);   // 未声明容器拒绝
    EXPECT_EQ(attrs.get_slot(7, 3), 0);
    EXPECT_EQ(attrs.get_slot(7, 0), 2);  // slot 0 读穿 = 长度格（set_slice_len 已设 2）
    EXPECT_EQ(attrs.get_slot(8, 1), 0);
    EXPECT_EQ(attrs.history().entries_since(0).size(), 1u);  // 只有 set_slice_len 那条
}

TEST(SliceContainer, ShrinkErasesDeadSlotValues) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 3);
    attrs.set_slot(7, 1, 11);
    attrs.set_slot(7, 2, 22);
    attrs.set_slot(7, 3, 33);

    attrs.set_slice_len(7, 1);  // 收缩即终态：越界格值擦除（快照面不携带死格）
    EXPECT_EQ(attrs.get_slot(7, 2), 0);
    EXPECT_EQ(attrs.get_slot(7, 3), 0);
    EXPECT_EQ(attrs.values().count(EntityAttrs::slice_slot_id(7, 2)), 0u);
    EXPECT_EQ(attrs.values().count(EntityAttrs::slice_slot_id(7, 3)), 0u);
    EXPECT_EQ(attrs.values().count(EntityAttrs::slice_slot_id(7, 1)), 1u);
}

TEST(SliceContainer, CoordDecomposeAndDomainFallback) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 4);
    attrs.set_domain(7, AttrDomain::kSelf);

    const auto coord = attrs.slice_coord(EntityAttrs::slice_slot_id(7, 2));
    ASSERT_TRUE(coord.has_value());
    EXPECT_EQ(coord->base, 7u);
    EXPECT_EQ(coord->slot, 2u);

    // 长度格 = base 本体（slot 0）
    const auto len_coord = attrs.slice_coord(7);
    ASSERT_TRUE(len_coord.has_value());
    EXPECT_EQ(len_coord->slot, 0u);

    EXPECT_FALSE(attrs.slice_coord(EntityAttrs::slice_slot_id(9, 2)).has_value());
    EXPECT_FALSE(attrs.slice_coord(9).has_value());

    // slot 继承 base 域（§2.2 域按属性声明，slot 随 base）
    EXPECT_EQ(attrs.domain(EntityAttrs::slice_slot_id(7, 2)), AttrDomain::kSelf);
    EXPECT_EQ(attrs.domain(EntityAttrs::slice_slot_id(7, 0)), AttrDomain::kSelf);
}

// ── DeltaBuilder × slice：先扩容后写的线序 + 收缩死格过滤 ─────────

TEST(DeltaBuilder, SliceDeltaLenBeforeSlots) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 3);   // seq1 长度
    attrs.set_slot(7, 1, 11);    // seq2
    attrs.set_slot(7, 2, 22);    // seq3

    ViewerAttrState viewer;      // acked=0 → 快照路径
    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);
    ASSERT_TRUE(frame.is_snapshot);
    // 快照序 = attr_id 升序 = [长度(base), slot1, slot2]（先扩容后写 slot）
    ASSERT_EQ(frame.deltas.size(), 3u);
    EXPECT_EQ(frame.deltas[0].attr_id, 7u);
    EXPECT_EQ(frame.deltas[0].value, 3);
    EXPECT_EQ(frame.deltas[1].attr_id, EntityAttrs::slice_slot_id(7, 1));
    EXPECT_EQ(frame.deltas[1].value, 11);
    EXPECT_EQ(frame.deltas[2].attr_id, EntityAttrs::slice_slot_id(7, 2));

    // delta 路径同序（下一拍新增 slot 3 写）
    attrs.set_slot(7, 3, 33);
    viewer.set_acked(42, attrs.history().world_seq());
    attrs.set_slice_len(7, 4);   // 先扩容
    attrs.set_slot(7, 3, 33);    // 后写格
    auto delta = builder.build(42, 999, attrs, viewer);
    ASSERT_FALSE(delta.is_snapshot);
    ASSERT_EQ(delta.deltas.size(), 2u);
    EXPECT_EQ(delta.deltas[0].attr_id, 7u);                              // 长度先到
    EXPECT_EQ(delta.deltas[0].value, 4);
    EXPECT_EQ(delta.deltas[1].attr_id, EntityAttrs::slice_slot_id(7, 3));
}

TEST(DeltaBuilder, SliceDeltaShrinkDropsDeadSlotEntries) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 3);
    attrs.set_slot(7, 1, 11);
    ViewerAttrState viewer;
    viewer.set_acked(42, attrs.history().world_seq());  // 前态已确认

    // 同批：先写 slot 3（当时 len=3 合法），后收缩到 1 —— 收缩越过 = 死格，
    // 下发面丢弃死格条目（否则客户端先收到长度 1 再收到越界格写）。
    attrs.set_slot(7, 3, 33);
    attrs.set_slice_len(7, 1);

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);
    ASSERT_FALSE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].attr_id, 7u);   // 只有长度终值
    EXPECT_EQ(frame.deltas[0].value, 1);
}

TEST(DeltaBuilder, SliceDeltaPerSlotLastWriteWins) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_slice_len(7, 2);
    ViewerAttrState viewer;
    viewer.set_acked(42, attrs.history().world_seq());

    attrs.set_slot(7, 2, 10);
    attrs.set_slot(7, 2, 20);   // 同拍同格合并：last-write-wins + 计数

    DeltaBuilder builder;
    auto frame = builder.build(42, 999, attrs, viewer);
    ASSERT_FALSE(frame.is_snapshot);
    ASSERT_EQ(frame.deltas.size(), 1u);
    EXPECT_EQ(frame.deltas[0].attr_id, EntityAttrs::slice_slot_id(7, 2));
    EXPECT_EQ(frame.deltas[0].value, 20);
    EXPECT_EQ(frame.deltas[0].change_n, 2u);
}

TEST(DeltaBuilder, SliceDomainHiddenFromOtherViewer) {
    EntityAttrs attrs(42);
    attrs.declare_slice(7, 8);
    attrs.set_domain(7, AttrDomain::kSelf);  // slot 继承 base 域
    attrs.set_slice_len(7, 2);
    attrs.set_slot(7, 1, 11);

    ViewerAttrState other;
    ViewerAttrState owner;
    owner.set_acked(42, 0);
    DeltaBuilder builder;

    auto for_other = builder.build(42, 999, attrs, other);
    ASSERT_TRUE(for_other.is_snapshot);
    EXPECT_TRUE(for_other.deltas.empty());  // 长度与 slot 全部随 base 域隐藏

    auto for_owner = builder.build(42, 42, attrs, owner);
    ASSERT_TRUE(for_owner.is_snapshot);
    EXPECT_EQ(for_owner.deltas.size(), 2u);
}
