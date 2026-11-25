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

// D19 真实新旧业务解码兼容（958/959 字段演进样本：请求+回执加
// settlement_id、回执加 duplicate；旧形 = 4 字段）——真拨真 Base 受理面：
//   1. 旧写新读：旧形请求帧新端解码受理（settlement_id=0 落「旧口径不记
//      重放」环语义）；旧形回执帧新端解码受理（duplicate=false）；
//   2. 新写旧读：新形帧对旧端实现快照（decodeFrameLegacyStrict：公共前部
//      + done()）整帧拒——不静默截断、不误受理；
//   3. 未升级客户端（旧形请求 → 新 Base）：回执按请求帧形回带旧形（旧端
//      严格解码整帧通过）；exp 照落；重复旧形请求不误判 duplicate（0=旧
//      口径不记重放，逐笔照落）；冻结（Transferring）与无锚点状态转换
//      回旧形 accepted=0（挂账语义不变）；
//   4. 帧纪律：旧形 body 尾部未知余字节（未来形）新端整帧拒，不默认吞。
void base_business_wire_d19_compat() {
    TemporaryDirectory files;

    player_store::PlayerStoreConfig store_config;
    store_config.host = "127.0.0.1";
    store_config.m1Enabled = true;
    store_config.m1Port = 0;
    store_config.m1SelfName = "base";
    store_config.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_config.dataDir = (files.path / "players").string();
    store_config.journalPath = (files.path / "journal/persist.log").string();

    StoreHost base(store_config, 0, 0, 0, 0);
    require(base.store->m1_listen_port() != 0, "d19 base store did not listen");

    player_store::StoreDial dial(APOLLO_TEST_ROUTE_JSON, "world", "base",
                                 "127.0.0.1", base.store->m1_listen_port());
    require(dial.ready(), "d19 store dial did not assemble");
    require(dial.waitUp(), "d19 store dial did not reach base face");

    require(base.store->adoptPlayer(21, "", 7101, 9, 2), "player 21 adopt failed");
    require(base.store->adoptPlayer(22, "", 7102, 10, 2), "player 22 adopt failed");

    // ---- 1. 旧写新读（帧级：旧形字节 → 新端解码器）----
    {
        business::ScoreCreditRequest legacy_req;
        legacy_req.request_id = 1;
        legacy_req.player_id = 21;
        legacy_req.score = 40;
        legacy_req.source_zone_id = 2;
        legacy_req.settlement_id = 999;  // 编码面丢弃（旧形无此字段）
        const auto legacy_frame = business::encodeFrameLegacy(legacy_req);
        business::ScoreCreditRequest parsed;
        apollo::protocol::SessionID sid = 0;
        require(business::decodeFrame(legacy_frame, parsed, sid),
                "legacy request frame rejected by new decoder");
        require(parsed.request_id == 1 && parsed.player_id == 21 &&
                    parsed.score == 40 && parsed.source_zone_id == 2 &&
                    parsed.settlement_id == 0,
                "legacy request decoded fields wrong (settlement_id must default 0)");
        // 旧端快照对旧形帧照常受理（两侧互通基线）
        business::ScoreCreditRequest legacy_parsed;
        require(business::decodeFrameLegacyStrict(legacy_frame, legacy_parsed, sid),
                "legacy request frame rejected by legacy snapshot decoder");
    }
    {
        business::ScoreCreditReply legacy_rep;
        legacy_rep.request_id = 1;
        legacy_rep.player_id = 21;
        legacy_rep.accepted = true;
        legacy_rep.settlement_id = 777;  // 编码面丢弃
        legacy_rep.duplicate = true;     // 编码面丢弃
        const auto legacy_frame = business::encodeFrameLegacy(legacy_rep);
        business::ScoreCreditReply parsed;
        apollo::protocol::SessionID sid = 0;
        require(business::decodeFrame(legacy_frame, parsed, sid),
                "legacy reply frame rejected by new decoder");
        require(parsed.accepted && parsed.settlement_id == 0 && !parsed.duplicate,
                "legacy reply decoded fields wrong (defaults must be 0/false)");
    }

    // ---- 2. 新写旧读（帧级：新形字节 → 旧端实现快照整帧拒）----
    {
        business::ScoreCreditRequest req;
        req.request_id = 2;
        req.player_id = 21;
        req.score = 10;
        req.source_zone_id = 2;
        req.settlement_id = 777;
        const auto frame = business::encodeFrame(req);
        business::ScoreCreditRequest parsed;
        apollo::protocol::SessionID sid = 0;
        require(!business::decodeFrameLegacyStrict(frame, parsed, sid),
                "new request frame must be rejected by legacy snapshot decoder");

        business::ScoreCreditReply rep;
        rep.request_id = 2;
        rep.player_id = 21;
        rep.accepted = true;
        rep.settlement_id = 777;
        business::ScoreCreditReply rep_parsed;
        require(!business::decodeFrameLegacyStrict(business::encodeFrame(rep),
                                                   rep_parsed, sid),
                "new reply frame must be rejected by legacy snapshot decoder");
    }

    // ---- 3. 未升级客户端 × 新 Base（真拨：旧形请求 → 旧形回执回带）----
    {
        business::ScoreCreditRequest legacy_req;
        legacy_req.request_id = 3;
        legacy_req.player_id = 21;
        legacy_req.score = 40;
        legacy_req.source_zone_id = 2;
        const auto reply = dial.call(business::encodeFrameLegacy(legacy_req));
        require(reply.has_value(), "d19 legacy credit wire: no reply");
        // 回执是旧形：新端与旧端快照都整帧解过
        business::ScoreCreditReply rep;
        apollo::protocol::SessionID sid = 0;
        require(business::decodeFrame(*reply, rep, sid),
                "legacy-echoed reply rejected by new decoder");
        business::ScoreCreditReply legacy_rep;
        apollo::protocol::SessionID legacy_sid = 0;
        require(business::decodeFrameLegacyStrict(*reply, legacy_rep, legacy_sid),
                "legacy-echoed reply rejected by legacy snapshot decoder");
        require(legacy_sid == sid, "legacy-echoed reply session lost");
        require(rep.accepted && !rep.duplicate && rep.settlement_id == 0,
                "legacy credit reply fields wrong");
        require(rep.request_id == 3 && rep.player_id == 21,
                "legacy credit reply correlation broken");
        const auto avatar = base.store->findAvatar(21);
        require(avatar != nullptr && avatar->progress("exp") == 40,
                "legacy credit did not land exp");
        // 旧口径无稳定结算 ID：重复旧形请求逐笔照落（0=不记重放环）
        legacy_req.request_id = 4;
        legacy_req.score = 5;
        const auto reply2 = dial.call(business::encodeFrameLegacy(legacy_req));
        require(reply2.has_value(), "d19 legacy repeat wire: no reply");
        business::ScoreCreditReply rep2;
        require(business::decodeFrame(*reply2, rep2, sid),
                "legacy repeat reply undecodable");
        require(rep2.accepted && !rep2.duplicate,
                "legacy repeat wrongly flagged duplicate");
        require(avatar->progress("exp") == 45,
                "legacy repeat did not accumulate (old-style must not dedupe)");
        // 新形请求回执仍是全字段（回带按帧形判不按值判——settlement_id=0
        // 的新形请求也得全字段回执）
        business::ScoreCreditRequest new_req;
        new_req.request_id = 5;
        new_req.player_id = 21;
        new_req.score = 1;
        new_req.source_zone_id = 2;  // settlement_id = 0（显式旧值、新形帧）
        const auto reply3 = dial.call(business::encodeFrame(new_req));
        require(reply3.has_value(), "d19 zero-id new-shape wire: no reply");
        business::ScoreCreditReply rep3;
        require(business::decodeFrame(*reply3, rep3, sid) && rep3.settlement_id == 0,
                "zero-id new-shape reply undecodable or fields wrong");
        require(!business::decodeFrameLegacyStrict(*reply3, rep3, sid),
                "zero-id new-shape reply must carry full fields (frame-shape echo)");
    }

    // ---- 3b. 状态转换兼容：冻结（Transferring）与无锚点 → 旧形 accepted=0 ----
    {
        migration::FreezeRequest freeze;
        freeze.player_id = 22;
        freeze.request_id = 11;
        auto fr_reply = dial.call(migration::encodeFrame(freeze));
        require(fr_reply.has_value(), "d19 freeze wire: no reply");
        migration::FreezeReply fr;
        apollo::protocol::SessionID sid = 0;
        require(migration::decodeFrame(*fr_reply, fr, sid) && fr.accepted,
                "d19 freeze on player 22 not accepted");

        business::ScoreCreditRequest legacy_req;
        legacy_req.request_id = 6;
        legacy_req.player_id = 22;
        legacy_req.score = 30;
        legacy_req.source_zone_id = 2;
        auto reply = dial.call(business::encodeFrameLegacy(legacy_req));
        require(reply.has_value(), "d19 frozen legacy wire: no reply");
        business::ScoreCreditReply rep;
        require(business::decodeFrame(*reply, rep, sid) && !rep.accepted &&
                    !rep.error_message.empty(),
                "frozen avatar legacy credit must defer (accepted=0 with reason)");
        require(business::decodeFrameLegacyStrict(*reply, rep, sid),
                "frozen legacy reply must stay legacy-shaped");

        legacy_req.request_id = 7;
        legacy_req.player_id = 31337;  // 无锚点
        reply = dial.call(business::encodeFrameLegacy(legacy_req));
        require(reply.has_value(), "d19 unknown legacy wire: no reply");
        require(business::decodeFrame(*reply, rep, sid) && !rep.accepted,
                "unknown player legacy credit must defer");
    }

    // ---- 3c. 新 World × 新 Base：全字段回执 + settlement_id 幂等照旧 ----
    {
        business::ScoreCreditRequest req;
        req.request_id = 8;
        req.player_id = 21;
        req.score = 20;
        req.source_zone_id = 2;
        req.settlement_id = 777;
        auto reply = dial.call(business::encodeFrame(req));
        require(reply.has_value(), "d19 new-shape wire: no reply");
        business::ScoreCreditReply rep;
        apollo::protocol::SessionID sid = 0;
        require(business::decodeFrame(*reply, rep, sid) && rep.accepted &&
                    !rep.duplicate && rep.settlement_id == 777,
                "new-shape credit reply fields wrong");
        business::ScoreCreditReply legacy_rep;
        require(!business::decodeFrameLegacyStrict(*reply, legacy_rep, sid),
                "new-shape reply must be rejected by legacy snapshot decoder");
        const auto avatar = base.store->findAvatar(21);
        require(avatar != nullptr && avatar->progress("exp") == 66,
                "new-shape credit did not land exp");
        // 同结算 ID 重放 → duplicate 不双记（既有 L132 语义不回归）
        req.request_id = 9;
        reply = dial.call(business::encodeFrame(req));
        require(reply.has_value() && business::decodeFrame(*reply, rep, sid) &&
                    rep.accepted && rep.duplicate,
                "settlement replay not flagged duplicate");
        require(avatar->progress("exp") == 66, "settlement replay double credited");
    }

    // ---- 4. 帧纪律：旧形 body 尾部未知余字节（未来形）整帧拒 ----
    {
        business::ScoreCreditRequest legacy_req;
        legacy_req.request_id = 10;
        legacy_req.player_id = 21;
        legacy_req.score = 1;
        legacy_req.source_zone_id = 2;
        auto body = business::encodeBodyLegacy(legacy_req);
        body.push_back(0xAB);
        body.push_back(0xCD);  // 未知未来字段残片（header.length 计入）
        const auto frame =
            business::encodeLegacyFrame(std::move(body), business::ScoreCreditRequest::TYPE, 0);
        business::ScoreCreditRequest parsed;
        apollo::protocol::SessionID sid = 0;
        require(!business::decodeFrame(frame, parsed, sid),
                "unknown trailing bytes must be rejected (no silent truncation)");
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
        base_business_wire_d19_compat();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Base business wire passed\n";
    return 0;
#endif
}
