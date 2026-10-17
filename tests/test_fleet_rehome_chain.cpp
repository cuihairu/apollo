// 编队面 re-home 真实进程链验收（P3-1：baseappmgr/base-app 退场后目录
// owner 面 = Login 进程、报告方 = World 进程）：
//   真 LoginServer（镜像 owner / 恢复 intake / 死亡订阅三面）+ 真 WorldApp
//   （FleetReporter 全量重报）+ 测试侧 machined 替身（DeathNotifier 收订阅
//   / 发死亡通知）——全 UDP 真回环，无进程内桩直调。
//
// 断言事实源（对齐 tooling.md 演练口径——日志不算证据）：
//   1. 恢复相位排他：intake 口开启期 bindSession 拒绝（门闸关）；
//   2. World 启动全量重报（真实 UDP datagram）→ 收敛开放（bindSession 放行）；
//   3. 锚点锚定后刷新重报 → intake 落目录 → 镜像消费侧（真 DirectoryMirror
//      收真 Delta/SnapshotReply）投影出 zone/会话/网关全量身份；
//   4. machined 死亡通知（zone 44）→ 目录按 zone 反查进保活窗口（Suspended）；
//   5. 窗口满 sweep → 条目终结 + SessionDown 增量达镜像（镜像行消失）；
//      非 zone 命中行（无 zone 探针行）不受牵连。

#include "apollo/net/discovery/udp_transport.hpp"
#include "login/login_server.hpp"
#include "world_app/world_app.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;
namespace disco = apollo::net::discovery;
namespace session = apollo::game::session;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-fleet-rehome-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// 轮询谓词（10ms 拍；全链路异步——重报/intake/sweep 各有节拍）
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

