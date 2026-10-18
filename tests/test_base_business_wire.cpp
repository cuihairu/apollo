// Base 业务受理面（958/959）真实进程链验收（P1 基础业务与结算迁入）：
// 独立 PlayerStore 宿主（M1 受理面，StoreHost 纯 store 位——无报告面）+
// StoreDial 真拨（同 migration 族底座，握手只验 route_version）：
//   1. 常规落账：adoptPlayer 建锚 → 958 → accepted=1，锚点 exp 累入分值
//      （progress 读数断言，非日志——与 AnchorRewardSink 语义对齐）；
//   2. 无锚点玩家 → accepted=0（挂账语义，不抛不崩）；
//   3. 冻结（933 真拨 → Transferring）后 958 → accepted=0（迟到结算
//      拒绝——跨域双记防线）；
//   4. SettlementLedger 业务受理装配：sink = 958/959 wire → apply Ok +
//      双投 AlreadyApplied（幂等归调用方账本，二次不达 wire）；
//   5. 受理面停 → AnchorUnavailable 挂账（传输不可达同挂账语义）。

#include "player_store/business_wire.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store/player_store.hpp"
#include "player_store/store_dial.hpp"
#include "squad_instance/settlement_ledger.hpp"
#include "store_host.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
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
                                        std::uint32_t zone) {
    business::ScoreCreditRequest req;
    req.request_id = request_id;
    req.player_id = player_id;
    req.score = score;
    req.source_zone_id = zone;
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
                                       std::int64_t score) {
    business::ScoreCreditRequest req;
    req.request_id = ++seq;
    req.player_id = player_id;
    req.score = score;
    req.source_zone_id = 2;
    const auto reply = dial.call(business::encodeFrame(req));
    business::ScoreCreditReply rep;
    apollo::protocol::SessionID sid = 0;
    if (!reply || !business::decodeFrame(*reply, rep, sid)) {
        return SettlementLedger::ApplyError::AnchorUnavailable;  // 传输不可达
    }
    return rep.accepted ? SettlementLedger::ApplyError::Ok
                        : SettlementLedger::ApplyError::AnchorUnavailable;
}

void base_business_wire() {
    TemporaryDirectory files;

    player_store::PlayerStoreConfig store_config;
    store_config.host = "127.0.0.1";
    store_config.m1Enabled = true;
    store_config.m1Port = 0;  // 内核分配（拨号面读回）
    store_config.m1SelfName = "baseServer";
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
        require(rep.accepted, "score credit on live anchor not accepted");
        const auto anchor = base.store->findAnchor(7);
        require(anchor != nullptr, "player 7 anchor missing after credit");
        require(anchor->progress("exp") == 120,
                "credited score did not land on anchor exp");
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
        require(!rep.accepted, "score credit on transferring anchor accepted");
    }

    // ---- 4. SettlementLedger 业务受理装配：Ok + 幂等（双投不双记）----
    require(base.store->adoptPlayer(9, "", 7003, 7, 2), "player 9 adopt failed");
    {
        SettlementLedger ledger;
        ledger.begin({9}, nullptr);  // World 无锚点可解析（资料归属在 Base）
        std::uint64_t seq = 100;
        ledger.set_business_sink([&](std::uint64_t player_id, std::int64_t score) {
            return wire_sink(dial, seq, player_id, score);
        });
        require(ledger.apply(9, 80) == SettlementLedger::ApplyError::Ok,
                "ledger business apply not Ok");
        require(ledger.apply(9, 80) == SettlementLedger::ApplyError::AlreadyApplied,
                "ledger double apply not idempotent");
        const auto anchor = base.store->findAnchor(9);
        require(anchor != nullptr && anchor->progress("exp") == 80,
                "ledger business credit did not land exactly once");
        const auto snap = ledger.snapshot();
        require(snap.applied == 1 && snap.pending == 0 && snap.complete,
                "ledger snapshot wrong after business apply");
    }

    // ---- 5. 受理面停 → AnchorUnavailable 挂账（分值保留不丢）----
    {
        SettlementLedger ledger;
        ledger.begin({10}, nullptr);
        std::uint64_t seq = 200;
        ledger.set_business_sink([&](std::uint64_t player_id, std::int64_t score) {
            return wire_sink(dial, seq, player_id, score);
        });
        base.store->stop();  // Base 受理面退场（真死亡拓扑：面关）
        require(ledger.apply(10, 90) ==
                    SettlementLedger::ApplyError::AnchorUnavailable,
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
