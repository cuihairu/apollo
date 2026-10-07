#include "apollo/game/session/directory_mirror.hpp"

#include <algorithm>
#include <cstring>

namespace apollo::game::session {

namespace {

// 定长小端序读写原语（与 net/discovery wire 同纪律：逐字段编码不 memcpy
// 结构体，防对齐/端序漂移）
void put_u16(std::uint8_t* p, std::uint16_t v) noexcept {
    p[0] = static_cast<std::uint8_t>(v & 0xFF);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
}

void put_u32(std::uint8_t* p, std::uint32_t v) noexcept {
    for (int i = 0; i < 4; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

void put_u64(std::uint8_t* p, std::uint64_t v) noexcept {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }
}

std::uint16_t get_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t get_u32(const std::uint8_t* p) noexcept {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        v |= static_cast<std::uint32_t>(p[i]) << (8 * i);
    }
    return v;
}

std::uint64_t get_u64(const std::uint8_t* p) noexcept {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    }
    return v;
}

void put_header(std::uint8_t* p, DirectoryWireKind kind) noexcept {
    put_u32(p, kDirectoryWireMagic);
    put_u16(p + 4, kDirectoryWireVersion);
    p[6] = static_cast<std::uint8_t>(kind);
    p[7] = 0;  // reserved
}

bool header_matches(const std::uint8_t* data, std::size_t len,
                    DirectoryWireKind kind) {
    if (len < kDirectoryWireHeaderSize) {
        return false;
    }
    return get_u32(data) == kDirectoryWireMagic &&
           get_u16(data + 4) == kDirectoryWireVersion &&
           data[6] == static_cast<std::uint8_t>(kind);
}

void put_entry(std::uint8_t* p, const MirrorEntry& e) noexcept {
    put_u64(p, e.player_id);
    put_u64(p + 8, e.anchor_epoch);
    put_u64(p + 16, e.session_id);
    put_u32(p + 24, e.gateway_id);
    put_u32(p + 28, e.zone_id);
    p[32] = static_cast<std::uint8_t>(e.state);
    put_u32(p + 33, e.assignment.world_id);
    put_u64(p + 37, e.assignment.map_id);
    put_u64(p + 45, e.assignment.instance_id);
    put_u64(p + 53, e.assignment.space_id);
    put_u64(p + 61, e.assignment.route_version);
}

MirrorEntry get_entry(const std::uint8_t* p) noexcept {
    MirrorEntry e;
    e.player_id = get_u64(p);
    e.anchor_epoch = get_u64(p + 8);
    e.session_id = get_u64(p + 16);
    e.gateway_id = get_u32(p + 24);
    e.zone_id = get_u32(p + 28);
    const auto raw_state = p[32];
    if (raw_state <= static_cast<std::uint8_t>(PlayerDirectory::EntryState::Leaving)) {
        e.state = static_cast<PlayerDirectory::EntryState>(raw_state);
    }
    e.assignment.world_id = get_u32(p + 33);
    e.assignment.map_id = get_u64(p + 37);
    e.assignment.instance_id = get_u64(p + 45);
    e.assignment.space_id = get_u64(p + 53);
    e.assignment.route_version = get_u64(p + 61);
    return e;
}

constexpr std::size_t kDeltaBodySize = 4 + 1 + 4 + kDirectoryEntryWireSize;  // 78
constexpr std::size_t kDeltaSize = kDirectoryWireHeaderSize + kDeltaBodySize;  // 86

} // namespace

// ---- 编解码 ----

std::size_t encode_delta(const DirectoryDeltaBody& body, std::uint8_t* buf,
                         std::size_t cap) {
    if (buf == nullptr || cap < kDeltaSize) {
        return 0;
    }
    std::memset(buf, 0, kDeltaSize);
    put_header(buf, DirectoryWireKind::Delta);
    put_u32(buf + 8, body.seq);
    buf[12] = static_cast<std::uint8_t>(body.event_kind);
    put_u32(buf + 13, body.reason);
    put_entry(buf + 17, body.entry);
    return kDeltaSize;
}

bool decode_delta(const std::uint8_t* data, std::size_t len,
                  DirectoryDeltaBody& out) {
    if (!header_matches(data, len, DirectoryWireKind::Delta) || len != kDeltaSize) {
        return false;
    }
    out.seq = get_u32(data + 8);
    const auto raw_kind = data[12];
    if (raw_kind > static_cast<std::uint8_t>(PlayerDirectory::Event::Kind::SessionKicked)) {
        return false;  // event_kind 越界整包丢
    }
    out.event_kind = static_cast<PlayerDirectory::Event::Kind>(raw_kind);
    out.reason = get_u32(data + 13);
    out.entry = get_entry(data + 17);
    return true;
}

std::size_t encode_snapshot_request(std::uint8_t* buf, std::size_t cap) {
    if (buf == nullptr || cap < kDirectoryWireHeaderSize) {
        return 0;
    }
    std::memset(buf, 0, kDirectoryWireHeaderSize);
    put_header(buf, DirectoryWireKind::SnapshotRequest);
    return kDirectoryWireHeaderSize;
}

