// Base 业务受理面（958/959）真实进程链验收（P1 基础业务与结算迁入）：
// 独立 PlayerStore 宿主（M1 受理面，StoreHost 纯 store 位——无报告面）+
// StoreDial 真拨（同 migration 族底座，握手只验 route_version）：
//   1. 常规落账：adoptPlayer 建锚 → 958 → accepted=1，锚点 exp 累入分值
//      （progress 读数断言，非日志——与 AvatarRewardSink 语义对齐）；
//   2. 无锚点玩家 → accepted=0（挂账语义，不抛不崩）；
//   3. 冻结（933 真拨 → Transferring）后 958 → accepted=0（迟到结算
//      拒绝——跨域双记防线）；
//   4. SettlementLedger 业务受理装配：sink = 958/959 wire → apply Ok +
//      双投 AlreadyApplied（幂等归调用方账本，二次不达 wire）；
//   5. 受理面停 → AvatarUnavailable 挂账（传输不可达同挂账语义）。

#include "player_store/business_wire.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store/player_store.hpp"
#include "player_store/store_dial.hpp"
#include "squad_instance/settlement_ledger.hpp"
#include "store_host.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
namespace business = player_store::business;
namespace migration = player_store::migration;
namespace session = apollo::game::session;
using squad_instance::SettlementLedger;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("apollo-base-business-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryDirectory() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

business::ScoreCreditReply fire_credit(player_store::StoreDial& dial,
                                        std::uint64_t request_id,
                                        std::uint64_t player_id,
                                        std::int64_t score,
                                        std::uint32_t zone,
                                        std::uint64_t settlement_id = 0) {
    business::ScoreCreditRequest req;
    req.request_id = request_id;
    req.player_id = player_id;
    req.score = score;
    req.source_zone_id = zone;
    req.settlement_id = settlement_id;
    auto reply = dial.call(business::encodeFrame(req));
    require(reply.has_value(), "score credit wire: no reply (store face down)");
    business::ScoreCreditReply rep;
    apollo::protocol::SessionID sid = 0;
    require(business::decodeFrame(*reply, rep, sid),
            "score credit wire: undecodable reply");
    require(rep.player_id == player_id && rep.request_id == request_id,
            "score credit reply correlation broken");
    return rep;
}

// 账本业务受理 sink：与 WorldApp 装配同款（958/959 全帧进出；面不可达/
// 回包畸形/受理拒绝一律挂账——fire_credit 的强断言只用于在线步）。
SettlementLedger::ApplyError wire_sink(player_store::StoreDial& dial,
                                       std::uint64_t& seq,
                                       std::uint64_t player_id,
                                       std::int64_t score,
                                       std::uint64_t settlement_id = 0) {
    business::ScoreCreditRequest req;
    req.request_id = ++seq;
    req.player_id = player_id;
    req.score = score;
    req.source_zone_id = 2;
    req.settlement_id = settlement_id;
    const auto reply = dial.call(business::encodeFrame(req));
    business::ScoreCreditReply rep;
    apollo::protocol::SessionID sid = 0;
    if (!reply || !business::decodeFrame(*reply, rep, sid)) {
        return SettlementLedger::ApplyError::AvatarUnavailable;  // 传输不可达
    }
    return rep.accepted ? SettlementLedger::ApplyError::Ok
                        : SettlementLedger::ApplyError::AvatarUnavailable;
}

void base_business_wire() {
    TemporaryDirectory files;

    player_store::PlayerStoreConfig store_config;
    store_config.host = "127.0.0.1";
    store_config.m1Enabled = true;
    store_config.m1Port = 0;  // 内核分配（拨号面读回）
    store_config.m1SelfName = "base";
    store_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_config.dataDir = (files.path / "players").string();
    store_config.journalPath = (files.path / "journal/persist.log").string();

    StoreHost base(store_config, 0, 0, 0, 0);  // 纯 store 位（无报告面）
    require(base.store->m1_listen_port() != 0, "base store did not listen");

    player_store::StoreDial dial(APOLLO_TEST_ROUTE_JSON, "world", "base",
                                 "127.0.0.1", base.store->m1_listen_port());
    require(dial.ready(), "store dial did not assemble");
    require(dial.waitUp(), "store dial did not reach base face");

    // ---- 1. 常规落账（锚点 exp 累入分值）----
    require(base.store->adoptPlayer(7, "", 7001, 5, 2), "player 7 adopt failed");
    {
        const auto rep = fire_credit(dial, 1, 7, 120, 2);
        require(rep.accepted, "score credit on live avatar not accepted");
        const auto avatar = base.store->findAvatar(7);
        require(avatar != nullptr, "player 7 avatar missing after credit");
        require(avatar->progress("exp") == 120,
                "credited score did not land on avatar exp");
    }

    // ---- 2. 无锚点玩家 → accepted=0 ----
    {
        const auto rep = fire_credit(dial, 2, 999, 50, 2);
        require(!rep.accepted, "score credit on unknown player accepted");
        require(!rep.error_message.empty(),
                "unaccepted credit reply missing reason");
    }

    // ---- 3. 冻结（933）→ 迟到结算拒绝（Transferring 不落——跨域双记防线）----
    require(base.store->adoptPlayer(8, "", 7002, 6, 2), "player 8 adopt failed");
    {
        migration::FreezeRequest freeze;
        freeze.player_id = 8;
        freeze.request_id = 10;
        auto reply = dial.call(migration::encodeFrame(freeze));
        require(reply.has_value(), "freeze wire: no reply");
        migration::FreezeReply fr;
        apollo::protocol::SessionID sid = 0;
        require(migration::decodeFrame(*reply, fr, sid) && fr.accepted,
                "freeze on player 8 not accepted");
    }
    {
        const auto rep = fire_credit(dial, 3, 8, 60, 2);
        require(!rep.accepted, "score credit on transferring avatar accepted");
    }

    // ---- 4. SettlementLedger 业务受理装配：settlement_id 穿透 + 幂等 ----
    //（L132：稳定玩法/结算 ID 随 begin 定账——重试/回执丢失重放不变）
    require(base.store->adoptPlayer(9, "", 7003, 7, 2), "player 9 adopt failed");
    {
        SettlementLedger ledger;
        ledger.begin({9}, 9001);  // 账本不持锚点解析（资料归属在 Base，唯一落账路径 = 业务请求）
        std::uint64_t seq = 100;
        ledger.set_business_sink([&](std::uint64_t player_id, std::int64_t score,
                                     std::uint64_t settlement_id) {
            return wire_sink(dial, seq, player_id, score, settlement_id);
        });
        require(ledger.apply(9, 80) == SettlementLedger::ApplyError::Ok,
                "ledger business apply not Ok");
        require(ledger.apply(9, 80) == SettlementLedger::ApplyError::AlreadyApplied,
                "ledger double apply not idempotent");
        const auto avatar = base.store->findAvatar(9);
        require(avatar != nullptr && avatar->progress("exp") == 80,
                "ledger business credit did not land exactly once");
        const auto snap = ledger.snapshot();
        require(snap.applied == 1 && snap.pending == 0 && snap.complete,
                "ledger snapshot wrong after business apply");
        // 回执丢失重放（同结算 ID、新 request_id）：Base 幂等——受理不双记
        const auto dup = fire_credit(dial, 101, 9, 80, 2, 9001);
        require(dup.accepted && dup.duplicate && dup.settlement_id == 9001,
                "replayed settlement id not flagged duplicate");
        require(avatar->progress("exp") == 80,
                "replayed settlement id double credited exp");
    }

    // ---- 5. D12「database 已写、回执丢失 → 同结算 ID 重试不重复」持久闭环 ----
    //（L132 结算幂等核心：环随档案落盘 = 与 exp 同一持久单元，重放命中不双记）
    require(base.store->adoptPlayer(11, "", 7004, 8, 2), "player 11 adopt failed");
    {
        const auto first = fire_credit(dial, 300, 11, 100, 2, 4242);
        require(first.accepted && !first.duplicate && first.settlement_id == 4242,
                "first settlement credit not accepted");
        const auto avatar = base.store->findAvatar(11);
        require(avatar != nullptr && avatar->progress("exp") == 100,
                "first settlement did not land exp");
        base.store->flushDirtyAvatars("test_d12");  // 环随档落盘（同一持久单元）
        // 回执丢失重放（新 request_id、同结算 ID）→ duplicate，exp 不双记
        const auto replay = fire_credit(dial, 301, 11, 100, 2, 4242);
        require(replay.accepted && replay.duplicate && replay.settlement_id == 4242,
                "lost-reply replay not flagged duplicate");
        require(avatar->progress("exp") == 100,
                "lost-reply replay double credited exp");
        // 持久断言：档案 JSON 携带环（Base 重启后同结算 ID 重放仍不双记）
        std::ifstream in((files.path / "players" / "player_11.json").string());
        const std::string profile((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        require(profile.find("4242") != std::string::npos,
                "settled ring not persisted to profile json");
        // 迁移接权（937 adopt 同口径）：档案 JSON → 新域锚点播种环 → 重放不双记
        require(base.store->adoptPlayer(11, profile, 8001, 8, 2),
                "player 11 re-adopt failed");
        const auto replay2 = fire_credit(dial, 302, 11, 100, 2, 4242);
        require(replay2.accepted && replay2.duplicate,
                "re-adopted avatar re-credited lost-reply replay");
        require(avatar->progress("exp") == 100,
                "re-adopted avatar double credited exp");
    }

    // ---- 6. 受理面停 → AvatarUnavailable 挂账（分值保留不丢）----
    {
        SettlementLedger ledger;
        ledger.begin({10});
        std::uint64_t seq = 200;
        ledger.set_business_sink([&](std::uint64_t player_id, std::int64_t score,
                                     std::uint64_t settlement_id) {
            return wire_sink(dial, seq, player_id, score, settlement_id);
        });
        base.store->stop();  // Base 受理面退场（真死亡拓扑：面关）
        require(ledger.apply(10, 90) ==
                    SettlementLedger::ApplyError::AvatarUnavailable,
                "ledger apply on down base face not deferred");
        require(ledger.snapshot().pending == 1 && ledger.snapshot().applied == 0,
                "deferred settlement disturbed ledger state");
    }

    dial.shutdown();
}

} // namespace

int main() {
#if defined(_WIN32)
    std::cout << "SKIP: base business wire requires POSIX TCP\n";
    return 0;
#else
    try {
        base_business_wire();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Base business wire passed\n";
    return 0;
#endif
}
