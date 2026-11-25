#pragma once

// 结算分值入账受理 wire 面（P1 基础业务与结算迁入；契约 958/959）：World
// 玩法域结算以「返回业务请求」落账——锚点写权在 Base（L19 资料归属切换后
// World 不跨界持锚点指针）。拨号底座 = StoreDial（同 migration 族）；Base
// 受理面 = PlayerStore M1 dispatch（同 933/935/937 分流）。
//
// 幂等口径：重复投递去重归调用方账本（World 侧 SettlementLedger applied
// 标记 + retry 补账），Base 只受理不回放判定——与本地锚点落账同语义
// （AvatarRewardSink 幂等面 = exp 键 + mark_dirty 随 SaveQueue 落档）。
// 锚点暂不可得（离线/迁移中 Transferring）→ accepted=0，调用方挂账待补。
// header-only 直编习语（app 域件，不进 game_session 模块）。

#include "apollo/protocol/codec.hpp"
#include "apollo_contract.h"
#include "player_store/migration_wire.hpp"  // encodeFrame/decodeFrame/BodyReader 同族复用

#include <cstdint>
#include <string>
#include <vector>

namespace player_store {

namespace protocol = apollo::protocol;  // 自足：任何包含顺序可用（重复
                                        // 声明同名别名合法）

namespace business {

// 编解码原语复用 migration 族（同目录同习语）：put_*/BodyReader 均在
// player_store::migration 命名空间。
using migration::put_u32;
using migration::put_u64;
using migration::put_bool;
using migration::put_str;
using migration::BodyReader;

static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_score_credit_request) == 958);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_score_credit_reply) == 959);

// ---- 载荷结构（字段序 = 契约 958/959 字段序）----

struct ScoreCreditRequest {
    static constexpr std::uint16_t TYPE = 958;
    std::uint64_t request_id = 0;    // 调用方关联号（单次投递关联；跨重试非稳定）
    std::uint64_t player_id = 0;
    std::int64_t score = 0;          // 结算分值（入 exp 键）
    std::uint32_t source_zone_id = 0; // 发起玩法域（排查面）
    std::uint64_t settlement_id = 0; // 稳定玩法/结算 ID（重试/回执丢失重放不变；0=旧口径）
};

struct ScoreCreditReply {
    static constexpr std::uint16_t TYPE = 959;
    std::uint64_t request_id = 0;
    std::uint64_t player_id = 0;
    bool accepted = false;           // 1=已入锚点长期态；0=暂不可落（挂账待补）
    std::string error_message;
    std::uint64_t settlement_id = 0; // 回带稳定玩法/结算 ID（关联/排查面）
    bool duplicate = false;          // 1=同结算 ID 已受理的重放——不再落账
};