bool is_snapshot_request(const std::uint8_t* data, std::size_t len) {
    return header_matches(data, len, DirectoryWireKind::SnapshotRequest) &&
           len == kDirectoryWireHeaderSize;
}

std::size_t encode_snapshot_part(const DirectorySnapshotPart& part,
                                 std::uint8_t* buf, std::size_t cap) {
    const std::size_t need = kDirectoryWireHeaderSize + 14 +
                             part.entries.size() * kDirectoryEntryWireSize;
    if (buf == nullptr || cap < need) {
        return 0;
    }
    if (part.entries.size() > kMaxSnapshotPartEntries) {
        return 0;
    }
    std::memset(buf, 0, need);
    put_header(buf, DirectoryWireKind::SnapshotReply);
    put_u32(buf + 8, part.snapshot_id);
    put_u16(buf + 12, part.total_parts);
    put_u16(buf + 14, part.part_index);
    put_u32(buf + 16, part.baseline_seq);
    put_u16(buf + 20, static_cast<std::uint16_t>(part.entries.size()));
    for (std::size_t i = 0; i < part.entries.size(); ++i) {
        put_entry(buf + 22 + i * kDirectoryEntryWireSize, part.entries[i]);
    }
    return need;
}

bool decode_snapshot_part(const std::uint8_t* data, std::size_t len,
                          DirectorySnapshotPart& out) {
    if (!header_matches(data, len, DirectoryWireKind::SnapshotReply) ||
        len < kDirectoryWireHeaderSize + 14) {
        return false;
    }
    const std::uint16_t count = get_u16(data + 20);
    if (count > kMaxSnapshotPartEntries ||
        len != kDirectoryWireHeaderSize + 14 +
                   static_cast<std::size_t>(count) * kDirectoryEntryWireSize) {
        return false;  // 条目数越界 / 长度与声明不符整包丢
    }
    out.snapshot_id = get_u32(data + 8);
    out.total_parts = get_u16(data + 12);
    out.part_index = get_u16(data + 14);
    out.baseline_seq = get_u32(data + 16);
    out.entries.clear();
    out.entries.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i) {
        out.entries.push_back(get_entry(data + 22 + i * kDirectoryEntryWireSize));
    }
    if (out.total_parts == 0 || out.part_index >= out.total_parts) {
        return false;
    }
    return true;
}

// ---- DirectoryPublisher ----

DirectoryPublisher::DirectoryPublisher(SendFn send)
    : send_(std::move(send)) {
}

void DirectoryPublisher::publish(const PlayerDirectory::Event& event) {
    if (!send_ || event.player_id == 0) {
        return;
    }
    DirectoryDeltaBody body;
    body.seq = ++seq_;
    body.event_kind = event.kind;
    body.reason = event.reason;
    MirrorEntry& e = body.entry;
    e.player_id = event.player_id;
    e.anchor_epoch = event.anchor_epoch;
    e.session_id = event.binding.session_id;
    e.gateway_id = event.binding.gateway_id;
    e.zone_id = event.zone_id;
    e.state = PlayerDirectory::EntryState::Online;  // Up 即 Online；Down/Kicked
                                                    // 走 erase，状态列不入增量
    e.assignment = event.assignment;
    std::uint8_t buf[kDeltaSize];
    const std::size_t len = encode_delta(body, buf, sizeof(buf));
    if (len != 0) {
        (void)send_(buf, len);
    }
}

std::size_t DirectoryPublisher::publish_snapshot(
    const std::vector<MirrorEntry>& authoritative) {
    if (!send_) {
        return 0;
    }
    const std::uint32_t snapshot_id = next_snapshot_id_++;
    std::size_t total_parts =
        (authoritative.size() + kMaxSnapshotPartEntries - 1) / kMaxSnapshotPartEntries;
    if (total_parts == 0) {
        total_parts = 1;  // 空表 = 一片空快照（镜像清空 + seq 基线同步）
    }
    std::size_t sent = 0;
    for (std::size_t part = 0; part < total_parts; ++part) {
        DirectorySnapshotPart p;
        p.snapshot_id = snapshot_id;
        p.total_parts = static_cast<std::uint16_t>(total_parts);
        p.part_index = static_cast<std::uint16_t>(part);
        p.baseline_seq = seq_;
        const auto begin = authoritative.begin() +
                           static_cast<std::ptrdiff_t>(part * kMaxSnapshotPartEntries);
        const auto end = authoritative.begin() +
                         static_cast<std::ptrdiff_t>(
                             std::min(authoritative.size(),
                                      (part + 1) * kMaxSnapshotPartEntries));
        p.entries.assign(begin, end);
        std::uint8_t buf[kDirectoryWireHeaderSize + 14 +
                         kMaxSnapshotPartEntries * kDirectoryEntryWireSize];
        const std::size_t len = encode_snapshot_part(p, buf, sizeof(buf));
        if (len != 0 && send_(buf, len)) {
            ++sent;
        }
    }
    return sent;
}

