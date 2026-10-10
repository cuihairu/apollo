// Recovery 测试（P1-5；lifecycle §3 矩阵 + 任务书 §20）。
//
// 覆盖：
//   1. 三档清单：六恢复对象逐项分级（必须持久化/可重算/可丢）；
//   2. RecoveryCoordinator 启动序列 restore→admission→ready（失败路径）；
//   3. restore_anchors：锚点重建置 Offline（档案在、会话无）；
//   4. 端到端：journal write-ahead → 「崩溃」→ coordinator replay → 档案
//      恢复可读（恢复位点语义）。

#include "apollo/game/session/anchor_manager.hpp"
#include "apollo/game/session/recovery.hpp"

#include "apollo/data/orm/persist_journal.hpp"
#include "base/database_service.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

namespace fs = std::filesystem;

using apollo::game::session::RecoveryPhase;
using apollo::game::session::RecoveryTier;
using apollo::game::session::RecoveryCoordinator;
using apollo::game::session::recovery_manifest;
using apollo::game::session::restore_anchors;

bool test_manifest_three_tiers() {
    const auto& manifest = recovery_manifest();
    TEST_ASSERT(manifest.size() == 6, "六恢复对象（lifecycle §3）");

    std::size_t must_persist = 0;
    std::size_t recomputable = 0;
    std::size_t lossy = 0;
    for (const auto& item : manifest) {
        switch (item.tier) {
            case RecoveryTier::MustPersist:
                ++must_persist;
                break;
            case RecoveryTier::Recomputable:
                ++recomputable;
                break;
            case RecoveryTier::Lossy:
                ++lossy;
                break;
        }
    }
    TEST_ASSERT(must_persist == 2, "必须持久化两档（玩家长期态+instance 结算）");
    TEST_ASSERT(recomputable == 3, "可重算三档（scene/会话/拓扑）");
    TEST_ASSERT(lossy == 1, "可丢一档（战斗中间态）");

    // 关键行核对
    TEST_ASSERT(std::string(manifest[0].object) == "player_anchored_state" &&
                    manifest[0].tier == RecoveryTier::MustPersist,
                "玩家长期态必须持久化");
    TEST_ASSERT(std::string(manifest[1].object) == "scene_runtime_state" &&
                    manifest[1].tier == RecoveryTier::Recomputable,
                "场景运行时态可重算（重开 scene）");
    TEST_ASSERT(std::string(manifest[3].object) == "session_connection" &&
                    manifest[3].tier == RecoveryTier::Recomputable,
                "会话窗口内可重建（P1-6 resume 承接）");
    return true;
}

bool test_coordinator_happy_path() {
    std::size_t replayed = 0;
    bool admitted = false;
    RecoveryCoordinator coordinator(
        "test",
        [&replayed]() {
            replayed = 3;
            return replayed;
        },
        [&admitted]() {
            admitted = true;
            return true;
        });

    TEST_ASSERT(coordinator.phase() == RecoveryPhase::Init && !coordinator.ready(),
                "初始 Init 未就绪");
    TEST_ASSERT(coordinator.run(), "完整序列成功");
    TEST_ASSERT(coordinator.phase() == RecoveryPhase::Ready && coordinator.ready(),
                "到达 Ready");
    TEST_ASSERT(admitted, "准入检查执行");
    TEST_ASSERT(coordinator.replayed() == 3, "回放计数透传");
    return true;
}

bool test_coordinator_failure_paths() {
    // restore 失败 → Failed，拒绝服务
    RecoveryCoordinator restore_fail(
        "test-restore", []() -> std::size_t { throw std::runtime_error("corrupt"); },
        []() { return true; });
    TEST_ASSERT(!restore_fail.run(), "restore 失败序列中止");
    TEST_ASSERT(restore_fail.phase() == RecoveryPhase::Failed, "落 Failed");
    TEST_ASSERT(!restore_fail.ready(), "未就绪");

    // admission 拒绝 → Failed
    RecoveryCoordinator admit_fail("test-admit", []() -> std::size_t { return 0; },
                                   []() { return false; });
    TEST_ASSERT(!admit_fail.run(), "admission 拒绝序列中止");
    TEST_ASSERT(admit_fail.phase() == RecoveryPhase::Failed, "落 Failed");

    // 失败后不可复跑（状态机单向）
    TEST_ASSERT(!restore_fail.restore(), "Failed 后 restore 拒绝");
    return true;
}

bool test_restore_anchors_offline() {
    apollo::game::session::AnchorManager manager;
    const auto restored = restore_anchors(manager, {1, 2, 3});
    TEST_ASSERT(restored == 3, "三锚点恢复");
    TEST_ASSERT(manager.anchor_count() == 3, "锚点在册");
    for (const auto id : {1, 2, 3}) {
        const auto anchor = manager.find(id);
        TEST_ASSERT(anchor != nullptr, "锚点可查");
        TEST_ASSERT(anchor->state() == apollo::game::session::AnchorState::Offline,
                    "恢复态 Offline（档案在、会话无）");
    }
    return true;
}

bool test_crash_replay_recovers_profile() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() /
                      ("apollo_recovery_test_" + std::to_string(now));
    const auto data_dir = root / "players";
    fs::create_directories(data_dir);
    const auto journal_path = (root / "persist.log").string();

    base::BaseConfig config;
    config.dataDir = data_dir.string();

    // 进程一：档案尚未落盘（保存只走了 write-ahead），「崩溃」
    {
        base::DatabaseService db(config);
        TEST_ASSERT(db.initialize(), "db init");
        apollo::data::journal::PersistJournal journal(journal_path);
        TEST_ASSERT(journal.open(), "journal open");

        base::PlayerData pending;
        pending.playerId = 77;
        pending.username = "dave";
        pending.level = 15;
        pending.exp = 555;
        TEST_ASSERT(journal.append("77", pending.toJson(), 12345), "write-ahead 落 journal");
    }

    // 进程二：启动序列 restore→admission→ready，replay 把位点压成档案
    {
        base::DatabaseService db(config);
        TEST_ASSERT(db.initialize(), "db init");
        apollo::data::journal::PersistJournal journal(journal_path);
        journal.open();

        RecoveryCoordinator coordinator(
            "zone-app",
            [&]() -> std::size_t {
                return journal.replay([&](const apollo::data::journal::JournalEntry& e) {
                    base::PlayerData data = base::PlayerData::fromJson(e.payload);
                    data.playerId = static_cast<base::PlayerID>(std::stoull(e.key));
                    return db.savePlayer(data);
                });
            },
            [&]() { return true; });

        TEST_ASSERT(coordinator.run() && coordinator.ready(), "恢复序列完成");
        TEST_ASSERT(coordinator.replayed() == 1, "回放一条位点");

        base::PlayerData loaded;
        TEST_ASSERT(db.loadPlayer(77, loaded), "崩溃前未落盘的档案经 replay 恢复");
        TEST_ASSERT(loaded.username == "dave" && loaded.level == 15 && loaded.exp == 555,
                    "恢复内容一致");
    }

    std::error_code ec;
    fs::remove_all(root, ec);
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
        {"recovery_manifest_three_tiers", test_manifest_three_tiers},
        {"recovery_coordinator_happy_path", test_coordinator_happy_path},
        {"recovery_coordinator_failure_paths", test_coordinator_failure_paths},
        {"recovery_restore_anchors_offline", test_restore_anchors_offline},
        {"recovery_crash_replay_recovers_profile", test_crash_replay_recovers_profile},
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

    std::cout << "RecoveryTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
