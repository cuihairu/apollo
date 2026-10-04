// PlayerDirectory 行为契约单测（P1-2，session-and-online-directory）。
// 直编源惯例同 avatar_tests——不依赖 GAME_MODULE 目标，保证三平台 Unit
// 门禁真实执行（CI 矩阵 GAME_MODULE=OFF，挂 game_session 目标的测试被跳过）。
#include "apollo/game/session/player_directory.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace {

using apollo::game::session::PlayerDirectory;
using apollo::game::session::SessionBinding;
using apollo::game::session::WorldAssignment;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// 事件收集 sink：按序记录 (kind, player, epoch, reason)
struct EventLog {
    struct Row {
        PlayerDirectory::Event::Kind kind;
        std::uint64_t player_id;
        std::uint64_t anchor_epoch;
        std::uint32_t reason;
    };
    std::vector<Row> rows;

    void record(const PlayerDirectory::Event& e) {
        rows.push_back({e.kind, e.player_id, e.anchor_epoch, e.reason});
    }
    void clear() { rows.clear(); }
};

SessionBinding makeBinding(std::uint64_t session_id) {
    SessionBinding b;
    b.session_id = session_id;
    b.gateway_id = 7;
    b.gateway_addr = "gateway://127.0.0.1:8888";
    b.bind_time_ms = static_cast<std::int64_t>(session_id);
    return b;
}

WorldAssignment makeAssignment(std::uint32_t world_id) {
    WorldAssignment a;
    a.world_id = world_id;
    a.map_id = 100;
    a.instance_id = 1;
    a.space_id = 1000;
    a.route_version = 1;
    return a;
}

// ---- §1/§2：登记锚点 → 条目级真值 + SessionUp ----
bool test_session_up_registers_entry() {
    std::cout << "Running: test_session_up_registers_entry..." << std::endl;

    PlayerDirectory dir;
    EventLog log;
    dir.set_event_sink([&log](const PlayerDirectory::Event& e) { log.record(e); });

    const auto epoch = dir.session_up(1001, makeBinding(9001), makeAssignment(1), 3);
    TEST_ASSERT(epoch == 1, "进程内 epoch 从 1 起单调");
    TEST_ASSERT(dir.size() == 1, "目录条目数 = 1");

    const auto* entry = dir.find(1001);
    TEST_ASSERT(entry != nullptr, "条目可查");
    TEST_ASSERT(entry->state == PlayerDirectory::EntryState::Online, "登记即 Online");
    TEST_ASSERT(entry->anchor_epoch == 1, "条目携带 epoch");
    TEST_ASSERT(entry->binding.session_id == 9001, "条目携带 binding");
    TEST_ASSERT(entry->assignment.world_id == 1, "条目携带 assignment");
    TEST_ASSERT(entry->zone_id == 3, "条目携带 zone");

    TEST_ASSERT(dir.online_ids() == std::vector<std::uint64_t>{1001}, "online_ids 含新条目");
    TEST_ASSERT(dir.suspended_ids().empty(), "无挂机条目");

    TEST_ASSERT(log.rows.size() == 1, "SessionUp 事件恰一条");
    TEST_ASSERT(log.rows[0].kind == PlayerDirectory::Event::Kind::SessionUp, "事件类别 Up");
    TEST_ASSERT(log.rows[0].player_id == 1001 && log.rows[0].anchor_epoch == 1, "事件带身份键");

    // §2 幂等键：player_id == 0 拒绝且无副作用
    log.clear();
    TEST_ASSERT(dir.session_up(0, makeBinding(1), {}) == 0, "player_id=0 登记返回 0");
    TEST_ASSERT(dir.size() == 1 && log.rows.empty(), "player_id=0 无副作用无事件");
    return true;
}