// ---- DirectoryMirror ----

void DirectoryMirror::set_snapshot_requester(SnapshotRequester requester) {
    requester_ = std::move(requester);
}

void DirectoryMirror::enter_stale() {
    if (!stale_) {
        stale_ = true;
        if (!request_inflight_) {
            request_inflight_ = true;
            if (requester_) {
                requester_();  // 每轮断档请求一次（快照到货复位）
            }
        }
    }
}

void DirectoryMirror::on_datagram(const std::uint8_t* data, std::size_t len) {
    if (data == nullptr || len < kDirectoryWireHeaderSize) {
        return;
    }
    if (is_snapshot_request(data, len)) {
        return;  // 请求面只对 owner 有意义，消费侧静默丢
    }
    if (data[6] == static_cast<std::uint8_t>(DirectoryWireKind::Delta)) {
        DirectoryDeltaBody body;
        if (decode_delta(data, len, body)) {
            apply_delta(body);
        }
        return;
    }
    if (data[6] == static_cast<std::uint8_t>(DirectoryWireKind::SnapshotReply)) {
        DirectorySnapshotPart part;
        if (decode_snapshot_part(data, len, part)) {
            apply_snapshot_part(part);
        }
    }
}

void DirectoryMirror::apply_delta(const DirectoryDeltaBody& body) {
    // seq 续传纪律（§7）：连续即应用；重复/迟到丢；断档 stale 等快照。
    // 首包以到包 seq 为基线（晚加入投影由周期快照收敛——骨架口径）。
    if (!has_seq_) {
        last_seq_ = body.seq;
        has_seq_ = true;
    } else if (body.seq <= last_seq_) {
        return;  // 重复/迟到（set 语义天然幂等，直接丢）
    } else if (body.seq != last_seq_ + 1) {
        enter_stale();
        return;
    }
    last_seq_ = body.seq;

    switch (body.event_kind) {
    case PlayerDirectory::Event::Kind::SessionUp:
        entries_[body.entry.player_id] = body.entry;  // 权威行覆盖（顶号流 =
                                                      // Kicked erase + Up 写入）
        break;
    case PlayerDirectory::Event::Kind::SessionDown:
    case PlayerDirectory::Event::Kind::SessionKicked:
        entries_.erase(body.entry.player_id);
        break;
    case PlayerDirectory::Event::Kind::SessionMoved: {
        auto it = entries_.find(body.entry.player_id);
        if (it != entries_.end()) {
            it->second.assignment = body.entry.assignment;  // 只动定位列
        }
        break;
    }
    }
}

void DirectoryMirror::apply_snapshot_part(const DirectorySnapshotPart& part) {
    // 分片重组：严格按 part_index 升序收（同 snapshot_id）；乱序/换 id 即
    // 重启重组（丢完整个快照——下一周期快照再来，不拼半套）
    if (part.total_parts == 0 || part.part_index >= part.total_parts) {
        return;
    }
    if (part.part_index == 0) {
        if (assembling_snapshot_id_ != part.snapshot_id || next_part_index_ != 0) {
            assembling_snapshot_id_ = part.snapshot_id;
            assembled_parts_ = 0;
            assembled_entries_.clear();
            next_part_index_ = 0;
        }
    } else if (assembling_snapshot_id_ != part.snapshot_id ||
               next_part_index_ != part.part_index) {
        return;  // 乱序/换 id：非起始片无处挂靠，丢弃
    }
    assembled_entries_.insert(assembled_entries_.end(), part.entries.begin(),
                              part.entries.end());
    ++assembled_parts_;
    next_part_index_ = static_cast<std::uint16_t>(part.part_index + 1);
    if (assembled_parts_ != part.total_parts) {
        return;
    }

    // 重组完成：快照为权威态全量重置（§2 对账失配走快照重置的标准路径）
    entries_.clear();
    for (const auto& e : assembled_entries_) {
        entries_[e.player_id] = e;
    }
    last_seq_ = part.baseline_seq;
    has_seq_ = true;
    stale_ = false;
    request_inflight_ = false;
    assembling_snapshot_id_ = 0;
    assembled_parts_ = 0;
    next_part_index_ = 0;
    assembled_entries_.clear();
}

const MirrorEntry* DirectoryMirror::find(std::uint64_t player_id) const {
    const auto it = entries_.find(player_id);
    return it != entries_.end() ? &it->second : nullptr;
}

std::vector<std::uint64_t> DirectoryMirror::online_ids() const {
    std::vector<std::uint64_t> ids;
    ids.reserve(entries_.size());
    for (const auto& [player_id, entry] : entries_) {
        if (entry.state == PlayerDirectory::EntryState::Online) {
            ids.push_back(player_id);
        }
    }
    return ids;
}

} // namespace apollo::game::session
