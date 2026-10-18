#pragma once

// 结算分值入账受理 wire 面（P1 基础业务与结算迁入；契约 958/959）：World
// 玩法域结算以「返回业务请求」落账——锚点写权在 Base（L19 资料归属切换后
// World 不跨界持锚点指针）。拨号底座 = StoreDial（同 migration 族）；Base
// 受理面 = PlayerStore M1 dispatch（同 933/935/937 分流）。
//
// 幂等口径：重复投递去重归调用方账本（World 侧 SettlementLedger applied
// 标记 + retry 补账），Base 只受理不回放判定——与本地锚点落账同语义
// （AnchorRewardSink 幂等面 = exp 键 + mark_dirty 随 SaveQueue 落档）。
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
    std::uint64_t request_id = 0;    // 调用方关联号（账本幂等在调用方）
    std::uint64_t player_id = 0;
    std::int64_t score = 0;          // 结算分值（入 exp 键）
    std::uint32_t source_zone_id = 0; // 发起玩法域（排查面）
};

struct ScoreCreditReply {
    static constexpr std::uint16_t TYPE = 959;
    std::uint64_t request_id = 0;
    std::uint64_t player_id = 0;
    bool accepted = false;           // 1=已入锚点长期态；0=暂不可落（挂账待补）
    std::string error_message;
};

inline std::vector<std::uint8_t> encodeBody(const ScoreCreditRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_u64(b, static_cast<std::uint64_t>(m.score));
    put_u32(b, m.source_zone_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const ScoreCreditReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.request_id);
    put_u64(b, m.player_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    return b;
}

inline bool decodeBody(BodyReader& r, ScoreCreditRequest& m) {
    std::uint64_t score = 0;
    return r.u64(m.request_id) && r.u64(m.player_id) && r.u64(score)
           && r.u32(m.source_zone_id)
           && (m.score = static_cast<std::int64_t>(score), true);
}

inline bool decodeBody(BodyReader& r, ScoreCreditReply& m) {
    return r.u64(m.request_id) && r.u64(m.player_id) && r.boolean(m.accepted)
           && r.str(m.error_message);
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
