// 跨 World 迁移真实进程链验收（P3-5，backup-revive §9.2 五步已接线段）：
//   真 LoginServer（目录 owner + 942 迁移编排受理面）+ 两个独立 Base 宿主
//  （源 zone 2 / 目标 zone 5，StoreHost 替身 = store M1 面 + 全量重报面
//   ——L19 资料归属切换后原 WorldApp 位；933/935/936/937 迁移 wire 真拨）——迁移命令走 dispatchRequest 真受理面（与
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
//      §9.2 形态）、目录不改指；
//   6. 迁移标识幂等：同 (player, request_id) 重放返回缓存成功回执，不重跑
//      编排（目标锚/落盘档案/目录代次全不动）；
//   7. 重复接管拒绝：目标侧权威代次高水位——new_epoch ≤ 已接代的迁移
//      937 直拨目标 = 拒（duplicate takeover，不落快照不 adopt）；
//   8. 迟到输入拒绝（源冻结只读）：933 后会话重绑拒、DB 存档 wire 拒、
//      锚点进度写静默拒（防脏）、结算落账面 AvatarUnavailable 挂账不落
//      （迟到结算不得双记——重复结算拒绝面在 SettlementLedger 幂等 +
//      冻结拒绝两道）。

#include "apollo/net/discovery/udp_transport.hpp"
#include "apollo/protocol/messages.hpp"
#include "login/login_server.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/database_service.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store/store_dial.hpp"
#include "squad_instance/settlement_ledger.hpp"
#include "player_store/player_store.hpp"
#include "store_host.hpp"

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
namespace base = apollo::game::base;
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

