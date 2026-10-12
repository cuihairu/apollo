#pragma once

// 玩家迁移 wire 面（P0-2 路 2 第一步，backup-revive §9.2 五步 ①②③ zone
// 侧受理）：契约 933-938 六条的编解码。id 钉生成器 MsgId（offload_wire
// 纪律——不手写第二份协议号）；全帧 = MessageCodec 头 + 小端 body（读端
// 严格定长截取：越界即拒、尾部余字节即拒）。M1 base 受理 handler 按帧型
// 分流（933/935/937 → BaseServer 迁移面；其余 dispatchRequest 直通）。
// header-only 直编习语（app 域件，不进 game_session 模块）。

#include "apollo/protocol/codec.hpp"
#include "apollo_contract.h"

#include <cstdint>
#include <string>
#include <vector>

namespace base {
namespace migration {

static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_freeze_request) == 933);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_freeze_reply) == 934);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_snapshot_request) == 935);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_snapshot_reply) == 936);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_handoff_request) == 937);
static_assert(static_cast<std::uint16_t>(apollo::contract::gen::MsgId::player_handoff_reply) == 938);

// ---- 载荷结构（字段序 = 契约 933-938 字段序）----

struct FreezeRequest {
    static constexpr std::uint16_t TYPE = 933;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;   // 编排关联键（manager 单调分配）
};

struct FreezeReply {
    static constexpr std::uint16_t TYPE = 934;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;
    bool accepted = false;          // 1=停写生效（Transferring + journal 段已排）
    std::string error_message;
};

struct SnapshotRequest {
    static constexpr std::uint16_t TYPE = 935;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;
};

struct SnapshotReply {
    static constexpr std::uint16_t TYPE = 936;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;
    bool success = false;           // 1=快照就绪（旧的先 flush 再发）
    std::string error_message;
    std::string profile_json;       // 玩家档案 JSON（ADR-007 原样携带）
    std::uint64_t session_id = 0;   // 锚点会话号（0 = 无会话）
    std::uint32_t gateway_id = 0;
    std::uint32_t home_zone_id = 0;
};

struct HandoffRequest {
    static constexpr std::uint16_t TYPE = 937;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;
    std::uint64_t new_epoch = 0;    // manager 目录 advance 分配
    std::uint64_t session_id = 0;   // resume 双锚
    std::uint32_t gateway_id = 0;
};

struct HandoffReply {
    static constexpr std::uint16_t TYPE = 938;
    std::uint64_t player_id = 0;
    std::uint64_t request_id = 0;
    bool accepted = false;          // 1=接权（新 epoch 生效，此后不可撤销）
    std::string error_message;
};

// ---- 写端（小端；put 族 offload_wire 同型）----

