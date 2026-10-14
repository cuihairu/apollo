// AttrSyncManager attr hook window tests — scripting-lua §5.1 接线（attrsync 侧）。
//
// 覆盖：
//   - 窗口时序：set_attribute 记录 → apply_attr_hook 修订 → collect 带修订值
//   - 去重：同 tick 同 (entity,attr) 多写收敛为 first-old / last-new
//   - 未修订条目（白名单外/预算跳过/错误保持原值/nil）原样通过
//   - 修订走正规写路径（values/dirty/history/sink 双条日志）
//   - 幂等：同 tick 二次 apply 空载
//   - 修订不重新入窗（下 tick 无重处理，防死循环）
//   - 空载零开销：无钩子不记录
//   - 写序保持
#include <cstdint>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"

using namespace apollo::game::attrsync;

namespace {

AttrSyncManager make_manager() {
    AttrSyncManager mgr;
    mgr.register_entity(1001);
    mgr.register_entity(1002);
    return mgr;
}

std::vector<AttrDelta> deltas_for(const std::vector<AttrSyncFrame>& frames,
                                  std::uint32_t attr_id) {
    std::vector<AttrDelta> out;
    for (const auto& f : frames) {
        for (const auto& d : f.deltas) {
            if (d.attr_id == attr_id) {
                out.push_back(d);
            }
        }
    }
    return out;
}

}  // namespace

TEST(AttrSyncManagerAttrHook, RevisesValueBeforeCollect) {
    auto mgr = make_manager();
    mgr.set_attr_hook([](std::vector<AttrSyncManager::HookChange>& changes) {
        for (auto& c : changes) {
            if (c.attr_id == 3) {
                c.new_value += 1;
            }
        }
    });

    mgr.set_attribute(1001, 3, 20);
    mgr.apply_attr_hook();
    EXPECT_EQ(mgr.get_attribute(1001, 3), 21);  // 存储已是修订值

    mgr.on_viewer_enter_collect(100, 1001);  // 快照先取基线
    mgr.set_attribute(1001, 3, 30);
    mgr.apply_attr_hook();
    auto frames = mgr.collect(100, 1001);
    ASSERT_EQ(frames.size(), 1u);
    auto deltas = deltas_for(frames, 3);
    // 同 tick 同变更两步（写入 + 修订）→ builder last-write-wins 收敛为一条最终值
    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].value, 31);
    EXPECT_EQ(deltas[0].change_n, 2u);
    EXPECT_EQ(mgr.get_attribute(1001, 3), 31);
}

TEST(AttrSyncManagerAttrHook, DedupesMultiWriteFirstOldLastNew) {
    auto mgr = make_manager();
    std::vector<std::pair<std::int64_t, std::int64_t>> seen;
    mgr.set_attr_hook([&](std::vector<AttrSyncManager::HookChange>& changes) {
        for (const auto& c : changes) {
            seen.push_back({c.old_value, c.new_value});
        }
    });

    mgr.set_attribute(1001, 3, 10);
    mgr.set_attribute(1001, 3, 30);  // 同 tick 同属性第二写
    mgr.apply_attr_hook();

    ASSERT_EQ(seen.size(), 1u);   // 去重为一帧
    EXPECT_EQ(seen[0].first, 0);   // first old = tick 前值（未设置 = 0）
    EXPECT_EQ(seen[0].second, 30); // last new
}

TEST(AttrSyncManagerAttrHook, UnmodifiedEntriesKept) {
    auto mgr = make_manager();
    mgr.set_attr_hook([](std::vector<AttrSyncManager::HookChange>& changes) {
        for (auto& c : changes) {
            if (c.attr_id == 3) {
                c.new_value += 1;  // 只修订白名单内
            }
        }
    });

    mgr.set_attribute(1001, 3, 20);
    mgr.set_attribute(1001, 7, 5);
    mgr.apply_attr_hook();

    EXPECT_EQ(mgr.get_attribute(1001, 3), 21);
    EXPECT_EQ(mgr.get_attribute(1001, 7), 5);  // 未修订原样
}

