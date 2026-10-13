// Scene × AttrSyncManager AOI 事件接线测试（attribute-sync §4.1，2026-10-14 批）。
//
// 覆盖：
//   - attach 后既有实体/avatar 注册面（attach 前 spawn / attach 后 spawn/enter）
//   - AOI Enter/Sync 首见 → ViewerState 建 + 快照帧（标记语义：deltas 空，
//     负载组装留后续批——DeltaBuilder §3.3 快照帧不带载荷）
//   - AOI Leave → ViewerState 回收
//   - 视野内移动（Sync）不重复触发；出视野再进 → 重建 + 重新首见
//   - detach 后 AOI 事件不再驱动 manager
//   - 首见后 set_attribute → collect 出 delta 帧（端到端路径）
//
// 空间布局：默认 SceneAoi(0,0,1,20) = 1x1 网格 + 半径 20；
//   A(1,1,0) B(2,1,0) 互见；A 移 (100,1,0) 出视野。

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"
#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/scene.hpp"

using namespace apollo::game::attrsync;
using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::world::Avatar;
using apollo::game::world::AvatarPtr;
using apollo::game::world::Scene;

namespace {

constexpr std::uint64_t kEntityA = 7001;
constexpr std::uint64_t kEntityB = 7002;

AvatarPtr make_avatar(std::uint64_t player, std::uint64_t entity) {
    return std::make_shared<Avatar>(PlayerId(player), EntityId(entity),
                                    "p" + std::to_string(player));
}

std::shared_ptr<apollo::game::core::Entity> make_entity(std::uint64_t id) {
    return std::make_shared<apollo::game::core::Entity>(EntityId(id), "NPC");
}

}  // namespace

// ── 注册面 ──────────────────────────────────────────────────

TEST(SceneAttrSyncWiring, AttachRegistersExistingEntities) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.spawn_entity(make_entity(9001));
    scene.spawn_entity(make_entity(9002));

    scene.attach_attrsync(mgr);

    EXPECT_TRUE(scene.attrsync_attached());
    EXPECT_TRUE(mgr.has_entity(9001));
    EXPECT_TRUE(mgr.has_entity(9002));
}

TEST(SceneAttrSyncWiring, SpawnAfterAttachRegisters) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);

    scene.spawn_entity(make_entity(9003));

    EXPECT_TRUE(mgr.has_entity(9003));
}

// ── 首见与双向可见 ──────────────────────────────────────────

TEST(SceneAttrSyncWiring, AvatarEnterBuildsMutualViewerStates) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);

    // A 先入：无既有实体，无事件
    EXPECT_TRUE(scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f}));
    // B 入 A 半径内：Sync(B,A) + Enter(A,B) 两侧首见
    EXPECT_TRUE(scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f}));

    EXPECT_TRUE(mgr.has_viewer(kEntityA, kEntityB));  // A 看得到 B
    EXPECT_TRUE(mgr.has_viewer(kEntityB, kEntityA));  // B 看得到 A

    // 双向各出一帧（无属性实体首见帧为空——快照标记面见下一用例）
    auto frames = scene.drain_attrsync_frames();
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_NE(frames[0].entity_id, frames[1].entity_id);
    EXPECT_TRUE(scene.drain_attrsync_frames().empty());
}

TEST(SceneAttrSyncWiring, FirstSightFramesCarrySubjectIds) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);

    EXPECT_TRUE(scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f}));
    mgr.set_attribute(kEntityA, 1, 100);  // A 的属性（注册随 enter 自动完成）
    EXPECT_TRUE(scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f}));

    auto frames = scene.drain_attrsync_frames();
    // 事件序：Sync(B,A) → 帧(entity=A)；Enter(A,B) → 帧(entity=B)
    ASSERT_EQ(frames.size(), 2u);
    EXPECT_EQ(frames[0].entity_id, kEntityA);
    // A 有属性 → 首见即快照标记（deltas 空：负载组装留后续批）
    EXPECT_TRUE(frames[0].is_snapshot);
    EXPECT_TRUE(frames[0].deltas.empty());
    EXPECT_EQ(frames[1].entity_id, kEntityB);  // B 进场时无属性 → 空帧
    EXPECT_FALSE(frames[1].is_snapshot);
}