inline void put_u16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    for (int i = 0; i < 2; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

inline void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

inline void put_u64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

inline void put_bool(std::vector<std::uint8_t>& b, bool v) {
    b.push_back(v ? 1 : 0);
}

inline void put_str(std::vector<std::uint8_t>& b, const std::string& s) {
    put_u32(b, static_cast<std::uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

inline std::vector<std::uint8_t> encodeBody(const FreezeRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const FreezeReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const SnapshotRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const SnapshotReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    put_bool(b, m.success);
    put_str(b, m.error_message);
    put_str(b, m.profile_json);
    put_u64(b, m.session_id);
    put_u32(b, m.gateway_id);
    put_u32(b, m.home_zone_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const HandoffRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    put_u64(b, m.new_epoch);
    put_u64(b, m.session_id);
    put_u32(b, m.gateway_id);
    return b;
}

inline std::vector<std::uint8_t> encodeBody(const HandoffReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.player_id);
    put_u64(b, m.request_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    return b;
}

// 全帧拼装（MessageCodec 头 + body；sequence 不参与 rpc 关联——corr 在
// linkrpc 层，置 0）
template <typename T>
std::vector<std::uint8_t> encodeFrame(const T& msg, protocol::SessionID session_id = 0) {
    auto body = encodeBody(msg);
    protocol::MessageHeader header;
    header.magic = protocol::MessageHeader::MAGIC;
    header.version = protocol::MessageHeader::CURRENT_VERSION;
    header.type = T::TYPE;
    header.length = static_cast<std::uint32_t>(body.size());
    header.sequence = 0;
    header.sessionId = session_id;
    auto head = protocol::MessageCodec::encodeHeader(header);
    std::vector<std::uint8_t> frame;
    frame.reserve(head.size() + body.size());
    frame.insert(frame.end(), head.begin(), head.end());
    frame.insert(frame.end(), body.begin(), body.end());
    return frame;
}

// ---- 读端（严格定长截取：越界即拒；尾部余字节由 done() 拒）----

class BodyReader {
public:
    explicit BodyReader(const std::vector<std::uint8_t>& wire, std::size_t off = 0)
        : wire_(wire), off_(off) {}

    bool u16(std::uint16_t& out) {
        if (off_ + 2 > wire_.size()) {
            return false;
        }
        out = static_cast<std::uint16_t>(wire_[off_]) |
              static_cast<std::uint16_t>(static_cast<std::uint16_t>(wire_[off_ + 1]) << 8);
        off_ += 2;
        return true;
    }

    bool u32(std::uint32_t& out) {
        if (off_ + 4 > wire_.size()) {
            return false;
        }
        out = 0;
        for (int i = 0; i < 4; ++i) {
            out |= static_cast<std::uint32_t>(wire_[off_ + i]) << (8 * i);
        }
        off_ += 4;
        return true;
    }

    bool u64(std::uint64_t& out) {
        if (off_ + 8 > wire_.size()) {
            return false;
        }
        out = 0;
        for (int i = 0; i < 8; ++i) {
            out |= static_cast<std::uint64_t>(wire_[off_ + i]) << (8 * i);
        }
        off_ += 8;
        return true;
    }

    bool boolean(bool& out) {
        std::uint8_t v = 0;
        if (off_ + 1 > wire_.size()) {
            return false;
        }
        v = wire_[off_++];
        out = v != 0;
        return true;
    }

    bool str(std::string& out) {
        std::uint32_t n = 0;
        if (!u32(n) || off_ + n > wire_.size()) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(wire_.data()) + off_, n);
        off_ += n;
        return true;
    }

    bool done() const { return off_ == wire_.size(); }

private:
    const std::vector<std::uint8_t>& wire_;
    std::size_t off_;
};

inline bool decodeBody(BodyReader& r, FreezeRequest& m) {
    return r.u64(m.player_id) && r.u64(m.request_id);
}

inline bool decodeBody(BodyReader& r, FreezeReply& m) {
    return r.u64(m.player_id) && r.u64(m.request_id) && r.boolean(m.accepted)
           && r.str(m.error_message);
}

inline bool decodeBody(BodyReader& r, SnapshotRequest& m) {
    return r.u64(m.player_id) && r.u64(m.request_id);
}

inline bool decodeBody(BodyReader& r, SnapshotReply& m) {
    return r.u64(m.player_id) && r.u64(m.request_id) && r.boolean(m.success)
           && r.str(m.error_message) && r.str(m.profile_json) && r.u64(m.session_id)
           && r.u32(m.gateway_id) && r.u32(m.home_zone_id);
}

inline bool decodeBody(BodyReader& r, HandoffRequest& m) {
    return r.u64(m.player_id) && r.u64(m.request_id) && r.u64(m.new_epoch)
           && r.u64(m.session_id) && r.u32(m.gateway_id);
}

inline bool decodeBody(BodyReader& r, HandoffReply& m) {
    return r.u64(m.player_id) && r.u64(m.request_id) && r.boolean(m.accepted)
           && r.str(m.error_message);
}

// 全帧解析（type 必须恰为 T::TYPE；帧长必须与 header.length 一致；
// parseHeader 抛错面（截断/魔数）在此收为 false）。
template <typename T>
bool decodeFrame(const std::vector<std::uint8_t>& frame, T& out,
                 protocol::SessionID& session_id) {
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
    return decodeBody(r, out) && r.done();
}

} // namespace migration
} // namespace base