inline std::vector<std::uint8_t> encodeBody(const ScoreCreditRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_u64(b, static_cast<std::uint64_t>(m.score));
    put_u32(b, m.source_zone_id);
    put_u64(b, m.settlement_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const ScoreCreditReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    put_u64(b, m.settlement_id);
    put_bool(b, m.duplicate);
    return b;
}

// ---- D19 真实字段演进兼容（旧形 = 4 字段请求 / 4 字段回执）----
// 958/959 曾各演进一次：请求+回执加 settlement_id、回执加 duplicate。
// 未升级端并存期旧形字节仍须互通：
//   旧写新读：旧端帧新端解码受理——缺尾字段取默认（settlement_id=0 落
//     既有「旧口径不记重放」环语义，duplicate=false）；
//   新写旧读：新端回执按请求帧形回带（score_credit_request_legacy_shape
//     + handlePlayerScoreCreditRequest）——旧形请求得旧形回执，新形请求
//     得全字段回执；新帧对真旧端的整帧拒语义以 decodeFrameLegacyStrict
//     （旧端实现快照：公共前部 + done()）样本验证——旧端拒 → 结构化
//     ERROR → 调用方挂账待补（不假受理不崩）。

inline std::vector<std::uint8_t> encodeBodyLegacy(const ScoreCreditRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_u64(b, static_cast<std::uint64_t>(m.score));
    put_u32(b, m.source_zone_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBodyLegacy(const ScoreCreditReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    return b;
}

inline bool decodeBody(BodyReader& r, ScoreCreditRequest& m) {
    std::uint64_t score = 0;
    const bool common = r.u64(m.request_id) && r.u64(m.player_id) &&
                        r.u64(score) && r.u32(m.source_zone_id);
    if (!common) {
        return false;
    }
    m.score = static_cast<std::int64_t>(score);
    if (r.done()) {
        m.settlement_id = 0;  // 旧端帧：缺尾部稳定结算 ID（0=旧口径）
        return true;
    }
    return r.u64(m.settlement_id);
}

inline bool decodeBody(BodyReader& r, ScoreCreditReply& m) {
    const bool common = r.u64(m.request_id) && r.u64(m.player_id) &&
                        r.boolean(m.accepted) && r.str(m.error_message);
    if (!common) {
        return false;
    }
    if (r.done()) {
        // 旧端回执：无稳定结算 ID/重放标记，不污染新面
        m.settlement_id = 0;
        m.duplicate = false;
        return true;
    }
    return r.u64(m.settlement_id) && r.boolean(m.duplicate);
}

// 旧端实现快照（严格 done()）：历史旧代码对 958/959 的读法——只读公共
// 前部字段，帧尾余字节整帧拒（新写旧读样本按此口径验证）。
inline bool decodeBodyLegacyStrict(BodyReader& r, ScoreCreditRequest& m) {
    std::uint64_t score = 0;
    return r.u64(m.request_id) && r.u64(m.player_id) && r.u64(score)
           && r.u32(m.source_zone_id)
           && (m.score = static_cast<std::int64_t>(score), true);
}

inline bool decodeBodyLegacyStrict(BodyReader& r, ScoreCreditReply& m) {
    return r.u64(m.request_id) && r.u64(m.player_id) && r.boolean(m.accepted)
           && r.str(m.error_message);
}

// 请求帧形探测（回执按请求形回带）：公共前部读尽 = 旧形（无 settlement_id
// 尾字段）。按帧形判不按值判——新形请求显式 settlement_id=0 仍是新形。
inline bool score_credit_request_legacy_shape(
    const std::vector<std::uint8_t>& frame) {
    try {
        const auto header = protocol::MessageCodec::parseHeader(frame);
        if (header.type != ScoreCreditRequest::TYPE) {
            return false;
        }
        BodyReader r(frame, sizeof(protocol::MessageHeader));
        std::uint64_t a = 0, b = 0, c = 0;
        std::uint32_t d = 0;
        return r.u64(a) && r.u64(b) && r.u64(c) && r.u32(d) && r.done();
    } catch (...) {
        return false;
    }
}

// 旧形帧拼装/解析（帧头同 migration 族；body 只含旧字段）。
inline std::vector<std::uint8_t> encodeLegacyFrame(std::vector<std::uint8_t> body,
                                                   std::uint16_t type,
                                                   protocol::SessionID session_id) {
    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = type;
    header.length = static_cast<std::uint32_t>(body.size());
    header.sequence = 0;
    header.sessionId = session_id;
    auto head = protocol::MessageCodec::encodeHeader(header);
    body.insert(body.begin(), head.begin(), head.end());
    return body;
}

inline std::vector<std::uint8_t> encodeFrameLegacy(const ScoreCreditRequest& m,
                                                  protocol::SessionID session_id = 0) {
    return encodeLegacyFrame(encodeBodyLegacy(m), ScoreCreditRequest::TYPE, session_id);
}

inline std::vector<std::uint8_t> encodeFrameLegacy(const ScoreCreditReply& m,
                                                  protocol::SessionID session_id = 0) {
    return encodeLegacyFrame(encodeBodyLegacy(m), ScoreCreditReply::TYPE, session_id);
}

// 旧端实现快照读帧：历史旧 decodeFrame——公共前部 + done() 严格（余字节
// 拒）。新形帧对真旧端整帧拒的样本口径。
template <typename T, typename DecodeLegacy>
bool decodeFrameLegacyStrictImpl(const std::vector<std::uint8_t>& frame, T& out,
                                 protocol::SessionID& session_id, DecodeLegacy decode) {
    protocol::MessageHeader header;
    try {
        header = protocol::MessageCodec::parseHeader(frame);
    } catch (...) {
        return false;
    }
    if (header.type != T::TYPE) {
        return false;
    }
    if (frame.size() != sizeof(protocol::MessageHeader) + header.length) {
        return false;
    }
    session_id = header.sessionId;
    BodyReader r(frame, sizeof(protocol::MessageHeader));
    return decode(r, out) && r.done();
}

inline bool decodeFrameLegacyStrict(const std::vector<std::uint8_t>& frame,
                                    ScoreCreditRequest& out,
                                    protocol::SessionID& session_id) {
    return decodeFrameLegacyStrictImpl(
        frame, out, session_id,
        [](BodyReader& r, ScoreCreditRequest& m) { return decodeBodyLegacyStrict(r, m); });
}

inline bool decodeFrameLegacyStrict(const std::vector<std::uint8_t>& frame,
                                    ScoreCreditReply& out,
                                    protocol::SessionID& session_id) {
    return decodeFrameLegacyStrictImpl(
        frame, out, session_id,
        [](BodyReader& r, ScoreCreditReply& m) { return decodeBodyLegacyStrict(r, m); });
}

inline std::vector<std::uint8_t> encodeFrame(const ScoreCreditRequest& m,
                                             protocol::SessionID session_id = 0) {
    return migration::encodeFrame(m, session_id);
}

inline std::vector<std::uint8_t> encodeFrame(const ScoreCreditReply& m,
                                             protocol::SessionID session_id = 0) {
    return migration::encodeFrame(m, session_id);
}

inline bool decodeFrame(const std::vector<std::uint8_t>& frame, ScoreCreditRequest& out,
                        protocol::SessionID& session_id) {
    return migration::decodeFrame(frame, out, session_id);
}

inline bool decodeFrame(const std::vector<std::uint8_t>& frame, ScoreCreditReply& out,
                        protocol::SessionID& session_id) {
    return migration::decodeFrame(frame, out, session_id);
}

} // namespace business

} // namespace player_store
