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
#include "player_store/database_service.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
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

player_store::PlayerStoreConfig make_config(const std::string& dir) {
    player_store::PlayerStoreConfig config;
    config.dataDir = dir;
    return config;
}

bool test_write_reload_roundtrip() {
    const auto dir = make_tmp_dir("roundtrip");
    player_store::PlayerData created;
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
        player_store::DatabaseService db(make_config(dir));
        TEST_ASSERT(db.initialize(), "initialize 建目录");
        TEST_ASSERT(db.createPlayer(created), "createPlayer 落盘");
    }  // 析构 = 「重启」

    player_store::DatabaseService revived(make_config(dir));
    TEST_ASSERT(revived.initialize(), "重启后 initialize");
    player_store::PlayerData loaded;
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
    player_store::DatabaseService db(make_config(dir));
    TEST_ASSERT(db.initialize(), "initialize");

    player_store::PlayerData out;
    TEST_ASSERT(!db.loadPlayer(424242, out), "未建号 load 失败");
    TEST_ASSERT(out.username != "player_424242", "不从空气长档案（bootstrap 恶龙已废）");

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

bool test_json_symmetry() {
    player_store::PlayerData in;
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

    const auto out = player_store::PlayerData::fromJson(in.toJson());
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
    player_store::DatabaseService db(make_config(dir));
    TEST_ASSERT(db.initialize(), "initialize");

    // 建号 + 现档
    player_store::PlayerData profile;
    profile.playerId = 55;
    profile.username = "carol";
    profile.level = 7;
    TEST_ASSERT(db.createPlayer(profile), "建号");

    // Anchor dirty → SaveQueue 落盘（flushDirtyAnchors 同形链路）
    apollo::game::session::PlayerAnchor anchor(55);
    anchor.mark_dirty("test_flush");
    TEST_ASSERT(anchor.needs_save(), "dirty 可观察");

    player_store::SaveQueue queue(1);
    std::atomic<bool> called{false};
    std::atomic<bool> ok{false};
    player_store::SaveTask task;
    task.playerId = 55;
    TEST_ASSERT(db.loadPlayer(55, task.data), "以现档为基线载荷");
    task.callback = [&](bool success) {
        ok = success;
        called = true;
        anchor.clear_dirty();
    };
    queue.set_worker([&db](const player_store::PlayerData& data) { return db.savePlayer(data); });
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
    player_store::DatabaseService revived(make_config(cfg_dir));
    revived.initialize();
    player_store::PlayerData loaded;
    TEST_ASSERT(revived.loadPlayer(55, loaded) && loaded.level == 7, "链路末端读回");

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

// 同玩家任务串行 + 合并（PlayerStore 存档串行化批）：SaveQueue 按玩家
// 分槽——同玩家落盘序恒等于入队序（旧快照不得晚于新数据落盘），跨玩家
// 任务并行不破坏该序；在途期间同玩家新任务被拒（合并跳过）。
bool test_savequeue_same_player_serial_and_coalesce() {
    // 相位一：4 worker + 慢落盘——同玩家 20 任务与其它玩家任务交错，
    // 同玩家子序必须严格递增
    {
        std::mutex orderMutex;
        std::vector<std::int64_t> player77Order;
        std::atomic<int> executed{0};
        player_store::SaveQueue queue(4);
        queue.set_worker([&](const player_store::PlayerData& data) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 交错窗
            std::lock_guard<std::mutex> lock(orderMutex);
            if (data.playerId == 77) {
                player77Order.push_back(data.exp);
            }
            executed.fetch_add(1);
            return true;
        });
        queue.start();

        for (int i = 1; i <= 20; ++i) {
            player_store::SaveTask task;
            task.playerId = 77;
            task.data.playerId = 77;
            task.data.exp = i;  // 载荷新旧标记
            queue.enqueue(task);
            // 交叉塞入其它玩家（同槽不同槽皆可能）验证跨玩家不破坏同玩家序
            player_store::SaveTask other;
            other.playerId = 100 + (i % 4);
            other.data.playerId = other.playerId;
            other.data.exp = i;
            queue.enqueue(other);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        queue.stop();  // 排干后退出

        TEST_ASSERT(executed.load() >= 20, "任务被执行");
        TEST_ASSERT(player77Order.size() >= 2, "同玩家样本足够");
        for (std::size_t i = 1; i < player77Order.size(); ++i) {
            TEST_ASSERT(player77Order[i] > player77Order[i - 1],
                        "同玩家落盘序 == 入队序（无旧覆盖新）");
        }
    }

    // 相位二：确定性合并——首个任务门控在 worker 内（promise 闸），
    // 在途期间同玩家第二任务必被拒、异玩家不受影响
    {
        std::promise<void> release;
        auto gate = release.get_future();
        std::atomic<bool> firstStarted{false};
        player_store::SaveQueue queue(2);
        queue.set_worker([&](const player_store::PlayerData&) {
            if (!firstStarted.exchange(true)) {
                gate.wait();  // 首个任务持住 worker
            }
            return true;
        });
        queue.start();

        player_store::SaveTask held;
        held.playerId = 7;
        held.data.playerId = 7;
        TEST_ASSERT(queue.enqueue(held), "首个任务受理");
        while (!firstStarted.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        player_store::SaveTask dup;
        dup.playerId = 7;
        dup.data.playerId = 7;
        TEST_ASSERT(!queue.enqueue(dup), "同玩家在途合并跳过（返回 false）");
        player_store::SaveTask other;
        other.playerId = 9;
        other.data.playerId = 9;
        TEST_ASSERT(queue.enqueue(other), "异玩家不受合并影响");
        release.set_value();
        queue.stop();
    }
    return true;
}

bool test_worker_failure_reports_false() {
    const auto dir = make_tmp_dir("fail");
    player_store::DatabaseService db(make_config(dir));
    db.initialize();

    player_store::SaveQueue queue(1);
    queue.set_worker([](const player_store::PlayerData&) { return false; });

    std::atomic<bool> called{false};
    std::atomic<bool> ok{true};
    player_store::SaveTask task;
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
        {"persistence_savequeue_same_player_serial_and_coalesce",
         test_savequeue_same_player_serial_and_coalesce},
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
