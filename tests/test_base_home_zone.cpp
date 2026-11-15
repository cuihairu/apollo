// L118 原区服（home_zone_id）业务语义接线——真源闭环验收：
//   1. 档案即真源：flush 自锚点回写 homeZoneId（PlayerData 新字段），
//      重启激活自档案播种原区服；旧档缺键缺省 0 = 未分配不幻值；
//   2. RPC 保存载荷的旧域 homeZoneId 被锚点现值覆盖（同 exp 防回卷口径）。
// 迁移接权路径（adoptPlayer 显式落 home_zone + 快照回带）已由
// migration 族既有用例覆盖，此处不重复。
// 宿主：独立 PlayerStore（无 M1/报告面），临时目录自净。

#include "player_store/player_store.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

namespace fs = std::filesystem;

#define TEST_ASSERT(cond, msg)                                                           \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            std::cerr << "FAIL: " << (msg) << " (" << __FILE__ << ":" << __LINE__ << ")" \
                      << std::endl;                                                      \
            return false;                                                                \
        }                                                                                \
    } while (0)

std::string make_tmp_dir(const char* tag) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto dir = fs::temp_directory_path() /
               ("apollo_home_zone_" + std::string(tag) + "_" + std::to_string(now));
    fs::create_directories(dir);
    return dir.string();
}

player_store::PlayerStoreConfig make_config(const std::string& dir) {
    player_store::PlayerStoreConfig config;
    config.workerThreads = 1;
    config.autoSaveIntervalMs = 10;
    config.dataDir = dir + "/players";
    config.journalPath = dir + "/journal/persist.log";
    config.shutdownFlushTimeoutMs = 2000;
    return config;
}

bool test_home_zone_persists_across_restart() {
    const auto dir = make_tmp_dir("restart");
    const auto config = make_config(dir);

    // 旧档（无 homeZoneId 键）：激活不幻值——锚点未分配保持 0
    player_store::PlayerData seed;
    seed.playerId = 1100;
    seed.username = "homezone-test";
    seed.exp = 10;
    {
        player_store::DatabaseService seeder(config);
        TEST_ASSERT(seeder.initialize(), "seed archive initialize");
        TEST_ASSERT(seeder.savePlayer(seed), "seed archive written");
    }

    // 代次一：分配原区服 7 → 停机 flush 落档
    {
        player_store::PlayerStore server(config);
        server.start();
        const auto avatar = server.activatePlayer(1100);
        TEST_ASSERT(avatar != nullptr, "avatar activated");
        TEST_ASSERT(avatar->home_zone_id() == 0,
                    "legacy archive keeps home zone unassigned");
        avatar->set_home_zone_id(7);
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.initialize(), "reload initialize");
        TEST_ASSERT(reloader.loadPlayer(1100, loaded) && loaded.homeZoneId == 7,
                    "flush stamps home zone into archive");
    }

    // 代次二：重启播种恢复 + RPC 旧载荷回卷被锚点真源挡下
    {
        player_store::PlayerStore server(config);
        server.start();
        const auto avatar = server.activatePlayer(1100);
        TEST_ASSERT(avatar != nullptr, "reactivated");
        TEST_ASSERT(avatar->home_zone_id() == 7,
                    "restart seeds home zone from archive");

        player_store::PlayerData payload;
        payload.playerId = 1100;
        payload.username = "homezone-test";
        payload.homeZoneId = 99;  // 旧域回放值
        apollo::protocol::DbSaveRequest req;
        req.playerId = 1100;
        req.jsonData = payload.toJson();
        (void)server.dispatchRequest(
            apollo::protocol::MessageCodec::encode(req, 0));
        server.stop();
    }
    {
        player_store::DatabaseService reloader(config);
        player_store::PlayerData loaded;
        TEST_ASSERT(reloader.initialize(), "reload initialize 2");
        TEST_ASSERT(reloader.loadPlayer(1100, loaded) && loaded.homeZoneId == 7,
                    "stale payload home zone blocked by anchor truth");
    }

    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"home_zone_persists_across_restart", test_home_zone_persists_across_restart},
    };

    bool all = true;
    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... " << std::endl;
        if (!t.fn()) {
            all = false;
        }
    }
    std::cout << (all ? "base_home_zone_tests passed" : "base_home_zone_tests FAILED")
              << std::endl;
    return all ? 0 : 1;
}
