// 跨 World 迁移真实进程链验收（P3-5，backup-revive §9.2 五步已接线段）：
//   真 LoginServer（目录 owner + 942 迁移编排受理面）+ 两个真 WorldApp
//  （源 zone 2 / 目标 zone 5，真 PlayerStore M1 面 + 933/935/936/937 迁移
//   wire 经 StoreDial 真拨）——迁移命令走 dispatchRequest 真受理面（与
//   fleetctl 942 同 handler），非进程内直调编排函数。
//
// 断言面（分别验证，日志不算证据）：
//   1. 源端冻结：迁移后源锚点 Transferring（⑤ 未接线形态——老侧停留，
//      写入被拒兜住，§9.9-2 如实边界）；
//   2. 导出/目标确认：目标锚点激活（原区服 home zone 保留、会话/网关双锚
//      恢复）+ 目标落盘档案字段与源一致（935/936 快照真拉取）；
//   3. 路由切换：目录改指（zone 2→5、权威代次 +1）+ SessionMoved 增量与
//      ④ 立即推快照达镜像消费侧；
//   4. 报告刷新不回卷改指：目标周期全量重报 intake 后 zone 仍 5——两道
//      缺一不可：报告条目 zone = 承载域（非原区服 home zone）+ 源冻结
//      锚点（Transferring）离源端在册集，不再以源域上报回卷 ④ 改指；
//   5. 失败恢复：源不可达 = 干净拒绝（目录不动）；目标不可达（冻结已生效）
//      = 如实失败，源停留 Transferring（③ 前可回退、③ 后不可撤销面的
//      §9.2 形态）、目录不改指。

#include "apollo/net/discovery/udp_transport.hpp"
#include "login/login_server.hpp"
#include "player_store/database_service.hpp"
#include "player_store/migration_wire.hpp"
#include "world_app/world_app.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace std::chrono_literals;
namespace disco = apollo::net::discovery;
namespace session = apollo::game::session;
namespace migration = player_store::migration;
namespace protocol = apollo::protocol;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

