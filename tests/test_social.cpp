// Guild/Party 最小 CRUD 单测（P2-4，P2 出口判据④）。
//
// 覆盖（improvement-plan P2-4 口径）：
//   1. Guild：创建/加入/退出/解散/会长转让 + 满员/重复/会长保护拒绝面；
//   2. Party：创建/加入/退队/踢人/解散 + 队长移交（退队自动移交最早成员）；
//   3. Manager：双唯一裁决（guild id/name；party id）、空队回收、越权拒绝。

#include "apollo/game/social/guild.hpp"
#include "apollo/game/social/party.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::game::social::Guild;
using apollo::game::social::GuildManager;
using apollo::game::social::Party;
using apollo::game::social::PartyManager;

bool test_guild_crud_lifecycle() {
    GuildManager mgr;
    auto* g = mgr.create(1, "曙光", 100);
    TEST_ASSERT(g != nullptr, "建会");
    TEST_ASSERT(g->leader_player_id() == 100, "创会人即会长");
    TEST_ASSERT(g->member_count() == 1 && g->has_member(100), "会长为首位成员");

    TEST_ASSERT(g->add_member(101) && g->add_member(102), "纳新");
    TEST_ASSERT(!g->add_member(101), "重复加入拒绝");
    TEST_ASSERT(g->member_count() == 3, "成员数");
    TEST_ASSERT(g->members()[0] == 100, "成员表保序（会长在前）");

    TEST_ASSERT(!g->remove_member(100), "会长不可直接移除");
    TEST_ASSERT(g->remove_member(101), "普通成员移除");
    TEST_ASSERT(!g->has_member(101) && g->member_count() == 2, "移除生效");
    TEST_ASSERT(!g->remove_member(101), "移除不存在的成员拒绝");

    TEST_ASSERT(!g->transfer_leader(999), "转会籍外者拒绝");
    TEST_ASSERT(g->transfer_leader(102), "会长转让");
    TEST_ASSERT(g->leader_player_id() == 102, "新会长生效");
    TEST_ASSERT(g->remove_member(100), "前会长现可退会");

    TEST_ASSERT(!mgr.disband(1, 100), "前会长（已非会长）解散拒绝");
    return true;
}

bool test_guild_manager_uniqueness_and_disband() {
    GuildManager mgr;
    auto* g = mgr.create(1, "曙光", 100);
    TEST_ASSERT(g != nullptr, "建会");
    TEST_ASSERT(mgr.create(1, "另一会", 200) == nullptr, "重 id 拒绝");
    TEST_ASSERT(mgr.create(2, "曙光", 200) == nullptr, "重名拒绝");
    TEST_ASSERT(mgr.guild_count() == 1, "无副作用");

    TEST_ASSERT(mgr.find(1) == g, "按 id 查");
    TEST_ASSERT(mgr.find_by_name("曙光") == g, "按名查");
    TEST_ASSERT(mgr.find(404) == nullptr && mgr.find_by_name("无") == nullptr, "未命中");

    TEST_ASSERT(!mgr.disband(1, 999), "非会长解散拒绝");
    TEST_ASSERT(mgr.disband(1, 100), "会长解散");
    TEST_ASSERT(mgr.guild_count() == 0 && mgr.find(1) == nullptr, "解散即销毁");
    TEST_ASSERT(!mgr.disband(1, 100), "重复解散拒绝");

    // 尾段下标回填（多会删除中间会后的索引一致性）
    mgr.create(10, "甲", 1);
    mgr.create(11, "乙", 2);
    mgr.create(12, "丙", 3);
    TEST_ASSERT(mgr.disband(11, 2), "删中间会");
    TEST_ASSERT(mgr.find(12) != nullptr && mgr.find_by_name("丙") != nullptr, "尾段索引有效");
    TEST_ASSERT(mgr.find_by_name("乙") == nullptr, "被删会名已清");
    return true;
}