// ---- §3：顶号新顶旧 ----
bool test_relogin_kicks_old_session() {
    std::cout << "Running: test_relogin_kicks_old_session..." << std::endl;

    PlayerDirectory dir;
    EventLog log;
    dir.set_event_sink([&log](const PlayerDirectory::Event& e) { log.record(e); });

    const auto first = dir.session_up(1001, makeBinding(9001), makeAssignment(1));
    TEST_ASSERT(first == 1, "首登 epoch=1");

    // 同账号再登（Online 态命中）→ 旧条目 Kicked + 新条目新 epoch
    log.clear();
    const auto second = dir.session_up(1001, makeBinding(9002), makeAssignment(2), 5);
    TEST_ASSERT(second == 2, "新条目分配递增 epoch");
    TEST_ASSERT(dir.size() == 1, "顶号后仍单条目");

    const auto* entry = dir.find(1001);
    TEST_ASSERT(entry != nullptr && entry->binding.session_id == 9002, "新 binding 生效");
    TEST_ASSERT(entry->assignment.world_id == 2 && entry->zone_id == 5, "新 assignment/zone 生效");

    TEST_ASSERT(log.rows.size() == 2, "顶号产 Kicked+Up 两个事件");
    TEST_ASSERT(log.rows[0].kind == PlayerDirectory::Event::Kind::SessionKicked,
                "先 Kicked（旧）");
    TEST_ASSERT(log.rows[0].anchor_epoch == 1, "Kicked 事件携带旧 epoch");
    TEST_ASSERT(log.rows[0].reason == PlayerDirectory::kReasonKickedByRelogin, "原因=顶号");
    TEST_ASSERT(log.rows[1].kind == PlayerDirectory::Event::Kind::SessionUp, "后 Up（新）");
    TEST_ASSERT(log.rows[1].anchor_epoch == 2, "Up 事件携带新 epoch");

    // Suspended 态同样可被顶（§3 不拘旧条目状态）
    log.clear();
    TEST_ASSERT(dir.mark_suspended(1001, 100, 50), "进保活窗口");
    const auto third = dir.session_up(1001, makeBinding(9003), makeAssignment(3));
    TEST_ASSERT(third == 3 && dir.find(1001)->state == PlayerDirectory::EntryState::Online,
                "挂机中条目同样被顶");

    // evict_for_relogin 直呼：无条目返回 0 且无事件
    log.clear();
    TEST_ASSERT(dir.evict_for_relogin(424242) == 0, "无条目顶号返回 0");
    TEST_ASSERT(log.rows.empty(), "无条目顶号无事件");
    return true;
}

// ---- §2/§1：显式下线 ----
bool test_session_down_removes_entry() {
    std::cout << "Running: test_session_down_removes_entry..." << std::endl;

    PlayerDirectory dir;
    EventLog log;
    dir.set_event_sink([&log](const PlayerDirectory::Event& e) { log.record(e); });

    dir.session_up(1001, makeBinding(9001), makeAssignment(1));
    log.clear();

    TEST_ASSERT(!dir.session_down(424242), "未知玩家下线 false");
    TEST_ASSERT(log.rows.empty(), "未知玩家无事件");

    TEST_ASSERT(dir.session_down(1001), "已知玩家下线 true");
    TEST_ASSERT(dir.find(1001) == nullptr && dir.size() == 0, "条目已删");
    TEST_ASSERT(dir.online_ids().empty(), "online 集为空");
    TEST_ASSERT(log.rows.size() == 1 &&
                    log.rows[0].kind == PlayerDirectory::Event::Kind::SessionDown &&
                    log.rows[0].anchor_epoch == 1 &&
                    log.rows[0].reason == PlayerDirectory::kReasonLogout,
                "SessionDown 事件携带 epoch 与登出原因");

    TEST_ASSERT(!dir.session_down(1001), "重复下线 false（set 语义幂等）");
    return true;
}

