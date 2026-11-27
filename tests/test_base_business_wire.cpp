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

#include "apollo/game/social/party.hpp"
#include "apollo/game/world/world.hpp"
#include "online_directory/online_directory.hpp"
#include "player_store/business_wire.hpp"
#include "player_store/migration_wire.hpp"
#include "player_store/player_store.hpp"
#include "player_store/settlement_base_router.hpp"
#include "player_store/store_dial.hpp"
#include "squad_instance/settlement_id.hpp"
#include "squad_instance/settlement_ledger.hpp"
#include "squad_instance/settlement_record_store.hpp"
#include "squad_instance/squad_instance.hpp"
#include "store_host.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
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

// D12 逐 Base 结算选路（同区多 Base 部分成功切片）真链路：真 OnlineDirectory
// M1 控制面（917/918 分派直通）× 两独立 StoreHost 异端点（A/B）——
//   1. 落点行带归属 Base 端点（assignWorld 真裁决）：登记玩家经 917/918 解析
//      后逐玩家拨 958——A 归属玩家落 A、B 归属玩家落 B，exp 各归各账；
//   2. 同玩家重发命中端点缓存（不重解析）；目录无册玩家落单 Base 兜底；
//   3. 端点迁移补齐：归属 Base 停面 → 首拨失败（挂账语义）；目录改指后重
//      拨新端点落账（失败已清缓存——重解析生效）；同 settlement_id 重放 Base
//      幂等不双记；
//   4. 旧口径零回归：目录面未配（port 0）= 恒走单 Base 兜底，异端锚点
//      误投被 Base 锚点校验拒绝（跨 Base 双记防线——不幻值不落账）。
void settlement_base_router_multi_base() {
    TemporaryDirectory files;
    player_store::PlayerStoreConfig store_a;
    store_a.host = "127.0.0.1";
    store_a.m1Enabled = true;
    store_a.m1Port = 0;
    store_a.m1SelfName = "base";
    store_a.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_a.dataDir = (files.path / "a/players").string();
    store_a.journalPath = (files.path / "a/journal/persist.log").string();
    StoreHost base_a(store_a, 0, 0, 0, 0);
    require(base_a.store->m1_listen_port() != 0, "router base A did not listen");

    player_store::PlayerStoreConfig store_b;
    store_b.host = "127.0.0.1";
    store_b.m1Enabled = true;
    store_b.m1Port = 0;
    store_b.m1SelfName = "base";
    store_b.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_b.dataDir = (files.path / "b/players").string();
    store_b.journalPath = (files.path / "b/journal/persist.log").string();
    StoreHost base_b(store_b, 0, 0, 0, 0);
    require(base_b.store->m1_listen_port() != 0, "router base B did not listen");

    // 真 OnlineDirectory + M1 控制面（ephemeral；drain 未置——assign 受理）。
    online_directory::OnlineDirectory directory(0);
    directory.set_m1_face(0, APOLLO_TEST_ROUTE_JSON);
    directory.start();
    require(directory.listenPort() != 0, "router directory face did not listen");

    // 锚点分置两 Base（101/103 在 A；104/102 在 B——102 随迁移演进入籍 B）。
    require(base_a.store->adoptPlayer(101, "", 7101, 5, 2), "adopt 101 failed");
    require(base_a.store->adoptPlayer(103, "", 7103, 5, 2), "adopt 103 failed");
    require(base_b.store->adoptPlayer(104, "", 7104, 5, 2), "adopt 104 failed");
    require(base_b.store->adoptPlayer(102, "", 7102, 5, 2), "adopt 102 failed");

    // 目录名册：bind + assign（resolve 门槛 = binding+assignment 双全——
    // 登录实拨归属 Base 端点随落点成行；101/103 → A、104/102 → B）。
    const auto seed_assignment = [&directory](std::uint64_t player_id,
                                              const std::string& host,
                                              std::uint16_t port) {
        apollo::game::base::SessionBinding binding;
        binding.session_id = player_id;
        binding.gateway_id = 1;
        binding.gateway_addr = "tcp://127.0.0.1:9002";
        binding.session_version = 1;
        require(directory.bindSession(player_id, binding), "router bind failed");
        apollo::game::base::WorldAssignment assignment;
        assignment.world_id = 2;
        assignment.space_id = 1;
        assignment.route_version = 1;
        assignment.base_host = host;
        assignment.base_port = port;
        require(directory.assignWorld(player_id, assignment), "router assign failed");
    };
    seed_assignment(101, "127.0.0.1", base_a.store->m1_listen_port());
    seed_assignment(103, "127.0.0.1", base_a.store->m1_listen_port());
    seed_assignment(104, "127.0.0.1", base_b.store->m1_listen_port());
    seed_assignment(102, "127.0.0.1", base_b.store->m1_listen_port());

    player_store::SettlementBaseRouter::Config config;
    config.route_json = APOLLO_TEST_ROUTE_JSON;
    config.self_name = "world";
    config.default_host = "127.0.0.1";
    config.default_port = base_a.store->m1_listen_port();  // 单 Base 兜底 = A
    config.directory_host = "127.0.0.1";
    config.directory_port = directory.listenPort();
    player_store::SettlementBaseRouter router(std::move(config));

    std::uint64_t seq = 500;
    const auto make_req = [&seq](std::uint64_t player_id, std::int64_t score,
                                 std::uint64_t settlement_id) {
        business::ScoreCreditRequest req;
        req.request_id = ++seq;
        req.player_id = player_id;
        req.score = score;
        req.source_zone_id = 2;
        req.settlement_id = settlement_id;
        return req;
    };

    // ---- 1. 逐玩家选路：A 归属落 A、B 归属落 B（不串账）----
    require(router.send(101, make_req(101, 120, 9001)),
            "router per-player credit to A not accepted");
    require(router.send(104, make_req(104, 90, 9002)),
            "router per-player credit to B not accepted");
    {
        const auto avatar_a = base_a.store->findAvatar(101);
        require(avatar_a != nullptr && avatar_a->progress("exp") == 120,
                "player 101 exp did not land on base A");
        require(base_a.store->findAvatar(104) == nullptr,
                "player 104 anchor leaked onto base A");
        const auto avatar_b = base_b.store->findAvatar(104);
        require(avatar_b != nullptr && avatar_b->progress("exp") == 90,
                "player 104 exp did not land on base B");
        require(base_b.store->findAvatar(101) == nullptr,
                "player 101 anchor leaked onto base B");
    }
    require(router.resolve_count() == 2, "first sends did not resolve via 917/918");

    // ---- 2. 端点缓存命中不重解析；目录无册玩家落单 Base 兜底 ----
    require(router.send(101, make_req(101, 30, 9003)),
            "router cached credit to A not accepted");
    require(router.resolve_count() == 2 && router.cache_hit_count() == 1,
            "known player re-resolved directory instead of cache hit");
    // 103 无册：解析 miss（计数涨）落兜底 A——不入缓存（目录缺席非归属）
    require(router.send(103, make_req(103, 40, 9004)),
            "router unassigned player fallback not accepted");
    {
        const auto avatar_a = base_a.store->findAvatar(101);
        require(avatar_a != nullptr && avatar_a->progress("exp") == 150,
                "player 101 exp accumulation broken");
        const auto avatar_103 = base_a.store->findAvatar(103);
        require(avatar_103 != nullptr && avatar_103->progress("exp") == 40,
                "fallback credit did not land on default base A");
    }

    // ---- 3. 端点迁移补齐：B 停面 → 首拨挂账；目录改指 A 后重拨落账；
    //         同 settlement_id 重放 Base 幂等不双记 ----
    base_b.store->stop();  // B 归属端点退场（真死亡拓扑：面关）
    require(!router.send(102, make_req(102, 60, 9005)),
            "credit on down base endpoint reported accepted");
    // 目录改指（端点迁移完成：归属 B → A）
    {
        apollo::game::base::WorldAssignment moved;
        moved.world_id = 2;
        moved.space_id = 1;
        moved.route_version = 2;
        moved.base_host = "127.0.0.1";
        moved.base_port = base_a.store->m1_listen_port();
        require(directory.assignWorld(102, moved), "router re-assign failed");
        require(base_a.store->adoptPlayer(102, "", 7102, 5, 2),
                "re-adopt 102 on A failed");
    }
    require(router.send(102, make_req(102, 60, 9005)),
            "router moved player credit not accepted after re-assign");
    require(router.send(102, make_req(102, 60, 9005)),
            "router moved player replay not accepted");
    {
        const auto avatar = base_a.store->findAvatar(102);
        require(avatar != nullptr && avatar->progress("exp") == 60,
                "moved player 102 exp wrong on A (missing or double credited)");
        const auto avatar_b = base_b.store->findAvatar(102);
        require(avatar_b == nullptr || avatar_b->progress("exp") == 0,
                "player 102 credited on the dead/old base B");
    }

    // ---- 4. 旧口径零回归：目录面未配 = 恒单 Base 兜底；异端锚点误投被
    //         Base 锚点校验拒（跨 Base 双记防线）----
    player_store::SettlementBaseRouter::Config single;
    single.route_json = APOLLO_TEST_ROUTE_JSON;
    single.self_name = "world";
    single.default_host = "127.0.0.1";
    single.default_port = base_a.store->m1_listen_port();
    player_store::SettlementBaseRouter single_router(std::move(single));
    require(single_router.send(103, make_req(103, 20, 9006)),
            "single-base fallback credit not accepted");
    require(single_router.resolve_count() == 1,
            "single-base fallback attempted directory resolution");
    // 104 归属 B（锚点不在 A）：兜底端 A 拒（accepted=0）——不幻值不落账
    require(!single_router.send(104, make_req(104, 20, 9007)),
            "misrouted credit on A accepted despite anchor on B");
    {
        const auto avatar = base_a.store->findAvatar(103);
        require(avatar != nullptr && avatar->progress("exp") == 60,
                "single-base fallback accumulation broken");
        require(base_a.store->findAvatar(104) == nullptr,
                "misrouted credit created phantom anchor on A");
    }

    directory.stop();
}