bool test_party_crud_and_leader_handover() {
    PartyManager mgr;
    auto* p = mgr.create(1, 100);
    TEST_ASSERT(p != nullptr, "建队");
    TEST_ASSERT(p->leader_player_id() == 100 && p->member_count() == 1, "队长即首位成员");

    TEST_ASSERT(p->add_member(101) && p->add_member(102), "入队");
    TEST_ASSERT(!p->add_member(101), "重复入队拒绝");
    TEST_ASSERT(p->member_count() == 3, "队伍人数");

    TEST_ASSERT(!p->kick(101, 101), "非队长踢人拒绝");
    TEST_ASSERT(!p->kick(100, 100), "队长踢自己走 leave（拒绝）");
    TEST_ASSERT(p->kick(101, 100), "队长踢人");
    TEST_ASSERT(!p->has_member(101), "被踢生效");

    TEST_ASSERT(p->transfer_leader(102), "队长转让");
    TEST_ASSERT(p->leader_player_id() == 102, "新队长生效");
    TEST_ASSERT(p->leave(100), "前队长退队");
    TEST_ASSERT(p->leader_player_id() == 102, "退队不移交（102 已是队长）");

    TEST_ASSERT(p->leave(102), "队长退队");
    TEST_ASSERT(p->member_count() == 0 && p->empty(), "空队");
    // 首位成员即队长的移交路径：重建一队验证
    auto* q = mgr.create(2, 200);
    TEST_ASSERT(q != nullptr, "重建队列");
    TEST_ASSERT(q->add_member(201) && q->add_member(202), "纳新");
    TEST_ASSERT(q->leave(200), "队长退队");
    TEST_ASSERT(q->leader_player_id() == 201, "移交最早成员（入队序）");
    return true;
}

bool test_party_manager_rules() {
    PartyManager mgr;
    TEST_ASSERT(mgr.create(1, 100) != nullptr, "建队");
    TEST_ASSERT(mgr.create(1, 200) == nullptr, "重 id 拒绝");
    TEST_ASSERT(mgr.party_count() == 1, "无副作用");

    TEST_ASSERT(!mgr.disband(1, 999), "非队长解散拒绝");
    TEST_ASSERT(!mgr.disband(404, 100), "解散不存在队伍拒绝");
    TEST_ASSERT(mgr.disband(1, 100), "队长解散");
    TEST_ASSERT(mgr.party_count() == 0, "回收");

    // 尾段下标回填
    mgr.create(10, 1);
    mgr.create(11, 2);
    mgr.create(12, 3);
    TEST_ASSERT(mgr.disband(11, 2), "删中间队");
    TEST_ASSERT(mgr.find(12) != nullptr && mgr.find(10) != nullptr, "尾段索引有效");
    TEST_ASSERT(mgr.find(11) == nullptr, "被删队已清");
    return true;
}

bool test_party_capacity_and_guild_boundary() {
    PartyManager pm;
    auto* p = pm.create(1, 100);
    for (std::uint64_t id = 101; id <= 104; ++id) {
        TEST_ASSERT(p->add_member(id), "补满五人队");
    }
    TEST_ASSERT(!p->add_member(105), "满员拒绝");
    TEST_ASSERT(Party::kMaxMembers == 5, "队伍容量口径");

    GuildManager gm;
    auto* g = gm.create(1, "满员会", 100);
    for (std::uint64_t id = 101; id <= 100 + (Guild::kMaxMembers - 1); ++id) {
        if (!g->add_member(id)) {
            std::cerr << "FAIL: 公会纳新至满员中断 @" << id << std::endl;
            return false;
        }
    }
    TEST_ASSERT(g->member_count() == Guild::kMaxMembers, "公会满员");
    TEST_ASSERT(!g->add_member(99999), "公会满员拒绝");
    return true;
}

} // namespace

int main() {
    int passed = 0;
    int failed = 0;

    struct Case {
        const char* name;
        bool (*fn)();
    } cases[] = {
        {"guild_crud_lifecycle", test_guild_crud_lifecycle},
        {"guild_manager_uniqueness_and_disband", test_guild_manager_uniqueness_and_disband},
        {"party_crud_and_leader_handover", test_party_crud_and_leader_handover},
        {"party_manager_rules", test_party_manager_rules},
        {"party_capacity_and_guild_boundary", test_party_capacity_and_guild_boundary},
    };

    for (const auto& c : cases) {
        std::cout << "[ RUN  ] " << c.name << std::endl;
        if (c.fn()) {
            std::cout << "[  OK  ] " << c.name << std::endl;
            ++passed;
        } else {
            std::cout << "[ FAIL ] " << c.name << std::endl;
            ++failed;
        }
    }

    std::cout << "SocialTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}
