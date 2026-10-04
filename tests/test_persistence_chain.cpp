// 持久化最小真链路测试（P1-4；P1 出口判据④「写盘→重启→读回」）。
//
// 直编源惯例（同 player_directory_tests）：不依赖 app 目标，三平台 Unit
// 门禁真实执行。覆盖：
//   1. createPlayer 落盘 → 新 DatabaseService（模拟重启）loadPlayer 读回一致；
//   2. loadPlayer 未命中返回 false——不再凭空 bootstrap「player_+id」；
//   3. toJson/fromJson 全字段对称（含 playerId/exp int64/x/y/z 浮点）；
//   4. Anchor mark_dirty → SaveQueue worker 落盘 → 读回（Anchor dirty 链路）；
//   5. worker 失败 → callback(false) 真结果上报。

#include "apollo/game/session/player_anchor.hpp"
#include "base/database_service.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
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

std::string make_tmp_dir(const char* tag) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = fs::temp_directory_path() /
               ("apollo_chain_test_" + std::string(tag) + "_" + std::to_string(now));
    fs::create_directories(dir);
    return dir.string();
}

base::BaseConfig make_config(const std::string& dir) {
    base::BaseConfig config;
    config.dataDir = dir;
    return config;
}

bool test_write_reload_roundtrip() {
    const auto dir = make_tmp_dir("roundtrip");
    base::PlayerData created;
    created.playerId = 1001;
    created.username = "alice";
    created.level = 42;
    created.exp = 9876543210LL;  // 超过 int32 的 int64 值
    created.hp = 88;
    created.maxHp = 120;
    created.mp = 30;
    created.maxMp = 75;
    created.x = 12.5f;
    created.y = -3.25f;
    created.z = 40.125f;

    {
        base::DatabaseService db(make_config(dir));
        TEST_ASSERT(db.initialize(), "initialize 建目录");
        TEST_ASSERT(db.createPlayer(created), "createPlayer 落盘");
    }  // 析构 = 「重启」

    base::DatabaseService revived(make_config(dir));
    TEST_ASSERT(revived.initialize(), "重启后 initialize");
    base::PlayerData loaded;
    TEST_ASSERT(revived.loadPlayer(1001, loaded), "重启后读回");
    TEST_ASSERT(loaded.username == "alice" && loaded.level == 42, "字段一致");
    TEST_ASSERT(loaded.exp == 9876543210LL, "int64 exp 无截断");
    TEST_ASSERT(loaded.x == 12.5f && loaded.y == -3.25f && loaded.z == 40.125f,
                "位置浮点对称读回");

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

bool test_load_miss_no_bootstrap() {
    const auto dir = make_tmp_dir("nobootstrap");
    base::DatabaseService db(make_config(dir));
    TEST_ASSERT(db.initialize(), "initialize");

    base::PlayerData out;
    TEST_ASSERT(!db.loadPlayer(424242, out), "未建号 load 失败");
    TEST_ASSERT(out.username != "player_424242", "不从空气长档案（bootstrap 恶龙已废）");

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

bool test_json_symmetry() {
    base::PlayerData in;
    in.playerId = 7;
    in.username = "bob";
    in.level = 9;
    in.exp = 1234567890123LL;
    in.hp = 1;
    in.maxHp = 2;
    in.mp = 3;
    in.maxMp = 4;
    in.x = 0.5f;
    in.y = 100.0f;
    in.z = -7.75f;

    const auto out = base::PlayerData::fromJson(in.toJson());
    TEST_ASSERT(out.playerId == 7, "playerId 对称");
    TEST_ASSERT(out.username == "bob", "username 对称");
    TEST_ASSERT(out.level == 9 && out.hp == 1 && out.maxHp == 2 && out.mp == 3 &&
                    out.maxMp == 4,
                "整数字段对称");
    TEST_ASSERT(out.exp == 1234567890123LL, "exp int64 对称");
    TEST_ASSERT(out.x == 0.5f && out.y == 100.0f && out.z == -7.75f, "x/y/z 对称");
    return true;
}

bool test_anchor_dirty_save_chain() {
    const auto dir = make_tmp_dir("chain");
    const std::string cfg_dir = dir;
    base::DatabaseService db(make_config(dir));
    TEST_ASSERT(db.initialize(), "initialize");

    // 建号 + 现档
    base::PlayerData profile;
    profile.playerId = 55;
    profile.username = "carol";
    profile.level = 7;
    TEST_ASSERT(db.createPlayer(profile), "建号");

    // Anchor dirty → SaveQueue 落盘（flushDirtyAnchors 同形链路）
    apollo::game::session::PlayerAnchor anchor(55);
    anchor.mark_dirty("test_flush");
    TEST_ASSERT(anchor.needs_save(), "dirty 可观察");

    base::SaveQueue queue(1);
    std::atomic<bool> called{false};
    std::atomic<bool> ok{false};
    base::SaveTask task;
    task.playerId = 55;
    TEST_ASSERT(db.loadPlayer(55, task.data), "以现档为基线载荷");
    task.callback = [&](bool success) {
        ok = success;
        called = true;
        anchor.clear_dirty();
    };
    queue.set_worker([&db](const base::PlayerData& data) { return db.savePlayer(data); });
    queue.start();
    queue.enqueue(task);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!called.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    queue.stop();

    TEST_ASSERT(called.load(), "回调到达");
    TEST_ASSERT(ok.load(), "落盘真结果");
    TEST_ASSERT(!anchor.needs_save(), "回调里 clear_dirty");

    // 重启读回验证链路末端
    base::DatabaseService revived(make_config(cfg_dir));
    revived.initialize();
    base::PlayerData loaded;
    TEST_ASSERT(revived.loadPlayer(55, loaded) && loaded.level == 7, "链路末端读回");

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

bool test_worker_failure_reports_false() {
    const auto dir = make_tmp_dir("fail");
    base::DatabaseService db(make_config(dir));
    db.initialize();

    base::SaveQueue queue(1);
    queue.set_worker([](const base::PlayerData&) { return false; });

    std::atomic<bool> called{false};
    std::atomic<bool> ok{true};
    base::SaveTask task;
    task.playerId = 1;
    task.callback = [&](bool success) {
        ok = success;
        called = true;
    };
    queue.start();
    queue.enqueue(task);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!called.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    queue.stop();

    TEST_ASSERT(called.load() && !ok.load(), "失败经 callback(false) 上报");

    std::error_code ec;
    fs::remove_all(dir, ec);
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
        {"persistence_write_reload_roundtrip", test_write_reload_roundtrip},
        {"persistence_load_miss_no_bootstrap", test_load_miss_no_bootstrap},
        {"persistence_json_symmetry", test_json_symmetry},
        {"persistence_anchor_dirty_save_chain", test_anchor_dirty_save_chain},
        {"persistence_worker_failure_reports_false", test_worker_failure_reports_false},
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

    std::cout << "PersistenceChainTests: " << passed << " passed, " << failed << " failed"
              << std::endl;
    return failed == 0 ? 0 : 1;
}