// ── Leave 回收 ──────────────────────────────────────────────

TEST(SceneAttrSyncWiring, LeaveSceneRecyclesViewerStates) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);
    scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f});
    scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f});
    static_cast<void>(scene.drain_attrsync_frames());
    ASSERT_TRUE(mgr.has_viewer(kEntityA, kEntityB));

    EXPECT_TRUE(scene.leave(PlayerId(2)));

    EXPECT_FALSE(mgr.has_viewer(kEntityA, kEntityB));
    EXPECT_FALSE(mgr.has_viewer(kEntityB, kEntityA));
    // 离开不出新帧（销毁消息发送面留 P3-2）
    EXPECT_TRUE(scene.drain_attrsync_frames().empty());
}

// ── 视野内移动不触发协议 ────────────────────────────────────

TEST(SceneAttrSyncWiring, MoveWithinViewDoesNotResnapshot) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);
    scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f});
    scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f});
    static_cast<void>(scene.drain_attrsync_frames());

    scene.aoi().move(EntityId(kEntityA), {3.0f, 2.0f, 0.0f});

    EXPECT_TRUE(mgr.has_viewer(kEntityA, kEntityB));  // 仍互见
    EXPECT_TRUE(scene.drain_attrsync_frames().empty());  // Sync 不触发
}

// ── 出视野回收 + 回进重建 ──────────────────────────────────

TEST(SceneAttrSyncWiring, MoveOutOfViewThenBackResnapshots) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);
    scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f});
    scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f});
    static_cast<void>(scene.drain_attrsync_frames());

    scene.aoi().move(EntityId(kEntityA), {100.0f, 1.0f, 0.0f});  // 出半径 20
    EXPECT_FALSE(mgr.has_viewer(kEntityA, kEntityB));
    EXPECT_FALSE(mgr.has_viewer(kEntityB, kEntityA));
    EXPECT_TRUE(scene.drain_attrsync_frames().empty());

    scene.aoi().move(EntityId(kEntityA), {1.5f, 1.0f, 0.0f});  // 回到 B 半径
    EXPECT_TRUE(mgr.has_viewer(kEntityA, kEntityB));
    EXPECT_TRUE(mgr.has_viewer(kEntityB, kEntityA));
    EXPECT_EQ(scene.drain_attrsync_frames().size(), 2u);  // 双向重新首见
}

// ── detach 断驱动 ──────────────────────────────────────────

TEST(SceneAttrSyncWiring, DetachStopsWiring) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);
    scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f});
    scene.detach_attrsync();

    EXPECT_FALSE(scene.attrsync_attached());
    EXPECT_TRUE(scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f}));
    EXPECT_FALSE(mgr.has_viewer(kEntityA, kEntityB));
    EXPECT_FALSE(mgr.has_viewer(kEntityB, kEntityA));
}

// ── 首见后 delta 端到端 ────────────────────────────────────

TEST(SceneAttrSyncWiring, DeltaFlowsAfterFirstSight) {
    AttrSyncManager mgr;
    Scene scene(1u, "s");
    scene.attach_attrsync(mgr);
    scene.enter(make_avatar(1, kEntityA), {1.0f, 1.0f, 0.0f});
    mgr.set_attribute(kEntityA, 1, 100);  // 首见前 A 已有属性
    scene.enter(make_avatar(2, kEntityB), {2.0f, 1.0f, 0.0f});
    // 首见：快照标记帧 + acked 推进到 world_seq=1
    static_cast<void>(scene.drain_attrsync_frames());

    mgr.set_attribute(kEntityA, 2, 200);  // 首见后的新变更 → delta

    auto frames = mgr.collect(kEntityB, kEntityA);  // B 视角的 A 属性增量
    ASSERT_EQ(frames.size(), 1u);
    EXPECT_FALSE(frames[0].is_snapshot);
    ASSERT_EQ(frames[0].deltas.size(), 1u);
    EXPECT_EQ(frames[0].deltas[0].attr_id, 2u);
    EXPECT_EQ(frames[0].deltas[0].value, 200);
}