TEST(AttrSyncManagerAttrHook, RevisionFiresChangeSink) {
    auto mgr = make_manager();
    std::vector<std::pair<std::uint32_t, std::int64_t>> log;
    mgr.set_change_sink([&log](std::uint64_t, std::uint32_t aid, std::int64_t v) {
        log.push_back({aid, v});
    });
    mgr.set_attr_hook([](std::vector<AttrSyncManager::HookChange>& changes) {
        for (auto& c : changes) {
            c.new_value += 1;
        }
    });

    mgr.set_attribute(1001, 3, 42);
    mgr.apply_attr_hook();

    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].first, 3u);
    EXPECT_EQ(log[0].second, 42);  // 写入时
    EXPECT_EQ(log[1].first, 3u);
    EXPECT_EQ(log[1].second, 43);  // 修订时
}

TEST(AttrSyncManagerAttrHook, IdempotentWithinTick) {
    auto mgr = make_manager();
    int calls = 0;
    mgr.set_attr_hook([&](std::vector<AttrSyncManager::HookChange>&) { ++calls; });

    mgr.set_attribute(1001, 1, 10);
    mgr.apply_attr_hook();
    mgr.apply_attr_hook();  // 同 tick 二次调用
    EXPECT_EQ(calls, 1);    // 变更集取走即空，二次空载
}

TEST(AttrSyncManagerAttrHook, RevisionNotReprocessedNextTick) {
    auto mgr = make_manager();
    int calls = 0;
    mgr.set_attr_hook([&](std::vector<AttrSyncManager::HookChange>& changes) {
        ++calls;
        for (auto& c : changes) {
            c.new_value += 1;
        }
    });

    mgr.set_attribute(1001, 3, 20);
    mgr.apply_attr_hook();  // → 21
    EXPECT_EQ(calls, 1);

    mgr.set_attribute(1001, 3, 50);  // 下 tick 新写入
    mgr.apply_attr_hook();            // → 51
    EXPECT_EQ(calls, 2);              // 仅新写入一次，修订值未重入窗
    EXPECT_EQ(mgr.get_attribute(1001, 3), 51);
}

TEST(AttrSyncManagerAttrHook, NoHookZeroOverhead) {
    auto mgr = make_manager();
    mgr.set_attribute(1001, 1, 10);
    mgr.set_attribute(1001, 2, 20);
    mgr.apply_attr_hook();  // 无钩子空载 no-op
    EXPECT_EQ(mgr.get_attribute(1001, 1), 10);
    EXPECT_EQ(mgr.get_attribute(1001, 2), 20);
}

TEST(AttrSyncManagerAttrHook, ClearStopsWindow) {
    auto mgr = make_manager();
    int calls = 0;
    mgr.set_attr_hook([&](std::vector<AttrSyncManager::HookChange>&) { ++calls; });

    mgr.clear_attr_hook();
    mgr.set_attribute(1001, 1, 10);
    mgr.apply_attr_hook();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(mgr.get_attribute(1001, 1), 10);
}

TEST(AttrSyncManagerAttrHook, PreservesWriteOrder) {
    auto mgr = make_manager();
    std::vector<std::pair<std::uint64_t, std::uint32_t>> order;
    mgr.set_attr_hook([&](std::vector<AttrSyncManager::HookChange>& changes) {
        for (const auto& c : changes) {
            order.push_back({c.entity, c.attr_id});
        }
    });

    mgr.set_attribute(1001, 1, 10);
    mgr.set_attribute(1002, 2, 20);
    mgr.set_attribute(1001, 3, 30);
    mgr.apply_attr_hook();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], (std::pair<std::uint64_t, std::uint32_t>{1001, 1}));
    EXPECT_EQ(order[1], (std::pair<std::uint64_t, std::uint32_t>{1002, 2}));
    EXPECT_EQ(order[2], (std::pair<std::uint64_t, std::uint32_t>{1001, 3}));
}