// D12 #2 服务级多 Base 部分成功真链路（L133 编排切片）：SquadInstanceService
// 装配真 SettlementBaseRouter 业务缝（生产 CellApp 同款——958 带稳定结算 ID
// 经 917/918 逐玩家拨归属 Base）× 两独立 StoreHost × 真 OnlineDirectory ×
// 恢复记录面（D13）：
//   1. 多 Base 分别确认：B 归属成员先落 B 逐玩家选路，恢复记录逐成员记
//      intent+confirm——各 Base 各自收口，互不等待；
//   2. 部分成功：B 停面 → A 归属成员照常落 A 独立确认（B 故障不拖 A），
//      B 归属成员挂账（AvatarUnavailable）——意图先于业务面落记录，挂账
//      不丢应发分值；
//   3. 账本补账纪律：retry 只重提挂账成员——已确认成员不再发业务请求
//      （缝调用计数不前进）；
//   4. 恢复补齐：端点迁移（目录改指 + 重锚）后周期重驱动（记录面路径，
//      跨重启语义）补齐挂账成员——全部确认记录 done 删除；账本路径随后
//      重提同结算 ID，Base 去重环收口不双记。
void squad_settlement_multi_base_partial_success() {
    TemporaryDirectory files;

    player_store::PlayerStoreConfig store_a;
    store_a.host = "127.0.0.1";
    store_a.m1Enabled = true;
    store_a.m1Port = 0;
    store_a.m1SelfName = "base";
    store_a.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_a.dataDir = (files.path / "a/players").string();
    store_a.journalPath = (files.path / "a/journal/persist.log").string();
    StoreHost base_a(store_a, 0, 0, 0, 0);
    require(base_a.store->m1_listen_port() != 0, "settlement base A did not listen");

    player_store::PlayerStoreConfig store_b;
    store_b.host = "127.0.0.1";
    store_b.m1Enabled = true;
    store_b.m1Port = 0;
    store_b.m1SelfName = "base";
    store_b.m1RouteJson = APOLLO_TEST_ROUTE_JSON;
    store_b.dataDir = (files.path / "b/players").string();
    store_b.journalPath = (files.path / "b/journal/persist.log").string();
    StoreHost base_b(store_b, 0, 0, 0, 0);
    require(base_b.store->m1_listen_port() != 0, "settlement base B did not listen");

    online_directory::OnlineDirectory directory(0);
    directory.set_m1_face(0, APOLLO_TEST_ROUTE_JSON);
    directory.start();
    require(directory.listenPort() != 0, "settlement directory face did not listen");

    constexpr std::uint64_t kLeader = 100;   // 归属 A
    constexpr std::uint64_t kMemberB = 101;  // 归属 B
    constexpr std::uint64_t kMemberC = 102;  // 归属 B

    require(base_a.store->adoptPlayer(kLeader, "", 7100, 5, 2),
            "settlement adopt leader failed");
    require(base_b.store->adoptPlayer(kMemberB, "", 7101, 5, 2),
            "settlement adopt member B failed");
    require(base_b.store->adoptPlayer(kMemberC, "", 7102, 5, 2),
            "settlement adopt member C failed");

    const auto seed_assignment = [&directory](std::uint64_t player_id,
                                              const std::string& host,
                                              std::uint16_t port) {
        apollo::game::base::SessionBinding binding;
        binding.session_id = player_id;
        binding.gateway_id = 1;
        binding.gateway_addr = "tcp://127.0.0.1:9002";
        binding.session_version = 1;
        require(directory.bindSession(player_id, binding), "settlement bind failed");
        apollo::game::base::WorldAssignment assignment;
        assignment.world_id = 2;
        assignment.space_id = 1;
        assignment.route_version = 1;
        assignment.base_host = host;
        assignment.base_port = port;
        require(directory.assignWorld(player_id, assignment),
                "settlement assign failed");
    };
    seed_assignment(kLeader, "127.0.0.1", base_a.store->m1_listen_port());
    seed_assignment(kMemberB, "127.0.0.1", base_b.store->m1_listen_port());
    seed_assignment(kMemberC, "127.0.0.1", base_b.store->m1_listen_port());

    player_store::SettlementBaseRouter::Config config;
    config.route_json = APOLLO_TEST_ROUTE_JSON;
    config.self_name = "world";
    config.default_host = "127.0.0.1";
    config.default_port = base_a.store->m1_listen_port();  // 单 Base 兜底 = A
    config.directory_host = "127.0.0.1";
    config.directory_port = directory.listenPort();
    player_store::SettlementBaseRouter router(std::move(config));

    // 服务装配（受理即开账前完成）：稳定结算 ID 盐 + 恢复记录面 + 真路由
    // 业务缝（生产 CellApp 同款：958 带结算 ID，router false = 挂账）。
    apollo::game::social::PartyManager parties;
    apollo::game::world::World world;
    auto* party = parties.create(1, kLeader);
    party->add_member(kMemberB);
    party->add_member(kMemberC);
    squad_instance::SquadInstanceService service(parties, &world, 1);
    service.register_world(1, 4);
    service.set_settlement_id_salt(9);
    service.set_settlement_record_dir((files.path / "records").string());

    std::uint64_t seq = 800;
    std::map<std::uint64_t, std::uint64_t> sink_calls;
    service.set_business_sink(
        [&](std::uint64_t player_id, std::int64_t score,
            std::uint64_t settlement_id) {
            ++sink_calls[player_id];
            business::ScoreCreditRequest req;
            req.request_id = ++seq;
            req.player_id = player_id;
            req.score = score;
            req.source_zone_id = 2;
            req.settlement_id = settlement_id;
            return router.send(player_id, req)
                       ? SettlementLedger::ApplyError::Ok
                       : SettlementLedger::ApplyError::AvatarUnavailable;
        });

    squad_instance::SettlementRecordStore records((files.path / "records").string());

    squad_instance::LaunchRequest request;
    request.party_id = 1;
    request.requester_id = kLeader;
    request.map_id = 42;
    request.map_name = "settlement-map";
    const auto launch = service.launch(request);
    require(launch.error == squad_instance::LaunchError::Ok,
            "settlement service launch refused");
    const auto sid = squad_instance::compose_settlement_id(9, launch.instance_id);
    require(sid != 0, "settlement id composed to zero");

    // ---- 1. 多 Base 分别确认：B 归属成员落 B（逐玩家选路），记录记
    //         intent+confirm——A 面零锚点零分值 ----
    require(service.credit_settlement(1, kMemberB, 30) ==
                SettlementLedger::ApplyError::Ok,
            "member B credit via router not accepted");
    {
        const auto avatar = base_b.store->findAvatar(kMemberB);
        require(avatar != nullptr && avatar->progress("exp") == 30,
                "member B exp did not land on base B");
        require(base_a.store->findAvatar(kMemberB) == nullptr,
                "member B anchor leaked onto base A");
        const auto snap = service.settlement(1);
        require(snap.has_value() && snap->applied == 1 && !snap->complete,
                "settlement ledger did not open with roster");
        const auto loaded = records.load_all();
        require(loaded.size() == 1 &&
                    loaded[0].lifecycle == squad_instance::SettlementLifecycle::Pending &&
                    loaded[0].intent.at(kMemberB) == 30 &&
                    loaded[0].confirmed.count(kMemberB) == 1,
                "record did not capture member B intent+confirm");
    }

    // ---- 2. 部分成功：B 停面——A 成员独立确认不拖带；B 成员挂账不丢
    //         意图（intent 先于业务面落记录）----
    base_b.store->stop();
    require(service.credit_settlement(1, kLeader, 50) ==
                SettlementLedger::ApplyError::Ok,
            "leader credit dragged by down base B");
    require(service.credit_settlement(1, kMemberC, 20) ==
                SettlementLedger::ApplyError::AvatarUnavailable,
            "member C credit on down base reported accepted");
    {
        const auto avatar = base_a.store->findAvatar(kLeader);
        require(avatar != nullptr && avatar->progress("exp") == 50,
                "leader exp did not land on base A");
        require(base_b.store->findAvatar(kLeader) == nullptr,
                "leader anchor leaked onto base B");
        require(base_a.store->findAvatar(kMemberC) == nullptr,
                "pending member C credited on base A early");
        const auto snap = service.settlement(1);
        require(snap.has_value() && snap->applied == 2 && snap->pending == 1 &&
                    !snap->complete,
                "partial settlement ledger state wrong");
        const auto loaded = records.load_all();
        require(loaded.size() == 1 && loaded[0].intent.size() == 3 &&
                    loaded[0].intent.at(kMemberC) == 20 &&
                    loaded[0].confirmed.count(kMemberC) == 0,
                "pending member intent not preserved while base down");
    }

    // ---- 3. 账本补账纪律：retry 只重提挂账成员——已确认成员缝调用计数
    //         不前进（不重发业务请求）----
    require(service.retry_settlement(1) == 0,
            "retry against down base reported progress");
    require(sink_calls.at(kLeader) == 1 && sink_calls.at(kMemberB) == 1 &&
                sink_calls.at(kMemberC) == 2,
            "retry re-sent confirmed members instead of pending only");

    // ---- 4. 恢复补齐：端点迁移（目录改指 + 重锚）→ 周期重驱动补齐不
    //         双记 → 记录 done 删除；账本路径重提命中 Base 去重环 ----
    {
        apollo::game::base::WorldAssignment moved;
        moved.world_id = 2;
        moved.space_id = 1;
        moved.route_version = 2;
        moved.base_host = "127.0.0.1";
        moved.base_port = base_a.store->m1_listen_port();
        require(directory.assignWorld(kMemberC, moved),
                "settlement re-assign failed");
        require(base_a.store->adoptPlayer(kMemberC, "", 7102, 5, 2),
                "re-adopt member C on A failed");
    }
    const auto redriven = service.maybe_redrive_pending(
        std::chrono::milliseconds{1000}, std::chrono::steady_clock::now(),
        /*link_ready=*/true);
    require(redriven == 1, "cadence redrive did not pick up the pending record");
    require(sink_calls.at(kMemberC) == 3,
            "redrive sent member C more than once");
    require(service.settlement_record_state(sid) == std::nullopt,
            "fully confirmed record not deleted");
    require(base_a.store->findAvatar(kMemberC) != nullptr &&
                base_a.store->findAvatar(kMemberC)->progress("exp") == 20,
            "member C exp wrong on A after redrive (missing or double credit)");
    require(base_b.store->findAvatar(kMemberB)->progress("exp") == 30,
            "member B exp changed after unrelated redrive");
    // 记录面路径不触账本：账本挂账成员由补账路径收口——同结算 ID 重提命中
    // Base 去重环（duplicate 回执照受理），分值不双记。
    require(service.retry_settlement(1) == 1,
            "ledger retry did not close the outstanding member");
    require(base_a.store->findAvatar(kMemberC)->progress("exp") == 20 &&
                base_a.store->findAvatar(kLeader)->progress("exp") == 50,
            "duplicate settlement replay double credited on base A");
    require(service.settlement(1)->complete, "settlement not complete");
    require(service.retry_settlement(1) == 0,
            "completed settlement re-credited");
    require(service.maybe_redrive_pending(
                std::chrono::milliseconds{1000},
                std::chrono::steady_clock::now() + std::chrono::seconds{2},
                /*link_ready=*/true) == 0,
            "deleted record redriven after completion");

    directory.stop();
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
        settlement_base_router_multi_base();
        squad_settlement_multi_base_partial_success();
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "Base business wire passed\n";
    return 0;
#endif
}