// 轮询谓词（10ms 拍；报告/intake/迁移全异步）
template <typename Pred>
void pump_until(Pred&& done, int timeout_ms, const char* tag) {
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(timeout_ms);
    while (!done()) {
        if (std::chrono::steady_clock::now() > deadline) {
            throw std::runtime_error(std::string("pump_until exceeded: ") + tag);
        }
        std::this_thread::sleep_for(10ms);
    }
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-migration-chain-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// 迁移命令（942 → 943）：dispatchRequest 真受理面进出
migration::MigrateReply migrate(online_directory::OnlineDirectory& directory,
                                std::uint64_t player_id, std::uint16_t source_port,
                                std::uint16_t target_port, std::uint32_t target_zone) {
    migration::MigrateRequest req;
    req.request_id = 1;
    req.player_id = player_id;
    req.source_host = "127.0.0.1";
    req.source_port = source_port;
    req.target_host = "127.0.0.1";
    req.target_port = target_port;
    req.target_zone_id = target_zone;
    const auto reply = directory.dispatchRequest(migration::encodeFrame(req));
    migration::MigrateReply out;
    protocol::SessionID sid = 0;
    require(migration::decodeFrame(reply, out, sid), "migrate reply undecodable");
    return out;
}

session::SessionBinding binding_of(std::uint64_t session_id, std::uint32_t gateway_id) {
    session::SessionBinding binding;
    binding.session_id = session_id;
    binding.gateway_id = gateway_id;
    return binding;
}

void migration_chain() {
    TemporaryDirectory files;

    // ---- 镜像消费端（④ 立即推 + SessionMoved 增量的观察面）----
    disco::UdpFeed mirror_feed;
    require(mirror_feed.open(0, "127.0.0.1"), "mirror feed did not bind");
    session::DirectoryMirror mirror;
    auto pump_mirror = [&] {
        std::uint8_t buf[disco::kMaxDatagramSize];
        std::size_t len = 0;
        while (mirror_feed.try_receive(buf, sizeof(buf), len)) {
            mirror.on_datagram(buf, len);
        }
    };

    // ---- 目录 owner：Login（恢复 intake 两报告方 + 镜像面；死亡面关）----
    login::LoginConfig login_config;
    login_config.host = "127.0.0.1";
    login_config.directoryEnabled = true;
    login_config.directoryPort = 0;  // 内核分配（start() 以 listenPort 判装配）
    login_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    login_config.directoryEpochPath = (files.path / "epoch/online.epoch").string();
    login_config.mirrorHost = "127.0.0.1";
    login_config.mirrorPort = mirror_feed.port();
    login_config.mirrorSnapshotMs = 30000;  // 周期面不进演练窗——镜像只收 ④ 立即推/增量
    login_config.recoveryPort = 45913;      // 面开需显式端口
    login_config.recoveryExpected = 2;      // 两 World 报告方
    login_config.recoveryTimeoutMs = 15000;

    login::LoginServer login(login_config);
    login.start();
    require(login.recoveryListenPort() == 45913, "recovery intake did not listen");

    // ---- 源 World（zone 2）：种子档案 42（迁移前后档案一致性事实源）----
    world_app::WorldAppConfig old_config;
    old_config.worldId = 2;
    old_config.host = "127.0.0.1";
    old_config.m1Port = 0;
    old_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    old_config.playerStoreEnabled = true;
    old_config.playerStore.m1Enabled = true;
    old_config.playerStore.host = "127.0.0.1";
    old_config.playerStore.m1Port = 46131;
    old_config.playerStore.dataDir = (files.path / "old/players").string();
    old_config.playerStore.journalPath = (files.path / "old/journal/persist.log").string();
    old_config.reportHost = "127.0.0.1";
    old_config.reportPort = login.recoveryListenPort();
    old_config.reportComponentId = 21;
    old_config.reportZoneId = 2;
    old_config.reportIntervalMs = 400;

    player_store::PlayerData seeded{};
    seeded.playerId = 42;
    seeded.username = "mig";
    seeded.level = 7;
    seeded.exp = 1234;
    seeded.hp = 66;
    {
        player_store::DatabaseService database(old_config.playerStore);
        require(database.initialize(), "profile database initialization failed");
        require(database.createPlayer(seeded), "profile creation failed");
        database.shutdown();
    }

    // ---- 目标 World（zone 5，空载承接）----
    world_app::WorldAppConfig new_config = old_config;
    new_config.worldId = 5;
    new_config.playerStore.m1Port = 46141;
    new_config.playerStore.dataDir = (files.path / "new/players").string();
    new_config.playerStore.journalPath = (files.path / "new/journal/persist.log").string();
    new_config.reportComponentId = 22;
    new_config.reportZoneId = 5;

    // 目标先起（构造序决定两报告方在同一 intake 排水批内的到达序——目标
    // 先报 = 源后报最后落笔；源冻结锚点若仍在报，目录在排水间稳定停在
    // 源域 zone 2，采样必见——不加过滤时本测试确定失败，非相位运气）
    world_app::WorldApp world_new(new_config);
    world_new.start();
    world_app::WorldApp world_old(old_config);
    world_old.start();
    require(world_old.playerStore()->m1_listen_port() == 46131 &&
                world_new.playerStore()->m1_listen_port() == 46141,
            "player store M1 faces did not listen");

    // 恢复收敛（两报告方全报）→ 门闸开
    pump_until([&] {
        return login.onlineDirectory()->bindSession(42, binding_of(4242, 7));
    }, 15000, "recovery convergence");

    // ---- 会话登记 + 源锚点（真实接纳路径）----
    require(login.onlineDirectory()->bindSession(43, binding_of(4343, 8)),
            "player 43 bind refused");
    require(login.onlineDirectory()->bindSession(44, binding_of(4444, 9)),
            "player 44 bind refused");
    require(world_old.playerStore()->adoptPlayer(42, seeded.toJson(), 4242, 7, 2),
            "player 42 adopt failed");
    require(world_old.playerStore()->adoptPlayer(44, "", 4444, 9, 2),
            "player 44 adopt failed");
    const auto epoch_before =
        login.onlineDirectory()->directory().find(42)->anchor_epoch;

    // ---- 失败恢复 A：源不可达 = 干净拒绝（目录不动——条目逐字段同迁移前）----
    {
        const auto* before = login.onlineDirectory()->directory().find(43);
        require(before != nullptr, "player 43 entry missing before failed migrate");
        const session::PlayerDirectory::Entry snapshot = *before;
        const auto reply =
            migrate(*login.onlineDirectory(), 43, 46000, 46141, 5);
        require(!reply.ok, "migrate to unreachable source unexpectedly succeeded");
        const auto* entry = login.onlineDirectory()->directory().find(43);
        require(entry != nullptr, "player 43 entry missing after failed migrate");
        require(entry->zone_id == snapshot.zone_id &&
                    entry->state == snapshot.state &&
                    entry->anchor_epoch == snapshot.anchor_epoch &&
                    entry->binding.session_id == snapshot.binding.session_id,
                "failed migrate disturbed directory entry");
    }

    // ---- 失败恢复 B：目标不可达（冻结已生效）= 如实失败 + 老侧停留 ----
    {
        const auto reply =
            migrate(*login.onlineDirectory(), 44, 46131, 46000, 5);
        require(!reply.ok, "migrate to unreachable target unexpectedly succeeded");
        const auto anchor = world_old.playerStore()->findAnchor(44);
        require(anchor != nullptr &&
                    anchor->state() == session::AnchorState::Transferring,
                "frozen source anchor not in Transferring after failed handoff");
        const auto* entry = login.onlineDirectory()->directory().find(44);
        require(entry && entry->zone_id == 2,
                "failed handoff must not re-point directory");
    }

    // ---- 主线迁移：①冻结 → ②快照拉取 → ③交权 → ④改指 ----
    const auto reply =
        migrate(*login.onlineDirectory(), 42, 46131, 46141, 5);
    require(reply.ok, "player 42 migration failed");
    require(reply.new_epoch == epoch_before + 1, "epoch did not advance by one");

    // 1. 源端冻结形态（⑤ 老侧停留——如实边界：写入被拒兜住）
    {
        const auto anchor = world_old.playerStore()->findAnchor(42);
        require(anchor != nullptr &&
                    anchor->state() == session::AnchorState::Transferring,
                "source anchor not frozen after migration");
    }
    // 2. 目标确认：锚点激活 + 原区服保留 + 会话/网关双锚
    {
        const auto anchor = world_new.playerStore()->findAnchor(42);
        require(anchor != nullptr, "target anchor missing after migration");
        require(anchor->state() == session::AnchorState::Online,
                "target anchor not Online");
        require(anchor->home_zone_id() == 2,
                "target anchor lost home zone (原区服)");
        require(anchor->session_binding().session_id == 4242 &&
                    anchor->session_binding().gateway_id == 7,
                "target anchor session/gateway binding not restored");
    }
    // 2b. 导出一致性：目标落盘档案字段 == 源种子档案
    {
        std::ifstream in(new_config.playerStore.dataDir + "/player_42.json",
                         std::ios::binary);
        require(in.good(), "target profile file missing");
        std::string json((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
        const auto landed = player_store::PlayerData::fromJson(json);
        require(landed.playerId == 42 && landed.level == 7 && landed.exp == 1234 &&
                    landed.hp == 66 && landed.username == "mig",
                "target profile does not match source snapshot");
    }
    // 3. 路由切换：目录改指（zone 5 + 代次）+ ④ 立即推达镜像
    {
        const auto* entry = login.onlineDirectory()->directory().find(42);
        require(entry && entry->zone_id == 5, "directory zone not re-pointed");
        require(entry->anchor_epoch == reply.new_epoch,
                "directory epoch not advanced");
        pump_until([&] {
            pump_mirror();
            const auto* mirrored = mirror.find(42);
            return mirrored != nullptr && mirrored->zone_id == 5 &&
                   mirrored->anchor_epoch == reply.new_epoch;
        }, 8000, "immediate snapshot to mirror");
    }
    // 4. 报告刷新不回卷改指（多周期采样：源冻结锚点离在册集——只有目标
    //    域报告 42；任一采样拍 zone≠5 = 源端 Transferring 锚点仍以上报回卷）
    for (int i = 0; i < 12; ++i) {
        std::this_thread::sleep_for(150ms);
        pump_mirror();
        const auto* entry = login.onlineDirectory()->directory().find(42);
        require(entry && entry->zone_id == 5,
                "periodic full report regressed re-pointed zone");
        const auto* mirrored = mirror.find(42);
        require(mirrored && mirrored->zone_id == 5,
                "mirror zone regressed after periodic reports");
    }

    login.stop();
    world_new.stop();
    world_old.stop();
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: migration chain requires POSIX UDP/TCP\n";
    return 0;
#else
    try {
        migration_chain();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Migration chain passed\n";
    return 0;
#endif
}
