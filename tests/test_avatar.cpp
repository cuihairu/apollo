// Avatar 玩家对象模型单测（P0-2 产物验证）。
//
// 覆盖（player-object-model §2/§3 + term-contract §1.2 口径）：
//   1. Avatar 身份与场景归属：进 scene 生（attach 即 Active）、出 scene 死；
//   2. 移动权威（Cell 职责一）：server-authority 位置 + reconcile 序门禁
//      （乱序/重放拒绝、非 Active 态拒绝）；
//   3. 战斗权威（Cell 职责二）：hp 结算、击杀判定、Leaving 态不结算；
//   4. AOI 广播（Cell 职责三）：viewer 集合幂等增删 + 广播计数；
//   5. Anchor 投影（player-object-model §3）：home_zone_id 单向拷贝、
//      Anchor 非 Online → Avatar 挂起、投影标记；
//   6. PlayerAnchor P0-2 扩展：home_zone_id 变更即脏 + journal 钩子触发；
//   7. PlayerId/EntityId 强分型：互不隐式转换（编译期校验）、无效哨兵、哈希键。
//
// 无网络、无定时器：全部纯内存断言，确定性单测。

#include "apollo/game/core/entity.hpp"
#include "apollo/game/session/player_anchor.hpp"
#include "apollo/game/world/avatar.hpp"

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <unordered_map>

#define TEST_ASSERT(cond, msg)                                                               \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")"     \
                      << std::endl;                                                          \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

