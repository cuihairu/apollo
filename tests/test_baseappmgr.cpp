#include "baseappmgr/baseappmgr.hpp"
#include "apollo/game/session/world_assignment.hpp"
#include "apollo/game/session/directory_mirror.hpp"
#include "base/base_server.hpp"
#include "base/migration_wire.hpp"
#include "apollo/net/link/tcp_transport.hpp"

#include <filesystem>
#include <iostream>

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

// baseappmgr（调度面）目录裁决与路由解析测试。
// 纯内存目录操作，不启动网络。
bool test_baseappmgr_directory_and_route() {
    std::cout << "Running: test_baseappmgr_directory_and_route..." << std::endl;

    baseappmgr::BaseAppMgr mgr(0); // 不 start()，只测目录方法

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9001;
    binding.gateway_id = 7;
    binding.gateway_addr = "gateway://127.0.0.1:8888";

    TEST_ASSERT(mgr.bindSession(1001, binding), "session bound in directory");

    const auto playerBySession = mgr.findPlayerBySession(9001);
    TEST_ASSERT(playerBySession.has_value(), "player resolved by session");
    TEST_ASSERT(*playerBySession == 1001, "resolved player id matches");

    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = 3;
    assignment.map_id = 100;
    assignment.instance_id = 200;
    assignment.space_id = 300;
    assignment.route_version = 1;

    TEST_ASSERT(mgr.assignWorld(1001, assignment), "world assignment applied");
    TEST_ASSERT(!mgr.assignWorld(0, assignment), "zero player id rejected");

    const auto resolvedAssignment = mgr.resolveWorldAssignment(1001);
    TEST_ASSERT(resolvedAssignment.has_value(), "world assignment resolved");
    TEST_ASSERT(resolvedAssignment->route_version == 1, "route version resolved");
    TEST_ASSERT(resolvedAssignment->instance_id == 200, "instance id resolved");

    // 按 session 反查（playerId=0 时走目录解析）
    const auto resolvedBySession = mgr.resolveWorldAssignment(0, 9001);
    TEST_ASSERT(resolvedBySession.has_value(), "assignment resolved via session");
    TEST_ASSERT(resolvedBySession->world_id == 3, "world id resolved via session");

    const auto resolvedBinding = mgr.resolveSessionBinding(1001, 9001);
    TEST_ASSERT(resolvedBinding.has_value(), "session binding resolved");
    TEST_ASSERT(resolvedBinding->gateway_addr == "gateway://127.0.0.1:8888", "gateway addr resolved");

    TEST_ASSERT(!mgr.resolveWorldAssignment(9999).has_value(), "unknown player has no assignment");

    TEST_ASSERT(mgr.clearWorldAssignment(1001), "world assignment cleared");
    TEST_ASSERT(!mgr.clearWorldAssignment(1001), "double clear returns false");
    TEST_ASSERT(!mgr.resolveWorldAssignment(1001).has_value(), "assignment removed");

    TEST_ASSERT(mgr.unbindSession(9001), "session unbound");
    TEST_ASSERT(!mgr.findPlayerBySession(9001).has_value(), "session locator cleaned");
    TEST_ASSERT(!mgr.resolveSessionBinding(1001).has_value(), "binding removed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// 准入闸门（P3-1 增量③：恢复相位排他——gate 关闭期 bindSession/assignWorld
// 拒新，开放后恢复；未设闸门恒放行）。
bool test_baseappmgr_admission_gate() {
    std::cout << "Running: test_baseappmgr_admission_gate..." << std::endl;

    baseappmgr::BaseAppMgr mgr(0);

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9101;
    binding.gateway_id = 7;
    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = 3;

    // 无闸门：恒放行
    TEST_ASSERT(mgr.bindSession(2001, binding), "no gate: bind admitted");
    TEST_ASSERT(mgr.assignWorld(2001, assignment), "no gate: assign admitted");
    TEST_ASSERT(mgr.unbindSession(9101), "cleanup bind");

    bool open = false;
    mgr.set_admission_gate([&open]() { return open; });

    // 恢复相位排他：拒新
    TEST_ASSERT(!mgr.bindSession(2002, binding), "closed gate: bind rejected");
    TEST_ASSERT(!mgr.assignWorld(2002, assignment), "closed gate: assign rejected");
    TEST_ASSERT(!mgr.findPlayerBySession(9101).has_value(),
                "rejected bind left no locator entry");

    // 收敛开放：恢复放行
    open = true;
    TEST_ASSERT(mgr.bindSession(2002, binding), "open gate: bind admitted");
    TEST_ASSERT(mgr.assignWorld(2002, assignment), "open gate: assign admitted");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// 更新排空闸（backup-revive §8 路1 缺口接线：排空指令 → WorldAssignment/
// 落点指派跳过更新窗内的进程）：drain 置位期 bindSession/assignWorld 拒新，
// 关闭后恢复；与恢复相位准入闸共用闸门语义（§9.6），排空不挡查询。
bool test_baseappmgr_update_drain() {
    std::cout << "Running: test_baseappmgr_update_drain..." << std::endl;

    baseappmgr::BaseAppMgr mgr(0);

    apollo::game::session::SessionBinding binding;
    binding.session_id = 9201;
    binding.gateway_id = 7;
    apollo::game::session::WorldAssignment assignment;
    assignment.world_id = 3;

    // 排空前：恒放行
    TEST_ASSERT(!mgr.drain_active(), "drain flag starts off");
    TEST_ASSERT(mgr.bindSession(3001, binding), "before drain: bind admitted");
    TEST_ASSERT(mgr.assignWorld(3001, assignment), "before drain: assign admitted");

    // 排空置位：拒新落点/新绑定（更新窗内编队不接新玩家）
    mgr.set_drain(true);
    TEST_ASSERT(mgr.drain_active(), "drain flag on");
    binding.session_id = 9202;
    TEST_ASSERT(!mgr.bindSession(3002, binding), "drain on: bind rejected");
    TEST_ASSERT(!mgr.assignWorld(3002, assignment), "drain on: assign rejected");
    TEST_ASSERT(!mgr.findPlayerBySession(9202).has_value(),
                "rejected bind left no locator entry");

    // 排空关闭：恢复放行（已有条目不受排空影响）
    mgr.set_drain(false);
    TEST_ASSERT(!mgr.drain_active(), "drain flag off");
    TEST_ASSERT(mgr.bindSession(3002, binding), "drain off: bind admitted");
    TEST_ASSERT(mgr.assignWorld(3002, assignment), "drain off: assign admitted");
    TEST_ASSERT(mgr.findPlayerBySession(9201).has_value(),
                "existing session survives drain window");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// §6 死亡行窗口处置透传（G-1 收尾批遗留项）：进程壳 → 目录批量反查 +
// 窗口满扫描。供数走 intake（restore-not-kick 面，增量③）。
bool test_baseappmgr_window_disposition_passthrough() {
    std::cout << "Running: test_baseappmgr_window_disposition_passthrough..."
              << std::endl;

    baseappmgr::BaseAppMgr mgr(0);

    namespace session = apollo::game::session;
    std::vector<session::MirrorEntry> report;
    for (const std::uint64_t pid : {3001ULL, 3002ULL, 3003ULL}) {
        session::MirrorEntry e;
        e.player_id = pid;
        e.session_id = 9000 + pid;
        e.gateway_id = 7;
        e.zone_id = 2;
        e.state = session::PlayerDirectory::EntryState::Online;
        report.push_back(e);
    }
    {
        session::MirrorEntry e;
        e.player_id = 3004;
        e.session_id = 9004;
        e.gateway_id = 9;
        e.zone_id = 3;
        e.state = session::PlayerDirectory::EntryState::Online;
        report.push_back(e);
    }
    TEST_ASSERT(mgr.intake_directory_full_report(report) == 4, "intake 4 条");
    TEST_ASSERT(mgr.directory().size() == 4, "目录 4 条目");

    // Zone 行反查：zone2 → 3 条进窗口；反查键 0 拒绝
    TEST_ASSERT(mgr.suspend_zone_sessions(2, 100, 50) == 3, "zone2 挂起 3 条");
    TEST_ASSERT(mgr.suspend_zone_sessions(0, 100, 50) == 0, "zone=0 不处置");
    TEST_ASSERT(mgr.directory().find(3001)->state ==
                    session::PlayerDirectory::EntryState::Suspended,
                "3001 挂起");
    TEST_ASSERT(mgr.directory().find(3004)->state ==
                    session::PlayerDirectory::EntryState::Online,
                "zone3 条目不受牵连");

    // gateway 行反查：gw9 → 3004 进窗口
    TEST_ASSERT(mgr.suspend_gateway_sessions(9, 100, 50) == 1, "gw9 挂起 1 条");
    TEST_ASSERT(mgr.suspend_gateway_sessions(0, 100, 50) == 0, "gateway=0 不处置");

    // 窗口满扫描：未到期无终结，到期 4 条全终结（resume 面 1001 例已在
    // player_directory 契约单测覆盖）
    TEST_ASSERT(mgr.sweep_suspended(149) == 0, "窗口内 sweep 无终结");
    TEST_ASSERT(mgr.sweep_suspended(150) == 4, "到期 sweep 终结 4 条");
    TEST_ASSERT(mgr.directory().size() == 0, "目录清空");
    TEST_ASSERT(mgr.sweep_suspended(151) == 0, "空目录 sweep 幂等");

    std::cout << "  PASSED" << std::endl;
    return true;
}

// ---- 玩家迁移编排真链路（P0-2 路 2 ①③④；两 zone 真 M1 面 + mgr 编排）。
// Windows 桩树 M1 面缺席——护栏跳过（base_anchor 真链路组同款）。
#if !defined(_WIN32)
bool test_baseappmgr_migrate_player() {
    std::cout << "Running: test_baseappmgr_migrate_player..." << std::endl;
    namespace migration = base::migration;
    namespace session = apollo::game::session;

    // 隔离数据目录护栏
    struct Dir {
        std::string path;
        explicit Dir(std::string p) : path(std::move(p)) {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            std::filesystem::create_directories(path);
        }
        ~Dir() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } oldDir("baseappmgr_mig_old_data"), newDir("baseappmgr_mig_new_data");

    // 空闲端口探针（同存活取双端口，拿完即关）
    std::uint16_t srcM1 = 0, dstM1 = 0;
    {
        apollo::net::link::TcpListener a, b;
        TEST_ASSERT(a.start("127.0.0.1", 0) && b.start("127.0.0.1", 0),
                    "port probes bind");
        srcM1 = a.port();
        dstM1 = b.port();
    }

    auto zoneCfg = [](const std::string& dir, std::uint16_t m1port) {
        base::BaseConfig cfg;
        cfg.workerThreads = 1;
        cfg.autoSaveIntervalMs = 10;
        cfg.host = "127.0.0.1";
        cfg.port = 39107;  // 旧 REP 面已退役，占位端口
        cfg.dataDir = dir + "/players";
        cfg.journalPath = dir + "/journal/persist.log";
        cfg.m1Port = m1port;
        cfg.m1RouteJson = APOLLO_BASEAPP_CONTRACT_ROUTE_JSON;
        std::filesystem::create_directories(cfg.dataDir);
        std::filesystem::create_directories(dir + "/journal");
        return cfg;
    };

    // 老 zone：播种 + 激活（脏锚——① freeze 排段后 ② 快照才 flush 到位）
    base::BaseConfig oldCfg = zoneCfg(oldDir.path, srcM1);
    {
        base::PlayerData seed;
        seed.playerId = 42;
        seed.username = "orchestrated";
        seed.level = 6;
        base::DatabaseService seeder(oldCfg);
        TEST_ASSERT(seeder.savePlayer(seed), "old zone seed written");
    }
    base::BaseServer oldZone(oldCfg);
    oldZone.start();
    TEST_ASSERT(oldZone.m1_listen_port() == srcM1, "old zone m1 face up");
    {
        auto anchor = oldZone.activatePlayer(42);
        TEST_ASSERT(anchor != nullptr, "old zone anchor activated");
        session::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(oldZone.bindSession(42, binding), "old zone session bound");
        oldZone.findAnchor(42)->set_home_zone_id(2);
        oldZone.findAnchor(42)->mark_dirty("migrate_drill");
    }

    // 新 zone：空载起（承接 937 + 现场拉 935/936）
    base::BaseConfig newCfg = zoneCfg(newDir.path, dstM1);
    base::BaseServer newZone(newCfg);
    newZone.start();
    TEST_ASSERT(newZone.m1_listen_port() == dstM1, "new zone m1 face up");

    // mgr：条目（epoch=1, zone=0）+ 快照发布捕获桩（拨出面用域内同一份
    // 契约路由表——set_m1_face 存的 route json）
    baseappmgr::BaseAppMgr mgr(0);
    mgr.set_m1_face(0, APOLLO_BASEAPP_CONTRACT_ROUTE_JSON);
    std::atomic<int> snapshotCalls{0};
    auto publisher = std::make_shared<session::DirectoryPublisher>(
        [&snapshotCalls](const std::uint8_t*, std::size_t) {
            ++snapshotCalls;
            return true;
        });
    mgr.set_snapshot_publisher(publisher);
    {
        session::SessionBinding binding;
        binding.session_id = 777;
        binding.gateway_id = 3;
        TEST_ASSERT(mgr.bindSession(42, binding), "mgr entry registered");
        session::WorldAssignment assignment;
        assignment.world_id = 1;
        assignment.route_version = 1;
        TEST_ASSERT(mgr.assignWorld(42, assignment), "mgr assignment set");
    }
    TEST_ASSERT(mgr.directory().find(42)->anchor_epoch == 1, "entry epoch 1");

    // 编排：① 933 → ③ 937（新 zone 现场拉）→ ④ 改指 + 立即推
    const auto res = mgr.migratePlayer(42, "127.0.0.1", srcM1,
                                       "127.0.0.1", dstM1, 5);
    TEST_ASSERT(res.ok, std::string("migration ok: ") + res.error_message);
    TEST_ASSERT(res.new_epoch == 2, "epoch bumped old+1");
    TEST_ASSERT(res.freeze_ms >= 0 && res.handoff_ms >= res.freeze_ms
                    && res.total_ms >= res.handoff_ms,
                "step timings monotonic");
    TEST_ASSERT(snapshotCalls.load() >= 1, "snapshot pushed immediately (§9.2-4)");

    // 老侧：Transferring 停留（⑤ 老进程摘除随排空退场——批记边界）
    {
        auto anchor = oldZone.findAnchor(42);
        TEST_ASSERT(anchor != nullptr
                        && anchor->state() == session::AnchorState::Transferring,
                    "old side stays Transferring");
        TEST_ASSERT(!anchor->needs_save(), "old side journal drained");
    }
    // 新侧：接权生效（Online + binding + home zone + 档案落盘）
    {
        auto anchor = newZone.findAnchor(42);
        TEST_ASSERT(anchor != nullptr
                        && anchor->state() == session::AnchorState::Online,
                    "new side anchor online");
        TEST_ASSERT(anchor->session_binding().session_id == 777
                        && anchor->home_zone_id() == 2,
                    "binding and home zone restored");
        base::PlayerData migrated;
        base::DatabaseService reader(newCfg);
        TEST_ASSERT(reader.loadPlayer(42, migrated) && migrated.level == 6,
                    "flushed profile landed");
    }
    // 目录：改指 + epoch 键推进（旧 epoch 回执拒已在 test_moved_epoch_bump
    // 目录单测覆盖——mgr 无 resume 透传，双锚裁决在 zone 侧）
    {
        const auto* entry = mgr.directory().find(42);
        TEST_ASSERT(entry != nullptr && entry->zone_id == 5
                        && entry->anchor_epoch == 2,
                    "directory rehomed with new epoch");
    }
    // 前置拒绝：未知玩家 / zone=0
    TEST_ASSERT(!mgr.migratePlayer(999, "127.0.0.1", srcM1,
                                   "127.0.0.1", dstM1, 5).ok,
                "unknown player rejected");
    TEST_ASSERT(!mgr.migratePlayer(42, "127.0.0.1", srcM1,
                                   "127.0.0.1", dstM1, 0).ok,
                "target zone 0 rejected");

    oldZone.stop();
    newZone.stop();
    std::cout << "  PASSED" << std::endl;
    return true;
}
#endif // !_WIN32

} // namespace

int main() {
    std::cout << "=== Apollo BaseAppMgr Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    run(test_baseappmgr_directory_and_route);
    run(test_baseappmgr_admission_gate);
    run(test_baseappmgr_update_drain);
    run(test_baseappmgr_window_disposition_passthrough);
#if !defined(_WIN32)
    run(test_baseappmgr_migrate_player);
#endif

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return total == passed ? 0 : 1;
}
