// AttrSyncScheduler tests — attribute-sync §5 优先级与节流。
//
// 覆盖：token bucket 带宽预算（§5.1）、实体优先级排序（§5.2）、
//       min_interval 节流分级（§5.3）、饥饿保护（§5.2）。
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"
#include "apollo/game/attrsync/attr_sync_scheduler.hpp"

using namespace apollo::game::attrsync;

namespace {

constexpr std::uint64_t kViewer = 100;
constexpr std::uint64_t kEntityA = 2001;
constexpr std::uint64_t kEntityB = 2002;

AttrSyncManager make_manager() {
    AttrSyncManager mgr;
    mgr.register_entity(kEntityA);
    mgr.register_entity(kEntityB);
    // 预设 attr 0（世界态存在后 viewer 进视野 → 后续变更走 delta 路径，
    // 避免 acked==0 触发快照切换）
    mgr.set_attribute(kEntityA, 0, 0);
    mgr.set_attribute(kEntityB, 0, 0);
    // viewer 进视野（collect 要求 viewer 态已建立）
    mgr.on_viewer_enter(kViewer, kEntityA);
    mgr.on_viewer_enter(kViewer, kEntityB);
    return mgr;
}

}  // namespace

// ── §5.1 token bucket ─────────────────────────────────────────────

TEST(AttrSyncScheduler, TokenBucketRefillsPerTick) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(10);  // 单帧 ~9 字节：每拍只够 1 帧

    mgr.set_attribute(kEntityA, 1, 100);
    mgr.set_attribute(kEntityB, 1, 200);

    auto t0 = sched.schedule(kViewer, {kEntityA, kEntityB});
    EXPECT_EQ(t0.size(), 1u);  // 预算只够 1 帧

    sched.advance_tick();
    auto t1 = sched.schedule(kViewer, {kEntityA, kEntityB});
    EXPECT_EQ(t1.size(), 1u);  // 预算补充，第二帧下发
}

TEST(AttrSyncScheduler, NoBudgetNoSend) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(0);

    mgr.set_attribute(kEntityA, 1, 100);
    auto frames = sched.schedule(kViewer, {kEntityA});
    EXPECT_TRUE(frames.empty());
}

TEST(AttrSyncScheduler, NoChangesNoFrame) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);

    // 实体无属性变更 → collect 返回空 → 调度器不产出帧
    auto frames = sched.schedule(kViewer, {kEntityA});
    EXPECT_TRUE(frames.empty());
}

// ── §5.2 实体优先级 ──────────────────────────────────────────────

TEST(AttrSyncScheduler, PriorityOrdersImportanceFirst) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);  // 两帧都装得下 → 检查顺序
    sched.set_entity_importance(kEntityB, 200);  // B 重要性高于 A（默认 0）

    mgr.set_attribute(kEntityA, 1, 100);
    mgr.set_attribute(kEntityB, 1, 200);

    auto frames = sched.schedule(kViewer, {kEntityA, kEntityB});
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].entity_id, kEntityB);  // 高优先级先发
    EXPECT_EQ(frames[1].entity_id, kEntityA);
}

TEST(AttrSyncScheduler, HigherDirtyCountBreaksTie) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    // 同重要性（默认 0），dirty 多者优先
    mgr.set_attribute(kEntityA, 1, 100);
    mgr.set_attribute(kEntityA, 2, 100);
    mgr.set_attribute(kEntityB, 1, 200);

    auto frames = sched.schedule(kViewer, {kEntityA, kEntityB});
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].entity_id, kEntityA);  // A 有 2 个 dirty，B 有 1
    EXPECT_EQ(frames[1].entity_id, kEntityB);
}

// ── §5.3 min_interval 节流 ────────────────────────────────────────

TEST(AttrSyncScheduler, MinIntervalThrottlesLowFrequency) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    sched.set_min_interval(kEntityA, 4);  // 每 4 拍最多发一次

    mgr.set_attribute(kEntityA, 1, 100);

    // 拍 0：首次发送（last_sent 从 -∞ 起，满足间隔）
    auto t0 = sched.schedule(kViewer, {kEntityA});
    ASSERT_EQ(t0.size(), 1u);

    // 拍 1-3：节流窗口内，不发
    for (int i = 0; i < 3; ++i) {
        sched.advance_tick();
        mgr.set_attribute(kEntityA, 1, 100 + i);  // 持续变更
        auto frames = sched.schedule(kViewer, {kEntityA});
        EXPECT_TRUE(frames.empty());
    }

    // 拍 4：窗口满，发送
    sched.advance_tick();
    auto t4 = sched.schedule(kViewer, {kEntityA});
    EXPECT_EQ(t4.size(), 1u);
}

