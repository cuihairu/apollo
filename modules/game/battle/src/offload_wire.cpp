#include "apollo/game/battle/offload_wire.hpp"

namespace apollo::game::battle::offload {

namespace {

// ---- 写端（逐字节小端，纪律同 discovery wire）----

void put_u16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put_u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void put_u64(std::vector<std::uint8_t>& b, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void put_i32(std::vector<std::uint8_t>& b, std::int32_t v) {
    put_u32(b, static_cast<std::uint32_t>(v));
}

void put_i64(std::vector<std::uint8_t>& b, std::int64_t v) {
    put_u64(b, static_cast<std::uint64_t>(v));
}

void put_bool(std::vector<std::uint8_t>& b, bool v) {
    b.push_back(v ? 1 : 0);
}

void put_str(std::vector<std::uint8_t>& b, const std::string& s) {
    put_u32(b, static_cast<std::uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

void put_bytes(std::vector<std::uint8_t>& b, const std::vector<std::uint8_t>& v) {
    put_u32(b, static_cast<std::uint32_t>(v.size()));
    b.insert(b.end(), v.begin(), v.end());
}

void put_i64_list(std::vector<std::uint8_t>& b, const std::vector<std::int64_t>& v) {
    put_u32(b, static_cast<std::uint32_t>(v.size()));
    for (const auto x : v) {
        put_i64(b, x);
    }
}

// ---- 读端（严格定长截取：越界即拒）----

class Reader {
public:
    Reader(const std::vector<std::uint8_t>& wire) : wire_(wire) {}

    bool u16(std::uint16_t& out) {
        if (off_ + 2 > wire_.size()) {
            return false;
        }
        out = static_cast<std::uint16_t>(wire_[off_]) |
              static_cast<std::uint16_t>(wire_[off_ + 1] << 8);
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

    bool i32(std::int32_t& out) {
        std::uint32_t v = 0;
        if (!u32(v)) {
            return false;
        }
        out = static_cast<std::int32_t>(v);
        return true;
    }

    bool i64(std::int64_t& out) {
        std::uint64_t v = 0;
        if (!u64(v)) {
            return false;
        }
        out = static_cast<std::int64_t>(v);
        return true;
    }

    bool boolean(bool& out) {
        if (off_ + 1 > wire_.size()) {
            return false;
        }
        out = wire_[off_] != 0;
        off_ += 1;
        return true;
    }

    bool str(std::string& out) {
        std::uint32_t n = 0;
        if (!u32(n) || off_ + n > wire_.size()) {
            return false;
        }
        out.assign(reinterpret_cast<const char*>(wire_.data() + off_), n);
        off_ += n;
        return true;
    }

    bool bytes(std::vector<std::uint8_t>& out) {
        std::uint32_t n = 0;
        if (!u32(n) || off_ + n > wire_.size()) {
            return false;
        }
        out.assign(wire_.begin() + static_cast<std::ptrdiff_t>(off_),
                   wire_.begin() + static_cast<std::ptrdiff_t>(off_ + n));
        off_ += n;
        return true;
    }

    bool i64_list(std::vector<std::int64_t>& out) {
        std::uint32_t n = 0;
        if (!u32(n)) {
            return false;
        }
        out.clear();
        for (std::uint32_t i = 0; i < n; ++i) {
            std::int64_t x = 0;
            if (!i64(x)) {
                return false;
            }
            out.push_back(x);
        }
        return true;
    }

    bool drained() const { return off_ == wire_.size(); }

private:
    const std::vector<std::uint8_t>& wire_;
    std::size_t off_ = 0;
};

} // namespace

std::vector<std::uint8_t> encode(const InstanceSpawnRequest& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.instance_id);
    put_u64(b, m.scene_id);
    put_u64(b, m.map_id);
    put_i32(b, m.tick_interval_ms);
    put_i32(b, m.participant_count);
    put_str(b, m.spawn_args);
    return b;
}

std::vector<std::uint8_t> encode(const InstanceSpawnReply& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.instance_id);
    put_bool(b, m.accepted);
    put_str(b, m.error_message);
    return b;
}

std::vector<std::uint8_t> encode(const BattleInputMsg& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.instance_id);
    put_u64(b, m.player_id);
    put_i64(b, m.tick);
    put_i64_list(b, m.opcodes);
    return b;
}

std::vector<std::uint8_t> encode(const BattleStateSync& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.instance_id);
    put_i64(b, m.tick);
    put_u64(b, m.snapshot_seq);
    put_i64_list(b, m.players);
    put_bytes(b, m.payload);
    return b;
}

std::vector<std::uint8_t> encode(const BattleResult& m) {
    std::vector<std::uint8_t> b;
    put_u64(b, m.instance_id);
    put_i64(b, m.tick);
    put_i64(b, m.replay_hash);
    put_i64_list(b, m.player_ids);
    put_i64_list(b, m.scores);
    return b;
}

bool decode(const std::vector<std::uint8_t>& wire, InstanceSpawnRequest& out) {
    Reader r(wire);
    if (!r.u64(out.instance_id) || !r.u64(out.scene_id) || !r.u64(out.map_id) ||
        !r.i32(out.tick_interval_ms) || !r.i32(out.participant_count) ||
        !r.str(out.spawn_args) || !r.drained()) {
        return false;
    }
    return true;
}

bool decode(const std::vector<std::uint8_t>& wire, InstanceSpawnReply& out) {
    Reader r(wire);
    if (!r.u64(out.instance_id) || !r.boolean(out.accepted) ||
        !r.str(out.error_message) || !r.drained()) {
        return false;
    }
    return true;
}

bool decode(const std::vector<std::uint8_t>& wire, BattleInputMsg& out) {
    Reader r(wire);
    if (!r.u64(out.instance_id) || !r.u64(out.player_id) || !r.i64(out.tick) ||
        !r.i64_list(out.opcodes) || !r.drained()) {
        return false;
    }
    return true;
}

bool decode(const std::vector<std::uint8_t>& wire, BattleStateSync& out) {
    Reader r(wire);
    if (!r.u64(out.instance_id) || !r.i64(out.tick) || !r.u64(out.snapshot_seq) ||
        !r.i64_list(out.players) || !r.bytes(out.payload) || !r.drained()) {
        return false;
    }
    return true;
}

bool decode(const std::vector<std::uint8_t>& wire, BattleResult& out) {
    Reader r(wire);
    if (!r.u64(out.instance_id) || !r.i64(out.tick) || !r.i64(out.replay_hash) ||
        !r.i64_list(out.player_ids) || !r.i64_list(out.scores) || !r.drained()) {
        return false;
    }
    return true;
}

bool is_offload_msg(apollo::contract::gen::MsgId msg) {
    const auto id = static_cast<std::uint16_t>(msg);
    return id >= 919 && id <= 923;
}

std::vector<std::uint8_t> encode_envelope(const OffloadEnvelope& envelope) {
    std::vector<std::uint8_t> b;
    put_u16(b, static_cast<std::uint16_t>(envelope.msg));
    b.insert(b.end(), envelope.payload.begin(), envelope.payload.end());
    return b;
}

bool decode_envelope(const std::vector<std::uint8_t>& wire, OffloadEnvelope& out) {
    Reader r(wire);
    std::uint16_t id = 0;
    if (!r.u16(id)) {
        return false;
    }
    const auto msg = static_cast<apollo::contract::gen::MsgId>(id);
    if (!is_offload_msg(msg)) {
        return false;
    }
    out.msg = msg;
    out.payload.assign(wire.begin() + 2, wire.end());
    return true;
}

} // namespace apollo::game::battle::offload