base::SessionBinding binding_of(std::uint64_t session_id, std::uint32_t gateway_id) {
    base::SessionBinding binding;
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

    // ---- 源 Base 宿主（zone 2）：种子档案 42（迁移前后档案一致性事实源）----
    player_store::PlayerStoreConfig old_store;
    old_store.host = "127.0.0.1";
    old_store.m1Enabled = true;
    old_store.m1Port = 46131;
    old_store.m1SelfName = "base";
    old_store.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    old_store.dataDir = (files.path / "old/players").string();
    old_store.journalPath = (files.path / "old/journal/persist.log").string();

    player_store::PlayerData seeded{};
    seeded.playerId = 42;
    seeded.username = "mig";
    seeded.level = 7;
    seeded.exp = 1234;
    seeded.hp = 66;
    {
        player_store::DatabaseService database(old_store);
        require(database.initialize(), "profile database initialization failed");
        require(database.createPlayer(seeded), "profile creation failed");
        database.shutdown();
    }

    // ---- 目标 Base 宿主（zone 5，空载承接）----
    player_store::PlayerStoreConfig new_store = old_store;
    new_store.m1Port = 46141;
    new_store.dataDir = (files.path / "new/players").string();
    new_store.journalPath = (files.path / "new/journal/persist.log").string();

    // 目标先起（构造序决定两报告方在同一 intake 排水批内的到达序——目标
    // 先报 = 源后报最后落笔；源冻结锚点若仍在报，目录在排水间稳定停在
    // 源域 zone 2，采样必见——不加过滤时本测试确定失败，非相位运气）
    StoreHost world_new(new_store, login.recoveryListenPort(), 22, 5, 400);
    StoreHost world_old(old_store, login.recoveryListenPort(), 21, 2, 400);
    require(world_old.store->m1_listen_port() == 46131 &&
                world_new.store->m1_listen_port() == 46141,
            "player store M1 faces did not listen");

    // 恢复收敛（两报告方全报）→ 门闸开。StoreHost 报告泵同生产 base
    // 语义（interval 到点即全量重报）——等待窗内持续泵，不让报告断流
    pump_until([&] {
        world_old.pump();
        world_new.pump();
        return login.onlineDirectory()->bindSession(42, binding_of(4242, 7));
    }, 15000, "recovery convergence");

    // ---- 会话登记 + 源锚点（真实接纳路径）----
    require(login.onlineDirectory()->bindSession(43, binding_of(4343, 8)),
            "player 43 bind refused");
    require(login.onlineDirectory()->bindSession(44, binding_of(4444, 9)),
            "player 44 bind refused");
    require(world_old.store->adoptPlayer(42, seeded.toJson(), 4242, 7, 2),
            "player 42 adopt failed");
    require(world_old.store->adoptPlayer(44, "", 4444, 9, 2),
            "player 44 adopt failed");

    // 采纳后报告刷新 → intake 落目录：在册行带承载域 zone 2（后续改指/
    // 不改指断言的基线——旧 WorldApp 宿主 gameLoop 每拍自泵，替身在此
    // 显式等报告到位）
    pump_until([&] {
        world_old.pump();
        world_new.pump();
        const auto* e42 = login.onlineDirectory()->directory().find(42);
        const auto* e44 = login.onlineDirectory()->directory().find(44);
        return e42 != nullptr && e42->zone_id == 2 && e44 != nullptr &&
               e44->zone_id == 2;
    }, 10000, "post-adopt report intake");
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
        const auto avatar = world_old.store->findAvatar(44);
        require(avatar != nullptr &&
                    avatar->state() == base::AvatarState::Transferring,
                "frozen source avatar not in Transferring after failed handoff");
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
        const auto avatar = world_old.store->findAvatar(42);
        require(avatar != nullptr &&
                    avatar->state() == base::AvatarState::Transferring,
                "source avatar not frozen after migration");
    }
    // 2. 目标确认：锚点激活 + 原区服保留 + 会话/网关双锚
    {
        const auto avatar = world_new.store->findAvatar(42);
        require(avatar != nullptr, "target avatar missing after migration");
        require(avatar->state() == base::AvatarState::Online,
                "target avatar not Online");
        require(avatar->home_zone_id() == 2,
                "target avatar lost home zone (原区服)");
        require(avatar->session_binding().session_id == 4242 &&
                    avatar->session_binding().gateway_id == 7,
                "target avatar session/gateway binding not restored");
    }
    // 2b. 导出一致性：目标落盘档案字段 == 源种子档案
    {
        std::ifstream in(new_store.dataDir + "/player_42.json",
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
        world_old.pump();
        world_new.pump();
        pump_mirror();
        const auto* entry = login.onlineDirectory()->directory().find(42);
        require(entry && entry->zone_id == 5,
                "periodic full report regressed re-pointed zone");
        const auto* mirrored = mirror.find(42);
        require(mirrored && mirrored->zone_id == 5,
                "mirror zone regressed after periodic reports");
    }

    // 6. 迁移标识幂等：同 (player, request_id) 重放 = 缓存成功回执，不重跑
    //    （目标锚点/落盘档案/目录代次全不动——重复编排不得二次接管）
    {
        std::string landed_json;
        {
            std::ifstream in(new_store.dataDir + "/player_42.json",
                             std::ios::binary);
            landed_json.assign((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
        }
        const auto replay =
            migrate(*login.onlineDirectory(), 42, 46131, 46141, 5);
        require(replay.ok, "replayed migrate must return cached success");
        require(replay.new_epoch == reply.new_epoch,
                "replayed migrate must not advance epoch");
        const auto* entry = login.onlineDirectory()->directory().find(42);
        require(entry && entry->anchor_epoch == reply.new_epoch,
                "directory epoch advanced by replayed migrate");
        const auto avatar = world_new.store->findAvatar(42);
        require(avatar != nullptr &&
                    avatar->state() == base::AvatarState::Online &&
                    avatar->session_binding().session_id == 4242,
                "replayed migrate disturbed target avatar");
        {
            std::ifstream in(new_store.dataDir + "/player_42.json",
                             std::ios::binary);
            std::string json((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
            require(json == landed_json,
                    "replayed migrate rewrote target profile");
        }
    }
    // 7. 重复接管拒绝：目标权威代次高水位（new_epoch ≤ 已接代 = 拒）
    {
        migration::HandoffRequest stale;
        stale.player_id = 42;
        stale.request_id = 2;
        stale.new_epoch = 1;  // 高水位已 2（主线迁移接代）——旧代次重放
        stale.session_id = 4242;
        stale.gateway_id = 7;
        player_store::StoreDial dial(APOLLO_TEST_ROUTE_JSON, "login", "world",
                                     "127.0.0.1", 46141);
        require(dial.ready() && dial.waitUp(), "target store dial failed");
        const auto raw = dial.call(migration::encodeFrame(stale));
        require(raw.has_value(), "stale handoff reply missing");
        migration::HandoffReply hr;
        protocol::SessionID sid = 0;
        require(migration::decodeFrame(*raw, hr, sid) && !hr.accepted,
                "stale handoff not rejected");
        require(hr.error_message.find("duplicate takeover") != std::string::npos,
                "stale handoff rejection must name duplicate takeover");
        dial.shutdown();
        const auto avatar = world_new.store->findAvatar(42);
        require(avatar != nullptr && avatar->state() == base::AvatarState::Online,
                "rejected takeover disturbed target avatar");
    }
    // 8. 迟到输入拒绝（源冻结只读——Transferring 不再受理任何写面）
    {
        // 8a. 会话重绑拒（绑定列不动）
        require(!world_old.store->bindSession(42, binding_of(9999, 9)),
                "late session bind on frozen avatar accepted");
        const auto avatar = world_old.store->findAvatar(42);
        require(avatar != nullptr &&
                    avatar->state() == base::AvatarState::Transferring &&
                    avatar->session_binding().session_id == 4242,
                "late bind disturbed frozen avatar");
        // 8b. DB 存档 wire 拒（迟到客户端存档不得置回写态）
        protocol::DbSaveRequest save;
        save.playerId = 42;
        save.jsonData = seeded.toJson();
        player_store::StoreDial dial(APOLLO_TEST_ROUTE_JSON, "login", "world",
                                     "127.0.0.1", 46131);
        require(dial.ready() && dial.waitUp(), "source store dial failed");
        const auto raw = dial.call(protocol::MessageCodec::encode(save, 0));
        require(raw.has_value(), "late db save reply missing");
        const auto hdr = protocol::MessageCodec::parseHeader(*raw);
        require(static_cast<protocol::MessageType>(hdr.type) ==
                    protocol::MessageType::DB_SAVE_RESPONSE,
                "late db save reply type wrong");
        const auto body = std::vector<std::uint8_t>(
            raw->begin() + sizeof(protocol::MessageHeader), raw->end());
        const auto resp =
            protocol::MessageCodec::decodeBody<protocol::DbSaveResponse>(body);
        require(!resp.success, "late db save on frozen avatar accepted");
        const auto after = world_old.store->findAvatar(42);
        require(after != nullptr &&
                    after->state() == base::AvatarState::Transferring,
                "late db save broke frozen read-only state");
        dial.shutdown();
        // 8c. 锚点进度写静默拒（迟到业务写不进内存账——防脏 + 防结算双记；
        // 锚点 exp = 启动播种的档案现值——迟到写不叠加）
        after->add_progress("exp", 100);
        require(after->progress("exp") == seeded.exp && !after->needs_save(),
                "frozen avatar accepted progress write");
        // 8d. 结算落账面：冻结锚点 = 旧 Base 业务受理拒（958 Transferring
        // 不受理）→ 账本挂账不落（迟到结算不叠在已冻结资料上，双记风险由
        // Base 侧拒收裁决——账本不解析本地锚点，todo L84 跨界纪律）
        squad_instance::SettlementLedger ledger;
        ledger.begin({42});
        std::uint64_t credit_seq = 1;
        ledger.set_business_sink([&world_old, &credit_seq](std::uint64_t pid,
                                                           std::int64_t score) {
            player_store::business::ScoreCreditRequest req;
            req.request_id = ++credit_seq;
            req.player_id = pid;
            req.score = score;
            const auto reply =
                world_old.store->dispatchRequest(player_store::business::encodeFrame(req));
            player_store::business::ScoreCreditReply rep;
            apollo::protocol::SessionID sid = 0;
            if (reply.empty() ||
                !player_store::business::decodeFrame(reply, rep, sid)) {
                return squad_instance::SettlementLedger::ApplyError::AvatarUnavailable;
            }
            return rep.accepted
                       ? squad_instance::SettlementLedger::ApplyError::Ok
                       : squad_instance::SettlementLedger::ApplyError::AvatarUnavailable;
        });
        require(ledger.apply(42, 50) ==
                    squad_instance::SettlementLedger::ApplyError::AvatarUnavailable,
                "late settlement on frozen avatar not held");
        require(ledger.snapshot().pending == 1 && ledger.snapshot().applied == 0,
                "held settlement disturbed ledger state");
    }

    login.stop();
    // 宿主收口：StoreHost 析构自停（先报告面后 store，随作用域退出）。
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