TEST(AttrSyncScheduler, InstantBypassesMinInterval) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    sched.set_min_interval(kEntityA, 0);  // INSTANT：每拍都发

    mgr.set_attribute(kEntityA, 1, 100);
    EXPECT_EQ(sched.schedule(kViewer, {kEntityA}).size(), 1u);

    sched.advance_tick();
    mgr.set_attribute(kEntityA, 1, 200);
    EXPECT_EQ(sched.schedule(kViewer, {kEntityA}).size(), 1u);  // 无节流
}

// ── §5.2 饥饿保护 ─────────────────────────────────────────────────

TEST(AttrSyncScheduler, StarvationForcesSend) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    sched.set_min_interval(kEntityA, 100);  // 强节流
    sched.set_max_defer_ticks(3);            // 但最多推迟 3 拍

    mgr.set_attribute(kEntityA, 1, 100);
    EXPECT_EQ(sched.schedule(kViewer, {kEntityA}).size(), 1u);  // 首拍发

    // 拍 1-2：节流窗口内，推迟（未超饥饿阈值）
    for (int i = 0; i < 2; ++i) {
        sched.advance_tick();
        mgr.set_attribute(kEntityA, 1, 100 + i);
        EXPECT_TRUE(sched.schedule(kViewer, {kEntityA}).empty());
    }

    // 拍 3：推迟已达 max_defer → 强制发送
    sched.advance_tick();
    auto forced = sched.schedule(kViewer, {kEntityA});
    EXPECT_EQ(forced.size(), 1u);
}

// ── 合并语义 ──────────────────────────────────────────────────────

TEST(AttrSyncScheduler, LastWriteWinsMergesJitter) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    sched.set_min_interval(kEntityA, 0);

    // 同拍内多次变更 → collect 只产 1 帧（last-write-wins）
    mgr.set_attribute(kEntityA, 1, 100);
    mgr.set_attribute(kEntityA, 1, 150);
    mgr.set_attribute(kEntityA, 1, 200);

    auto frames = sched.schedule(kViewer, {kEntityA});
    ASSERT_EQ(frames.size(), 1u);
    ASSERT_EQ(frames[0].deltas.size(), 1u);
    EXPECT_EQ(frames[0].deltas[0].value, 200);  // 最终值
}

// ── 帧内容校验 ────────────────────────────────────────────────────

TEST(AttrSyncScheduler, EmittedFrameCarriesDeltas) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);

    mgr.set_attribute(kEntityA, 7, 42);
    auto frames = sched.schedule(kViewer, {kEntityA});
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_EQ(frames[0].entity_id, kEntityA);
    EXPECT_FALSE(frames[0].is_snapshot);
    ASSERT_EQ(frames[0].deltas.size(), 1u);
    EXPECT_EQ(frames[0].deltas[0].attr_id, 7u);
    EXPECT_EQ(frames[0].deltas[0].value, 42);
}

TEST(AttrSyncScheduler, ScheduleTracksPerViewer) {
    auto mgr = make_manager();
    AttrSyncScheduler sched(mgr);
    sched.set_bandwidth_budget(1024);
    sched.set_min_interval(kEntityA, 2);

    constexpr std::uint64_t kViewer2 = 999;
    mgr.on_viewer_enter(kViewer2, kEntityA);
    mgr.set_attribute(kEntityA, 1, 100);

    // viewer 1 发送
    EXPECT_EQ(sched.schedule(kViewer, {kEntityA}).size(), 1u);

    sched.advance_tick();
    // viewer 2 独立节流窗口：仍可发
    EXPECT_EQ(sched.schedule(kViewer2, {kEntityA}).size(), 1u);

    // viewer 1 仍在窗口内
    EXPECT_TRUE(sched.schedule(kViewer, {kEntityA}).empty());
}
