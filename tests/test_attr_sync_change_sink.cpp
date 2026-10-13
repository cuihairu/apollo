// AttrSyncManager change sink tests — attribute-sync §8.2 write-behind 入口。
//
// 覆盖：set_change_sink 注入回调（§8.2 持久化管线接入点）、
//       每次 set_attribute 触发、多实体/多属性逐条回调、
//       无 sink 时静默、清 sink 后停报。
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "apollo/game/attrsync/attr_sync_manager.hpp"

using namespace apollo::game::attrsync;

namespace {

struct ChangeRecord {
    std::uint64_t entity_id;
    std::uint32_t attr_id;
    std::int64_t value;
};

AttrSyncManager make_manager() {
    AttrSyncManager mgr;
    mgr.register_entity(1001);
    mgr.register_entity(1002);
    return mgr;
}

}  // namespace

TEST(AttrSyncManagerChangeSink, FiresOnSet) {
    auto mgr = make_manager();
    std::vector<ChangeRecord> log;
    mgr.set_change_sink([&log](std::uint64_t eid, std::uint32_t aid, std::int64_t v) {
        log.push_back({eid, aid, v});
    });

    mgr.set_attribute(1001, 5, 42);
    ASSERT_EQ(log.size(), 1u);
    EXPECT_EQ(log[0].entity_id, 1001u);
    EXPECT_EQ(log[0].attr_id, 5u);
    EXPECT_EQ(log[0].value, 42);
}

TEST(AttrSyncManagerChangeSink, FiresPerEntity) {
    auto mgr = make_manager();
    std::vector<ChangeRecord> log;
    mgr.set_change_sink([&log](std::uint64_t eid, std::uint32_t aid, std::int64_t v) {
        log.push_back({eid, aid, v});
    });

    mgr.set_attribute(1001, 1, 100);
    mgr.set_attribute(1002, 1, 200);
    ASSERT_EQ(log.size(), 2u);
    EXPECT_EQ(log[0].entity_id, 1001u);
    EXPECT_EQ(log[1].entity_id, 1002u);
}

TEST(AttrSyncManagerChangeSink, FiresPerAttribute) {
    auto mgr = make_manager();
    std::vector<ChangeRecord> log;
    mgr.set_change_sink([&log](std::uint64_t eid, std::uint32_t aid, std::int64_t v) {
        log.push_back({eid, aid, v});
    });

    mgr.set_attribute(1001, 1, 10);
    mgr.set_attribute(1001, 2, 20);
    mgr.set_attribute(1001, 3, 30);
    ASSERT_EQ(log.size(), 3u);
    EXPECT_EQ(log[0].attr_id, 1u);
    EXPECT_EQ(log[1].attr_id, 2u);
    EXPECT_EQ(log[2].attr_id, 3u);
}

TEST(AttrSyncManagerChangeSink, NoSinkNoCrash) {
    auto mgr = make_manager();
    // 无 sink 注入 → set_attribute 正常工作，无回调
    mgr.set_attribute(1001, 1, 100);
    EXPECT_EQ(mgr.get_attribute(1001, 1), 100);
}

TEST(AttrSyncManagerChangeSink, ClearStopsReporting) {
    auto mgr = make_manager();
    int count = 0;
    mgr.set_change_sink([&count](std::uint64_t, std::uint32_t, std::int64_t) {
        ++count;
    });

    mgr.set_attribute(1001, 1, 100);
    EXPECT_EQ(count, 1);

    mgr.clear_change_sink();
    mgr.set_attribute(1001, 2, 200);
    EXPECT_EQ(count, 1);  // 清后不再触发
}

TEST(AttrSyncManagerChangeSink, UnregisteredEntityNoFire) {
    auto mgr = make_manager();
    std::vector<ChangeRecord> log;
    mgr.set_change_sink([&log](std::uint64_t eid, std::uint32_t aid, std::int64_t v) {
        log.push_back({eid, aid, v});
    });

    // 未注册实体静默忽略（set_attribute 不触发回调）
    mgr.set_attribute(9999, 1, 100);
    EXPECT_TRUE(log.empty());
}
