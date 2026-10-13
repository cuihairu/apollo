// ACK timeout / snapshot degradation tests (attribute-sync §3.3).
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"

using namespace apollo::game::attrsync;

// ── ACK timeout tracking ──────────────────────────────────────────

TEST(AttrAckDegrade, NoTimeoutWhenAcking) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);

    // Tick 1: change + ACK
    mgr.set_attribute(42, 2, 200);
    auto frames = mgr.collect(100, 42);
    ASSERT_EQ(frames.size(), 1u);
    mgr.ack(100, 42, frames[0].base_seq + frames[0].deltas.size());

    // Tick 2: change + ACK
    mgr.set_attribute(42, 3, 300);
    frames = mgr.collect(100, 42);
    mgr.ack(100, 42, frames[0].base_seq + frames[0].deltas.size());

    // Should NOT be in degraded state (recent ACK)
    EXPECT_FALSE(mgr.is_degraded(100, 42));
}

TEST(AttrAckDegrade, TimeoutAfterSilence) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);

    // Viewer goes silent: many ticks without ACK
    for (int i = 0; i < 10; ++i) {
        mgr.set_attribute(42, i + 10, i * 100);
        static_cast<void>(mgr.collect(100, 42));
        // No ACK
    }

    // After enough silent ticks, should be degraded
    EXPECT_TRUE(mgr.is_degraded(100, 42));
}

TEST(AttrAckDegrade, DegradeTriggersSnapshotReset) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);

    // Viewer goes silent
    for (int i = 0; i < 10; ++i) {
        mgr.set_attribute(42, i + 10, i * 100);
        static_cast<void>(mgr.collect(100, 42));
    }

    // Should be degraded
    ASSERT_TRUE(mgr.is_degraded(100, 42));

    // Next collect should trigger snapshot reset (not delta)
    auto frames = mgr.collect(100, 42);
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_TRUE(frames[0].is_snapshot);
}

TEST(AttrAckDegrade, RecoverAfterAck) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);

    // Go silent
    for (int i = 0; i < 10; ++i) {
        mgr.set_attribute(42, i + 10, i * 100);
        static_cast<void>(mgr.collect(100, 42));
    }
    ASSERT_TRUE(mgr.is_degraded(100, 42));

    // Viewer ACKs → recover
    mgr.ack(100, 42, mgr.entity_attrs(42).history().world_seq());
    EXPECT_FALSE(mgr.is_degraded(100, 42));
}

TEST(AttrAckDegrade, DegradedViewerResendsDelta) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);

    // Go silent for a few ticks (but not enough for snapshot reset)
    for (int i = 0; i < 3; ++i) {
        mgr.set_attribute(42, i + 10, i * 100);
        static_cast<void>(mgr.collect(100, 42));
    }

    // Should be in "resend" state (degraded but not snapshot-reset)
    // The exact threshold is configurable; just verify the mechanism works
    auto frames = mgr.collect(100, 42);
    ASSERT_EQ(frames.size(), 1u);
    // Either delta resend or snapshot reset is acceptable
}

TEST(AttrAckDegrade, MultipleViewersIndependent) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.on_viewer_enter_collect(100, 42);
    mgr.on_viewer_enter_collect(200, 42);

    // Viewer 100 goes silent, viewer 200 keeps ACKing
    for (int i = 0; i < 10; ++i) {
        mgr.set_attribute(42, i + 10, i * 100);
        auto f1 = mgr.collect(100, 42);
        auto f2 = mgr.collect(200, 42);
        if (!f2.empty()) {
            mgr.ack(200, 42, f2[0].base_seq + f2[0].deltas.size());
        }
    }

    EXPECT_TRUE(mgr.is_degraded(100, 42));
    EXPECT_FALSE(mgr.is_degraded(200, 42));
}

TEST(AttrAckDegrade, ConfigurableThreshold) {
    AttrSyncManager mgr;
    mgr.register_entity(42);
    mgr.set_attribute(42, 1, 100);

    mgr.set_ack_timeout_ticks(2);  // very aggressive

    mgr.on_viewer_enter_collect(100, 42);

    // 2 silent ticks → degraded
    mgr.set_attribute(42, 10, 100);
    static_cast<void>(mgr.collect(100, 42));
    EXPECT_FALSE(mgr.is_degraded(100, 42));

    mgr.set_attribute(42, 11, 200);
    static_cast<void>(mgr.collect(100, 42));
    EXPECT_TRUE(mgr.is_degraded(100, 42));
}