namespace {

using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::session::AnchorState;
using apollo::game::session::PlayerAnchor;
using apollo::game::world::Avatar;
using apollo::game::world::AvatarState;

bool test_avatar_identity_and_scene_attachment() {
    PlayerId pid(42);
    EntityId eid(1001);
    Avatar avatar(pid, eid, "Alice");
    TEST_ASSERT(avatar.player_id() == pid, "player_id 应保留强分型");
    TEST_ASSERT(avatar.entity_id() == eid, "entity_id 应保留强分型");
    TEST_ASSERT(avatar.name() == "Alice", "name 投影快照");

    TEST_ASSERT(avatar.state() == AvatarState::Active, "初始即 Active（生来就在场）");
    TEST_ASSERT(avatar.scene_id() == 0, "初始未挂 scene");

    avatar.attach_scene(9);
    TEST_ASSERT(avatar.scene_id() == 9, "attach 后归属 scene");
    TEST_ASSERT(avatar.state() == AvatarState::Active, "attach 保持 Active");

    avatar.detach_scene();
    TEST_ASSERT(avatar.scene_id() == 0, "detach 清归属");
    TEST_ASSERT(avatar.state() == AvatarState::Leaving, "detach 即出场景（死前最后一态）");
    return true;
}

bool test_avatar_lifecycle_states() {
    Avatar avatar(PlayerId(1), EntityId(2), "Bob");
    avatar.suspend();
    TEST_ASSERT(avatar.state() == AvatarState::Suspended, "suspend 进入挂起");

    avatar.suspend();  // 幂等
    TEST_ASSERT(avatar.state() == AvatarState::Suspended, "重复 suspend 幂等");

    avatar.resume();
    TEST_ASSERT(avatar.state() == AvatarState::Active, "resume 回 Active");

    avatar.begin_leave();
    TEST_ASSERT(avatar.state() == AvatarState::Leaving, "begin_leave 进入 Leaving");

    avatar.suspend();  // Leaving 不可回退
    TEST_ASSERT(avatar.state() == AvatarState::Leaving, "Leaving 不可被 suspend 覆盖");
    avatar.resume();
    TEST_ASSERT(avatar.state() == AvatarState::Leaving, "Leaving 不可被 resume 覆盖");
    return true;
}

bool test_avatar_move_authority_reconcile() {
    Avatar avatar(PlayerId(1), EntityId(2), "Move");
    avatar.attach_scene(5);

    TEST_ASSERT(avatar.position().x == 0.0f && avatar.position().y == 0.0f,
                "初始位置默认原点");

    Avatar::Position target{1.0f, 2.0f, 3.0f};
    TEST_ASSERT(avatar.reconcile(1, target), "首个 reconcile 应接受");
    TEST_ASSERT(avatar.position().x == 1.0f && avatar.position().z == 3.0f,
                "接受后位置生效");
    TEST_ASSERT(avatar.last_reconciled_seq() == 1, "应用序记录");

    Avatar::Position rollback{0.0f, 0.0f, 0.0f};
    TEST_ASSERT(!avatar.reconcile(1, rollback), "同序重放应拒绝");
    TEST_ASSERT(avatar.position().x == 1.0f, "拒绝后位置不变");
    TEST_ASSERT(!avatar.reconcile(0, rollback), "乱序（更小）应拒绝");

    Avatar::Position forward{4.0f, 5.0f, 6.0f};
    TEST_ASSERT(avatar.reconcile(2, forward), "更大序应接受");
    TEST_ASSERT(avatar.position().y == 5.0f, "新位置生效");

    // 非 Active 态不接收移动输入
    avatar.suspend();
    TEST_ASSERT(!avatar.reconcile(3, rollback), "Suspended 态拒绝移动输入");
    avatar.resume();

    // 场景初始化落点走 set_position（server 权威直接写）
    avatar.set_position({7.0f, 8.0f, 9.0f});
    TEST_ASSERT(avatar.position().z == 9.0f, "set_position 为权威直写路径");
    return true;
}

bool test_avatar_combat_authority() {
    Avatar avatar(PlayerId(1), EntityId(2), "Fight");
    TEST_ASSERT(avatar.hp() == 100 && avatar.max_hp() == 100, "默认满血");

    avatar.set_hp(150, 200);
    TEST_ASSERT(avatar.hp() == 150 && avatar.max_hp() == 200, "set_hp 同步上下限");

    TEST_ASSERT(!avatar.apply_damage(50), "未致死不返回击杀");
    TEST_ASSERT(avatar.hp() == 100, "扣血生效");

    avatar.apply_damage(100);
    TEST_ASSERT(avatar.hp() == 0, "hp 下限钳 0");
    TEST_ASSERT(avatar.hp() == 0, "致死判定于 hp<=0");

    // Leaving 态不结算（出场景后战斗权威终止）
    Avatar leaving(PlayerId(2), EntityId(3), "Left");
    leaving.begin_leave();
    leaving.apply_damage(10);
    TEST_ASSERT(leaving.hp() == 100, "Leaving 态不接受伤害结算");
    return true;
}

bool test_avatar_aoi_broadcast() {
    Avatar avatar(PlayerId(1), EntityId(2), "Viewer");

    TEST_ASSERT(avatar.viewer_count() == 0, "初始无 view");

    EntityId v1(11);
    EntityId v2(12);
    avatar.add_viewer(v1);
    avatar.add_viewer(v2);
    TEST_ASSERT(avatar.viewer_count() == 2, "两个 viewer 入列");
    TEST_ASSERT(avatar.is_viewer(v1) && avatar.is_viewer(v2), "viewer 在列");

    avatar.add_viewer(v1);  // 幂等
    TEST_ASSERT(avatar.viewer_count() == 2, "重复 add 幂等");

    avatar.remove_viewer(v1);
    TEST_ASSERT(avatar.viewer_count() == 1, "remove 出列");
    TEST_ASSERT(!avatar.is_viewer(v1), "remove 后不在列");

    TEST_ASSERT(avatar.broadcast_count() == 0, "初始未广播");
    avatar.broadcast();
    avatar.broadcast();
    TEST_ASSERT(avatar.broadcast_count() == 2, "广播计数（P0-3 接 scene 后为实路径）");
    return true;
}

bool test_avatar_projection_from_anchor() {
    PlayerAnchor anchor(PlayerId(7).value());
    anchor.set_home_zone_id(3);
    anchor.set_state(AnchorState::Online);

    Avatar avatar(PlayerId(7), EntityId(2), "Proj");
    avatar.attach_scene(4);
    TEST_ASSERT(!avatar.has_projection(), "投影前标记未置");
    TEST_ASSERT(avatar.home_zone_id() == 0, "投影前无 home zone");

    avatar.project_from(anchor);
    TEST_ASSERT(avatar.has_projection(), "投影完成置标记");
    TEST_ASSERT(avatar.home_zone_id() == 3, "home_zone_id 从 Anchor 单向拷贝");
    TEST_ASSERT(avatar.state() == AvatarState::Active, "Anchor Online 投影保持 Active");

    // Anchor 离开 Online → Avatar 挂起（断线保活窗口，player-object-model §3）
    PlayerAnchor offline(PlayerId(7).value());
    offline.set_home_zone_id(3);
    offline.set_state(AnchorState::Disconnected);
    avatar.resume();
    avatar.project_from(offline);
    TEST_ASSERT(avatar.state() == AvatarState::Suspended, "Anchor 非 Online 投影 → Avatar 挂起");
    return true;
}

bool test_anchor_home_zone_journal() {
    PlayerAnchor anchor(PlayerId(9).value());

    TEST_ASSERT(anchor.home_zone_id() == 0, "初始未分配 home zone");

    // 变更 → 脏 + journal 引发（用可观测计数验证）
    int call_count = 0;
    anchor.set_journal([&call_count](const PlayerAnchor& anchor_ref,
                                     std::string_view reason) -> bool {
        (void)anchor_ref;
        (void)reason;
        ++call_count;
        return true;
    });
    anchor.set_home_zone_id(3);
    TEST_ASSERT(anchor.home_zone_id() == 3, "home_zone_id 生效");
    TEST_ASSERT(anchor.needs_save(), "home_zone 变更即脏");
    TEST_ASSERT(call_count == 1, "journal 钩子随脏上报触发一次");
    TEST_ASSERT(anchor.dirty_reasons().size() == 1, "脏原因入队");

    anchor.set_home_zone_id(3);  // 同值幂等
    TEST_ASSERT(call_count == 1, "同值设置不重复上报");

    anchor.set_home_zone_id(4);
    TEST_ASSERT(call_count == 2, "再次变更继续上报");
    TEST_ASSERT(anchor.dirty_reasons().size() == 2, "脏原因累积");

    anchor.clear_dirty();
    TEST_ASSERT(!anchor.needs_save(), "clear_dirty 复位");
    return true;
}

bool test_playerid_strong_typing() {
    // 编译期：PlayerId 与 EntityId 互不隐式转换（强分型的意义所在）
    static_assert(!std::is_convertible_v<EntityId, PlayerId>,
                  "EntityId 不得隐式转换为 PlayerId");
    static_assert(!std::is_convertible_v<PlayerId, EntityId>,
                  "PlayerId 不得隐式转换为 EntityId");
    static_assert(!std::is_convertible_v<std::uint64_t, PlayerId>,
                  "raw uint64 不得隐式转换为 PlayerId");

    TEST_ASSERT(!PlayerId{}.is_valid(), "默认构造为无效哨兵");
    TEST_ASSERT(PlayerId(5).is_valid(), "显式构造有效");
    TEST_ASSERT(PlayerId(5) == PlayerId(5), "同值相等");
    TEST_ASSERT(PlayerId(5) != PlayerId(6), "异值不等");
    TEST_ASSERT(PlayerId(5).value() == 5, "value 访问原值");
    TEST_ASSERT(PlayerId::invalid() == PlayerId{}, "invalid 哨兵 == 默认构造");

    // 哈希键（WorldSessionManager::player_index_ 同型用法）
    std::unordered_map<PlayerId, int, apollo::game::core::PlayerIdHash> index;
    index[PlayerId(1)] = 100;
    index[PlayerId(2)] = 200;
    TEST_ASSERT(index[PlayerId(1)] == 100, "PlayerId 可作哈希键");
    TEST_ASSERT(index.size() == 2, "两个玩家键互不覆盖");
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
        {"avatar_identity_and_scene_attachment", test_avatar_identity_and_scene_attachment},
        {"avatar_lifecycle_states", test_avatar_lifecycle_states},
        {"avatar_move_authority_reconcile", test_avatar_move_authority_reconcile},
        {"avatar_combat_authority", test_avatar_combat_authority},
        {"avatar_aoi_broadcast", test_avatar_aoi_broadcast},
        {"avatar_projection_from_anchor", test_avatar_projection_from_anchor},
        {"anchor_home_zone_journal", test_anchor_home_zone_journal},
        {"playerid_strong_typing", test_playerid_strong_typing},
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

    std::cout << "AvatarTests: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed == 0 ? 0 : 1;
}