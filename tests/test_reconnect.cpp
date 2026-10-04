// Reconnect/保活窗口测试（P1-6；lifecycle §2.4；P1 出口判据⑤
// 「断线→wait→重连→Avatar 重建」）。
//
// 覆盖：
//   1. 断线挂机：session+Avatar 双侧 Suspended，驻留场景（AOI 保留）；
//   2. 窗口内 resume：双键 (session_id, token) 校验、过期判据、token 死；
//   3. 窗口满 sweep：close→finalize 收口链 + 场景侧移除；
//   4. 窗口外 resume 拒绝 → 完整重登（Avatar 按场景规则重建）。

#include "apollo/game/world/scene.hpp"
#include "apollo/game/world/avatar.hpp"
#include "apollo/game/world/world_session_manager.hpp"

#include <iostream>
#include <memory>
#include <vector>

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

namespace {

using apollo::game::core::EntityId;
using apollo::game::core::PlayerId;
using apollo::game::world::Avatar;
using apollo::game::world::Scene;
using apollo::game::world::WorldSession;
using apollo::game::world::WorldSessionManager;
using apollo::game::world::WorldSessionState;

struct Fixture {
    Scene scene{1, "town"};
    WorldSessionManager manager;

    std::shared_ptr<Avatar> enter(WorldSession::PlayerId pid, EntityId eid) {
        auto avatar = std::make_shared<Avatar>(pid, eid, "p" + std::to_string(pid.value()));
        if (!scene.enter(avatar, {5, 0, 5})) {
            return nullptr;
        }
        return avatar;
    }
};

bool test_disconnect_suspends_session_and_avatar() {
    Fixture fx;
    auto avatar = fx.enter(PlayerId(7), EntityId(70));
    auto session = fx.manager.create_session(9001, PlayerId(7));
    TEST_ASSERT(session->resume(), "Entering→Active 激活");
    session->bind_avatar(EntityId(70));

    // 断线：会话窗口化挂机 + 场景侧 Avatar 挂机（驻留，AOI 保留）
    TEST_ASSERT(fx.manager.suspend_session(9001, 1000, 100, 7777) != nullptr,
                "会话窗口化挂机");
    TEST_ASSERT(fx.scene.suspend_avatar(PlayerId(7)), "Avatar 挂机");
    TEST_ASSERT(session->state() == WorldSessionState::Suspended, "会话 Suspended");
    TEST_ASSERT(avatar->state() == apollo::game::world::AvatarState::Suspended,
                "Avatar Suspended");
    TEST_ASSERT(fx.scene.has_avatar(PlayerId(7)), "挂机驻留场景");
    TEST_ASSERT(fx.scene.aoi().contains(EntityId(70)), "AOI 保留");
    TEST_ASSERT(session->resume_deadline_tick() == 1100, "deadline = now + window");
    TEST_ASSERT(session->resume_token() == 7777, "token 在册（与窗口同源 TTL）");

    // 重复断线拒绝（已 Suspended 非 Active）
    TEST_ASSERT(fx.manager.suspend_session(9001, 1001, 100, 8888) == nullptr,
                "非 Active 不可再挂起");
    return true;
}

bool test_resume_within_window() {
    Fixture fx;
    fx.enter(PlayerId(8), EntityId(80));
    auto session = fx.manager.create_session(9002, PlayerId(8));
    TEST_ASSERT(session->resume(), "Entering→Active 激活");
    session->bind_avatar(EntityId(80));
    fx.manager.suspend_session(9002, 2000, 100, 4242);
    fx.scene.suspend_avatar(PlayerId(8));

    // token 失配：拒绝且状态不受污染
    TEST_ASSERT(fx.manager.resume_session(9002, 1111, 2050) == nullptr, "错 token 拒绝");
    TEST_ASSERT(session->state() == WorldSessionState::Suspended, "拒绝后仍挂机");
    // 窗口内正确 resume
    TEST_ASSERT(fx.manager.resume_session(9002, 4242, 2099) != nullptr, "窗口内 resume");
    TEST_ASSERT(session->state() == WorldSessionState::Active, "会话恢复 Active");
    TEST_ASSERT(fx.scene.resume_avatar(PlayerId(8)), "Avatar 恢复 Active");
    TEST_ASSERT(session->resume_token() == 0, "resume 后 token 消亡（一次性）");
    // token 已死：再 resume 拒绝
    TEST_ASSERT(fx.manager.resume_session(9002, 4242, 2100) == nullptr, "token 一次性");
    return true;
}

bool test_window_expiry_sweep_finalizes() {
    Fixture fx;
    auto avatar = fx.enter(PlayerId(9), EntityId(90));
    auto session = fx.manager.create_session(9003, PlayerId(9));
    TEST_ASSERT(session->resume(), "Entering→Active 激活");
    session->bind_avatar(EntityId(90));
    fx.manager.suspend_session(9003, 3000, 50, 999);
    fx.scene.suspend_avatar(PlayerId(9));

    // deadline 不可过（判据严格大于）
    TEST_ASSERT(fx.manager.sweep_suspended(3050).empty(), "deadline 当刻未满");
    // 窗口满：close→finalize 收口
    const auto swept = fx.manager.sweep_suspended(3051);
    TEST_ASSERT(swept.size() == 1 && swept[0] == 9003, "窗口满终结");
    TEST_ASSERT(fx.manager.find_session(9003) == nullptr, "索引摘除");
    TEST_ASSERT(fx.manager.find_by_player(PlayerId(9)) == nullptr, "玩家索引摘除");

    // 场景侧按 swept 清单移除（调用方驱动）
    TEST_ASSERT(fx.scene.leave(PlayerId(9)), "挂机超时移除");
    TEST_ASSERT(fx.scene.avatar_count() == 0, "Avatar 出场景销毁");
    TEST_ASSERT(!fx.scene.aoi().contains(EntityId(90)), "AOI 出列");
    return true;
}

bool test_resume_after_expiry_rebuilds_avatar() {
    Fixture fx;
    fx.enter(PlayerId(10), EntityId(100));
    auto session = fx.manager.create_session(9004, PlayerId(10));
    TEST_ASSERT(session->resume(), "Entering→Active 激活");
    fx.manager.suspend_session(9004, 4000, 50, 321);
    fx.scene.suspend_avatar(PlayerId(10));
    fx.manager.sweep_suspended(4100);  // 窗口满，会话终结

    // 窗口外 resume：token 已随会话终结而死
    TEST_ASSERT(fx.manager.resume_session(9004, 321, 4150) == nullptr,
                "窗口外 resume 拒绝");

    // sweep 调用方据清单移除挂机 Avatar（窗口满 Cell 移除）
    TEST_ASSERT(fx.scene.leave(PlayerId(10)), "挂机 Avatar 移除");

    // 完整重登：Avatar 按场景规则重建（新会话 + 新实体进场）
    auto rebuilt = fx.enter(PlayerId(10), EntityId(101));
    auto fresh = fx.manager.create_session(9005, PlayerId(10));
    fresh->bind_avatar(EntityId(101));
    TEST_ASSERT(fresh->resume() || fresh->state() == WorldSessionState::Active,
                "新会话激活");
    TEST_ASSERT(rebuilt->state() == apollo::game::world::AvatarState::Active,
                "重建 Avatar Active");
    TEST_ASSERT(fx.scene.has_avatar(PlayerId(10)), "重建后在场");
    return true;
}

bool test_zero_token_and_state_guards() {
    Fixture fx;
    fx.enter(PlayerId(11), EntityId(110));
    auto session = fx.manager.create_session(9006, PlayerId(11));
    TEST_ASSERT(session->resume(), "Entering→Active 激活");

    // token=0 哨兵：不可作为凭据（resume 侧 0 恒拒）
    TEST_ASSERT(fx.manager.suspend_session(9006, 5000, 100, 0) != nullptr,
                "token 0 允许挂机但不可恢复");
    TEST_ASSERT(fx.manager.resume_session(9006, 0, 5050) == nullptr, "0 token 不可作凭据");

    // 无窗口挂机（deadline=0 路径）不参与 sweep；9006（有窗）届时该满
    auto s2 = fx.manager.create_session(9007, PlayerId(12));
    TEST_ASSERT(s2->resume(), "Entering→Active 激活");
    TEST_ASSERT(s2->suspend(), "无窗挂机（旧口径）");
    const auto swept = fx.manager.sweep_suspended(9999);
    bool swept_9007 = false;
    for (const auto id : swept) {
        if (id == 9007) {
            swept_9007 = true;
        }
    }
    TEST_ASSERT(!swept_9007, "无窗口永不满（9007 不参与 sweep）");
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
        {"reconnect_disconnect_suspends_session_and_avatar",
         test_disconnect_suspends_session_and_avatar},
        {"reconnect_resume_within_window", test_resume_within_window},
        {"reconnect_window_expiry_sweep_finalizes", test_window_expiry_sweep_finalizes},
        {"reconnect_resume_after_expiry_rebuilds_avatar",
         test_resume_after_expiry_rebuilds_avatar},
        {"reconnect_zero_token_and_state_guards", test_zero_token_and_state_guards},
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

    std::cout << "ReconnectTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