void fleet_rehome_chain() {
    TemporaryDirectory files;

    // ---- 测试侧镜像消费端 + machined 替身（真 UDP 面）----
    disco::UdpFeed mirror_feed;
    require(mirror_feed.open(0, "127.0.0.1"), "mirror feed did not bind");
    session::DirectoryMirror mirror;

    disco::UdpFeed machined_feed;
    require(machined_feed.open(0, "127.0.0.1"), "machined feed did not bind");
    disco::UdpBeaconTransport machined_send;
    require(machined_send.open(), "machined send transport did not open");
    disco::DeathNotifier notifier(machined_send, 1000);

    // ---- 目录 owner 进程：Login（三面全开）----
    login::LoginConfig login_config;
    login_config.host = "127.0.0.1";
    login_config.directoryEnabled = true;
    // 目录 M1 控制面（内核分配端口）——Login start() 以 listenPort()!=0 为
    // 目录装配成功判据，测试不拨此面
    login_config.directoryPort = 0;
    login_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    login_config.directoryEpochPath = (files.path / "epoch/online.epoch").string();
    login_config.mirrorHost = "127.0.0.1";
    login_config.mirrorPort = mirror_feed.port();
    login_config.mirrorSnapshotMs = 300;
    login_config.recoveryPort = 45873;  // 面开需显式端口（旧壳同款：0 = 关）
    login_config.recoveryExpected = 1;
    login_config.recoveryTimeoutMs = 30000;  // 收敛靠真实重报，不靠超时兜底
    login_config.machinedPort = machined_feed.port();
    login_config.machinedComponentId = disco::kLoginDirectoryComponentId;
    login_config.suspendWindowTicks = 2;

    login::LoginServer login(login_config);
    login.start();
    require(login.onlineDirectory() != nullptr, "online directory not hosted");
    require(login.recoveryListenPort() != 0, "recovery intake did not listen");
    require(login.deathFeedPort() != 0, "death feed did not listen");

    // 恢复相位排他（增量③）：收敛前 bindSession 拒绝——门闸关
    session::SessionBinding probe_binding;
    probe_binding.session_id = 7777;
    probe_binding.gateway_id = 9;
    require(!login.onlineDirectory()->bindSession(777, probe_binding),
            "admission gate did not refuse during recovery");

    // ---- 报告方进程：World（恢复相位全量重报 → Login intake 口）----
    world_app::WorldAppConfig world_config;
    world_config.worldId = 44;
    world_config.host = "127.0.0.1";
    world_config.m1Port = 0;  // 本链路不走 M1 意图面（spawn 链归 squad e2e）
    world_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    world_config.playerStoreEnabled = true;
    world_config.playerStore.m1Enabled = true;
    world_config.playerStore.host = "127.0.0.1";
    world_config.playerStore.dataDir = (files.path / "players").string();
    world_config.playerStore.journalPath = (files.path / "journal/persist.log").string();
    world_config.reportHost = "127.0.0.1";
    world_config.reportPort = login.recoveryListenPort();
    world_config.reportComponentId = 11;
    world_config.reportZoneId = 44;
    world_config.reportIntervalMs = 400;

    world_app::WorldApp world(world_config);
    world.start();
    require(world.isRunning(), "world app did not start");

    // World 启动全量重报（真实 datagram）→ 收敛开放——门闸开。timeout
    // (30s) 远大于重报到达，收敛是报告驱动不是超时兜底
    pump_until([&] {
        return login.onlineDirectory()->bindSession(777, probe_binding);
    }, 10000, "gate open");
    const auto* probe = login.onlineDirectory()->directory().find(777);
    require(probe != nullptr, "gate-open probe row missing");
    require(probe->state == session::PlayerDirectory::EntryState::Online,
            "probe row not Online");

    // ---- 锚点（真实宿主迁移接纳路径 adoptPlayer：会话绑定 + home zone）----
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> roster{
        {9001, 100}, {9002, 101}, {9003, 102}};
    for (const auto [session_id, player_id] : roster) {
        require(world.playerStore() != nullptr, "player store not hosted");
        require(world.playerStore()->adoptPlayer(player_id, "", session_id, 7, 44),
                "adoptPlayer failed");
    }

    // ---- 刷新重报 → intake 落目录 → 镜像投影（真 Delta/快照 wire）----
    auto pump_mirror = [&] {
        std::uint8_t buf[disco::kMaxDatagramSize];
        std::size_t len = 0;
        while (mirror_feed.try_receive(buf, sizeof(buf), len)) {
            mirror.on_datagram(buf, len);
        }
    };
    bool roster_mirrored = false;
    pump_until([&] {
        pump_mirror();
        roster_mirrored = true;
        for (const auto [session_id, player_id] : roster) {
            const auto* entry = mirror.find(player_id);
            if (entry == nullptr || entry->zone_id != 44 ||
                entry->session_id != session_id || entry->gateway_id != 7 ||
                entry->state != session::PlayerDirectory::EntryState::Online) {
                roster_mirrored = false;
            }
        }
        return roster_mirrored;
    }, 15000, "mirror roster");
    require(mirror.find(100) != nullptr && mirror.find(101) != nullptr &&
                mirror.find(102) != nullptr,
            "mirror projection incomplete");

    // ---- World 退场（真死亡拓扑：死亡进程不再刷新重报——后续 intake 不再
    // 复活条目；intake 的 restore-not-kick 复活面正是对「活进程重报」的
    // 语义，与本 死亡窗口 拍不冲突）----
    world.stop();

    // ---- machined 替身：收订阅（真 DeathSubscribe wire）→ 发死亡通知 ----
    bool subscribed = false;
    for (int drained = 0; drained < 200 && !subscribed; ++drained) {
        std::uint8_t buf[disco::kWireSize];
        std::string sender_host;
        std::uint16_t sender_port = 0;
        if (!machined_feed.try_receive(buf, sender_host, sender_port)) {
            std::this_thread::sleep_for(10ms);
            continue;
        }
        disco::WirePacket packet;
        require(disco::WirePacket::decode_from(buf, packet),
                "machined feed got undecodable packet");
        if (packet.op != disco::Op::DeathSubscribe) continue;
        // 真链路核对：订阅回投端口 = Login 死亡收包口
        require(packet.service_port == login.deathFeedPort(),
                "death subscribe did not carry login death feed port");
        require(packet.component_id == disco::kLoginDirectoryComponentId,
                "death subscribe did not carry login component id");
        require(notifier.on_subscribe(packet.component_id, packet.service_port,
                                      sender_host, now_ms()),
                "machined stand-in did not register subscriber");
        subscribed = true;
    }
    require(subscribed, "no death subscribe reached machined stand-in");

    disco::MemberId dead;
    dead.component_id = 11;  // World 报告方组件号
    dead.zone_id = 44;
    require(notifier.notify(dead, disco::DeathKind::TtlExpired, 0),
            "death notify send failed");

    // ---- 死亡 → 目录按 zone 反查进保活窗口（Suspended；无事件）----
    pump_until([&] {
        const auto* entry = login.onlineDirectory()->directory().find(100);
        return entry != nullptr &&
               entry->state == session::PlayerDirectory::EntryState::Suspended;
    }, 8000, "zone suspend");
    for (const auto player_id : {101u, 102u}) {
        const auto* entry = login.onlineDirectory()->directory().find(player_id);
        require(entry != nullptr &&
                    entry->state == session::PlayerDirectory::EntryState::Suspended,
                "zone suspend missed roster row");
    }
    // 非 zone 命中行不受牵连（探针行 zone 未分组）
    {
        const auto* entry = login.onlineDirectory()->directory().find(777);
        require(entry != nullptr &&
                    entry->state == session::PlayerDirectory::EntryState::Online,
                "probe row wrongly suspended by zone death");
    }

    // ---- 窗口满 sweep → 条目终结 + SessionDown 达镜像（镜像行消失）----
    pump_until([&] {
        pump_mirror();
        return login.onlineDirectory()->directory().find(100) == nullptr &&
               login.onlineDirectory()->directory().find(101) == nullptr &&
               login.onlineDirectory()->directory().find(102) == nullptr &&
               mirror.find(100) == nullptr && mirror.find(101) == nullptr &&
               mirror.find(102) == nullptr;
    }, 15000, "sweep expiry");
    // 探针行存活（zone 死亡处置不越界）
    require(login.onlineDirectory()->directory().find(777) != nullptr &&
                mirror.find(777) != nullptr,
            "probe row wrongly swept");

    login.stop();
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: fleet re-home chain requires POSIX UDP\n";
    return 0;
#else
    try {
        fleet_rehome_chain();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Fleet re-home chain passed\n";
    return 0;
#endif
}