// ---- §4：掉线保活窗口 + sweep 到期 ----
bool test_suspend_window_and_sweep() {
    std::cout << "Running: test_suspend_window_and_sweep..." << std::endl;

    PlayerDirectory dir;
    EventLog log;
    dir.set_event_sink([&log](const PlayerDirectory::Event& e) { log.record(e); });

    dir.session_up(1001, makeBinding(9001), makeAssignment(1));
    dir.session_up(1002, makeBinding(9002), makeAssignment(1));
    log.clear();

    // Leaving 不回窗口
    TEST_ASSERT(dir.begin_leave(1002), "显式登出进 Leaving");
    TEST_ASSERT(!dir.mark_suspended(1002, 0, 50), "Leaving 不进保活窗口");
    TEST_ASSERT(dir.mark_suspended(424242, 0, 50) == false, "未知玩家不进保活窗口");

    TEST_ASSERT(dir.mark_suspended(1001, 100, 50), "Online 进窗口");
    TEST_ASSERT(dir.suspended_ids() == std::vector<std::uint64_t>{1001}, "挂机集含 1001");
    TEST_ASSERT(dir.online_ids().empty(), "online 集空（1001 挂机、1002 Leaving 均不计在线）");

    // 未到期 sweep 不终结
    TEST_ASSERT(dir.sweep(149) == 0, "窗口内 sweep 无终结");
    TEST_ASSERT(dir.find(1001) != nullptr, "窗口内条目保留");

    // 到期 sweep：删条目 + SessionDown(窗口满)
    TEST_ASSERT(dir.sweep(150) == 1, "到期 sweep 终结 1 条");
    TEST_ASSERT(dir.find(1001) == nullptr, "到期条目已删");
    TEST_ASSERT(log.rows.size() == 1 &&
                    log.rows[0].kind == PlayerDirectory::Event::Kind::SessionDown &&
                    log.rows[0].player_id == 1001 &&
                    log.rows[0].reason == PlayerDirectory::kReasonWindowExpired,
                "窗口满终结产 SessionDown(窗口满)");
    return true;
}

// ---- §4/§3-③：resume 双锚校验 ----
bool test_resume_dual_anchor() {
    std::cout << "Running: test_resume_dual_anchor..." << std::endl;

    PlayerDirectory dir;
    dir.session_up(1001, makeBinding(9001), makeAssignment(1));
    const auto* entry = dir.find(1001);
    const auto epoch = entry->anchor_epoch;
    TEST_ASSERT(epoch == 1, "首登 epoch");

    // 仅 Suspended 可 resume
    TEST_ASSERT(!dir.resume(1001, 9001, epoch), "Online 态 resume 拒绝");
    TEST_ASSERT(dir.mark_suspended(1001, 100, 50), "进保活窗口");

    TEST_ASSERT(!dir.resume(424242, 9001, epoch), "未知玩家 resume 拒绝");
    TEST_ASSERT(!dir.resume(1001, 9999, epoch), "旧/错 session_id 拒绝");
    TEST_ASSERT(!dir.resume(1001, 9001, epoch + 10), "错 epoch 拒绝（§3-③ 竞态守卫）");
    TEST_ASSERT(dir.find(1001)->state == PlayerDirectory::EntryState::Suspended,
                "拒绝后仍挂机");

    TEST_ASSERT(dir.resume(1001, 9001, epoch), "双锚匹配 resume 成功");
    TEST_ASSERT(dir.find(1001)->state == PlayerDirectory::EntryState::Online, "回 Online");
    TEST_ASSERT(dir.find(1001)->deadline_tick == 0, "窗口截止清零");
    TEST_ASSERT(dir.suspended_ids().empty() &&
                    dir.online_ids() == std::vector<std::uint64_t>{1001},
                "状态列迁移正确");

    // 顶号后旧 epoch 的 resume 拒绝（§3-③：旧会话复活被顶号裁决拦下）
    dir.session_up(1001, makeBinding(9002), makeAssignment(2));
    const auto new_epoch = dir.find(1001)->anchor_epoch;
    TEST_ASSERT(new_epoch == 2, "顶号后新 epoch");
    TEST_ASSERT(!dir.resume(1001, 9002, 1), "旧 epoch resume 拒绝");
    TEST_ASSERT(dir.mark_suspended(1001, 200, 50) && dir.resume(1001, 9002, new_epoch),
                "新 epoch resume 成功");
    return true;
}

// ---- §2：对账 + 快照重置 ----
bool test_reconcile_and_snapshot_reset() {
    std::cout << "Running: test_reconcile_and_snapshot_reset..." << std::endl;

    PlayerDirectory dir;
    EventLog log;
    dir.set_event_sink([&log](const PlayerDirectory::Event& e) { log.record(e); });

    dir.session_up(1001, makeBinding(9001), makeAssignment(1));
    dir.session_up(1002, makeBinding(9002), makeAssignment(1));
    dir.session_up(1003, makeBinding(9003), makeAssignment(1));
    dir.mark_suspended(1003, 0, 50);
    log.clear();

    TEST_ASSERT(dir.reconcile({1001, 1002}), "Online 集比对一致（挂机不算在线）");
    TEST_ASSERT(dir.reconcile({1002, 1001}), "顺序无关");
    TEST_ASSERT(!dir.reconcile({1001}), "目录多出 → 失配");
    TEST_ASSERT(!dir.reconcile({1001, 1002, 424242}), "上报多出 → 失配");

    // 快照重置：目录多出的静默删除（无事件），返回删除数。挂机条目同样按
    // 快照对表（424242 在目录无条目 → 上报有而目录无，重建留 P3，不误删）
    TEST_ASSERT(dir.snapshot_reset({1001, 1002, 424242}) == 1,
                "挂机条目 1003 不在权威集 → 删除 1");
    TEST_ASSERT(dir.size() == 2 && dir.find(1003) == nullptr && dir.find(424242) == nullptr,
                "多余条目删除、上报独有条目不凭空重建");
    TEST_ASSERT(log.rows.empty(), "快照重置不产事件（§2 与事件层互不兜底）");

    TEST_ASSERT(dir.snapshot_reset({1001, 1002}) == 0, "全集匹配删除 0");
    TEST_ASSERT(dir.snapshot_reset({1001}) == 1, "目录多出 1 条静默删除");
    TEST_ASSERT(dir.find(1001) != nullptr && dir.find(1002) == nullptr,
                "仅权威集内条目保留");
    TEST_ASSERT(dir.reconcile({1001}), "重置后对账一致");
    TEST_ASSERT(log.rows.empty(), "静默删除全程无事件");
    return true;
}

// ---- §1 状态机：Leaving → 显式下线 ----
bool test_begin_leave_lifecycle() {
    std::cout << "Running: test_begin_leave_lifecycle..." << std::endl;

    PlayerDirectory dir;
    dir.session_up(1001, makeBinding(9001), makeAssignment(1));

    TEST_ASSERT(!dir.begin_leave(424242), "未知玩家 begin_leave false");
    TEST_ASSERT(dir.begin_leave(1001), "Online → Leaving");
    TEST_ASSERT(!dir.begin_leave(1001), "Leaving 不重复迁移");
    TEST_ASSERT(!dir.mark_suspended(1001, 0, 50), "Leaving 不回窗口");
    TEST_ASSERT(dir.find(1001)->state == PlayerDirectory::EntryState::Leaving,
                "状态列=Leaving");
    TEST_ASSERT(dir.online_ids().empty(), "Leaving 不计在线");
    TEST_ASSERT(dir.session_down(1001), "Leaving → 显式下线终结");
    TEST_ASSERT(dir.find(1001) == nullptr, "条目删除");
    return true;
}

} // namespace

int main() {
    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_session_up_registers_entry);
    run(test_relogin_kicks_old_session);
    run(test_session_down_removes_entry);
    run(test_suspend_window_and_sweep);
    run(test_resume_dual_anchor);
    run(test_reconcile_and_snapshot_reset);
    run(test_begin_leave_lifecycle);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
